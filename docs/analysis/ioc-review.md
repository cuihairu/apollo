# Apollo IoC 复审系列报告（ioc-review）

> **承接说明（文首）**：本文件承接 `docs/analysis/architecture-review.md` 的 IoC 复审系列。architecture-review.md 原名 `ioc-review.md`（2026-09-29 更名记录见该报告 :3——起点是 IoC/DI 评审，后扩展为全仓审计，C-1…C-82），更名后该文件名空缺；本文件**不是旧文件复活**，而是系列续篇按巡检派发指定的文件名新建——设计缺口复审批次的结论只落此一文件。缺陷编号与主报告共用同一 C 系列（承接后自 C-83 续用；建文前已全仓核实 C-83/C-84 未占用——主报告最新编号止于 C-82，§23 四teenth轮占用 C-80…C-82）。
>
> **门禁（2026-09-30 本轮巡检令）**：只读分析项目——只写本报告这一份文件；零源码改动；不改其他任何文件（含 gap-inventory / architecture-review 主报告，回填候选只登记不执行）；不派子代理。

---

## 1. 第一轮（2026-09-30）：设计缺口 #13「登录链路整体设计」复审批次（C-83、C-84）

### 1.1 范围与方法

- **复审对象**：design-gap-inventory #13（:112-116，OPEN，2026-09-30 增补）——缺口判定、三条证据、依赖与落点行的现状复核。**不写 #13 设计稿本身**（设计批是后续批次，本轮只出复审结论与设计批输入）。
- **方法（三类取证，全部只读实测）**：
  1. **缺口证据负空间复测**：design/ 全量 grep `login|登录|选服|排队|login_token`（排 session-and-online-directory 单列 + 不排各命中文件），复核 gap #13 证据行「仅三处一句带过」的计数在 #12 落盘后的现状；
  2. **先例引注复核**：deep-dive :100/:315（KBE loginapp + clientsdk_downloader）、36 号 :55/:66（BW/KBE loginapp 指派）逐条对行；
  3. **存量实读**：apps/login-app 全文（src 670 行 + include 214 行）与 apps/gateway-app 准入面（1591 行中的 ingress 六文件），核对「存量实现 vs 设计目标」的错位面（缺口判据 (b) 存量冒充）。
- **边界**：本报告不执行 gap-inventory 状态回填（#13 维持 OPEN 的登记动作归 gap 下一更新批——本轮禁改他件），回填候选见 §1.6-5。

### 1.2 逐件核对表

