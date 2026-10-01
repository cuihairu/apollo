# Apollo 入站第三方对接面设计（inbound-interfaces）

> 状态：设计稿（评审中）。定位：**外部世界主动进来的 HTTP 面**——渠道支付回调、第三方账号绑定回调、健康检查——的承载进程、鉴权、投递、幂等与面限定。缺口登记 design-gap-inventory #14（B11 批，2026-10-01 落盘）。互引：net-abstraction §5.10（出站 HTTP 三裁决——本设计是其镜像面：出站已裁、入站在此）、§5.7（InterServerLink——internal 域投递通道）、§7（manager 域/G-1）；login-flow（#13 同批——third_party_bindings 同表、channel 登录模式入口）；session-and-online-directory（#12——在线投递定位消费 #12 目录）；attribute-sync §8.2/§8.3（storage.xml 语句/DDL 纪律）；scripting-lua §8（异步交接同型）；logging §5.2（密钥 env 纪律同源）；sdk-contract §11.3（internal 域分段）；architecture-review §25/§26（存量实读与设计批输入五问）。

---

## 执行摘要

1. **承载 = 独立 interfaces 进程**（KBE `tools/server/interfaces` 同型命名同型职责）：专职入站 HTTP 对接面，编队配置态声明（G-1 组件），不与任何游戏进程共生。三候选（gateway / manager / 独立进程）对比裁决唯一胜出——**故障域与信任域双隔离**：回调洪峰打 interfaces 不打玩家流量（gateway）也不打仲裁面（manager）；公网暴露面收敛到一个无游戏态进程。BW 无此进程（billing 直进 dbutil 族）不构成反证——BW 的回调面由运营平台侧承接，apollo 自持。
2. **实现件 = modules/net/http 存量 HttpServer 升格候选（审计门前置）**：http.cpp:444 `class HttpServer`（:463-473 bind/listen/acceptThread）现零生产消费方（仅 examples/http_demo.cpp:21 内部类自认）——§4.2 已列 modules/net/{http,websocket} 代码面审计候选，升格（真实现/测试资产/桩边界）以该审计门结论为前置；**Drogon 不复议**（§5.10 :340 删除裁决维持——web 框架双形态是「第五套网络栈」种子，入站对接是薄监听层不是 web 应用）。
3. **鉴权 = HMAC-SHA256 签名主 + IP 白名单辅，mTLS 不进路线图**：每渠道一密钥（channel_id 维度），签名覆盖 method+path+timestamp+body（时间窗防重放，防信道抓包重发——与 login_token 一次性核销是两层不同防线：回调时间窗宽（分钟级，渠道重试语义）、登录 token 窄（60s 一次性））；密钥表 = 编队配置态（密钥 env 纪律与 logging §5.2 :126 同源——不落代码不落日志）；mTLS = 运维负担 > 收益（渠道侧证书管理不可控），列入「明确不做」。
4. **投递 = 两段式（落库 → 定位投递），幂等键 = 渠道订单号**：第一段验签通过即写 DB（对账表 + 业务结果表）——「先持久后处理」是回调面的铁律（渠道按 HTTP 响应判成败，内存态丢失 = 渠道重试风暴）；第二段查 #12 目录定 Zone（在线 → internal 域消息 + tick 边界应用；离线 → 已在库，登录时消费）。DB 唯一约束（channel, order_id）+ 内存去重窗（近期 order_id LRU）双层幂等；**异步不进场景线程**——interfaces 投递走 InterServerLink，Zone 在 tick 边界消费（scripting-lua §8 同型纪律，回调永不直接进任何 tick 热路径）。
5. **入站面限定 = 三类白名单，其余一律拒绝**：①渠道回调（/callback/{channel} 族）②健康检查（/healthz）③预留运维触发（受 IP 白名单 + 管理鉴权双闸）。**明确不入**：后台 UI、报表、GM 操作（走 admin/control 通道——logging §5.1 exporter 同哲学：HTTP 面只做机器对机器薄层，人机界面不在此进程）；限流 = per-route + 全局 + 有界队列（§5.7 有界排队同源——无界排队 = 把背压推迟成内存炸弹）。

