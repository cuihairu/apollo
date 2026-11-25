# D03 / D14 / D29 / D30 · 拓扑分配、多进程用例、分区契约、Real/Ghost 边界

> 来源：docs/review/2026-10-30-decisions.md D03（todo.md:139）、D14（todo.md:163）、
> D29（todo.md:189）、D30（todo.md:190）。本文件为设计稿；运行实现按各依赖批次推进。

## 1. D03 · Space→Cell 分区→执行进程关系表与版本变更规则

### 1.1 首阶段分配规则（已代决）

- 一个 Space 一个完整 Cell；World 独占分配决定；新 Space 按就绪/能力/容量选择执行端；
  已有 Space 保持归属（稳定分配，无自动迁移）；每个 Instance 固定一个逻辑负责人。
- 负载数变化**不**当作拓扑变化（明确不做：同 Space 多 Cell、邻接算法、自动动态重平衡）。

### 1.2 关系表

| 概念 | 标识 | 权威 | 说明 |
|---|---|---|---|
| Space | `space_id` | World | 空间（场景实例） |
| Cell 分区 | `cell 分区 id` | World | 首阶段退化 = scene id（单 Cell 契约，见 D29） |
| 执行进程 | `cell_id`（进程 id） | World（注册/分配） | 实际推进模拟的 cellServer |
| 分区范围 | 坐标/范围 | 设计保留 | 多 Cell 时才消费，本轮不传输 |
| 邻接 | 邻接表 | 设计保留 | 多 Cell 时才消费，本轮不传输 |
| 拓扑版本 | 布局版本 | World | 表示「布局变了」，与实体权威代次/负载分离 |

### 1.3 版本变更规则（四种例子）

| 事件 | 版本动作 | 记录 | 说明 |
|---|---|---|---|
| **创建** | 分配版本 +1 | journal `A space_id\|cell_id\|version`（先 fsync 再推 962） | 新 Space 按就绪/能力/容量选 cell |
| **释放** | 版本不变（owner→0） | journal `R space_id` | 过期释放；重放以每 space 最后一条 A/R 为终态 |
| **重认领** | 版本 +1（防 ABA） | journal `A ...` 新行 | 旧端撤销/到期后重认领，版本单调前进 |
| **重启** | 管理代次 = 最大 B + 1 | journal `B management_epoch` 行 | World 启动先记后开受理面；水位拦旧端 |

> 规则：一次 Instance 只有一个提交结算者；同一 Space 的每次转移都记一行 A（版本单调），
> 杜绝两写者通过版本回退同时生效。

> D03 单提交者落地批 2026-11-26 已执行（commit 3594b6fe，见 todo.md L106）：结算三写面
> `credit_settlement`/`retry_settlement`/`restore_settlement` 补齐与 enter/start/abort/dismiss
> 同款的归属 World 守卫——远端 World 持有绑定但本进程无本地实例时，显式奖励口拒
> （UnknownPlayer）、补账与恢复回填空操作；归属方同路径照常落账。测试：
> `squad_settlement_single_owner_world`（远端三写面全拒且不改账本/业务面、归属方逐玩家
> 落账一次），SquadInstanceTests 51/51；主树/off-tree2/examples ctest 各 128/128；
> san 树目标 6/6（ASan+UBSan）。仍缺（D03 明确延后，不重试）：邻接算法、同 Space
> 多 Cell、自动动态重平衡、单实体独立权威模拟代次。

## 2. D14 · 两 cell 真实隔离与回收用例设计

### 2.1 用例拓扑

一个 World（管理 M1 9101）+ 两个独立 cellServer（cell-21 @9301、cell-22 @9302）+ 一个登录/网关真实链路。

### 2.2 空间分布

- **Space 501:arena** 归 cell-21；**Space 502:lobby** 归 cell-21；**Space 503:duel** 归 cell-22；
- **临时 Instance 504:instance**（如副本）归 cell-22。

### 2.3 断言（真链路验证）

| # | 断言 | 判定 |
|---|---|---|
| 1 | 玩家进入 501 → 输入（移动/技能）到达 **cell-21**，非 cell-22 | 选路正确 |
| 2 | 玩家进入 503/504 → 输入到达 **cell-22** | 选路正确 |
| 3 | 旁观者在另一 cell 的观察者 **收不到** 501 的实体广播 | 无跨 cell 误广播 |
| 4 | 释放 504 → cell-22 确认远端销毁（销毁回执） | 远端回收有确认 |
| 5 | 同 Space 内两玩家（同 cell）正常 AOI/战斗 | 单 cell 模拟不受影响 |

> 明确不做：不按实时负载搬迁正在运行的 Space；不用玩家档案搬迁（旧 World 间演示）替代模拟选路验收。

> D14 首切片 2026-11-23 已落地（commit 88900e71，见 todo.md L134）：cell 侧能力位声明接线——cell 随 960 携带声明能力位（`--declare-capabilities`，0=未声明），World 侧 `world_space_assign` capability_fit 以「声明覆盖 Space needs 全部位」裁决认领资格（needs=0 无要求 = 首就绪独占语义不变）→ 确定性两 cell 分空间（501:1→cell-21、503:2→cell-22，非首就绪）；真实两 cell 选路 e2e（两独立 cellServer + 登录/网关真链路，网关路由客户端接 login 内建 OnlineDirectory M1 控制面 `directoryM1Port=listenPort`——缺省 0 不装配路由客户端，首意图懒解析 fetchRouteSnapshot 得 nullopt→buildDefaultRoute 空间 0→真实按玩家空间选路退化首就绪兜底），覆盖断言 #1（1001 进 501 输入到 cell-21）、#2（1002 目录覆写 503 输入到 cell-22）、#3（跨 cell 隔离：cell-21 无 1002、cell-22 无 1001）、#5（同 cell 锚点在线不受影响）。

