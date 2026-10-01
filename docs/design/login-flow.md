# Apollo 登录链路整体设计（login-flow）

> 状态：设计稿（评审中）。定位：**从客户端启动到进入世界的完整登录链**——账号鉴权、login_token 签发/验签/核销、选服/排队/准入、契约包下发时机、与在线目录的登记衔接。缺口登记 design-gap-inventory #13（B11 批，2026-10-01 落盘）。互引：net-abstraction §5.9（login_token/握手族）、§7（G-1/manager 准入）、§5.7（InterServerLink）；session-and-online-directory（#12——登记锚点/顶号/窗口，本设计的直接下游）；sdk-contract §11（消息域）/§11.6（契约包）；attribute-sync §8.2（storage.xml 账号表）；scripting-lua §7.5（GM 踢人执行位）；inbound-interfaces（#14 同批——账号绑定同域）；capacity-and-benchmark（洪峰容量）；architecture-review §25（存量实读）/§26（设计批输入六问）。

---

## 执行摘要

1. **两阶段连接，login-app 独立进程不经 gateway**：阶段一 = 登录连接（client ↔ login-app，独立端口、短连接、完成鉴权与 token 签发即断开）；阶段二 = 游戏连接（client ↔ gateway-app，ClientHello 携 login_token——§5.9 既有口径）。「拓扑入口 = gateway」指游戏会话面；登录连接是前置短面（BW/KBE loginapp 独立进程同型二十年先例，36号 :55/:66）。理由：账号凭据敏感面不进 gateway；登录/重连洪峰与四通道游戏流量故障域、容量域隔离（CCU 风暴打 login-app 不打 gateway）；login-app 无会话态可横向扩。
2. **登录连接也是加密的（LoginHello 握手族）**：§5.9 的 ClientHello **带 login_token**（token 已在手才握手）——token 从哪来是本设计补的蛋生蛋缺口：登录连接先走**匿名 X25519 握手**（无 token，凭据在 AEAD 内传），算法族与 §5.9 完全同源（X25519 + HKDF-SHA256，info = `"apollo-login-v1"` 域分离），凭据不过明文（KBE loginapp 裸 TCP 明文是老引擎通病，不学）。
3. **login_token = HMAC-SHA256 自包含票据 + manager 准入临界区一次性核销**：payload = {account_id, issue/exp_tick, nonce, gateway_id, zone_hint, purpose}，短 TTL（默认 60s 建议，容量批校准）；gateway **本地验签**（不触 login-app/账号库——§5.9 :313 意向兑现；存量 gateway RPC 回问是错位实现，迁移归代码批）；**一次性核销点 = manager 准入临界区**（pending 准入表记 nonce，SessionUp 转正式条目——与 #12 登记锚点同临界区白拿，零新增状态面；重放 = nonce 失配拒绝 + violation_score）。
4. **鉴权归 login-app，准入/选服/顶号/排队归 manager（单点串行）**：login-app 鉴权通过后发一次 `AdmissionRequest`（RequestReply 低频控制面）→ manager 临界区内四连：负载闸门（LoginConditions 同型，net §7 :389/:395）→ 顶号预裁（#2 §3 临界区：旧条目 Removed + 踢除下发）→ 最轻落点 Zone（minAppLoad 同型）→ pending 表记 nonce → 返回 {gateway_addr, zone_hint, world_assignment 初值}。login-app 无目录、无负载表（存量 GatewayAllocator 的网关选择职能并入 manager——全局视图只在 manager）。
5. **账号域 = DB 表（批次 2 storage.xml 语句集）+ P1 内存降级**：accounts 表（account_id/credential_salt/credential_hash/state/banned_until/last_login）+ third_party_bindings 表（#14 共享同域）；凭据哈希 = PBKDF2-HMAC-SHA256（OpenSSL 内建——§5.9 单一 crypto 源不破，不引 bcrypt/argon2）；P1 降级 = 进程内测试账号表（存量 :22-41 硬编码形态的合法退化位，明文比较禁）。风控消费：violation_score 断开 + 账号级风控记录（§4.3 :128）在准入闸门检查（banned_until）。
6. **排队 = P3 组件，P1-P2 拒绝码**：过载返回 busy + retry_after（BW LoginConditions 即拒绝式，KBE 同——排队非两家内建）；P3 = manager 侧准入队列（水位驱动、queue_position 事件经 login-app 下发，消息位预留）。「排队是国产 MMO 特有需求、两家先例没有」——设计成组件而非默认件。
7. **契约包下发 = 指针制，login-app 不做文件服务**：登录响应携带 {client_hash, bundle_url}；客户端比对本地 manifest（§11.6 三件套逐文件 SHA-256），失配走 bundle_url 拉取——CI contract_pack 已产 `dist/client/<client_hash>/`（sdk-contract §12.2）。KBE clientsdk_downloader 的「引擎即 CDN」形态不采（apollo 有 CI 组包管线，引擎不下发字节只发指针）；P1 退化 = 配置态静态 URL。
8. **全链与 #12 咬合**：鉴权（login-app）→ 准入+顶号预裁+落点（manager 临界区）→ token 签发（login-app）→ ClientHello 验签（gateway）→ Zone 创建会话实体 → SessionUp → 目录可见（#2 §2 锚点）——**准入前的链路归本设计，准入后的登记/窗口/顶号执行归 #12**，边界即 #12 :71 的声明线。存量 login-app 884 行烟囱（§25 实读四点错位）不推翻：定位为 P1 演示件，迁移面 §10 逐项登记归代码批。

