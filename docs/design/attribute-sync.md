# Apollo 属性同步系统设计（attribute-sync）

> 状态：设计稿（评审中）。目标：以 BigWorld 的完整模型为基准，为 apollo 设计 MMO 属性/状态的权威同步系统；对照对象包括 apollo 现有草稿（docs/03、`include/apollo/game/attributes`）、SSEngine（老引擎，无此层）、KBEngine（.def 契约 + 多端 SDK）、Aeron（仅传输层思想，见 net-abstraction.md）。
> 关联：`docs/design/sdk-contract.md`（契约与多端 SDK）、`docs/design/scripting-lua.md`（属性钩子与公式热更）、`docs/design/net-abstraction.md`（QoS 通道与背压）、`docs/analysis/ioc-review.md`。

---

## 执行摘要

1. **现有方案的定性：半成品**（与 BigWorld 对照）。docs/03 草稿与 `game/attributes` 雏形只做到了"标志位 + 50ms 批处理队列"：同步决策是每变更即时派发（`SyncTask` 逐条进线程队列，docs/03 §4.1），没有 per-viewer 状态、没有变更历史、没有 ACK/重传语义、没有带宽预算——丢包、乱序、慢客户端三个必现问题全部没有答案。BigWorld 用三个机制补齐：**属性所有权/可见域声明制、变更历史驱动的按客户端 delta、per-client 带宽优先级**。本设计把这三件事作为一等公民。
2. **核心数据结构只有两个**：每实体的 `ChangeHistory`（序号化变更环形日志，真相源）与每观察者的 `viewer progress`（对方已知到哪条）。delta = 两者差集；全量快照 = 历史重置。其余（AOI 进出、节流合并、优先级丢弃）都是在这两个结构上的策略。
3. **属性四分层 + 双轴标记**：静态配置/持久状态/易失状态/派生计算四层分治；每属性带**所有权轴**（base 权威 / cell 空间权威，当前单进程实现为同一权威，标记保留）× **可见域轴**（SELF/TEAM/GUILD/AOI/WORLD）。
4. **协议按"契约生成"而非手写**：属性 delta 包的字段语义写进 `docs/design/sdk-contract.md` 的单一契约（属性表 + 域标记），服务端 C++ 与 Unity/Cocos/Laya SDK 从同一契约生成——KBEngine 的精髓、修正其存储短板。
5. **持久化独立于同步管线**：KBEngine 存储弱点的分析见 §8（实体表 + blob 化复杂类型 + 仅 base 持久化 + 无写后日志）；对策是"快照 + write-behind 日志 + 可查询字段列式提升 + BI 分流"，复用 apollo `modules/data` 的异步 DB 模型（见 ssengine-reference.md §4.1）。
6. **分三阶段落地**：P1 单进程 + AOI + history/delta/ACK（本阶段即可上线）；P2 复合类型 slice + 带宽预算 + 压缩；P3 跨进程 cell 镜像（协议位已在 P1 预留）。

---

## 0. 现状定性：为什么说现有方案在 BigWorld 面前是半成品

### 0.1 三方对照