## 0. 现状与缺口

- **存量（§26 实读）**：modules/net/http/http.cpp:444 `class HttpServer`（bind :463 / listen / acceptThread :463-473）——零生产消费方；examples/http_demo.cpp:21 内部类自认是全仓唯一引用。apps/ 五件（base-app/bench/gateway-app/login-app/world-app）无 interfaces 型进程。**承载三候选（gateway/manager/独立）此前零裁决、存量 HttpServer 未入证据列**（C-86）——本节即补裁。
- **出站已裁、入站空白**：net-abstraction §5.10（:333-343）三裁决全是**出站**方向（curl 收口/Drogon 删/异步交接）；入站 HTTP 除该节末句「入站面归 #14」外零设计。Drogon 双处删除（CMakeLists.txt:81-91 http、:124-133 websocket）已定——入站承载不得复活该分支。
- **先例**：KBE = `tools/server/interfaces` 独立进程（HTTP 收渠道回调 → 查询/投递 KBEngine 内部，deep-dive :101）；BW = 无独立对接进程（billing 记录直进 dbutil/mysql_billing_system，36 号 :266 问11 A 级判定）——**两家共同点：对接面与游戏逻辑进程分离**；apollo 取 KBE 的独立进程形态 + 补 BW 没做透的投递语义。

## 1. 承载裁决：独立 interfaces 进程

| 候选 | 裁决 | 理由 |
|---|---|---|
| **gateway-app 兼任** | 拒 | 玩家流量进程不背公网回调面：渠道回调洪峰/重试风暴与玩家会话共生死；gateway 的 L2 会话语义（seq/ack/四通道）对无会话 HTTP 回调是错配；凭据与渠道密钥进玩家面进程 = 敏感面扩大 |
| **manager 域兼任** | 拒 | manager 是仲裁单点（#12 目录临界区/准入/恢复相位）——回调洪峰打仲裁面 = 外部流量可拖垮全局裁决；且 manager 零公网暴露是安全基线 |
| **独立 interfaces 进程** | **采** | 故障域隔离（挂了只影响回调，渠道重试兜底）+ 信任域隔离（公网暴露面收敛到无游戏态进程）+ 容量域独立（限流/扩缩容不耦合游戏容量模型）+ KBE 二十年同型先例 |

- 编队形态：G-1 组件 id `interfaces`，配置态声明实例数（渠道多了可横扩——无会话态，任意实例可处理任意回调，负载均衡归前置接入）；P1-P2 单进程形态内嵌于主进程（同 login-app 降级逻辑——进程内直调，零 RPC）。
- **与出站面的关系**：出站（渠道 API 调用：订单查询/发货通知/绑定发起）走 net-abstraction §5.10 curl 设施（目标白名单含渠道域）；入站（回调）走本进程——**同一渠道域的两个方向、两套通道、同一个对接配置块**（channel 表同时声明出站端点与入站路由）。

## 2. 入站 HTTP 形态

- **薄监听层四段**（accept → 读请求 → 路由表分发 → handler）：无模板/无会话/无静态文件——web 框架的三大件全不需要，这正是 Drogon 裁决（§5.10 :340）的入站版论据。handler = 验签（§3）→ 幂等检查（§4）→ 落库（§4）→ 异步投递（§4）→ 立即应答渠道（200/40x/50x 语义：验签失败 403、重复 200（幂等语义——渠道收到成功即停重试）、内部落库失败 500（渠道稍后重试））。
- **路由表（配置态）**：

