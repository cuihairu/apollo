# D12 / D13 / D16 / D17 / D25 / S05 · 幂等结算、恢复记录、迁移状态、跨 World 协调、故障表、耐久边界

> 来源：docs/review/2026-10-30-decisions.md D12（todo.md:161）、D13（todo.md:162）、
> D16（todo.md:165）、D17（todo.md:166）、D25（todo.md:182）、S05（todo.md:126/132/134）。
> 本文件为设计稿；结算/迁移真实接线随 P1/L161 等批次推进。

## 1. D12 · 逐 Base 幂等结算

### 1.1 原则

稳定玩法 ID + 结算 ID 记录结果；各原 Base 独立持久确认；可重试的最终完成流程。

### 1.2 验证（先接 cell→Base→database 真实链路）

| # | 场景 | 断言 |
|---|---|---|
| 1 | database 已写、回执丢失 | 同结算 ID 重试**不重复**发放 |
| 2 | 两 Base 部分成功 | 已成功 Base 不重发；失败 Base 补齐（不默认扣回） |

> 第 1 行 2026-11-23 已落地（commit c5966bbe，见 todo.md L132）：稳定结算 ID（cell `instance->id()` 单调）经 `ledger.begin(members, settlement_id)` → BusinessSink 三参 → 958/959（+settlement_id/+duplicate，承载域 internal-only、client_hash 不动）→ Base `Avatar` 去重环（宽 128），环与 exp 记账**同一次 SaveTask 落盘**（`PlayerData.settledIds` 随档案 JSON 原子写）；回执丢失重放受理为 accepted+duplicate 不双记（测试：`squad_instance_tests` 41/41、`base_business_wire` 持久闭环含迁移接权播种、主树 ctest 128/128、san 三套件绿、off-tree2/examples 全量重建 exit 0）。第 2 行选路前置 2026-11-26 已落地（commit 392cdf71，见 todo.md L132）：915/918 +base_host/base_port 目录承载登录实拨端点（承载域，不裁决），cell 侧 `SettlementBaseRouter` 按玩家 917/918 解析归属 Base 逐玩家拨 958（端点缓存 + 传输失败/业务失配清缓存重解析自愈 + 每端点懒拨 + 单 Base 兜底零回归）——已成功 Base 不重发（Base 去重环 accepted+duplicate）、失败 Base 补齐（挂账重驱动重解析新端点）经真链路样本验证（`settlement_base_router_multi_base`：A/B 异端点分账不串、停面挂账→目录改指重拨补齐、同 ID 重放不双记、误投异端锚点拒）；**仍缺**：多 Base 真机编队演练（dev_fleet 现仍单 Base）、Base↔Base 锚点迁移改指的编排接线与补偿编排——不重试。
>
> 模拟代次校验（L132 主文点名缺口「稳定玩法/结算 ID 校验模拟代次」）2026-11-26 已落地（commit dd3fb4c5）：稳定结算 ID 原 = cell 实例号——实例号是 cell **进程内裸计数**（`World::create_instance` 的 `next_instance_id_` 从 1 起步，进程重启归 1、多 cell 并存各自从 1 铸号），直用作结算 ID 在跨进程代次/跨 cell 两个维度撞号，Base 去重环按 ID 单键判重把新代次的合法结算误判 duplicate = **静默漏奖**。新件 `settlement_id.hpp`：结算 ID = `(盐 << 32) | 实例号`，盐 = FNV-1a32(world_id, cell_id, start_epoch) 折叠 31 位（0 保留=未装配盐旧口径 instance_id 直用、折叠 0→1 装配面永不退化）；`start_epoch` 装载时单铸（`load_cell_config`，`steady_now_ms` 单调重启必增，与 960 注册代次同源——注册面与结算 ID 域共用一个值，避免两处铸分叉）；service `set_settlement_id_salt` 装配，开账合成经 ledger/记录库/意图/确认/回执全链（`record_store_->open` 分离原始实例引用）；超宽实例号（≥2^32，单进程 >40 亿实例不可达域）确定性折叠。同代次内盐恒定——重试/回执丢失重放同 ID，D12 #1 去重契约不变；跨代次/跨 cell 必异。测试：`settlement_id_salt_and_compose_faces`（盐逐域区分/0 保留/折叠确定）、`squad_settlement_id_generation_distinct`（同实例号跨重启代次/跨并行 cell 结算 ID 必异、旧口径零回归直用）、`squad_settlement_id_salted_lost_reply_retry_same_id`（盐装配后回执丢失重放同 ID 去重不双记）；主树 ctest **128/128**（首轮 BaseDatabaseTests 负载时窗假红，隔离绿+全量复跑绿）；off-tree2/examples 全量重建 exit 0 + ctest **128/128**；san 树 SquadInstance/CellApp 2/2 + BaseBusinessWire/MigrationChain/SquadWire 3/3（ASan+UBSan）。**仍缺**：多 Base 真机编队演练、Base↔Base 锚点迁移改指编排（同上，不重试）。

