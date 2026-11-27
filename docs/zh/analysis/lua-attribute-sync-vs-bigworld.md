# Lua 属性变更的感知与同步：Apollo 现状与 BigWorld 对照

> 分析范围：属性**变更如何被感知**（写路径/声明/钩子）与**如何被同步**（可见域/增量/恢复/预算），
> 对照 BigWorld 的 Entity 属性机制。本文是机制分析，不是能力验收结论——「已有实现」只指
> 机制原语存在，端到端客户端交付另见 §7 缺口。
> 机制设计总稿：[属性系统与同步](../design/attribute-sync.md)；
> 脚本边界：[Lua 脚本与更新](../design/scripting-lua.md)；
> 进程对照：[BigWorld 与 Apollo 概念对照](../design/bigworld-apollo-mapping.md)；
> 机制取证基线：BigWorld `27446bab`（14.4.1）。

## 0. 一句话结论

Apollo 已把「感知」收窄为**单一写路径 + 钩子窗口 + 序号化变更史**（C++ 直写 / Lua 改值
两入口，均落到 `AttrSyncManager::set_attribute` 正规写面），把「同步」收窄为 **AOI 裁剪的
per-viewer delta + seq/ACK 水位 + 域投影 + 降级快照**；BigWorld 的同族能力建立在 **.def
声明 + 生成访问器 + 客户端可见标记 + 细节层级 + 易失属性**上。Apollo 当前缺的不是同步
算法，是**三块声明与投递链**：属性元数据声明（域/可见级/持久性）、客户端线帧契约、
以及 Scene 出帧到 Gateway/客户端的宿主消费。Lua 感知面已具备受控边界（白名单+帧预算+
错误保持原值），但**尚未有真实玩法属性经此路径写入**——机制未与玩法接死，这是最要紧的缺口。

## 1. Apollo 的感知路径（现状）

### 1.1 单一写入者 + 正规写面

