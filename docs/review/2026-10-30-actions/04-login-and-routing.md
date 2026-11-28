# D08 / D09 / D10 / D11 / D15 · 网关直达、登录链路、多网关重连、准入凭证、换场路径

> 来源：docs/review/2026-10-30-decisions.md D08（todo.md:153）、D09（todo.md:154）、
> D10（todo.md:155）、D11（todo.md:160）、D15（todo.md:164）。
> 本文件为设计稿；真实接线随 P3/P4 链路批次推进。

## 1. D08 · 网关直达 Base/cell 与路由恢复验收条件

### 1.1 前提

L146 最小位置投影（D05）可用后，用**真实 gateway + cell** 验证输入与下行会话绑定。

### 1.2 验收用例

| # | 场景 | 断言 |
|---|---|---|
| 1 | 正常输入 | 基础业务直达当前 Base；模拟输入直达当前 cell；下行绑定真实会话 |
| 2 | 旧投影注入 | 缓存退回旧位置 → 旧执行端**明确拒绝**（版本比较） |
| 3 | 迁移后缓存 | 位置已迁移 → 网关按新投影重定向到新 cell |
| 4 | 重复重定向 | 重定向次数受限（限次后回退查目录） |
| 5 | 正常消息不逐条经目录/管理端代理 | 仅冷启动、过期、拒旧后查目录 |

> 验收 #1「基础业务直达当前 Base」2026-11-26 已接线（commit 2f9b6d30，见 todo.md L122）：
> 归属 Base 的 M1 端点随 915 落点成行（登录实拨端点），目录 918 回带进网关
> `RouteSnapshot.baseHost/basePort`；`setBaseForwarding` 按快照端点经
> `baseClientForEndpoint` 直连归属 Base，端点未声明回退激活面静态链路（旧语义
> 零回归），已装 `baseResolver_` 权威裁决仍优先。测试
> `test_base_forward_via_directory_endpoint`（活区 GatewayPushDownlinkTests；
> `test_gateway_route.cpp` 属 `APOLLO_BUILD_GTESTS=OFF` 死区，覆盖不落死文件）：
> 静态链路 A / 目录归属 B 分置两真 Base——声明端点玩家积分落 B 不落 A、未声明
> 玩家回退落 A 不留痕 B，回执各回本会话。四树验证：主树/off-tree2/examples
> ctest 各 128/128；san 目标 6/6（ASan+UBSan）。**仍缺**（延后项，不重试）：
> 被拒意图的客户可见重同步（协议设计拍板），以及 D10 case 3。（D08 #4 重定向
> 限次已于 2026-11-27 关闭，commit 55097e17，见下。）

> 验收 #1「模拟输入直达当前 cell」2026-11-22 已接线（commit 1c602137，todo.md
> L122 过期注记 2026-11-27 复核刷新）：gateway 不再经 World 管理端物理中继模拟
> 输入——端点发现链 968 投影（World 发布归属 cell 注册端点）→ 登录投影名册 →
> 915/918 → `RouteSnapshot.cellHost/cellPort`；M1 出口逐 cell active 端点按
> host:port 拨装（`target_link`，逐条目就绪门：未就绪 cell 不阻塞其它端点），
> 929 应答与 939 下行直回直连链，World 前递保留为回退面（端点未声明/未连接）；
> 请求号只对实发帧分配（requeue 不耗号）。测试：`test_gateway_m1_egress` 直连
> 观测面（`directCellIntents` 计数）+ login_chain 落点断言
> `assigned->cell_host/cell_port`。D08 #1 两半（Base 直达 + cell 直达）验收面
> 均关闭；本节仍缺：重同步拍板、D10 case 3（#4 限次 2026-11-27 已关闭）。