## 0. 现状与缺口

- **设计面**：net-abstraction §5.9 只有 login_token 一行归属（:313）与握手序列（:302-311，且以 token 已在手为前提）；选服/排队/准入只有 manager 闸门半句（§7 :395）；SDK 下发时机零设计——gap #13 证据三处一句带过（复审见 architecture-review §25）。
- **存量面（§25 实读）**：apps/login-app 884 行零框架烟囱——硬编码明文鉴权（login_server.cpp:27 自注该用 hash）、自造 xorshift PRNG ticket（:284-301）、滑动续期非一次性（:245，与「短 TTL+一次性」冲突）、preparePlayerOnline 直呼 base 三连绕过 gateway/manager 且 catch 吞异常假成功（:476-542/:536-541）；gateway ingress 准入 = RPC 回问 login-app（session_admission_service.cpp:33）而非验签。四点错位 = 缺口判据 (b) 存量冒充实锤；**迁移归代码批（§10），源码冻结不动**。
- **先例**：BW = 客户端先连 loginapp 被指派 baseapp（36号 :55）、mgr 持负载分流（baseappmgr :1117 上报 loginapp）；KBE = loginapp 独立进程（36号 :66）+ clientsdk_downloader SDK 下发（deep-dive :100）；两家排队均非内建。

## 1. 职责分工与拓扑

```text
阶段一（登录连接，短）                阶段二（游戏连接，长）
Client ──LoginHello/LoginAuth──► login-app    Client ──ClientHello{login_token}──► gateway-app
        ◄─LoginResponse─┘          │                  │本地验签+按 zone_hint 转发
              │ AdmissionRequest   │签发 token          ▼
              ▼ (RequestReply)     │              Zone（创建会话实体 → SessionUp）
            manager（闸门/顶号/落点/pending nonce）──► #12 目录（可见锚点）
```

| 方 | 职责 | 明确不做 |
|---|---|---|
| login-app | 账号鉴权（读账号域）、准入请求代理、token 签发、契约包指针下发、（P3）排队通知转发 | 不持目录/负载表（manager 权）、不验游戏连接、不触 gateway/Zone、不做文件服务 |
| manager | 准入闸门（水位）、顶号预裁（#12 §3 临界区）、落点（最轻 Zone + gateway 对）、pending nonce 核销、（P3）排队队列 | 不鉴权（凭据不出 login-app）、不签 token |
| gateway-app | token 本地验签、按 zone_hint 透传路由（sdk-contract §10.6） | 不触账号库（§5.9 :313）、不做准入裁决（问 manager 的事在阶段一已完成） |
| Zone | 创建会话实体、SessionUp 上报（#12 锚点） | 不感知登录链（token 对 Zone 透明——它只收 manager 下发的落点指令） |