| 能力 | BigWorld（基准） | KBEngine | apollo 现状（docs/03 + game/attributes） |
|---|---|---|---|
| 属性声明 | .def 文件：类型 + 所有权标记 + 持久化 + 目录 | .def 同源简化 + 代码生成多端 SDK | `AttributeDef` 结构体/配置表（docs/03 §2）有雏形，**无所有权轴、无契约生成** |
| 同步决策 | 属性标记决定去向（OWN_CLIENT/OTHER_CLIENT/CELL_PUBLIC…），按客户端 维护 lastSeq，**变更历史驱动 delta** | 同思路简化版，属性 ID + flags，客户端按 entity 收 delta | 每次变更即时生成 SyncTask 派发到后台线程；**无 per-viewer 进度、无历史、无 ACK** |
| 可见性 | AOI/interest + ghost（跨 cell 镜像） | AOI 简化 + entity 创建/销毁消息 | `aoi.hpp` 有 AOIGrid/ENTER/LEAVE 事件，**但与属性系统未接线**（AOIEvent 无人消费成同步决策） |
| 带宽控制 | per-client 预算 + 实体优先级（距离/重要度），预算不够时低优先级实体降频 | 有 per-client 选项，粒度粗 | **无**。50ms 固定批处理，快慢客户端同待遇 |
| 复杂类型 | FIXED_DICT/ARRAY 按 slice 追踪脏 | 支持 nested，粒度到字段 | `BAG_DATA` 类 blob 属性只能整体重发 |
| 持久化 | base 权威 + backup + 快照迁移 | MySQL 实体表，复杂类型 blob（**弱点**，§8） | docs/07/08 有设计，实现为同步 mock |
| 客户端预测 | 移动预测 + 服务端 reconcile（cell 权威） | 同 | 无设计 |

### 0.2 现有雏形的具体缺陷（读码结论）

- `AttributeContainer`（`include/apollo/game/attributes/attribute.hpp:52-97`）：`std::mutex` 包住整个 map + **单一** `listener_`（一个实体只能挂一个回调，无法同时支撑"同步管线 + 脚本钩子 + BI"）；`AttributeValue = std::variant<...>`（六类型变体）在热路径上带来分支与体积成本，且与 `attribute_id.h` 的 `AttrValueType`（12 类型枚举）**重复定义了两套类型系统**。
- `AttributeManager`（`:100-137`）：全局单例 + 全局锁 + `shared_ptr<AttributeContainer>` 按 objectId 查找——正是 ioc-review.md P0-1 批判的"每 tick 全局锁查找"模式。
- `AttributeDef::syncToClient`（`:40`）是 bool——只回答"同步与否"，回答不了"同步给谁、以什么频率、允许谁写"。
- `attribute_id.h` 的 ~300 个 ID 是旧项目属性表平移（ID 分段 1-12/101-500/301-700/3001+…），**分段间有重叠冲突**（PLAYER_START=301 落在 CREATURE 段 101-500 内部，CURRENT_STAGE=301 与 POISON_PERCENT=155 等共存于 CREATURE 区间）——契约化的第一件事就是重建 ID 空间。
- docs/03 §4.1 的 `AttributeSyncManager`：独立同步线程 + `ThreadSafeQueue<SyncTask>`——把"广播"放到逻辑线程之外，意味着同一实体的属性顺序无法保证（写线程与同步线程无屏障），`excludeList` 挂在每个任务上（O(viewers) 拷贝）；`sequence` 是全局计数器而非 per-viewer，客户端无法据此判断缺了什么。

**结论**：不是在现有雏形上"补功能"，而是按本设计重建骨架，雏形中值得保留的只有属性 ID 表的语义资产（业务含义）与 AOI 网格实现。

## 1. 设计原则（从 BigWorld/KBEngine 提炼的精髓）

1. **声明即策略**：一个属性的同步行为（谁能写、发给谁、多久发一次、是否持久化）全部在属性定义（契约）中声明，运行时零 if 分支查表执行——不写"每个系统自己挑消息类型同步"的散装代码。
2. **真相源唯一且序号化**：服务端属性存储是唯一真相；每次变更追加进带序号的历史；一切下发（delta/快照/重传）都是历史的投影。客户端状态 = 某个序号处的历史切片，因此"补发/重连/慢客户端"都是同一机制的不同入口。
3. **按观察者思考，不按变更思考**：同步的基本单位是"某个客户端对这个实体的已知状态"，不是"一次属性变更"。带宽、优先级、快照切换全部挂在 (viewer, entity) 二元组上。
4. **带宽是一等约束**：预算制下发，宁可推迟低优先级实体，不制造突发拥塞；被推迟的数据不丢（真相在历史里），下一预算周期自然补上。
5. **权威分层，协议先行**：当前单进程，但 base/cell 所有权与"远程镜像只读"的协议位现在就定好，BigWorld 化时属性管线零改动、只换广播后端。
6. **上行只有意图，下行才有状态**：客户端永远不发"SetAttribute"，只发意图（Move/UseSkill/Buy）；属性单向权威同步。反作弊与一致性都从这一条推出。

