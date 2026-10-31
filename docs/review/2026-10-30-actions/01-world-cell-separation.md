# D01 / D04 / S01 · 管理与执行分离、入口一致性、遗留测试目标

> 来源：docs/review/2026-10-30-decisions.md D01（todo.md:104）、D04（todo.md:142）、S01（todo.md:32）。
> 本文件为「立即执行动作」的设计稿产出；实现与验收按各切片另行立项。
> 切片1 部署缺省形态子片已落地：scripts/dev_fleet.sh 默认 `APOLLO_FLEET_CELLS=1`（常驻 cellServer）、`=0` 显式管理测试模式、World 分配池随 cell 数对齐（见 todo.md L104 批⑩）。
> 切片1 967 真链路对账证明面子片已落地：WorldApp 对账匹配补观测日志 + 独立 cellServer 进程 smoke 断言（见 todo.md L104 批⑩b）。

## 1. D01 · WorldApp 执行面调用与相关测试清点

### 1.1 当前事实（静态核对，2026-10-30）

- `apps/world/include/world_app/world_app.hpp:274` 持有 `std::unique_ptr<CellApp> cell_`；
  执行面方法（eval / rollout / drain / 模拟循环入口）经该指针**转发**到内嵌内核。
- 独立执行入口 `apps/world/src/main.cpp` 解析 `cellServer`（`apps/world/CMakeLists.txt:73` 目标），
  管理入口 `world`（同文件 :11）仅接受 `manager`（`role_options.hpp` 显式拒绝 `cell`）。
- 执行面调用方当前主要分布在**测试**（编译 `world_app.cpp` 的测试目标）与 `dev_fleet.sh` 的
  `APOLLO_FLEET_CELLS` 样例路径；生产主路径（`main.cpp`）以 manager 角色启动，不再走 `--role=cell`。

### 1.2 依赖 WorldApp 内嵌 CellApp 的测试目标

以下 12 个测试目标编译 `world_app.cpp`，其执行面断言依赖 `world_app.hpp` 的转发面；
`cell_app_tests` 仅编译 `cell_app.cpp`（不含 world_app 壳）。

| 测试目标 | 执行面依赖 | 切片归属 |
|---|---|---|
| `cell_register_tests` | 960/961 注册裁决、962 分配应用 | 切片2 |
| `fleet_rehome_chain_tests` | 重挂链（重注册/旧端隔离） | 切片2 |
| `gateway_m1_egress_tests` | 下行出口接线 | 切片1 |
| `gateway_push_downlink_tests` | 下行 push 接线 | 切片1 |
| `hosted_services_tests` | 内嵌宿主服务 | 切片3 |
| `migration_chain_tests` | 迁移链路 | 切片2 |
| `squad_wire_e2e_tests` | 小队真实链路 | 切片1 |
| `update_continuity_tests` | 更新连续性 | 切片3 |
| `world_app_acceptance_tests` | 验收面 | 切片1 |
| `world_app_m1_link_tests` | M1 链路 | 切片1 |
| `world_app_script_host_tests` | Lua 脚本宿主 | 切片3 |
| `world_drain_tests` | 排空 | 切片2 |

### 1.3 切片与验收表（先独立 cell 真链路通过 → 管理命令转发 → 删除内嵌内核）

| 切片 | 内容 | 验收条件 | 退出条件 |
|---|---|---|---|
| **切片1 · 独立 cell 真链路通过** | dev_fleet `APOLLO_FLEET_CELLS>=1` 起独立 cellServer；960 注册、961 裁决、962 分配应用、967 自报走真链路；Space 归属与 tick 在独立进程内推进 | 两 cell 注册（cell-21/cell-22）各自认领声明的 Space；网关/登录真实链路命中正确 cell；967 自报对账面一致 | 所有「切片1」测试目标在独立 cell 形态下绿 |
| **切片2 · 管理命令转发** | eval / rollout / drain / 迁移命令改为面向执行端的控制消息（963 retire 等），不再经内嵌内核直调 | 对运行中的独立 cell 发 drain/retire，活动归零后 cell 退出，World 回收分配；迁移决定经 World 提交后目标 cell 激活 | 全部「切片2」测试目标迁移到独立 cell 驱动 |
| **切片3 · 删除内嵌内核** | 从 `world_app.hpp` 移除 `cell_` 与执行面转发；对应测试改用 cellServer 驱动宿主 | 全树构建绿、测试绿；`world --help` 仅 manager；无 `cell` 角色残留引用 | 无（切片3 完成即 D01 收口） |