**为什么登录不经 gateway（反方案记录）**：单入口地址的运维收益 < 代价——gateway 需同时多路复用「未握手明文路由」与「加密四通道会话」两种形态（L2 栈先嗅探再分流，状态机复杂化）；账号凭据进 gateway = 敏感面扩大；登录风暴与游戏流量共生死。BW/KBE 均独立 loginapp，二十年先例一致。客户端两地址（login/gateway）来自配置或 launcher 拉取（P3 可加 bootstrap 端点，本设计不展开）。

## 2. 登录连接协议（阶段一）

**握手（§5.9 同族，域分离）**：

```text
C → L  LoginHello      { nonce_c, pub_c(X25519), aead[] }          // 明文（公钥本就明文，同 §5.9 ClientHello 形态）
L → C  LoginHelloAck   { nonce_s, pub_s(X25519), aead_selected, salt }
双方    login_key = HKDF-SHA256(X25519(·), salt = nonce_c‖nonce_s, info = "apollo-login-v1"‖login_conn_id)
C → L  LoginAuth       { credential(AEAD), auth_mode, client_hash, platform }   // 凭据在密文内
L → C  LoginResponse   { result, account_id?, login_token?, gateway_addr?, zone_hint?, client_hash_ok, bundle_url? }
       → 连接关闭（登录连接一次性，无续传无 resume——失败重走全握手）
```

- **auth_mode 两型**：`password`（username + password → accounts 凭据校验）与 `channel`（第三方渠道 token → 出站验证 §5.10 → 绑定查找 #14 §5）——一型一验，P1 只实现 password。
- **凭据永不落日志**（logging 分级旁路面白名单外）；LoginAuth 失败计数进 login-app 侧锁定策略（存量 :48-121 maxLoginAttempts/lockout 雏形的保留面——账号级，PBKDF2 前置）。
- **错误码族**进 errors.xml（随代码批，契约冻结不动）：bad_credentials / account_banned / server_busy(+retry_after) / queue（P3）/ client_hash_mismatch / channel_rejected（#14）。
- 帧层：同一 L1 帧格式（magic/seq/CRC32C）+ CryptoFilter（§5.5 槽位，压缩后位次照旧）；登录消息归 **client 域**（客户端可见面，id 分段 §11.3 管辖）。

## 3. login_token（结构/签发/验签/核销）

| 项 | 裁决 |
|---|---|
| 结构 | `base64(payload ‖ HMAC-SHA256(K_login, payload))`；payload = {account_id, issue_tick, exp_tick, nonce(128bit), gateway_id, zone_hint, world_assignment 初值, purpose="game_entry"} |
| TTL | 短（默认 60s 建议——「登录响应到 ClientHello」的窗口，P3 容量批校准）；exp 拒绝码 token_expired → 客户端重走阶段一（无续传） |
| 签发 | login-app 独有（K_login 私持）；purpose 字段防跨用途重放（未来扩展 purpose="gm_entry" 等互不通用） |
| 验签 | gateway 本地（K_login 验证密钥经 G-1 编队配置分发——静态配置态，非运行期协商）；验签 = 完整性 + exp + purpose 三查，**不触 login-app/账号库**（§5.9 :313「游戏进程只验签」的兑现——gateway 与 Zone 同为游戏进程） |
| 一次性 | **核销点 = manager 准入临界区**：AdmissionRequest 时 pending 表记 nonce（TTL = token TTL + 余量）→ SessionUp 时 pending 转 #12 正式条目并核销 → 同 nonce 二次使用 = pending 无此记录/条目已存在 → 拒绝 + violation_score（§4.3 :128 同桶）。**零新增状态面**：pending 表是 #12 目录的登录侧伴生表（manager 内存、TTL 自清、manager 定时器族计时——与 #12 §4 窗口计时同组件依赖） |
| 密钥 | 对称 HMAC-SHA256（内部编队信任域；gateway 持验证密钥 = gateway 受信边界内，与 CryptoFilter 同级信任）；轮换 = 编队配置版本化（P3，K_login_v[N] 双活窗口） |
| 算法合规 | HMAC-SHA256 在 §5.9 算法分级表许可面内（SHA-256 摘要行）；nonce/随机数只用 RAND_bytes（§5.9 用法纪律——存量 :284-301 xorshift 自造 PRNG 是反面，迁移面 §10） |