## 2. 属性模型

### 2.1 四层分类（时间维度）

| 层 | 例 | 存储 | 同步 | 持久化 |
|---|---|---|---|---|
| L1 静态配置 | 怪物攻击力、升级曲线 | 配置表（启动加载，Lua 可热更版本） | 不增量同步；进视野时按模板版本下发引用 | 随版本 |
| L2 持久状态 | 金币、等级、任务进度 | 属性存储 + 持久层 | delta，SELF/GUILD 域为主 | 是（write-behind） |
| L3 易失状态 | HP、位置、buff 剩余 | 属性存储（内存权威） | 高频 delta / 专用移动通道 | 否（或秒级采样进 BI） |
| L4 派生计算 | FINAL_* 全部（docs/03 与 attribute_id.h 中的 TOTAL_/FINAL_ 段） | **不存储**，重算缓存 | 重算后打脏，随 L2/L3 下发 | 否 |

### 2.2 类型系统（收敛为一套）

- **数值属性**：统一 `int64` 承载（万分比定标，docs/03 已是此惯例）——缓存友好、delta/zigzag 编码高效、协议稳定。浮点只允许在服务端计算中间值，落属性一律定标整数。
- **字符串属性**：DISPLAY_NAME/APPEARANCE 等低频属性，独立桶存储（与数值分离，避免 variant）。
- **复合属性**（BAG_DATA/REFINE_*/RUNE_INFO 等）：不再用 blob 整体同步。定义为 **slice 容器**：契约里声明 `slice_count` 与 `slice_type`，每个 slot 是独立同步单元（脏标记、序号、下发都以 slot 为粒度）——这是 BigWorld slice 思想，解决"改一个背包格重发整个背包"。
- **事件性变更**（获得/失去物品的"动作"）：不属于属性流，走可靠事件通道；属性流只表达最终状态。两者在客户端汇合（属性是状态机，事件是刺激）。
- 删除 `AttributeContainer` 的 variant 与 `AttrValueType` 双轨，契约里每个属性一个确定类型（`sdk-contract.md` 生成各端强类型访问器：`attr.get(AttributeId::CUR_HP)`）。

### 2.3 双轴标记（契约核心）

所有权轴（谁有权写/权威在哪，BigWorld base/cell 语义）：

| 标记 | 语义 | 单进程阶段行为 |
|---|---|---|
| `BASE` | 持久权威（登录/存档侧） | 同进程写路径，持久化管线接入点 |
| `CELL` | 空间权威（战斗/移动侧） | 同进程写路径；P3 时此标记决定该属性随 cell 广播 |
| `RO_MIRROR` | 远程只读镜像 | P3 引入：非权威进程收到广播后写入本地镜像并打脏 |

可见域轴（发给谁）：

| 标记 | 域 | 典型 |
|---|---|---|
| `SELF` | 仅 owner 客户端 | CASH、任务进度 |
| `TEAM` | owner + 同队成员（AOI 交集） | TEAM_STATE、buff |
| `GUILD` | 公会成员（可跨场景，走全局路由） | GUILD_ID 变更 |
| `AOI` | 兴趣范围内所有观察者 | 外观、LEVEL、ACTION、可见战斗属性子集 |
| `WORLD` | 全服广播（限流白名单） | 世界 BOSS 阶段、公告级状态 |

- **域决定"字段可见性"**：同一实体对不同观察者投影不同（观察者看到的怪物没有 CASH 字段）——快照与 delta 都按域裁剪，这是 BigWorld CELL_PUBLIC/CELL_PRIVATE 的本质。
- docs/03 的 PROPERTY_APPR/PRO/DB/SELF/TEAM/GUILD/WORLD 位标记：**语义保留、位定义废弃**——APPR/PRO 的区别（立即/批量）不属于"域"，归入 §5 的频率分级；DB 归入持久化声明。三件事（发给谁/多快发/存不存）在旧位标记里混在一个 uint 里，拆开。