| 路由 | 方法 | 鉴权 | 限流档 |
|---|---|---|---|
| /callback/{channel}/payment | POST | HMAC 签名 + IP 白名单 | 中（渠道重试风暴档） |
| /callback/{channel}/binding | POST | HMAC 签名 + IP 白名单 | 低 |
| /healthz | GET | 无（内网限定——监听地址族隔离） | 高 |
| /ops/{action}（预留） | POST | IP 白名单 + 管理令牌 | 低 |

- **监听**：生产 = 独立端口 + 前置接入（LB/网关做 TLS 终结——interfaces 自身 HTTP，TLS 归部署面不进引擎，与 login-app 不做文件服务同哲学：引擎进程不背传输基础设施）；/healthz 与管理路由监听内网地址族（或独立端口）与公网回调路由物理隔离。
- **限流**：per-route 令牌桶 + 全局字节率 + **有界队列**（溢出 = 503 + 渠道重试兜底——重试是渠道协议自带的，不排队不阻塞哲学的入站版）。

## 3. 鉴权

- **HMAC-SHA256 签名（主）**：渠道密钥 per-channel；签名串 = `method + "\n" + path + "\n" + timestamp + "\n" + SHA256(body)`；header 携 channel_id + timestamp + sign。**时间窗**（默认 ±300s 建议，按各渠道重试节奏配置）防抓包重放——与 login_token 的 60s 一次性不同层不同值（回调是渠道机器对机器、重试语义要求宽窗；token 是玩家客户端、一次性核销收窄窗）。密钥比较恒定时间（`CRYPTO_memcmp`，§5.9 用法纪律同源）。
- **IP 白名单（辅）**：渠道出口网段表（配置态），签名验证前置——省 HMAC 计算成本 + 缩小攻击面；网段表为空 = 不启用（开发态）。
- **密钥表 = 编队配置态**：channel_id → {密钥, 签名算法版本, 时间窗, 出站端点, IP 段}；密钥值 env 注入不落配置文件（logging §5.2 :126「密钥 env」纪律同源）；轮换 = 双密钥重叠窗（新旧并验，渠道切换后撤旧）。
- **mTLS 不进路线图（明确不做）**：渠道侧证书发放/续期/轮换的管理链路在渠道方手里不可控，强推 mTLS = 对接摩擦换边际安全；HMAC + IP + 时间窗已覆盖回调面威胁模型（伪造/重放/窃听——窃听由前置 TLS 承接）。

## 4. 回调投递（两段式）

**第一段：持久（同步，handler 内完成）**

```
验签通过 → 幂等检查（内存 LRU 近期 order_id + DB 唯一约束 (channel, order_id)）
        → 事务写：callback_ledger（原文 + 验签要素 + 接收时刻）+ 业务结果表（payment_orders / binding_requests 初始态）
        → 应答渠道 200
```

- **先持久后处理是铁律**：渠道按 HTTP 应答判成败——内存态「先处理再落库」在进程重启/崩溃窗口丢回调，且渠道已停止重试（它收到了 200）；ledger 原文留存 = 对账与争议仲裁的依据（BW billing 记录进 mysql 的经验面：记录本身是资产，36 号 :266）。
- 幂等键 = **渠道订单号**（渠道侧唯一）；DB 唯一约束是权威幂等（约束冲突 = 重复回调 → 直接 200），内存 LRU 是热路径省一次 DB 往返（容量 = 每渠道近期订单量，配置态）。

**第二段：投递（异步，出 handler 即返回）**

```
在线判定：查 #12 目录（account → Zone 定位，#12 §5 RouteResolver 镜像同源数据）
  ├─ 在线 → internal 域消息（CallbackDelivered{type, order_ref}）→ InterServerLink → 宿主 Zone
  │         Zone 在 tick 边界消费（进当 tick 后续输入——battle-determinism §1 异步不进判定同律）
  └─ 离线 → 终态已在库（第二段对离线者无事可做——登录时业务逻辑读库消费，见 login-flow §4 风控/消费位）
```

