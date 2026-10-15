# apollo 巡检台账（仓库根 todo.md）

> 分工：本文件 = 巡检需求清单；`docs/todo.md` = 重构任务书批次台账（权威任务源）。
> 验收口径：三树门禁（GAME_MODULE ON/OFF）ctest 全绿 + sanitizer 零报告；文档/设计稿不计入已实现。

## 待办（需求）

- [x] trace 四件套余件：RemoteEntityRef / RemoteMethodSchema / RouteResolver 与 RPC 链 trace（49d3d3a8，14 用例）
- [x] Lua 脚本：`docs/design/scripting-lua.md`——运行时底座十三批已交付（沙盒宿主 + §3.2 热替换 + §5.1/§5.2 属性钩子窗口 + §6.1 模块熔断与三级事件（WARN 去抖/FAULT/TRIP-RECOVER + 结构化字段） + §6.1 WARN 阈值可配（warn_threshold_percent，0 = 关闭） + §3.4 契约表装载（路由完整性闸/白名单数据化/契约路由分发） + §7.1 attach eval（沙盒面/结果截断/限流/失败面错误原文） + §5.1 Scene×ScriptRuntime 适配桥（tick 六阶段接线/契约白名单/帧预算） + §7.1 attach 执行队列（跨线程投递/tick 首拍固定调试点/结果 id 回传） + §6.1 熔断状态持久化（隔离/去抖账跨重启） + cell 宿主接线（gameThread 随生灭 runtime/boot 源加载失败不停机/scene 幂等补挂与停机摘挂/eval 经宿主 tick 窗口） + debug eval 族契约（944 请求/945 回执，internal 域） + admin attach transport（cell 侧 944 受理/945 异步回执 + fleetctl eval 入口） + §3.5 字节码缓存（构建期 lua_precompile 产物 + manifest 三元组校验装载 + APOLLO_LUA_BYTECODE_CACHE 门控与回退源码） + §7.2 状态面只读内省（gc_state 内存与增量参数只读查询 + reload_history 热替换审计环 64 条 + env_snapshot 裸遍历快照） + 宿主侧 Lua 结构访问全 raw（global 读写与装载/调用路径探测不经元表——__index/__newindex 恶意表不再崩进程） + §7.3 性能归因分桶（六调用边界 per-chunk 指令与墙钟入账 + profile_snapshot/reset + 超标事件状态面环 64 条；C++ 侧外采与 MetricRegistry 聚合归 observability 批） + apollo_gen 吐 contract.lua 装载面形态收口（M.routes 平表 + 整数白名单 id 数组——load_contract 零歧义消费，生成器产物集成装载测试锚死形态），script_tests 58 用例 + scene_script_tests 16 用例 + cell_script_host_tests 7 用例 + cell_m1_link_tests eval 族 3 用例 + bytecode_cache_tests 10 用例 + lua_precompile 冒烟 1 例 + script_state_tests 21 用例 + §8 异步底座（L1 交接原语 register_async_service 扩展点 + apollo.async.call/complete/fail/cancel_pending/drain_async + set_async_validator re-validate + 注入错误不计熔断 + 指令预算跨 yield + Lua 长跳资源先释放纪律 + §7.2 协程清单 coroutine_snapshot 与 coroutine.create/wrap/resume 登记，async_tests 20 用例；L0 真实后端 Redis/DB/跨服随各自批次、async.all 随消费批） + §3.2 Phase A 两段式热替换（prepare_module 独立预备沙盒编译+求值+冒烟+预备镜像拓扑序 + commit_module 进程内字节码交接+hash 完整性校验+与 swap 共用换表尾+冒烟不重跑，phase_a_tests 13 用例） + §3.2 current 版本指针持久化（version_store 清单 swap/commit/rollback 三处回写 + 原子 rename + current_version_manifest 读面 + 防坏版本崩溃循环，version_store_tests 15 用例） + §3.2 Phase A 工程面 patch 目录读与 hash 校验控制面（load_patch_directory manifest 驱动逐条目 + 源 hash 不符拒载 + 字节码 hash/format_sig 双校验不符丢字节码留源码，patch_loader_tests 12 用例） + §3.2 Phase C 灰度观察期 canary 判定原语（ModuleCanary 观察窗 + per-module fault 流水 module_faults_since + 超阈早失败 + 回滚全回路，canary_tests 6 用例） + §3.2 Phase A 工程面灰度 per 场景线程投递编排（ModuleRollout 一次预备服务全部目标 + Kahn 拓扑序 + per-target 分步投递队列 + 每(灰度目标,模块)一 canary + 任一窗判坏早失败回滚 + 推广失败 per-target + 跨轮复用预备沙盒保留，rollout_tests 12 用例） + §3.4 宿主启动装载（cell --script-handlers/--script-contract 启动序 handler 面→契约→boot + 契约路由 handler 面占位模块随仓 45 路由 + golden 契约装载闸同步，contract_handler_tests 2 用例 + cell_script_host_tests 3 用例）
- [x] 属性增量同步：管线核心十三批已交付（c7b6c3dc/b4449d9d/0d0b6def/2fcb80c2 + SceneAoi 接线批 + §5 优先级与节流派 + §8.2 写后日志入口 + §3.3 快照负载组装 + §2.2 域投影机制层 + GameConnection Facade L3 适配层 + scripting-lua §5.1 钩子窗口注入点（attrsync 侧）+ Scene × ScriptRuntime 适配桥（Scene tick 六阶段接线 + 契约白名单 + 帧预算）+ Facade L2 会话增量（四通道队列/trim 分级/场景线程投递/Resuming），112 用例 + gameconnection_tests 20 用例 + §2.2 slice 复合类型机制层（组合 id = slot<<16|base、slot 0 = 长度格、per-slot 脏标记/序号/下发、收缩即终态擦除 + DeltaBuilder 死格过滤 + 域继承 base，attrsync 家族 107 用例；声明面 slice_count/slice_type 归契约批）），attrsync 管线核心全清（16ef9534，四树绿：build 116/116 + san 3/3 + off 2/2 + examples 1/1）
- [ ] battle-verification-service：ADR-013 明示暂缓（随 Lua/玩法批）
- [ ] 外部存储接线（MySQL/Redis/ClickHouse）：ADR-007 现行=文件+journal 零外部依赖（未排期）
- [ ] Kafka 观测管道 + Prometheus/Grafana：架构决策 #14 计划，仓内零实现（未排期）
- [ ] protobuf+zstd 消息路径：protobuf 现为依赖+测试不在消息路径，zstd 未入 vcpkg.json（未排期）
- [ ] G-2 热备仲裁面：ADR-010 已裁「案 A 先行、standby 不立项」（有裁决搁置；R1015F 已代决维持关闭，无反裁不重开）