### 2.4 计算管线（L4 派生属性）

- 依赖图：契约声明派生属性的输入（基础属性/装备槽/buff 类别），构成 DAG；禁止环（构建期校验）。
- 触发：输入属性变更 → 所属派生属性进入 recalc set → tick 的 recalc 阶段统一重算 → 结果打脏（进入 §3 管线）。
- 公式载体：**Lua 函数**（热更友好，见 scripting-lua.md §公式），C++ 侧只保留依赖图调度与定标。docs/03 的 `Formula` 字符串编译方案废弃（自建表达式编译器成本高且不可热更）。

## 3. 脏标记与增量同步

### 3.1 核心结构

```
EntityAttrs (per entity, owning thread 独占)
├── values[]            // 数值桶 int64 / 字符串桶 / slice 桶
├── dirty   : bitset    // 本 tick 待重算/待收集的属性位
├── history : ring[(seq, attr_id, new_value|slice_ref)]  // 变更环形日志（容量 N，默认 1024）
└── world_seq : u64     // 该实体最新 seq

ViewerState (per (client, entity))
└── acked_seq : u64     // 客户端已确认收到的实体 seq
```

- 写路径：`set(attr, v)` → 校验写权限（所有权轴 + 调用域）→ 更新 values → 追加 history（携带变更者上下文：技能/脚本/GM）→ 置 dirty → 派生依赖进 recalc set。**全程无锁**：属性写只发生在 owning thread（§10）。
- `history` 是同步的唯一真相投影：容量按"最慢合法观察者 × 最高变更率"定（溢出策略见 §3.3）。

### 3.2 收集与下发（每广播 tick，如 10Hz）

1. 对每个 dirty 实体，找出其观察者集（§4 的 viewer set 缓存）；
2. 对每个 viewer：`delta = history[(acked_seq, world_seq]]` 按该 viewer 的可见域裁剪 → 生成 per-viewer delta 包；
3. delta 进入该客户端的**发送优先级队列**（§5），预算内尽量发；
4. 发送成功并不等于确认：客户端回 `AttrAck{entity_id, seq}`（可捎带在任意上行包里），推进 `acked_seq`。

### 3.3 全量快照 vs delta 的选择（切换时机）

| 时机 | 动作 |
|---|---|
| 进入视野 / login / 重连 | 全量快照（按域裁剪的当前值集）+ 记 `acked_seq = world_seq`，此后走 delta |
| ACK 超时（如 3 个广播 tick 无任何 ack） | 该 viewer 降级：重发最近 delta；再超时 → 快照重置 |
| `world_seq - acked_seq > history 容量` | 快照重置（历史已覆盖不了对方进度） |
| delta 属性数 > 阈值（如 >128 或编码后 > 快照估计） | 直接快照（更省） |
| 实体重生/透明度切换导致域变化 | 域投影变化的部分以快照子集补发 |

快照与 delta 共用序号空间：快照本质是"把对方进度直接推到当前 seq"，因此乱序/重复在两侧都可用 seq 判定丢弃。

### 3.4 合并语义

- 同一广播 tick 内同属性多次变更：**last-write-wins + 变更次数**（次数供 BI/调试，不参与客户端状态）。
- 跨 tick 积压（低优先级被推迟）：发终值即可——属性流是状态不是事件，中间值无意义。
- 不可合并的例外：slice 结构的"数组长度变化"必须保序到达（先扩容后写 slot），slice 操作以子序列保序下发。

## 4. 可见性与广播域

### 4.1 viewer set：与 AOI 的接线