> 第 2 行服务级编排切片 2026-11-27 已落地（commit 1b83d0ca，见 todo.md L133）：
> `SquadInstanceService` 装配真 `SettlementBaseRouter` 作业务缝（生产 CellApp 同款
> ——958 带稳定结算 ID 经目录 917/918 逐玩家拨归属 Base）× 两真 StoreHost ×
> 真 OnlineDirectory × D13 恢复记录面，服务级验收 `squad_settlement_multi_base_
> partial_success`（活区 BaseBusinessWireTests）四段断言：① 名册跨两 Base
> 分别确认（B 归属成员落 B、A 面零锚点零分值，恢复记录逐玩家 intent+confirm，
> 账本 applied 独立推进）；② 部分成功（B 停面 → A 归属成员照常落 A 独立确认
> 不被拖带，B 归属成员挂账 AvatarUnavailable，意图先于业务面落盘不丢应发分值）；
> ③ 账本 retry 只重提挂账成员（已确认成员缝调用计数不前进）；④ 端点迁移
> （目录改指 + 重锚）后周期重驱动补齐挂账成员 → 全部确认记录 done 删除，
> 账本路径随后重提同结算 ID 命中 Base 去重环不双记，头部 exp 分账不变。
> 主树全量重建 exit 0 + ctest **129/129**；off-tree2/examples 全量重建 exit 0
> + ctest 各 **129/129**；san 树 GatewayM1EgressTests/BaseBusinessWireTests/
> SquadInstanceTests 3/3（ASan+UBSan；GatewayM1Egress 在扫及本批 D08 #4 限次账
> 时暴露同拍 `now` 与 `trace::now_ms()` 无符号直减回绕误删活账、重试预算静默
> 清零——已饱和下探修复，隔离 25/25 稳绿）。**仍缺**：多 Base 真机编队演练
> （dev_fleet 单 Base）、Base↔Base 锚点迁移改指的编排接线——不重试。
>
> 多 Base 真机演练 2026-11-27 已落地（commit 56284629，见 todo.md L132）：
> 真实进程全链无一桩——新 `BaseMultiBaseProcessSmoke` 起 2×database + 2×base
> 独立进程独立工作目录，login 宿主目录（915/918 承载面）+ M1 准入面
> （`--enable-test-users` 显式装载内建测试用户 player1/player2，生产默认关
> 不进生产路径）；LOGIN_REQUEST 全链认证绑定（激活落 base A）→ `fleetctl
> assign` 盖章归属 Base 端点（915 新参 `--base-host/--base-port`）→
> `fleetctl resolve` 逐玩家回带断言（918 `base=<host>:<port>`；无绑定玩家
> RESOLVE_FAILED）→ 958 逐 Base 送分：各归属 Base 受理（exp 1000+120 /
> 2000+90）、同结算 ID 重放 accepted+duplicate 不双记（Base 去重环真盘）、
> 错 Base（player1 在 B 无锚点）与无册玩家拒；优雅停机 flush 后探针经真
> database 链回读 + 档案 JSON 断言逐 Base exp 与 settledIds、零串扰
> （player1 不落 db_b、player2 在 db_a 无分值无结算环）。四树门禁：主树
> ctest **130/130**（终态 login 重链后新烟复跑绿）、off-tree2 **130/130**、
> examples **130/130**、san 树全量重建 exit 0 + ctest **129/130**
> （CellAppTests 全量扫负载时窗假红、隔离复跑绿）+ 受影响三套件 ASan+UBSan
> 绿。**仍缺**：Base↔Base 锚点迁移改指编排、dev_fleet 两 Base 编队编排
> （编排批）——不重试。
>
> 明确不做：延后跨区运行和全局强原子事务；不因跨服未启用而延后单区多 Base 的结算正确性。