> 验收 #4「重复重定向」2026-11-27 已落地（commit 55097e17）：会话级重定向限次
> 进 M1 出口重试闸——同一路由身份（space/cell/直连端点四元组，受理定格）上
> 连续 2001 计数（重试应答也计入——重试本身即重定向）达上限
>（`M1EgressConfig.max_redirects_per_route`，缺省 2；0 = 不限次旧语义）后
> 停透明重试，但每次 2001 照常作废会话路由缓存（宿主回退查目录重解析——
> 既有 ensureRouteSnapshot→fetchRouteSnapshot 链即回退面，不新增协议）；宿主
> 重解析换路（提示变化）或 code 0 证明路由可用 → 预算重置。限次账 idle 超
> 4×ack_timeout 回收（会话消亡无通知面，出口侧闲置窗界定账面规模）。观测面
> 新增 `redirectLimitHits()`。测试 `test_egress_redirect_limit_resets_on_
> route_change`（活区 GatewayM1EgressTests）六段断言：① 同路由 2001 对预算
> 内重试两发；② 第三次 2001 越限停重试、缓存照常作废；③ 持续越限仅作废；
> ④ 换路预算重置重试恢复；⑤ code 0 清账；⑥ 清账后重试恢复。计数收口
> failedAcks=8/sent=9/redirectLimitHits=2/invalidated=8。主树全量重建 exit 0 +
> ctest **129/129**；off-tree2/examples 全量重建 exit 0 + ctest 各 **129/129**；
> san 树 GatewayM1EgressTests+GatewayPushDownlinkTests 2/2（ASan+UBSan）。

> 验收 #1「下行绑定真实会话」2026-11-27 已落真链缆线 drill（`GatewayPushDownlinkTests`
> 新增 `downlink_binds_live_session_only`，todo.md L122）：正半面（939 按会话号真投
> + 双活会话隔离）既有 `push_session_addressed` 已钉；本批补负半面与恢复半面——
> World 侧无断线通知接线 = 掉线玩家实体留场，A 的寻址拷贝持续可造，三面全走
> 939 → 网关 `sendToClient` 会话闸（保留窗缩窗 800ms）：① 保留窗内
> （connectionId=0）AOI 广播仍寻址 sid_a → 闸落不投任何 socket（B 缆面零 sid_a
> 帧）+ 新观测面 `staleDownlinkDrops()` 计数前进；② 窗内同号重连恢复 → 广播帧头
> sid_a 真投**新** socket（绑定跟随活会话，不随旧连接死亡）；③ 保留窗到期销毁 →
> 迟到 939(sid_a) 落 unknown-session 闸 → 不投 + 网关存活（B PING→PONG）。

## 2. D09 · 登录完整链路分段表

### 2.1 分段与成功条件/超时动作

| 段 | 请求 ID | 成功条件 | 超时动作 |
|---|---|---|---|
| 认证 | 认证请求 | 凭证校验通过 | 撤销会话，返回失败 |
| gateway 会话 | 会话创建 | 会话建立、绑定 gateway | 释放会话资源 |
| Base 激活 | 激活请求 | 经 database 激活成功，保有 Base 对象 | **先查已有激活结果**，不盲目重建 |
| World 准入/分配 | 准入请求 | 容量预留、Space 分配 | 撤销准入，释放预留 |
| cell 创建实体 | 创建请求 | 实体创建、进入模拟 | 可重试（撤销准入后重试） |

> 关键：登录超时后**先查已有激活结果再重试**——盲目重建会出两个资料写入者。

### 2.2 三个验收场景

1. **登录到可移动**：认证→会话→激活→准入→cell 创建→可移动（完整链路）。
2. **未进玩法仍保有 Base**：准入前断开，Base 对象保留，重连恢复。
3. **cell 创建失败可重试**：注入 cell 不可达，重试后成功，无重复 Base。

> 明确不做：延后跨服登录和自动选区；不为等待进入地图的角色创建第二个 Base。

## 3. D10 · 两 gateway 两客户端最小真实网络用例

### 3.1 用例拓扑

两个 gateway（g1、g2）+ 两个真实客户端（c1、c2）+ 单 Base/单 World/单 cell/单 database。

### 3.2 覆盖矩阵

| # | 场景 | 断言 |
|---|---|---|
| 1 | 同时登录（c1@g1、c2@g2） | 两会话各自有效，互不覆盖 |
| 2 | 旧连接迟到输入 | 旧会话输入被会话版本裁决拒绝 |
| 3 | 网关故障后恢复 | 免重新认证恢复原 Base 与当前模拟位置 |
| 4 | 唯一资料写入者 | 旧会话撤销后仅新会话可写 |

> 明确不做：延后 TCP 连接句柄交接；不把同进程假客户端重连当作多网关验收。

## 4. D11 · 小队资格与有期限准入凭证

### 4.1 凭证字段