- **两段之间无原子性承诺、有对账兜底**：第一段成功第二段丢失（进程死在中间）→ ledger 有记录 + 渠道对账文件核对（日终对账任务，运维触发路由候选）——回调面的可靠性模型是**至少一次 + 幂等收敛**，不是恰好一次（渠道协议本身至少一次，恰好一次在分布式两侧不可承诺）。
- **internal 域新消息**（sdk-contract §11 分段 900+，随代码批）：`CallbackDelivered`（interfaces → Zone）+ 对账差异报告（运维查询面，P3）。RequestReply 只用于低频订单状态反查（interfaces ↔ db 路径）；投递本体 OneWay——**回调不因 Zone 慢而阻塞 interfaces**（per-Link 背压 §5.7，慢 Zone 拖累自己的 Link）。

## 5. 账号绑定域（与 #13 同域）

- **表 = third_party_bindings**（login-flow §4 同表同列：channel / open_id / account_id / bound_at，UNIQUE(channel, open_id)——UNIQUE 双向消费：#13 channel 登录查 account、本设计绑定落库查重）；DDL 归 attribute-sync §8.3 纪律（迁移脚本 + 语句集进 storage.xml，批次 2）。
- **绑定流程时序**（客户端发起，出站与入站接力）：

```
1. 客户端经 login-app 发起绑定请求（auth_mode=channel 或已登录态附加绑定）
2. login-app/interfaces 出站调渠道 API（§5.10 curl，渠道域在出站白名单）→ 得渠道侧绑定票据
3. 客户端在渠道侧完成授权（渠道 App/H5——apollo 链路之外）
4. 渠道回调 /callback/{channel}/binding → 本进程验签 → 落 binding_requests
5. 在线则投递通知（第二段）；账号映射 = 新绑定建行 / 已绑定他号 → 拒绝码 channel_bound_elsewhere
```

- **解绑**：对称流程（出站发起 + 回调确认 + 软删标记——硬删破坏对账链）；绑定/解绑审计行进 ledger（同表原文留存纪律）。
- **职责边界**：绑定**业务语义**（哪次活动送什么、绑定奖励）归 Zone 业务（Lua 层消费 CallbackDelivered）；绑定**协议与安全**（验签/幂等/落库）归本设计——与 #13 的「协议归设计、玩法归业务」同一条分界线。

## 6. 入站面限定（白名单纪律）

- **只有三类**（§2 路由表即全量清单——新增路由 = 配置变更走评审，不是运行期注册）：渠道回调 / 健康检查 / 预留运维触发。路由表外任意 path = 404 + 计数（扫描探测观测面）。
- **明确不入清单（反扩张防线）**：后台管理 UI / 报表查询 / GM 操作 / 玩家 API——人机界面与运维操作走 admin/control 通道（GM 指令链 = scripting-lua §7.5 既有口径；运维观测 = logging §5.1 exporter 单面）。**理由**：interfaces 的安全模型是「验签机器流量」——塞进人机界面就引入会话/权限/UI 三套新攻击面，且每一条都违背「薄监听层」形态；这是 KBE interfaces 二十年只做回调对接的边界经验。
- **不开放脚本可注册的入站路由**：Lua 层零 HTTP 入站能力（logging §5.2 :126「Lua 层不开 HTTP 出站」的镜像句）——业务要响应外部事件，走渠道回调 → internal 消息 → 业务 handler 的既有链，不开第二入口。