## 2. D13 · 结算恢复记录与异常结束

### 2.1 记录与生命周期

- 持久结算意图：`settlement_id`（稳定玩法 ID + 结算 ID）→ 唯一记录。
- 生命周期：`pending → per-base confirmed（逐 Base 独立持久确认）→ done / cancelled`。
- **只有全部所需持久确认完成才删除唯一记录**；失败取消不发成功奖励，已发部分进后续处理状态。

### 2.2 故障注入清单

| # | 注入 | 断言 |
|---|---|---|
| 1 | Base 暂不可达 | 恢复记录保留，重试补齐 |
| 2 | 回执丢失 | 同 ID 重试不重复 |
| 3 | 进程重启 | 恢复扫描重建意图 |
| 4 | 解散/超时 | 明确取消，不发成功奖励 |
| 5 | 奖励数 | 已发奖励数核对，无漏发/重发 |

> 明确不做：延后高级自动补偿策略；不把网络超时当成支付或奖励必然失败。

> 单 Base 路径首切片 2026-11-23 已落地（commit cd8849d4，见 todo.md L133）：唯一结算意图记录以稳定结算 ID（= Instance ID）为键持久化（`SettlementRecordStore`，`settlement_records/settlement_<id>.rec`，生命周期 pending/done/cancelled），逐玩家落账确认经账本 `ConfirmationSink` 写入、全部名册成员 applied 即 done 并删记录（只有全部所需确认才删）；失败取消（全员退出/策略收尾/超时）置 cancelled 不发成功奖励、已发部分随记录保留；重启恢复扫描计数 pending 不自动重发。故障注入覆盖（单 Base 路径）：#1 Base 暂不可达（挂账 pending→重试补齐→done）、#2 回执丢失（同 ID 重试不重复，随 L132 c5966bbe）、#3 进程重启（恢复扫描见 pending）、#4 解散/超时（cancelled 不发奖励）；#5 奖励数核对（确认位与账本 applied 对齐，无漏发/重发）部分覆盖。

> pending 自动重驱动批 2026-11-23 已落地（commit 0496d63c，见 todo.md L133）：记录补记 `intent`（已定分值 = 应发什么，intent ⊇ confirmed——未确认成员的分值重启不丢）；账本 `IntentSink`（apply 起始即记持久意图，与业务面是否受理无关，同路径覆盖 credit_settlement/on_reward）；恢复扫描 `ingest_restored_settlement_records` 改为自动重驱动——pending 记录中 intent 已知而未确认的成员重发 958（同结算 ID，Base 去重环兜底不双记：已发不重发、未发补齐），全部所需确认完成才删唯一记录（§2.1）；无业务面（未装配）或分值未裁决（无 intent = 人工修复面）成员不自动驱动，记录保留。故障注入补充：#3 进程重启自动补齐、#1 Base 仍暂不可达（挂账保留，恢复后再次扫描补齐）。