> 依据决策「明确不做/延后」：延后自动伸缩、跨机启动；**不重新加入**已移除的 `world --role=cell` 入口。

## 2. D04 · 二进制 / CMake 目标 / 参数解析器 / 编队命令 / 教程对应表

以当前真实入口为准（world / cellServer 独立）；`--role=cell` 不再作为 world 的取值。

| 二进制 | CMake 目标 | 参数解析器 | 编队命令（dev_fleet.sh） | 教程/文档入口 |
|---|---|---|---|---|
| `world` | `apps/world/CMakeLists.txt:11` | `apps/world/src/main.cpp` + `role_options.hpp`（仅 manager） | `world ... --assign-spaces 401,402`（9101 M1） | README 目标架构、world-cell-management |
| `cellServer` | `apps/world/CMakeLists.txt:73` | `cellServer` 入口 main | `APOLLO_FLEET_CELLS>=1` 起 cell-21/22（9301/9302） | Directory_Structure、world-cell-management |
| `gateway` | `apps/gateway` | `apps/gateway/src/main.cpp` | `gateway ... --port 9002` | 架构图、quick-start |
| `login` | `apps/login` | `apps/login/src/main.cpp` | `login ... --m1-port 9103` | 登录链路 |
| `base` | `apps/base` | `apps/base/src/main.cpp` | `base ... --port 9201` | base 资料面 |
| `database` | `apps/database` | `apps/database/src/main.cpp` | `database ... --port 9200` | 持久存储 |
| `interfaces` | `apps/interfaces` | `apps/interfaces/src/main.cpp` | `interfaces_entry()`（S08 可配置） | S08 决策 |
| `machined` | `apps/machined` | `apps/machined/src/main.cpp` | `machined --roster ...` | 编队启动 |
| `logger` / `admin` / `fleetctl` / `apolloc` | `apps/logger` / `apps/admin` / `apps/fleetctl` / `apps/apolloc` | 各自 main | logger/admin 行 | 运维工具 |

> 说明：内嵌宿主（WorldApp 内嵌 CellApp）的删除放在**切片3**（L104 真链路通过）之后，
> 与 D01 切片表一致；本表不预设 `world` 承担模拟角色。

## 3. S01 · 遗留 baseappmgr_tests 处置（行为保留、无变化目标改名）

### 3.1 两处 baseappmgr_tests 分支与调用者清点

- `tests/CMakeLists.txt:202` 与 `:541`：两处 `baseappmgr_tests` 目标定义（对应不同配置分支，
  如 game-module ON/OFF），CTest 套件名均为 **OnlineDirectoryTests**（测试行为 = 在线目录/准入验证）。
- `tests/CMakeLists.txt:2515`：`baseappmgr_m1_control_tests`，CTest 套件名 **OnlineDirectoryM1ControlTests**。
- 全仓引用：`tests/CMakeLists.txt` 内部（目标名、add_test 名）；`docs/zh/36-*.md`、
  `docs/zh/{en/,}architecture/kbengine-analysis.md`、`docs/zh/qa/q99-*.md`、`docs/zh/analysis/*.md`
  的 `baseappmgr` 均为**第三方引擎域术语**（BigWorld/KBE 对比与源码分析），保留原文，不改。
- `todo.md:29/32` 已注明「活跃测试目标 baseappmgr_tests 在册，与遗留测试目标处置待拍板」——
  本 S01 即该拍板的落地件。
- CI workflow 无二进制名引用（todo.md:32 已核实），改名不触碰 CI 过滤。

### 3.2 无行为变化的目标改名设计

| 现目标名 | 新目标名 | CTest 套件名（不变） | 行为 |
|---|---|---|---|
| `baseappmgr_tests` | `online_directory_tests` | `OnlineDirectoryTests` | 不变（仅目标名） |
| `baseappmgr_m1_control_tests` | `online_directory_m1_control_tests` | `OnlineDirectoryM1ControlTests` | 不变（仅目标名） |

- 改动面：`tests/CMakeLists.txt` 两处 `add_executable`/`add_test` 目标名（`:202`、`:541`、`:2515`）
  及其内部 `set_target_properties`/依赖引用；无源码行为变化。
- 验证：改名后 build-off 全量构建绿 + 该两套件 ctest 绿；`grep -rn baseappmgr tests/` 仅剩第三方
  域术语引用（36-MMO 等文档）。
- **设计已定、实现另立项**（决策 S01 为「设计一次无行为变化的目标改名」，本次只产出设计）。