| 字段 | 说明 |
|---|---|
| `credential_id` | 凭证唯一 ID（单次消费） |
| `player_id` | 绑定玩家 |
| `squad_id` | 所属小队 |
| `target_space` / `target_instance` | 目标玩法（用途绑定） |
| `target_cell` | 目标执行端 |
| `roster_hash` / 资格版本 | 成员/资格快照 |
| `issued_at` / `expires_at` | 有效期（有期限） |
| `issuer` | 签发方（Base，经 World 预留容量后签发） |

### 4.2 拒绝原因与四个负例

| 负例 | 拒绝原因 | 判定 |
|---|---|---|
| 过期 | `expires_at` 已过 | 拒入 |
| 错实例 | 目标 space/instance 不匹配 | 拒入 |
| 重复使用 | `credential_id` 已消费 | 拒入（单次消费） |
| 资格变更 | roster_hash 与当前快照不符 | 拒入 |

> 跨区成员复用这套规则，等 L168 触发再接；先接一 Base 的真实入场链。

### 4.3 issuer=Base 签发与容量预留切片设计（2026-11-28 开卷，多日批）

> L131 剩余缺口（todo.md:131「仍缺」）的设计面：受理面自签（2026-11-23 批
> 1c3e9bf0）→ issuer=Base 真实链。开卷条件已变——双 Base 真机编队已落
> （dev_fleet 941b289b：database-b/base-b + 目录逐玩家归属）、容量槽位语义
> 已有设计（07 §3.1 S03）、cell 校验面已落地（凭证字段/负例/重臂/回收全
> 口径）。原「不重试」标记按条件变化解除，实现按下列五批推进。

#### 现状锚点（设计输入）

| 面 | 现状 | 权威方 |
|---|---|---|
| 名册（party roster） | `PartyManager` cell 侧内存（构造注入 SquadInstanceService） | cell（社交易变面） |
| launch 路径 | client → gateway → World M1 → cell（`SQUAD_LAUNCH`） | 不改入口 |
| 凭证签发 | 受理收口自签 `cell:<worldId>`，进程内自持不走 wire | cell（待移 Base） |
| 凭证核验 | `enter()` 内部查（`resolveIntentPlayer` 会话→玩家已在认证面绑定） | cell |
| 资格判定依据 | Base 档案（exp/settledIds/迁移冻结标记），零小队存在 | Base（档案权威，D06） |
| 容量语义 | S03 槽位（07 §3.1）：967 自报、心跳窗过期、World 准入消费 | World |

#### 职责切分（不反转 launch 入口，受理点改道）

Base 校验**资格**（档案面），不接手**名册权威**——party 仍是 cell 侧内存社
交面（空队回收、队长移交语义不动），Base 只对送来的名册快照逐成员盖资格章
并铸 roster_hash。launch 路径保持 client→gateway→World→cell；受理点从
「自签」改「代申请」：cell 经 World 向 Base 送名册快照 → Base 盖章 → Base
向 World 申请容量 → World 预留（S03 槽位记账）→ grant → Base 签发 → 凭证
包经 World 下推目标 cell → enter 时核验消费。装配 flag 门控（未启用=自签
旧口径，零回归）。

#### 新增 M1 消息（internal-only 域，client hash 不动）

| 消息 | 走向 | 载荷要点 |
|---|---|---|
| `squad_validation_request` | cell→World→Base | party_id、名册快照（成员列表）、目标 instance/cell、request_id |
| `squad_validation_reply` | Base→World→cell | 逐成员资格裁定 + roster_hash 盖戳，或拒绝码 |
| `squad_capacity_request` | Base→World | 已盖章名册 + 目标 + 申请槽数（=成员数） |
| `squad_capacity_grant` | World→Base | 预留 id + 过期窗，或拒绝码（无容量/实例未知/排空中） |
| `squad_credential_bundle` | Base→World→cell | 逐成员凭证（issuer=`base:<id>` + 原 Base 端点绑定） |

新链路：base → World M1（9101）出向一条（cell_register_client 同型拨装）；
M1 request/reply 双向，同链承载校验来向与签发去向。契约加消息须同步
test_contract 计数断言 + contract_handlers.lua 占位行（两套闸）。

#### 资格规则（最小口径，不发明门槛）

成员在原 Base 档案存在 + 未冻结（迁移冻结标记）即合格；roster_hash 沿用
已落地序无关 FNV-1a，盖戳含资格版本（受理快照代次）。不做等级/付费等发明
性门槛——产品化资格规则触发时另批。

#### 会话可信绑定（原缺口③的收口口径）