> D13 重试节奏批 2026-11-23 已落地（commit 55539edb，见 todo.md L133）：挂账 pending 记录按节奏补齐不依赖重启——服务 `maybe_redrive_pending`（节奏门：interval 内不重扫；Base 链路未起跳过且不推进节奏——恢复后首拍即补扫；interval 0 = 关闭仅启动扫描一次；now 由调用面注入）、`WorldAppConfig.settlementRedriveMs`（缺省 30s）、`CellApp::driveSettlementRedrive`（game 拍触发，冻结拍不触发，拨号 peerUp 门，`settlementRedriveCalls` 接线观测面）。故障注入补充：#1 Base 延后恢复（启动扫描时不可达 → 链路恢复后按节奏补扫补齐，不再依赖重启）。测试：`squad_settlement_redrive_cadence`（链路未起跳过 / 恢复首拍即补扫 / interval 内不重扫 / 关闭）与 `cell_settlement_redrive_hook`（每拍触发 / 冻结停 / 关闭停）。
> D13 人工修复面批 2026-11-23 已落地（commit 7bd185bc，见 todo.md L133）：pending 记录中分值未裁决（无 intent）的受理成员此前不可见、不可修——观察面 `SettlementRepairEntry`（settlement_id + awaiting + confirmed_count）与 `pending_repair_entries`（pending 且有未裁决成员的记录点名；cancelled/done 不入列），`adjudicate` 修复写入口仅受理 pending 记录：终局记录拒（done 已回收、cancelled 已发部分走后续处理状态，不做裁决防复活失败取消的奖励发放）、名册外玩家拒、已确认玩家拒覆写（确认位是权威事实）。裁决成功交回既有周期重驱动接管重发（同结算 ID，Base 去重环兜底不双记——修复不旁路自动面）。测试：`squad_settlement_manual_repair_surface`（点名待裁决 + 进度、四拒（终局/名册外/已确认/无记录）、裁决后队清空、自动重驱动补齐删除记录不双记、cancelled 记录 store 面裁决拒）squad_instance_tests 49/49；主树 ctest 128/128；off-tree2/examples ctest 128/128；san 树 ctest 128/128（ASan+UBSan）。
> D13 核对面批 2026-11-23 已落地（commit bb9e3742，见 todo.md L133）：失败取消不发成功奖励但取消前已发部分是已发生事实——记录留盘却从未可观测。`SettlementCancelledPartial`（settlement_id/party/world/instance + granted 已发部分 = 取消前确认位）+ `cancelled_partials`（每份 cancelled 记录点名取消前已发成员与分值；pending/done 不入列）+ 服务级 `cancelled_settlement_partials` 转发；终局态不回收（complete 对 cancelled 不覆盖）；观测按结算 ID 查不按目录迭代序。**只核对不改账**——补偿策略（回收/补发口径）另拍板，本面不动账本。测试：`squad_settlement_cancelled_partial_surface`（单份点名已发 40、complete 不回退、无已发部分列空 granted、pending+done 不入列、服务级转发、未装配空列）squad_instance_tests 50/50；主树 ctest 128/128；off-tree2/examples 全量重建 exit 0 + ctest 128/128；san 树 ctest 128/128（ASan+UBSan）。（多 Base 分别确认与部分成功编排 2026-11-27 已落地，commit 1b83d0ca，见 §1 D12 #2 服务级验收。）**仍缺**（另起批）：cancelled 已发部分的补偿策略（待拍板）。

## 3. D16 · 迁移状态机与冻结提交顺序

### 3.1 小型副本迁移样本状态清单

| 状态 | 说明 | 迁移处理 |
|---|---|---|
| 实体 ID | 稳定 ID | 跨迁移不变 |
| 血量 | 当前 HP | 复制 |
| 冷却 | 技能/冷却计时 | 复制（含剩余时间） |
| 时间基准 | 玩法时钟/模拟代次 | 复制基准，不复制对象指针 |
| 脚本状态 | Lua 模块私有状态 | 按模块边界表示（D26） |

### 3.2 阶段与唯一允许动作（失败后）

| 阶段 | 确认 | 失败后唯一允许动作 |
|---|---|---|
| 准备 | 源冻结、目标未激活准备 | 取消（源解冻，目标丢弃） |
| 持久提交 | 稳定迁移 ID 持久化（源 World） | 查同 ID 结果，保持隔离 |
| 激活 | 目标激活、路由投影更新 | 按已提交决定完成（不自动回滚） |
| 清理 | 源清理 | 重试清理（幂等） |

> 迁移 ID 稳定；单一提交点让超时后可查询；只搬玩家/重新开副本不算完整迁移。