- `aoi.hpp` 的 `AOIGrid` 已经产出 ENTER/LEAVE/SYNC/UPDATE 事件与 `GetAOIEntities`——**属性系统是它的第一个正式消费者**：
  - ENTER(entity A 进入 B 的 AOI)：为 (B, A) 建 `ViewerState`，A 向 B 发创建+快照；同时反向（AOI 是对称关系，按格子邻接自然对称，仍以事件为准）。
  - LEAVE：发销毁消息，回收 ViewerState。
  - UPDATE（跨格移动）：只更新 viewer set 的缓存归属，不触发协议。
- 观察者计算每 tick 增量维护（AOI 事件的增量性），属性收集阶段**不做任何空间查询**——空间计算与同步收集解耦，各自 O(变更量)。
- `SELF` 域不依赖 AOI：owner 对自己的 ViewerState 在登录即建立、离线才销毁。

### 4.2 域叠加与投影

- 对 viewer V 的可见属性集 = 属性契约的可见域标记 与 (V 与 owner 的关系集合：SELF/TEAM/GUILD/AOI 距离) 的交集。
- 队伍/公会成员关系由社交域提供关系位图（每个实体维护 `team_mask/guild_id`），收集阶段 O(1) 判定。
- 同一实体对不同 viewer 的 delta 不同（域裁剪在各 ViewerState 上独立进行）——这是 per-viewer 设计的代价（不能全局组播一个包），换来的正确性收益：快照/delta/ACK/带宽全部闭环。AOI 密集场景的包量优化靠 §5 的优先级与合并，不靠牺牲 per-viewer 正确性。

### 4.3 特殊可见性规则（契约可表达）

- 隐身/死亡：AOI flags（`aoi.hpp:39` 已有 flags 位）+ 属性域联动——隐身时对该实体的 viewer set 收缩，属性系统不特判（就是 viewer set 变了）。
- 战争迷雾/分段可见（副本）：scene 隔离由 AOI 的 sceneId 保证（`AOIEntity::sceneId`），无需属性系统参与。

### 4.4 跨进程预留（P3，BigWorld 化）

- `CELL` 标记属性在 cell 进程间由空间管理器转发（ghost：邻接 cell 的实体镜像）；镜像实体在本进程是 `RO_MIRROR`——属性管线无感（照常写镜像 + 打脏 + 服务本地观察者），只是写来源变成"远程广播"。
- 协议位预留：delta 包携带 `authority_epoch`（权威纪元），镜像侧拒绝回写；迁移（实体过界）= 新 cell 建快照 + 旧 cell 发销毁，复用 §6 的进出机制。

## 5. 优先级与节流

### 5.1 客户端带宽预算

- 每客户端 token bucket（默认如 32KB/s，登录配置 + 拥塞信号动态下调；上行不限）。
- 每广播 tick 从预算领取字节数，按优先级队列装包；预算耗尽 → 队列保留（不丢真相），下 tick 续传。
- 突发保护：单 tick 内单实体 delta 上限（防一次 recalc 风暴打爆预算）。

### 5.2 实体优先级（per viewer）

```
priority(entity, viewer) = w1·importance(class)      // BOSS > 精英 > 玩家 > 怪 > 掉落物
                         + w2·f(distance)            // 近者优先
                         + w3·recency(changes)       // 刚变过的优先
                         + w4·visible(遮挡/战斗参与)  // 可选
```

- docs/03 的"外观立即同步、其余批量"直觉是对的，但实现为 per-task 分支；本设计统一为优先级参数（importance 是契约字段），机制只有一条队列。
- 饥饿保护：每实体最大推迟 tick 数，超限强制发送（即使预算超支一点点）。

### 5.3 节流分级（契约字段 `min_interval`）

| 级别 | min_interval | 例 |
|---|---|---|
| INSTANT | 0（当 tick 发） | 死亡、隐身、名字 |
| HIGH | 1 广播 tick | HP 大变动、LEVEL |
| NORM | 4 tick | BUFF 类、非关键战斗数值 |
| LOW | 20 tick + 前端插值 | HP 自然回复跳动、坐标微调（位置本身走移动通道） |