**与另外两凭证的分工**（易混，concept-glossary §2.5）：login_token = 账号域一次性入场券（本设计）；session_key = 连接域帧加密密钥（§5.9 握手协商，每连接新）；resume token = 同会话断线恢复凭证（§3 L2，TTL 与掉线窗口同源 #12 §4）。三层互不通用、互不派生（密钥层次 §5.9 :317 的凭证面补充）。

## 4. 账号域

- **owner = DB**（storage.xml 语句集，批次 2 db-app——attribute-sync §8.2 语句面）：`accounts`（account_id PK / credential_salt / credential_hash / state / banned_until / created_at / last_login_wall_ms——**最后在线时间/最后 Zone 即 #12 :114 预告的存档列**，列提升归批次 2 语句集）+ `third_party_bindings`（channel / open_id / account_id / bound_at，UNIQUE(channel, open_id)——#14 §5 消费同表）。
- **凭据哈希 = PBKDF2-HMAC-SHA256**（OpenSSL `PKCS5_PBKDF2_HMAC` 内建——单一 crypto 源不破、不引新依赖；迭代参数进配置，P3 校准）；盐 per-account 128bit RAND_bytes。存量明文比较（:27/:62）为 P1 反面。
- **鉴权路径**：P2 单进程 = login-app 内嵌语句执行（同进程 db 面）；P3 = RequestReply → db 路径（低频控制面，§5.7 invoke_mode 合规——鉴权不进任何 tick 热路径）。
- **风控消费**：violation_score abuse 断开写账号级风控记录（§4.3 :128）→ banned_until 由 GM 封禁指令置位（scripting-lua §7.5 踢人/封禁执行位归 login-app 闸门的既有口径）；准入时 manager 或 login-app 查 state/banned_until 拒登（检查点定 login-app——manager 不触账号库）。
- **P1 降级**：进程内测试账号表（存量 :22-41 形态的合法化：内存 map + SHA-256 盐值最低档，PBKDF2 P2 接入）；第三方 channel 模式 P3 随 #14。

## 5. 选服/排队/准入（manager 侧）

**AdmissionRequest 处理序（manager 临界区，串行）**：

1. **负载闸门**：编队水位（net §7 :395 准入闸门，指标同源 G-5）——过载 → busy + retry_after（P1-P2 终形态）/ 入队（P3）。
2. **顶号预裁**：命中 #12 目录既有条目（Online/Suspended/Leaving）→ 同临界区旧条目 Removed + 异步踢除下发（SessionKicked + gateway close(kicked)——#12 §3 全套，本设计只触发不重定义）。
3. **落点**：最轻 Zone（minAppLoad 同型 + 水位硬约束）+ gateway 对（gateway 集群择一——manager 全局视图）→ world_assignment 初值（P1 = 配置默认世界；P3 = 玩家上次在线/副本重入逻辑，业务钩子位）。
4. **pending 记录**：nonce + 落点 + TTL → 返回 login-app。

- **为什么准入在登录时点而非 ClientHello 时点**：BW 同型（loginapp 问 mgr 拿 (addr,load)——:1117 分流）；顶号裁决自然发生在「新登录」语义最早可判定处（#12 §3「新登录命中条目」即此刻）；ClientHello 时再裁 = 客户端多一轮失败重试。**代价显式声明**：签发后玩家不连（弃 token）→ pending TTL 自清，无幽灵（同 #12 「不留幽灵」纪律）。
- **排队（P3）**：manager 侧准入队列组件——容量水位驱动入队/出队、queue_position 事件经 login-app 下发（登录连接保持期间）、出队即走临界区第 2-4 步。两家先例无内建（BW LoginConditions 拒绝式）——apollo 按国产 MMO 需求做成可选组件，默认关。
- **与 #12 的临界区共用声明**：准入裁决与目录裁决在同一 manager 串行点（#12 :9 单点串行论证的直接受益者）——顶号-准入-登记三事一个临界区，无跨进程锁。

## 6. 契约包下发时机（指针制）

