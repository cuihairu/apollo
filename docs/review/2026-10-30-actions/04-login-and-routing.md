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
> 被拒意图的客户可见重同步（协议设计拍板），以及 D08 #4 重定向限次、D10 case 3。

> 验收 #1「模拟输入直达当前 cell」2026-11-22 已接线（commit 1c602137，todo.md
> L122 过期注记 2026-11-27 复核刷新）：gateway 不再经 World 管理端物理中继模拟
> 输入——端点发现链 968 投影（World 发布归属 cell 注册端点）→ 登录投影名册 →
> 915/918 → `RouteSnapshot.cellHost/cellPort`；M1 出口逐 cell active 端点按
> host:port 拨装（`target_link`，逐条目就绪门：未就绪 cell 不阻塞其它端点），
> 929 应答与 939 下行直回直连链，World 前递保留为回退面（端点未声明/未连接）；
> 请求号只对实发帧分配（requeue 不耗号）。测试：`test_gateway_m1_egress` 直连
> 观测面（`directCellIntents` 计数）+ login_chain 落点断言
> `assigned->cell_host/cell_port`。D08 #1 两半（Base 直达 + cell 直达）验收面
> 均关闭；本节仍缺同上（重同步拍板、#4 限次、D10 case 3）。

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