写路径只有一条入口：`AttrSyncManager::set_attribute`（[attr_sync_manager.hpp](https://github.com/cuihairu/apollo/blob/main/modules/game/attrsync/include/apollo/game/attrsync/attr_sync_manager.hpp#L31)）。
它更新 `EntityAttrs.values_`、追加 `ChangeHistory`、置 dirty，并触发 `ChangeSink`
（持久化交接，§5）。所有感知者共用这一面：C++ 模拟写、Lua 钩子修订值，最终都经它落账，
不存在第二份影子表。这是 BigWorld 生成访问器收敛到的同一目标，但 Apollo 用运行时
显式调用而非代码生成实现。

### 1.2 两种感知入口

- **C++ 直写**：模拟逻辑直接调 `set_attribute`，天然感知。
- **Lua 钩子窗口**（scripting-lua §5.1）：Scene 六阶段 tick 在 **dirty 固定后、收集前**
  执行 `apply_attr_hook`（[scene.cpp:335](https://github.com/cuihairu/apollo/blob/main/modules/game/world/src/scene.cpp#L335)）。
  桥 `install_attr_hook_bridge` 把 attrsync 的 `HookChange` 窗口翻译成 `ScriptRuntime::AttrChange`
  快照，喂 `run_attr_hook_window`（白名单取 `contract_write_whitelist` 数据面、帧预算逐帧
  取 `attr_hook_frame_budget_`），修订值写回 HookChange 后**经正规写路径落存储+history+sink**。
  白名单外跳过计数、帧预算耗尽剩余跳过、钩子错误保持原值记 fault——三重隔离。
  注意这是**事后修饰钩子**（dirty 已固定、仅改值不改"变没变"），不是 BigWorld 的
  "写时订阅"。

### 1.3 感知的去向：ChangeHistory 即唯一投影

每次变更追加 `(seq, attr_id, value)` 到环形日志（容量默认 1024，溢出旧条目被覆盖）。
delta = `entries_since(acked_seq)`，快照 = 当前值集。**感知不需要 diff**：值变了即有序
条目，seq 单调即进度语义。这避开了 BigWorld 也避不开的「轮询比较」类成本，但代价是
环形覆盖对停摆 viewer 触发快照降级（§3）。

## 2. Apollo 的同步路径（现状）

### 2.1 触发与裁剪

Scene 挂 `AttrSyncManager` 后，AOI Enter/Sync 建 `ViewerAttrState`、Leave 回收
（[scene.cpp:287](https://github.com/cuihairu/apollo/blob/main/modules/game/world/src/scene.cpp#L287)）。AOI 决定**能否看见实体**，
`AttrDomain` 决定**能否看到其属性**，二者独立：Public/World 恒可见、Self 仅 owner、
Team/Guild 经 group gate（未设置=不可见）。快照与 delta 用同一裁剪规则。

### 2.2 增量与恢复

`DeltaBuilder` 按 viewer 水位从 history 取 `(acked_seq, world_seq]` 区间条目，按 attr_id
升序确定输出（per-slot 合并、先扩容后写 slot 的保序由组合 id 纪律天然满足）。
**快照切换**（任一成立即降级）：首见（acked_seq==0 且 world_seq>0）、历史被覆盖
（world_seq-acked_seq > 容量）、delta 属性数超阈值、ACK 超时（默认 3 拍未 ACK）。
ACK 超时降级即"恢复"：慢客户端/丢包者拿全量快照重建，而不是无限重放。

### 2.3 预算与节流

`AttrSyncScheduler` 每 viewer token bucket；`peek` 先算候选负载再 `collect` 提交副作用。
min_interval 控低频属性；预算不足保留可恢复状态，不丢最终权威值。importance/饥饿保护
的语义已定，但**距离与玩法重要性的真实数据来源仍未接**（design §5.2 明示）。

### 2.4 帧编码

`AttrDelta`/`FrameCodec` 自写 varint/zigzag（attr_id varint + value zigzag + change_n），
零 protobuf 依赖；帧边界校验在解码侧。**此编码尚无对应线上消息**——见 §7。

## 3. 与 BigWorld 的机制对照

| 维度 | BigWorld（Entity 属性体系） | Apollo 现状 | 差距性质 |
|---|---|---|---|
| 变更感知入口 | `.def` XML 声明属性 → 生成 Python 访问器（`__init__` 里 `self.x = v` 即订阅） | 显式 `set_attribute` 调用 / Lua 钩子窗口 | 声明化 vs 显式调用；BigWorld 少写样板但依赖生成链 |
| 客户端可见标记 | `ALL_CLIENTS/OTHER_CLIENTS/OWN_CLIENT/CELL_PUBLIC/CELL_PRIVATE/BASE_ONLY` + `<DetailLevel>` 0–3 | `AttrDomain`（Public/Self/Team/Guild/World） | Apollo 域未声明化，宿主手动 set，未声明=Public（默认过宽，需收紧） |
| 变化检测 | 访问器内 `changed` 标记 per-client 列表，tick 末 Witness 汇总 | seq 化 `ChangeHistory`，dirty 位 | 等价，Apollo 免轮询 |
| 可见集 | Witness 维护每个 Observer 的 details change 列表，AOI 进视野时按 level 0–3 逐步填充 | AOI 裁剪 + 域投影 + per-viewer seq 水位 | BigWorld 有细节分层渐进填充；Apollo 首见即快照+delta，无分层（但无 Ghost 时也不需要） |
| 易失属性 | position/yaw/pitch 独立通道、定频量化、不入属性流 | 移动走 `CELL_ENTITY_MOVE` 显式线帧（AOI 重编头），与属性流分家 | 设计一致：易失量本就不该挤属性通道 |
| 持久化 | `PersistentId`/`writeToDB` + offload flags；CELL_PUBLIC 镜像 Base | `ChangeSink` → 宿主（journal→database），不直连数据层 | 语义对齐；宿主业务侧字段映射/去重/重试未补齐（design §8） |
| 预算 | Witness 按带宽/细节层级限流 | token bucket + min_interval + 快照降级 | 机制齐；玩法级 importance 来源未接 |
| 序号与恢复 | 客户端按实体维护已收 level，缺级补发 | seq/acked_seq + 环形历史 + ACK 超时快照降级 | 等价语义；Apollo 用"降级快照"而非"补发缺级"，更简单 |

**关键差异**：BigWorld 的属性同步是**声明驱动**——写标记、可见级、持久性、细节层级都
在 `.def` 里，运行期只生成访问器。Apollo 是**机制驱动**——同步原语已完备，但声明层
（哪些属性、什么域、是否持久）与玩法数据都还空着。这意味着 Apollo 当前**感知链路通了
但没东西可感知**：`contract_write_whitelist` 来自 `load_contract` 的契约表，而当前
`contract_handlers.lua` 是占位空壳，没有真实属性 id 写入经此路径。

### 3.1 Lua 侧的本质差异

BigWorld 用 **Python 全脚本**（Entity 即 Python 对象，属性即 Python 属性），感知在
语言层免费获得，代价是沙盒边界粗（os 可达）。Apollo 用 **Lua 受控插件**：钩子只在
tick 固定边界运行、白名单+帧预算+错误保持、模块熔断/回滚独立。感知不是语言层
属性代理，而是**显式的 `onAttrChanged(entity, attr, old, new)` 钩子**（`set_attr_hook`
注册）与钩子窗口双形态。代价是每写一次要经一次受控边界（预算耗尽即跳过）；收益是
脚本坏/超时不会污染权威值或阻塞 tick。这是 Apollo 主动选的边界，不是缺陷。

## 4. 缺口与优先级（机制内、可独立推进）

1. **属性声明层**（最高）：把 `.def` 的等价物——属性表（域、可见级、持久性、min_interval）
   从契约 XML 生成，喂 `set_attr_domain`/`contract_write_whitelist`/持久化映射。当前全部
   机制在等这份声明，没有它，域投影默认 Public 过宽、白名单空、sink 不接。
2. **客户端线帧**：`FrameCodec` 的帧需要一个契约消息（attr_sync delta/snapshot/ack），
   目前 messages.xml 无此族——机制产出帧但无线上出口（design §0 明示"完整 World→Gateway→
    client 交付仍需验收"）。
3. **Scene 出帧宿主消费**：`drain_attrsync_frames` 已实现（[scene.cpp:281](https://github.com/cuihairu/apollo/blob/main/modules/game/world/src/scene.cpp#L281)），
   **apps/world 无任何消费者**——帧出 Scene 后在 cell 进程内落地即止。需要 cell→World
   中继或 939 出口接线。
4. **玩法属性真实写入**：哪怕一个战斗属性（如 ATK）从 Lua/模拟经 `set_attribute` 写入、
   经域投影到 owner、经 sink 到 Base 持久——把感知→同步→持久三环用真实数据钉一次。
   当前 D21 迁移验收里实体项已走 `execute_migration_transfer` 路径，但属性同步环未
   有真实玩法数据。

不在本分析范围：Ghost/Witness 跨 Cell（多 Cell 才需要）、无缝地图、客户端预测端侧。

## 5. 可证伪的验收口径（沿用仓库纪律）

- 感知：一个属性写一次，`ChangeHistory` 恰一条、seq+1；Lua 钩子白名单外不落账、预算
  耗尽剩余跳过、hook 报错保持原值。
- 同步：AOI 内两个 viewer 对同一实体收到同 seq 的 delta；域裁剪后 Team/Guild 非成员零帧；
  ACK 超时（3 拍）触发快照重建而非重放；历史覆盖（world_seq-acked_seq>1024）降级快照。
- 持久：ChangeSink 每写一次恰一次，journal→database 顺序与重放一致（design §8.2 已立的
  "不能只看 mark_dirty"）。
- 线帧：一条 delta 帧经真实编码→解码往返等价；契约消息加入后 golden 计数同步
  （[[contract-golden-count-sync]]）。

以上均不宣称"超过 BigWorld"：BigWorld 的生成声明、细节分层、生产级容错是有据的既有能力；
Apollo 的边界是机制齐、声明与投递待接。等声明层落地并用真实属性跑通三环，再谈维度对比。