凭证按 player_id 绑定即可：enter 只能来自该玩家已认证的 gateway 会话
（`resolveIntentPlayer(sessionId)` 已在受理面把会话钉到玩家），会话→玩家
→凭证传递闭包成立，凭证无需携带会话号。重登新会话照常凭在期凭证进入；
凭证过期/消费后重入须重走签发面。

#### 预留生命周期

申请槽数 = 成员数，对目标 instance 所在 cell 的自报容量记账（S03 槽位）；
grant 带过期窗——签发时预留已过期则按拒绝处理（不放凭证）；成员 enter 消
费各自凭证即释放对应槽；到期未消费整批回收。容量拒绝 = 明确码回传（不静
默降级为无凭证放行）。

#### 分批与验收

| 批 | 内容 | 验收 |
|---|---|---|
| ① wire | 970-974 契约五件套 + base→World M1 链路（未启用） | 契约 golden 闸二；echo/编解码测试；全树既有套件零回归 |
| ② 预留 | World 侧槽位记账 + grant/拒绝/过期回收 | 预留记账单测（无容量/排空/过期/消费释放四路径） |
| ③ 校验签发 | Base 资格盖章 + 签发 + bundle 下推 | 资格单测（存在/缺席/冻结/序无关 hash 稳定）；签发字段口径 |
| ④ 受理改道 | flag 门控自签→代申请 + enter 消费接 wire 凭证 | dev_fleet 真链 e2e：签发→下推→enter 消费→leave 重臂；四负例真链化 |
| ⑤ 双 Base 落点 | 原 Base 端点绑定逐成员落 base/base-b | 真机双 Base：两库玩家各自凭证 issuer/原 Base 绑定正确、串库负例 |

#### 明确不做（边界）

- 名册权威不移 Base（不落库 party；社交易变面保持 cell 内存）；
- 多 World 容量仲裁不在本卷（单 World 先真链，多 World 随 D17 跨 World
  唯一决定面）；
- 跨区成员复用随 L168 触发（§4.2 既有边界不变）；
- 资格门槛止步「存在+未冻结」，产品化规则另批；
- 凭证密码学签名不在首卷（签发方身份以 issuer 字段 + 链路可信为界，与
  S02 启动凭据「开发模式受限传递」同口径；生产签名面留替换接口）。

## 5. D15 · 显式换场与同 Space 连续交接两条路径

### 5.1 两条路径的 UX 与状态清单

| 路径 | UX | 状态 | 状态清单 |
|---|---|---|---|
| **显式换 Space**（现交付） | 明确短暂换场等待（loading/等待屏） | 冻结 → 准备 → 提交 → 失败返回 | `idle → transferring → committed → failed(rollback to idle)` |
| **同 Space 跨 Cell 连续交接**（延后） | 无感知（目标：不可见中断） | 设计保留 | 依赖 L139 拓扑/邻接（BLOCKED→D03 已代决） |

### 5.2 显式换场原语（SceneTransfer 离场重建）

- 复用 `scene_transfer.hpp:34` 的离场重建原语作换场原语；
- 冻结：离场前停新输入；准备：目标空间准备；提交：World 提交位置；失败返回：还原到原位置。
- **不能**用它证明跨边界无缝（离场重建影响可见集与输入连续性）。

> 明确不做：延后同 Space 多 Cell 连续交接，触发条件为明确跨边界玩法需求；
> 不把该延后项阻塞所有普通换场。

### 5.3 显式换场真链路验证（2026-11-23，commit 054fc004，见 todo.md L135）

D15 立即执行动作「验证真实链路」的落地面——同一 cell 两 Space（501/502 均归
cell-21，池需 0x1；cell-22 无 0x1 不落 501/502）：

- **提交**：真实客户端经 gateway→World→cell-21 发 `cell_cross_border`，
  `scene_transfer` 走完 prepare→begin→detach→attach→complete（离场重建
  Avatar，易失态不带走），1001 从 501 的 scene 迁到 502 的 scene；
- **失败返回**：目标 503 归 cell-22（本 cell 无路由）→ prepare 显式驳回，
  1001 留在 502（不静默同场搬运）；
- **观测面**：`CellApp::space_of_player`（Avatar 挂 scene 归属是唯一权威——
  scene_transfer 的会话 space 字段只是路由投影，不反推；内核线程上下文约束）
  + 宿主 post_tick 内核线程采样经 atomic 发布（测试线程直读 scene 表即数据
  竞争，同 reportedReady 纪律）。