| # | 复审件 | gap #13 登记内容 | 2026-09-30 实测 | 结论 |
|---|---|---|---|---|
| 1 | 证据行计数 | 「design/ 中 login 相关仅三处一句带过（§5.9 login_token、sdk-contract schema_hash 握手、§5.7 epoch）」 | 三处原证**仍在**：net-abstraction §5.9 :305/:313/:317（ClientHello 携 login_token / 签发归属 / 三层密钥）、sdk-contract :13/:172（schema_hash 连接握手）、§5.7 :203/:216（握手三元组 authority_epoch）；但计数已过时——#12 落盘（f3aa8a30）后 design/ 登录相关命中点增至 10+ 处（session-and-online-directory :30/:40/:71/:77/:114/:141 六处、concept-glossary :44、scripting-lua :233、battle-verification :117、net §7 :436 交集行）。命中全部是**周边消费面**（风控供数/准入闸门消费/token 入场前置/边界指派）——login-app 职责链本体仍零设计 | 判定**不变**；计数**过时** → C-83 |
| 2 | KBE 先例 | 「KBE loginapp + clientsdk_downloader.{h,cpp}（deep-dive §4 已核）」 | deep-dive :100 属 §4 节内（:96-:114）：`kbe/src/server/loginapp/clientsdk_downloader.{h,cpp}` 路径复核行在案；:315 KBE 进程清单含 loginapp；36 号 :66 KBE 拓扑「loginapp → baseappmgr/cellappmgr → …」 | **成立**（引注准确） |
| 3 | BW 先例 | 「BW loginapp 指派 baseapp（36 号 §2.1）」 | 36 号 :55（§2.1 标题 :53 节内）：「客户端先连 loginapp，被指派给 baseapp，baseapp 再把实体投递进 cell 空间」 | **成立** |
| 4 | 依赖行 | 「依赖 #12（目录）与 G-1」 | #12 已 CLOSED（f3aa8a30 落盘 session-and-online-directory.md）——依赖**就位**且划出边界：:71「准入之前的链路（账号鉴权/token 签发/选服/排队）归 #13」、:10 同型、:114 DB 账号位「归 #13/批次 2 语句集」；G-1 = net-abstraction §7 已设计（machined+UDP 双层，36 号 #13 裁决同步） | 依赖就位，**批次可排**；但 :114 指派的 DB 账号位子项未进 #13「缺什么」清单 → C-84 |
| 5 | 落点行 | 「拓扑入口 = gateway-app（sdk-contract §10.6 网关透传）」 | sdk-contract :502 实为**解码装配**讨论（「网关进程用案二形态（纯透传、零反射设施）」——装配谱系两端），非拓扑入口声明；拓扑入口的权威出处 = docs/index 拓扑图（Client──Gate）+ 36 号进程拓扑 | 引注**可用但不精确**（§10.6 证的是「网关=透传角色」，不证「入口在 gateway」）——记入核对表不立号（补强随 C-83 同批回填） |
| 6 | 缺口状态 | OPEN（2026-09-30 增补） | 三处原证在 + 存量四点错位实锤（§1.3）+ #12 边界已划清 + 依赖就位 | **维持 OPEN 成立**；设计批可排（见 §1.6-3） |

### 1.3 存量实读：登录链三段烟囱（缺口判据 (b) 的实锤面）

**login-app（apps/login-app，884 行）**——src 670 行（login_server.cpp 577 + main.cpp 93）+ include 214 行（login_server.hpp 173 + config.hpp 41）；`namespace login` 自包含，**零框架使用**（WorldHost/ServiceHost/ApplicationHost 全仓 grep 零命中，唯一 apollo 依赖 = login_server.hpp:15 `namespace protocol = apollo::protocol`——§21.3 C-72「577 行自包含」结论复核成立）。形态 = RepSocket 自监听 :9001 + protocol::RpcClient 直呼 base-app + 独立 cleanup 线程（60s 轮询），与 WorldHost/manager 域零关系。逐件：

| 件 | 位置 | 现状 | 与设计面的关系 |
|---|---|---|---|
| Authenticator | :22-127 | 硬编码两测试用户（player1/pass1、player2/pass2）明文比较（:27 自注「简化，实际应该用 hash」）；失败锁定策略（maxLoginAttempts/lockout）已有雏形 | 鉴权源零设计的存量冒充——账号库形态归 #13/批次 2 |
| GatewayAllocator | :133-179 | 连接数最少的网关选择 | 与 BW minAppLoad 最轻分配（net §7 :389）同型雏形 |
| SessionManager | :185-301 | sessionId↔loginTicket 双哈希；**loginTicket = 自造 xorshift 系 PRNG 32 hex**（:284-301，seed = 时间戳^playerId^sessionId——可预测，非密码学安全）；validateSession **滑动续期**（:245 `lastAccessMs = now`） | 与 §5.9「短 TTL + 一次性」（:313）**直接冲突**——非 TTL 非一次性非验签 |
| preparePlayerOnline | :476-542 | 直呼 base-app 三连 RPC（PlayerActivate → PlayerBindSession → PlayerAssignWorld 初始 world/map），**绕过 gateway 与 manager 目录**；:536-541 catch 吞异常 `return true`（stub 模式「假成功」） | 与 #12 登记锚点（:71 manager 准入 → Zone 接受 → SessionUp 才可见）**错位**；假成功是链路可信度缺陷面 |
| main | :20-47 | 命令行参数（--port/--base-app/--gateway），无配置系统 | 与 config 桩审计面（C 系列）同族，非本轮对象 |