- LoginResponse 携带 `{client_hash, bundle_url}`；客户端本地 manifest（§11.6 三件套）比对 → 失配按 bundle_url 拉取（CI `contract_pack` 已产 `dist/client/<client_hash>/`——sdk-contract §12.2 落地注记）→ 加载期逐文件 SHA-256 闸（既有）。
- **login-app 不做文件服务**：字节分发归部署面（CDN/静态目录），引擎只发指针——KBE clientsdk_downloader「引擎即 CDN」不采（apollo 有 CI 组包管线；引擎进程背带宽与缓存职责是形态错位）。P1 退化 = 配置态静态 URL；client_hash_ok = false 且无 URL = 版本过旧提示升级（强更位，launcher 语义，本设计不展开）。
- schema_hash 握手照旧在**游戏连接**（sdk-contract §6 N/N-1 窗口）——登录连接只比 client_hash（粗粒度入口校验，细粒度装载期闸既有）。

## 7. 全链时序（四场景）

1. **正常**：LoginHello → LoginAuth → (login-app: 鉴权) → AdmissionRequest → (manager: 闸门/落点/pending) → LoginResponse{token, gateway, zone_hint, bundle} → 断开 → ClientHello{token}（gateway 验签转发）→ Zone 创建实体 → SessionUp（manager 核销 pending、目录可见）→ §5.9 会话建立、进入世界。
2. **顶号**：同账号新登录 → AdmissionRequest 命中目录条目 → 临界区旧 Removed + 踢除下发（旧 Zone SessionKicked / 旧 gateway close(kicked)）→ 新链路照常（旧端 kicked 原因码 #12 §8）。旧端 resume 竞态由 anchor_epoch 拒（#12 §3-3）。
3. **掉线重连**：游戏连接死 → 目录 Suspended（#12 §4 窗口）→ 客户端**直接对 gateway 重走 ClientHello + resume token**（L2 §3——不经阶段一、不新签 login_token；resume ≠ 新登录两事分开 #12 :11）；窗口满未恢复 → 新登录全链（阶段一重来）。
4. **过载**：AdmissionRequest 撞闸门 → busy + retry_after → LoginResponse 拒绝 → 客户端退避重试（P3 排队则保持登录连接收 queue_position）。

## 8. 消息面

| 族 | 域 | 消息 | invoke_mode/通道 | 说明 |
|---|---|---|---|---|
| 登录握手 | client | LoginHello / LoginHelloAck / LoginAuth / LoginResponse | 登录连接（L1 帧 + CryptoFilter） | §2 全序列；随代码批进 messages.xml client 段 |
| 排队通知（P3） | client | QueueNotice{position, eta} | 登录连接 | manager → login-app 转发 |
| 准入 | internal | AdmissionRequest / AdmissionReply | RequestReply（低频控制面合规，§5.7 :217 超时不复活——login-app 重发幂等靠 nonce） | login-app ↔ manager |
| 准入撤销 | internal | AdmissionCancel | OneWay | 登录连接断开且未发 token → 撤 pending（防占位；TTL 兜底） |
| （既有）#12 事件族 | internal | SessionUp/Down/Moved/Kicked + Snapshot 对账 | 见 #12 §8 | SessionUp 即核销点（§3） |

- internal 域两新消息进 sdk-contract §11 分段（900+，随代码批；与 #12 五消息族同域共存）；信封 = InternalMessageEnvelope（net §7）。
- **client 域新增 = 登录族四消息**——#12 曾立「client 域零新增」是其目录面口径；登录族是 #13 的合法新增（客户端可见面本就在 client 域），域纪律（跨域禁令/分段 §11.3）照旧。