> D14 回收确认批 2026-11-23 已落地（commit 39f74e87，见 todo.md L134）：释放→远端销毁有确认（断言 #4）——releaseCellSpaces 对在册 releasing cell 直发 962 ready=0 撤销标记（D03「释放 版本不变」，cell 侧版本闸按 ready 1→0 迁移放行一次），腾空即 remove_space 销毁、经新契约 969 cell_space_reclaim_done 回执（request_id/cell_id/start_epoch/space_id，967 同型验证），World 侧 reclaimConfirmations_ 记账即确认面；e2e 经 963 retire 触发释放 504 → cell-22 确认销毁回执（503 占用中无回执）。**仍缺**（另起批）：#3 的真观察者无广播语义（本切片以隔离断言覆盖）。
> D14 动态创建分配路径批 2026-11-23 已落地（commit 995ae9b4，见 todo.md L134）：临时 Instance 动态创建——World 运行时铸造 space_id（静态 assignSpaces 池外），新契约 970 space_create_request → 971 space_create_reply（code 0=成功 / 1=目标 cell 未注册/未就绪/排空/缩容/存疑 / 2=journal 写入失败或证据冻结）；资格=已注册+就绪+非排空+非缩容+非存疑；write-ahead D 行（铸标记）+ A 行（归属 v1）落盘成功才推 962 + 回 971；D 行区分动态铸造空间与移除池静态空间（装载时 D 标记池外条目重建归属、无 D 标记跳过，批B 语义不变），铸造计数抬升 max(池 ∪ D ∪ A)+1——重启不重铸已铸/已复用 id；缺失 journal 证据冻结拒铸造；pushSpaceAssignments 增动态空间第二循环（重注册 cell 对自有动态空间推 ready=1、他方 ready=0 撤销、owner=0 不推——临时 Instance 不复活）。测试：`test_cell_space_create_dynamic`（合法铸 9/10 单调 + cell 0/未知拒 code 1 + request_id 0 保留无回执）+ `test_journal_dynamic_space_across_restart`（D+A 行落盘、重启重建 62/移除池 99 跳过、计数抬升 100 防撞、重注册 962 刷新、重启后续铸）。主树 ctest 128/128；off-tree2/examples 128/128；san 树 128/128（ASan+UBSan）。**仍缺**：#3 真观察者无广播语义（D30 observer/Ghost 依赖）。

## 3. D29 · 同 Space 多 Cell 契约预留（分区描述草案）

### 3.1 独立语义（本轮钉清，不混用）

| 层级 | 身份 | 本轮真实传输 | 仅设计 |
|---|---|---|---|
| 空间 | `space_id` | ✅ | |
| Cell 分区 | `cell 分区 id`（首阶段退化 = scene id） | ✅（退化语义） | |
| 执行进程 | `cell_id`（执行进程 id） | ✅（960/961/962 注册与分配） | |
| 全空间坐标 | `position`（空间内坐标） | ✅ | |
| 实体 ID | `entity_id`（稳定，跨迁移不变） | ✅ | |
| 分区范围/邻接 | 范围、邻接表 | | ✅ |
| 拓扑版本 | 布局版本 | | ✅（本轮未消费） |
| 实体权威代次 | 权威代次 | | ✅（本轮未消费） |

### 3.2 两 Cell 示例

```
Space 1001（大地图，2 个 Cell 分区）：
  cell_0 分区 id=0，范围 x∈[0,100)，邻接 cell_1
  cell_1 分区 id=1，范围 x∈[100,200)，邻接 cell_0
  分配：cell_0 → cellServer #21，cell_1 → cellServer #22
实体 e=777 位于 x=50 → 权威 cell_0；e=888 位于 x=150 → 权威 cell_1
拓扑版本 = 布局版本（分区/邻接/分配变更时 +1）
```

> 说明：以上两 Cell 示例仅演示契约表达，**本轮运行仍一个 Space 一个 Cell**；
> 未消费字段不扩 wire（「明确不做/延后」：延后多 Cell 算法与未消费的全量 wire 扩展）。

## 4. D30 · Real/Ghost/观察者权限表与跨边界裁决

### 4.1 权限表

| 角色 | 模拟权 | 持久写权 | 可见性 | 说明 |
|---|---|---|---|---|
| **Real** | ✅ 唯一 | ✅ 唯一写者 | 自身权威 | 持有模拟权威；迁移快照**不是** Ghost |
| **Ghost** | ❌ | ❌ | 只读复制 | 仅展示对端状态；不推进、不写盘 |
| **观察者** | ❌ | ❌ | 决定看见什么 | 可见集连续/去重；不参与模拟 |

### 4.2 边界裁决与顺序（设计）

| 事件 | 由谁裁决 | 规则 |
|---|---|---|
| 迟到复制 | 接收方（Real） | 按复制版本丢弃过期副本 |
| 实体离开邻接区 | 源 Real | 清理 Ghost 引用并下发移除 |
| 重复可见事件 | 接收方 | 按 entity_id + 序号去重 |
| 跨 Cell 请求 | 目标 Real | 目标 Real 唯一裁决；顺序/幂等/故障对账 |

> 明确不做：延后邻接复制与跨边界战斗实现；不把一次性迁移快照称为 Ghost
> （迁移快照属 D16 迁移状态交接，非复制视图）。