**gateway-app（apps/gateway-app，1591 行；ingress 六文件 333 行）**——`session_admission_service.cpp` :13-58 持 `loginAppClient_`（RpcClient），准入 = 检查 loginTicket 非空（:18-19）后 **RPC 回问 login-app validateSession**（:33）——网关准入依赖 login-app 存活，是**中心化校验**形态；与 §5.9 :313 意向的「游戏进程只验签不触账号库」（自包含 token 本地验签）**形态不同**。

**四点错位（= #13 缺口的存量实锤；迁移归代码批，本轮零改动）**：

1. **token 语义**：滑动续期可复用 ticket vs §5.9 短 TTL + 一次性；
2. **校验形态**：gateway RPC 回问 login-app vs 设计意向的本地验签（一次性核销点在哪，是设计批裁决问①②）；
3. **拓扑与登记**：客户端直连 login-app:9001 + login 直呼 base 三连，绕过 gateway 入口与 manager 准入登记锚点；
4. **鉴权源**：硬编码测试用户 vs 账号库（批次 2）。

### 1.4 缺陷登记

| 编号 | 内容 | 证据 | 严重度与处置 |
|---|---|---|---|
| C-83 | gap-inventory #13 证据行「design/ 中 login 相关仅三处一句带过」**计数过时**——#12 落盘后 design/ 登录相关命中点增至 10+ 处（清单见 §1.2-1）；三处原证本身仍准，缺口本体判定不受影响 | f3aa8a30 落盘 session-and-online-directory.md 六处 + glossary/scripting-lua/battle-verification/net §8 交集行（本轮 design/ 全量 grep 实测） | 低（P 系列勘误型）。**回填候选**：随 gap-inventory 下一更新批改写 #13 证据行（本轮禁改他件，只登记）；顺带补 :10.2 核对表第 5 项引注补强（§10.6 → index 拓扑图） |
| C-84 | gap-inventory #13「缺什么」五项清单**未收录** #12 显式指派的 DB 账号位子项（最后在线时间/最后 Zone 存档列提升） | session-and-online-directory:114「账号位…归 #13/批次 2 语句集」vs gap-inventory:114 五项（鉴权/token/选服排队准入/SDK 下发/会话裁决衔接）无此项 | 低（范围登记不全）。**回填候选**：#13 设计批立项时直接并入范围，或 gap 回填批补一句（本轮只登记） |

### 1.5 实读核对记录

- **负空间**（design/ 全量）：`grep -rn -i "login\|登录\|选服\|排队\|login_token" docs/design/`——命中逐文件复看（见 §1.2-1 清单）；session-and-online-directory 单列后剩余三处原证逐行对号（net §5.9:305/:313/:317、sdk-contract:13/:172、net §5.7:203/:216）。attribute-sync 的「登录」命中（:78/:130/:153/:211/:259/:287）全部是登录**时点**（快照/token bucket/schema_hash），非登录链设计——不计入缺口证据。
- **先例引注**：deep-dive :100（§4 节内 96-114，clientsdk_downloader 路径复核行）、:315（进程清单）；36 号 :55（§2.1 标题 :53）、:66（§2.2）——逐条与 gap 证据行对行，全中。
- **存量**：`wc -l` apps/login-app src/include 全文件（577+93+173+41=884）；login_server.cpp / main.cpp **全文读毕**；框架符号 grep（WorldHost|ServiceHost|ApplicationHost|apollo::）apps/login-app 全树 = 仅 login_server.hpp:15 命名空间别名；apps/gateway-app 全树 wc（1591 行）+ ingress 六文件 grep login/准入（session_admission_service :13-58 完整实读）。
- **编号占用**：`grep -o "C-8[0-9]" architecture-review.md | sort -u` = C-80/C-81/C-82；`grep -rn "C-8[3-9]" docs/` 零命中——C-83/C-84 建文前核实未占用。
- **基线**：`git log --oneline -1` = 2ef8bf05（origin/main 同）；工作树仅三项受保护 untracked（BIGWORLD_AUDIT.md / Testing/Testing/ / ipc_audit_results.md）——未碰。