## 7. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| net-abstraction | §5.10 出站三裁决的镜像面（curl 出站复用/目标白名单含渠道域/Drogon 不复活）；§5.7 InterServerLink = 第二段投递通道（OneWay + per-Link 背压 + 具名端点 :231——interfaces 是编队内具名组件，不开放任意投递目标）；§7 G-1 编队声明 interfaces 组件 |
| login-flow（#13） | third_party_bindings 同表两消费（#13 查映射/本设计落绑定）；auth_mode=channel 登录的出站验证段在两设计接力线中；channel_rejected / channel_bound_elsewhere 错误码同族 |
| session-and-online-directory（#12） | 第二段在线判定消费目录查询（#12 §5 三消费方之外的本设计新消费者——镜像只读，与 RouteResolver 同源数据）；不写目录（回调不改在线状态——顶号/登记语义不涉） |
| attribute-sync | §8.2 storage.xml 语句集（callback_ledger/payment_orders/binding_requests/third_party_bindings 四表）；§8.3 DDL 迁移纪律；write-behind journal 不适用 ledger（同步事务写——回调持久先于应答，无 behind 窗口） |
| scripting-lua | §8 异步交接同型（回调永不进 tick 热路径，tick 边界消费）；§7.5 GM 封禁/踢人不经本面（入站限定的反扩张防线例证）；Lua 零 HTTP 入站能力 |
| sdk-contract | §11.3 internal 域新消息（CallbackDelivered 族，900+ 分段随代码批）；client 域零新增（玩家客户端不感知回调链） |
| logging | §5.1 exporter 单面照旧（/healthz 是进程健康不是指标面——指标仍在 exporter）；§5.2 :126 密钥 env + 出站白名单纪律同源；ledger 原文留存与结构化日志分层（ledger 是业务资产不是日志） |
| clock-and-time | 第二段投递的 tick 边界消费；时间窗验签的时间源（机器时钟同步前提与 #12 周期对账同假设） |
| battle-determinism | 回调进当 tick 后续输入、不进判定（§1 同律——充值到账改变属性是异步外部事件） |
| capacity-and-benchmark | interfaces 独立容量项（回调洪峰/重试风暴档限流参数）入 P3 校准清单 |

## 8. 分期与存量处置

- **存量（§26 实读，源码冻结不动）**：http.cpp:444 HttpServer 零生产消费方——**升格候选**（承接入站薄监听层）前置 §4.2 审计门（modules/net/http 代码面：桩质量/测试资产/C-57 event_loop 勘误后续）；审计结论若判重写更廉，则本设计形态不变、实现件另起（apps/interfaces 薄层直连 InterServerLink）——**设计面不赌存量质量**。examples/http_demo.cpp:21 的内部类引用随审计门处置。
- **P1（单进程演示）**：进程内回调处理（interfaces 职责同进程直调，零 RPC、零独立监听——用例驱动：内存 ledger 表 + 直调业务 handler）。
- **P2（Compact）**：/healthz + 内存渠道表 + HMAC 验签全规格；ledger 落 storage.xml（批次 2 依赖）；投递 = 同进程事件（无 InterServerLink——Compact 单进程天然零 RPC）。
- **P3（多进程全量）**：独立进程 + 独立监听 + InterServerLink 投递 + #12 目录在线判定 + 出站白名单渠道域 + 对账任务（运维触发路由）+ 限流参数容量校准。**验收咬合**：#13 channel 登录模式（绑定链全程）+ #15 Bots（回调风暴压测——bots 模拟渠道重试洪峰打限流与有界队列）。

---

*基线：apollo main @ edc94cfa（文档态；源码冻结未动）。存量实读 = architecture-review §26（http.cpp:444/:463-473、examples/http_demo.cpp:21、apps/ 五件清单、Drogon 双处删除登记）；先例 = deep-dive :101（KBE tools/server/interfaces）、36 号 :266（BW mysql_billing_system A 级）；出站镜像面 = net-abstraction §5.10 :333-343 实读；#13 咬合 = login-flow.md §4/§8（同日同批落盘，行号即本批版本）；#12 咬合 = session-and-online-directory §5/§8。设计批输入 = architecture-review §26 五问（本稿五答：承载①/鉴权②/投递③/绑定④/入站限定⑤）。时间窗/LRU 容量/限流档位为初始建议，P3 容量批校准。*