## 代决记录

- **2026-10-15（R1015F 已代决）初始导入脚手架测试目标 8 个删除**：data_structures / log / network / protobuf / rest_template / utils / id_pool / timer 测的是已删 API（`apollo/net/protobuf_message.hpp`、`HttpHeader`、`utils::Random` 函数式调用形态等），无改写对象；CMake/CTest 接线同批清除（buffer_tests 绿，保留），四树构建 + ctest 全绿。遗留注记：`include/apollo/utils/lock.h` 与 `thread_pool.h` 唯一消费者即已删 test_utils.cpp，已成孤儿头——后续批已删（2026-10-15，全仓零引用核实：代码引用全指向 `apollo/base/thread_pool.hpp` 与 `apollo/utils/threading/thread_pool.hpp` 两处现役实现）。
- **2026-10-15（R1015F 已代决）覆盖率不硬造**：两份 todo 均无覆盖率条目，维持不动；如需补覆盖率由用户明示范围。
- **2026-10-15（R1015F 已代决）G-2 维持硬关闭**：ADR-010 案 A 先行、standby 不立项、批 1-4 取消维持；无 ADR 反裁或生产数据触发不重开（决策记录见 `docs/architecture/adr.md` ADR-010 代决注记）。

## 已完成（证据）

- [x] 核心框架（`apollo::core::di` + `ApplicationHost` 六态生命周期 + `ConfigRegistry`）：P0-1 起，fd61ff7f
- [x] 网络自持四层（net M1）：nng 迁移五批 2026-10-12 全清，9121ebc2
- [x] linkrpc 关联原语（契约 940/941 + corr_id + call_async/poll/wait 桥）：5ae86969
- [x] Scene/Instance 模型 + PlayerAnchor 权威 + 换幕 scene_transfer + PlayerDirectory + AOI 3×3 + 持久化栈 + Recovery/Reconnect + 确定性战斗 BattleRuntime + 契约系统 + Social/Guild/Party：P0-P2 各批（docs/todo.md 批记链）
- [x] cell 受理面 + 延时应答（队列化异步受理 + 水位/过载 + pending-reply 升级）：2ffc4711 / 6b48984a / 0b8273fc
- [x] 可观测性最小面（P3-3 批 A-L7）：日志四套收敛 + LoggerApp + L4 push 通道 + MetricRegistry + metrics admin 通道 + trace 子集 + 告警 notifier
- [x] 崩溃采集（crashpad + machined 对照链 + 符号化）：批①②③④
- [x] 进程编队（G-1）：machined 监督面 + discovery 库 + 目录镜像 + FleetRecoveryCoordinator + 死亡行窗口处置
- [x] 玩家迁移（backup-revive §9.2）：契约族 933-938（62d53d35）+ 服务器双面（22b0431e）+ zone 迁移 pull on handoff（fb0cba36）+ directory moved epoch（2372c3fc）+ manager 编排器（af884542）+ migrate 命令接线（e4404801）+ 编排演练（897cfabf）
- [x] 更新排空与演练工具：drain 闸 SIGUSR1/2（fa201213）+ fleetctl probe（8b155a04）+ route1 更新演练（33e577be）
- [x] 性能基准：三模型 benchmark 桩 + 受理面实测 ~0.1µs/意图 + flush 段实测（934f5df2）
- [x] DevEx：dev_fleet.sh 一键起停 + gateway ingress 接线 + 编队成员注册批 0de62ea9（21/21 绿）
- [x] 契约 slice 声明面（attribute-sync 批 10 余项）：attrs.xml/XSD 新增 slice_count/slice_type 成对声明 + 解析校验（成对性、≥1、元素类型可解析）+ apollo_contract.h AttrMeta/apollo_contract.json/contract.lua 产物携带 + 编译期 static_assert；随仓契约零 slice 内容（玩法 authoring 归玩法批），golden 重吐一致性绿
- [x] 测试面：tests/ 86 个 test_*.cpp（gated by ctest，ctest -N 53 项）
- [x] P3-2 / P3-3 / P3-4 全部收口：2026-10-13（批次台账 28/28 全勾）
- [x] P4-4 术语承接：697c49d5（`APOLLO_USE_NNG|nng_` 代码 0 残余）
- [x] 移交件② bare 924-928 双受理退役：9eac60d8
- [x] 移交件③ collector 归档滚动：67fa7fd8