高频小抖动在合并窗口内自然收敛（last-write-wins），无需独立"降采样器"。

### 5.4 QoS 通道划分（与 net-abstraction.md 对接）

| 通道 | QoS | 内容 |
|---|---|---|
| 移动通道 | 不可靠、最新值覆盖 | 位置/朝向/速度（20Hz，丢旧不丢新） |
| 属性通道 | 可靠有序 | 属性 delta/快照/ACK（10Hz 批） |
| 事件通道 | 可靠有序 | 伤害数字/获得物品/系统事件 |
| 控制通道 | 可靠有序 | 登录/重连/心跳/ACK 捎带 |

可靠通道的有序性由会话层保证（net-abstraction.md §会话层），属性 seq 用于 ACK 与快照切换判定，不承担传输层职责——两层序号职责分明。

## 6. 实体生命周期同步

| 阶段 | 服务端动作 | 客户端收到 |
|---|---|---|
| Spawn（进入视野） | 建 ViewerState → `CreateEntity{template_id, 位置, flags}` + 按域快照（属性通道，与 Create 同 tick 保证先建后改） | 创建实体 → 应用快照 |
| 驻留 | delta 批次（§3.2） | 应用 delta |
| Leave（出视野/销毁） | `DestroyEntity{reason}` + 回收 ViewerState（延迟一 tick 回收防乒乓：短暂出视野不立即清 acked_seq，重进视野若进度仍连续则只补 delta——AOI 边界抖动的常见优化） | 销毁实体 |
| 重连 | 会话 resume（net 层 token）→ 对 SELF 域实体全部快照重置；AOI 实体重新走 Spawn | 全场景重建 |
| 死亡 | STATUS 属性（INSTANT，AOI 域）+ flags 联动；尸体计时归 scene | 状态表现 |
| 迁移/换线（P3） | 旧处 Destroy + 新处 Create（对观察者而言）；owner 视角是场景重建快照 | 重建 |

## 7. 协议形态

### 7.1 消息定义（protobuf，字段级 delta）

```protobuf
message AttrDelta {
  uint32 attr_id   = 1;              // varint；slice 属性高 16 位为 slot
  sint64 value     = 2;              // zigzag（数值/字符串桶则走 bytes）
  uint32 change_n  = 3;              // 合并次数（诊断用）
}
message AttrBatch {
  uint64 entity_id = 1;
  uint64 base_seq  = 2;              // 本批起点（history seq）
  repeated AttrDelta deltas = 3;
  bytes  snapshot  = 4;              // 快照模式：域裁剪后的全量（packed attr k/v）
  uint32 epoch     = 5;              // 权威纪元（P3 镜像防回写）
}
message AttrSyncFrame {               // 每客户端每广播 tick 至多一帧
  repeated AttrBatch batches = 1;
  uint32 frame_ms   = 2;
  bool   zstd       = 3;             // payload 压缩标记
}
```

- 变长优先：attr_id varint、数值 zigzag；同实体批量打包后 **>256B 才 zstd**（小包压缩负收益）。
- **快照/重置也走同一消息**（snapshot 字段置位）——客户端只处理一种同步消息。

### 7.2 序号、乱序与丢包

- 可靠有序通道保证到达顺序 ⇒ base_seq 单调；客户端发现 seq 跳跃（理论上不该发生）→ 请求快照重置（自愈路径永远可用）。
- 移动通道允许乱序丢包：包内带时间戳，旧于本地最新则丢弃（标准 dead-reckoning 接收侧）。
- **属性表版本**：登录握手交换 `attr_schema_hash`（由契约生成，见 sdk-contract.md）；不一致 → SELF 域全量下发 + 告警（防止两端语义漂移静默腐蚀状态）。

### 7.3 与 SDK 契约的关系