## 9. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| net-abstraction | §5.9 login_token 归属行兑现为 §3 全规格（结构/核销/密钥）；登录握手 = §5.9 同族域分离实例；§5.7 Admission RPC = RequestReply 控制面；§7 闸门/最轻分配 = §5 处理序 1/3 步；§4.3 violation_score = token 重放计数源之一 |
| session-and-online-directory | §2 登记锚点 = 本设计链路终点；§3 顶号 = 准入临界区第 2 步（触发方在本、裁决语义在彼）；§4 窗口/resume 分工（场景 3）；pending nonce 表 = 目录伴生表（§3）；:114 账号位存档列 = §4 accounts 表 |
| sdk-contract | §11.3 client 域登录族四消息 + internal 域两消息（随代码批）；§11.6/§12.2 契约包指针制；§6 schema_hash 握手留在游戏连接 |
| attribute-sync | §8.2 storage.xml accounts/third_party_bindings 语句（批次 2）；§8.3 迁移纪律适用账号表 DDL |
| scripting-lua | §7.5 封禁 GM 指令 → banned_until 置位（§4 风控消费闭环）；§8 异步交接不适用于鉴权（低频控制面同步 RequestReply 合规） |
| inbound-interfaces（#14） | third_party_bindings 同表两消费；channel 登录模式 = #14 绑定域的入口侧；channel_rejected 错误码同族 |
| capacity-and-benchmark | 登录洪峰（重连风暴）= login-app 容量项；token TTL/pending 表规模/排队深度入 P3 校准清单 |
| clock-and-time | issue/exp_tick = tick 号；pending TTL = manager 定时器 deadline（#12 §4 同组件依赖） |
| concept-glossary | 登录链词条 + §2.5 三凭证辨析（本批配套） |

## 10. 存量迁移与分期

**存量处置（§25 实读逐项，迁移归代码批、源码冻结不动）**：

| 存量件 | 判定 | 目标形态 |
|---|---|---|
| Authenticator（:22-121 硬编码/锁定雏形） | 保留骨架 | 账号域组件化（§4：PBKDF2 + accounts 表 + P1 降级表）；锁定策略保留 |
| GatewayAllocator（:133-179 最少连接选网关） | **职能并入 manager** | login-app 删此件——选网关/选 Zone 归 manager 落点（§5-3） |
| SessionManager（:185-301 ticket 双哈希/滑动续期） | **删除** | token 自包含（§3）+ manager pending 核销替代——无服务端会话表 |
| RepSocket :9001 + protocol 旧编解码 | 迁移面 | L1 帧格式 + client 域登录族（§2）——随 net M1 内核同批 |
| preparePlayerOnline 三连（:476-542，含 :536-541 假成功） | **删除** | manager 准入 + Zone SessionUp 替代（阶段二链路）；假成功路径无对应物 |
| gateway session_admission_service RPC 回问（:33） | 迁移面 | 本地验签（§3）——gateway 改持 K_login 验证密钥 |
| main.cpp 命令行配置（:20-47） | 迁移面 | 编队配置（G-1 配置态）随装配批（architecture-review §17/§21 口径） |

**分期**：

- **P1（单进程演示）**：内存测试账号（§4 P1 降级）+ 进程内准入（manager 职责同进程直调，零 RPC）+ token HMAC 全规格（密钥配置态）+ 登录消息族进契约——**零新增进程**。
- **P2（Compact/单进程完整）**：accounts 表落 storage.xml（批次 2 依赖）+ PBKDF2 + 契约包指针 + 顶号/窗口闭环（#12 P2 退化形态对接）。
- **P3（多进程全量）**：login-app/manager 分进程 + Admission RPC + 排队组件 + channel 登录（#14 同批）+ 密钥轮换 + 洪峰容量校准。**#15 Bots 互为验收**（bots 登录脚本化走本链路——§26-④ 握手依赖兑现；顶号风暴/断线重连压测 = #12 §9 验收建议同源）。

---

*基线：apollo main @ edc94cfa（文档态；源码冻结未动）。存量实读 = architecture-review §25（apps/login-app 884 行全文、gateway ingress 准入面）；先例引注 = 36号 :55/:66（BW/KBE loginapp）、net-abstraction §7 :389/:1117 分流与 LoginConditions、deep-dive :100（clientsdk_downloader）；#12 咬合面 = session-and-online-directory :9/:40-48/:55-63/:71/:77-85/:87-91/:114/:119-127 实读。设计批输入 = architecture-review §25 六问（本稿六答：拓扑①/核销②/鉴权源③/准入分工④/SDK⑤/迁移⑥）+ §26-④ 握手依赖。TTL/迭代参数/洪峰容量为初始建议，P3 容量批校准。*