### 1.6 复审结论与设计批输入

1. **缺口判定维持成立（OPEN）**：三条证据逐一对行全中（§1.2-2/3），存量三段烟囱与设计面四点错位（§1.3）是缺口判据 (b) 存量冒充的实锤——#12 落盘不关闭 #13，反而以 :71 边界声明把「准入之前的链路」整体划给 #13。
2. **依赖就位**：#12 CLOSED（目录数据面与登记锚点已定）+ G-1 已设计（net §7）——gap 落点行的两前置均满足，#13 设计批**可排**（P2-P3 落点维持）。
3. **建议批次归属**：下一个设计批 **B11 = #13 登录链路整体设计**（+ **#14 同批**——gap:122「随 #13 同批」；**#15 互为验收**——gap:128「与 #13 登录链互为验收对象，建议同批」；#16 无依赖关系不动）。
4. **设计批裁决问题清单（本轮复审给设计批的输入，六问）**：
   - ① **拓扑形态**：客户端先连 login-app（KBE/36 号 :55 与存量同型）还是全程经 gateway（index 拓扑 Client──Gate + §10.6 透传角色）——login-app 是否藏在 gateway 之后、登录连接与游戏连接是否分端口；
   - ② **token 校验与一次性核销形态**：自包含验签（§5.9 :313「游戏进程只验签」意向）vs RPC 回问中心核销（存量 gateway 形态）——核销单点与网关存活解耦的取舍，ticket 生成须换密码学随机（存量 :284-301 反面）；
   - ③ **账号鉴权源**：账号库形态（批次 2 storage.xml）+ P1/P2 硬编码降级形态（存量的合法退化位）+ 第三方登录接出（§5.10 出站 HTTP）与入站（#14 同批）的分工；
   - ④ **选服/排队/准入分工**：BW (addr,load) 上报 loginapp 分流先例（net §7 :389）vs apollo manager 集中准入闸门（net §7 已定大半）——login-app 与 manager 的职责分界线（#12 :71 锚点已定「准入通过」时刻归 manager，准入判定过程归谁）；
   - ⑤ **客户端 SDK 下发时机**：KBE clientsdk_downloader 同型（登录链一环）vs 独立请求——与 todo 批次 7 客户端 SDK 投影（:64-71）的配套；
   - ⑥ **与 #12 衔接与存量迁移**：登录成功 → manager 准入 → Zone 接受 → SessionUp 登记的完整时序（session :71）；存量 preparePlayerOnline 三连与 gateway RPC 回问的迁移路径归代码批。
5. **回填候选登记（本轮不执行）**：C-83（#13 证据行计数改写 + §10.6 引注补强）、C-84（DB 账号位并入 #13 范围）——随 gap-inventory 下一更新批；#13 状态行维持 OPEN 无回填动作。本报告不改 gap-inventory / architecture-review 主报告 / 登记簿。
6. **本轮状态**：只写本报告（新建 docs/analysis/ioc-review.md 一份）；零源码改动；三项 untracked 未碰；不派子代理；缺陷登记 C-83/C-84（建文前核实未占用）；单笔提交，push 前 fetch --rebase，无 tag/release/force push。

---

*评审基线（源码与文档）：main @ 2ef8bf05（= origin/main）。apps/login-app src+include 884 行全文、apps/gateway-app 1591 行准入面、design/ 全量 login 负空间 grep、gap #13 行、deep-dive :100/:315、36 号 :55/:66 均为 2026-09-30 本会话实测。*