> D16 首切片 2026-11-26 已落地（commit 974ac98c，见 todo.md L136）：同进程显式换场路径从 D15「离场重建、易失态清零」升级为**完整运行态随迁 + 五步逐步确认**——①源冻结（准入校验 + `AvatarRuntimeState` 捕获 + `AvatarState::Frozen` 不收输入/伤害，源无 Avatar 显式驳回不降级重建）；②目标未激活准备（实体 ID 不变重建 + `GameplaySnapshot` 长期属性投影 + 运行态覆盖播种，血量/序号水位随迁保旧包重放门）；③条件提交（scene+instance 成员资格成立——无实例空间 = 无需 enter 即成立；提交前失败 = 取消，目标丢弃 + 源解冻，源对象全程未离场、原态无损，区别于 D15 快照重建回滚）；④路由投影更新（会话 TransferringOut + 字段切换，提交后失败不自动回滚，以 `CommitInconsistent` 上报对账）；⑤目标激活与源清理（complete resume 后源清理，冻结 Avatar 出列）。`confirmed_steps` 位掩码逐步确认可观察（失败定位到步）。`handleCellCrossBorder` 生产接线已切换迁移路径，契约零改动。字段边界：`AvatarRuntimeState` 只收 Avatar 上真实存在的状态（实体 ID/血量/输入序号水位）；§3.1 冷却/玩法时钟/脚本状态尚无字段与消费点——不空画，随对应玩法批接入再扩。测试：`avatar_migration_freeze_unfreeze`/`avatar_runtime_capture_restore`（AvatarTests 10/10）、`test_scene_migration_runtime_preserved`/`_commit_cancel_unfreezes_source`/`_rejections`（SessionWorldTests 15/15）、CellAppTests 19/19（首轮此套件红=无实例空间条件提交三元链误判，已修）；四树门禁：主树 ctest 128/128、off-tree2 全量重建+ctest 128/128、examples 全量重建+ctest 128/128（两轮 WorldAppM1LinkTests 负载时窗红，隔离+复跑绿）、san 树 7 套件绿（ASan+UBSan）。**仍缺**：冷却/时钟/脚本状态字段（需求触发）、迁移 ID 持久提交与对账（D17 跨 World 批）、跨 Cell 连续交接（D15/D17/D30 延后）——不重试。

## 4. D17 · 跨 World 唯一迁移决定设计约束

| 约束 | 设计 |
|---|---|
| 协调责任 | 源 World 持有唯一持久提交记录；目标只按该决定激活 |
| 查询接口 | 同一迁移 ID 可查询（结果不明先对账） |
| 不可达动作 | 隔离旧源，保持隔离；不因超时自行回滚已提交迁移 |
| 双端解冻 | 禁止（结果不明保持隔离，防止双端同时激活） |

> 跨 World 启用时执行；同 World L165 验证后再排跨 World 故障链路。当前跨服暂不启用。

## 5. D25 · 阶段 × 故障 × 允许动作表（L165 迁移机制模板）

| 阶段 | 故障 | 允许动作 |
|---|---|---|
| 准备 | 目标不可达 | 取消（可重试） |
| 准备 | 源状态不完整 | 取消 + 记录缺口 |
| 持久提交 | 提交不明 | 查同迁移 ID（幂等） |
| 持久提交 | 写入失败 | 冻结（不激活） |
| 激活 | 路由更新失败 | 按已提交决定重放 |
| 清理 | 源清理失败 | 幂等重试 |

> 原则：提交前可取消；提交后只能按已提交决定完成或补偿；重复请求带稳定操作 ID 并核对代次。
> 其余更新（cell/Base/World）复用本模板，在各自实现时补真实重启与超时用例。

## 6. S05 · flush/fsync 决策边界（已有实现决定，需补强边界）

### 6.1 现状（批A/批C 已代决并实现）

| 记录族 | 行 | 写序 | 失败处理 |
|---|---|---|---|
| A（分配决定） | `A space_id\|cell_id\|version` | **先 fsync 再推 962**（write-ahead） | 回滚内存认领，跳过 962，Space 留池重试 |
| R/B | `R space_id`、`B management_epoch` | flush | 告警降级（批㉚ 可用性优先） |
| C（对账记录） | 现有对账记录 | flush | 告警降级 |
| T（登记行） | cell 注册行 | flush | 告警降级 |

### 6.2 进程崩溃 vs 主机断电恢复（区分）

- **进程崩溃**：A 已 fsync → 重启后 journal 重放仍见该分配；R/B flush → 已见。
- **主机断电**：A 的 fsync 保证对外推 962 前决定已落介质；R/B 的 flush 不保证断电存活，
  故**B 管理代次与隔离水位等关键记录**在正式承诺断电恢复前须完成耐久写入或失败关闭。

### 6.3 依赖图

```
分配决定(A) --fsync--> 发布(962) --> 目录投影(D05) --> 网关路由
管理代次(B) --flush--> 启动水位 --> 拦旧端(code 3)
对账(C) --flush--> 冲突集 --> 人工解冻(S04)
登记(T) --flush--> 重放终态
```

> 边界（设计验收）：为 B/关键水位写失败**拒启或冻结**；不重新问用户「所有日志一律 flush 还是 fsync」；
> 延后以性能为由放宽耐久性；本次不声称已执行断电测试。