属性 ID、域标记、min_interval、slice 定义、派生依赖——**全部只写在契约里一份**（`docs/design/sdk-contract.md`），服务端生成 C++ 访问器，Unity/Cocos/Laya SDK 生成强类型 AttrId 常量与容器类。服务端 `attribute_id.h` 现状（手写 enum + 分段冲突）迁移为"契约生成 + 语义审校"。

## 8. 持久化（含 KBEngine 存储弱点分析）

### 8.1 KBEngine 为什么"太弱"（逐条）

1. **实体表 + blob 化复杂类型**：FIXED_DICT/ARRAY 落库为字符串/blob——MySQL 能存不能查（无法 `WHERE bag->slot=...`），运维/运营查询全靠全量加载。
2. **仅 base 持久化 + 快照式写**：属性脏了不即时落，靠 entity 销毁/迁移/定期写快照；崩溃恢复粒度粗，回档窗口不可控。
3. **无写后日志（journal）**：没有"重放变更"能力，恢复=回到上次快照，中间的变更永久丢失。
4. **查询能力 = 按 entityID 取整实体**：没有二级索引/聚合，排行榜、GM 查询、BI 全部自谋出路。
5. **分库分表策略薄弱**：账号维度的哈希有了，实体冷热/归档/跨服迁移没有完整故事。

### 8.2 apollo 对策

- **写模型：快照 + write-behind 变更日志**。
  - L2 属性变更追加进 per-entity 的 `PersistJournal`（与 §3.1 的同步 history 分开：同步历史面向观察者可容量淘汰，持久日志面向 DB 不可丢）；
  - DB 工作线程按预算批量落库（复用 §异步 DB 模型，见 ssengine-reference.md §4.1）；落库成功推进 `persisted_seq`；崩溃恢复 = 最近快照 + 重放日志（重放幂等：属性 set 语义天然幂等）；
  - L3 不进日志（易失状态本来就丢弃），关键 L3（如战斗结算前 HP）由业务显式 snapshot。
- **落库形态：可查询字段列式提升**。契约给属性加 `column: true` 标记（金币/等级/职业等运营查询热点），生成独立列；其余进压缩快照列。兼顾"整实体读写快"与"SQL 可查"。
- **读路径**：登录 = 快照 + 未落库日志尾部重放；跨服/迁移 = 快照 + 日志跟随。
- **BI 分流**：docs/08 的 BI/TLog 走独立采集通道（属性变更事件在收集阶段旁路采样导出，§5 预算外），**不允许 BI 需求反过来污染属性表结构**（KBEngine blob 化的诱因正是"什么都想存下来查"）。
- **Redis 层**：只做跨进程共享热数据（公会/排行榜/全局状态），**不做实体属性缓存**（双写一致性成本 > 收益，属性权威在进程内存 + DB）。

## 9. 客户端预测与服务端校验

| 类别 | 属性/行为 | 规则 |
|---|---|---|
| 可预测 | 自身移动（位置/朝向） | 客户端先行 + 服务端权威 reconcile：上行输入（方向+时长），服务端按同规则模拟，偏差超阈值发权威位置纠正（移动通道） |
| 可预测（表现层） | 自身技能起手/前摇动作 | 客户端先播动画，服务端校验 CD/资源/距离，非法 → 权威状态覆盖（动作取消事件） |
| 仅表现预测 | 敌方 HP 掉落显示 | 客户端可按伤害事件做插值表现，**状态以属性流为准**；表现与状态短期不一致是允许的（这就是表现层预测的定义） |
| 不可预测 | 他人属性、金币、背包、任务 | 纯服务端权威，零预测 |

- **校验原则**（服务端）：一切上行是"意图"；速度/CD/资源/距离按契约常量校验；校验失败不 crash 不踢人，**权威纠正 + 计数**（超阈值进风控）。
- 预测不引入新同步机制：纠错就是"属性流/移动通道的权威值自然覆盖客户端本地值"，客户端 SDK 的职责是维护预测值与权威值的 blend（`sdk-contract.md` 的客户端容器接口为此设计）。

## 10. 线程模型与 tick 集成

```
主循环（每 tick，owning thread 串行）:
  1. simulate       — 业务/脚本写属性（只允许本阶段；set() 无锁，因为单写者）
  2. recalc         — 派生属性重算（依赖图拓扑序）
  3. aoidecay       — AOI 增量事件 → viewer set 增删（已有 aoi.hpp 事件流）
  4. collect        — 按 (viewer, entity) 组装 delta/快照（读 history，无锁）
  5. budget/flush   — 预算装包 → net 抽象层发送（net-abstraction.md）
  6. persist-batch  — 持久日志批量交给 DB 工作线程（异步，回调排回下一 tick 阶段 1 前执行）
```

- **单写者纪律**替代锁：实体归属其 scene/空间线程（将来多 cell = 实体静态归属线程，跨线程访问一律投递意图/消息）。`AttributeContainer` 现有的 mutex（`attribute.hpp:96`）与 `AttributeManager` 全局锁随重建废除。
- docs/03 的"独立同步线程"方案废弃：广播在逻辑线程内阶段化完成（collect/flush 是纯内存操作 + 非阻塞发送，耗时可控），跨线程只发生在 net 发送与 DB 落库两处、且都走无锁队列——顺序性由单写者天然保证。
- 超时兜底：阶段 4/5 预算超时（如 >1.5ms）立即截断，剩余下 tick——广播延迟上限可控。

## 11. 度量与验收

| 指标 | 目标 |
|---|---|
| 属性变更到客户端 p99 延迟 | ≤ 150ms（NORM 级），INSTANT ≤ 1 tick |
| 单客户端稳态带宽 | ≤ 预算 80%（100 人同屏战斗压测） |
| delta 压缩率 | 批次 >256B 时 zstd 后 ≤ 50% |
| 快照切换率 | 稳态 < 0.1%/viewer·min（只应由进出视野/重连产生） |
| 重连恢复 | ≤ 1 RTT + 1 快照包（≤ 50KB） |
| 崩溃回档窗口 | ≤ write-behind 刷库间隔（默认 2s）的日志重放，零丢失 |
| 契约一致性 | CI：attr_schema_hash 三端（server/unity/ts）比对通过 |

## 12. 实施切分

- **P1（单进程可用）**：属性存储重建（四层 + 双轴标记）→ ChangeHistory/ViewerState/delta/ACK → AOI 接线（spawn/leave）→ 协议三消息 + SDK 契约生成（unity/ts 最小端）→ 移动通道分离。验收：AOI 同屏 100 实体压测。
- **P2（体验与带宽）**：slice 复合类型 → 带宽预算 + 实体优先级 → 节流分级 + zstd → 持久化 write-behind + 列式提升 → 客户端预测（移动）。
- **P3（BigWorld 化）**：CELL/RO_MIRROR 镜像 + authority_epoch → 实体迁移 → ghost 广播后端（net-abstraction.md 的进程间通道落地时启用）。

## 附录：三方机制速查对照

| 机制 | BigWorld | KBEngine | 本设计 |
|---|---|---|---|
| 属性声明 | .def + flags | .def + 代码生成 | 契约（yaml/idl 形态见 sdk-contract.md）+ 三端生成 |
| 增量依据 | per-client lastSeq + 历史 | per-entity 版本 | per-viewer acked_seq + ChangeHistory |
| 可见性 | interest areas + ghost | AOI 简化 | AOI viewer set + 域投影 |
| 带宽 | per-client 优先级预算 | 粗粒度配置 | token bucket + 实体优先级 + min_interval |
| 持久化 | base + backup | MySQL 实体表（弱） | 快照 + write-behind 日志 + 列式提升 |
| 预测 | 移动预测 + reconcile | 客户端可选 | 移动预测 + 表现层预测 + 意图上行 |

---

*基线：apollo main @ 35a9c528（attribute.hpp/attribute_id.h/aoi.hpp/docs03 现状以其为准）。*
