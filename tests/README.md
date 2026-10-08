# Apollo 测试套件

## 概述

统一测试树：单 cpp 直编惯例（`TEST_ASSERT` 宏 + `bool test_xxx` 用例 + `main` runner），`tests/CMakeLists.txt` 挂 CTest；契约测试三件套注册在 `modules/contract/` 与 `sdks/gen/`，与 tests/ 树共同构成 ctest 全集（当前 36 项）。

tests/ 下 48 个 `test_*.cpp` 中 33 个注册进门禁；其余 15 个属 legacy GTest 批（`APOLLO_BUILD_GTESTS=OFF` 默认关：protobuf / buffer / network / log / timer / id_pool / utils / rest_template / data_structures / channel / crypto / utils 系及配套 main），不编不跑、不计入门禁。

## 门禁口径

三树构建 + CTest 全绿（ON 树 36/36、OFF 树 36/36、Examples 树 34/34，数字随批增长，各批交付注记为凭）：

```bash
# 树 1/2：默认树 ON / OFF（GAME_MODULE 开关各验一遍，OFF 验完还原 ON）
cmake -S . -B build && cmake --build build -j && ctest --test-dir build
cmake -S . -B build -DAPOLLO_BUILD_GAME_MODULE=OFF && cmake --build build -j \
  && ctest --test-dir build
cmake -S . -B build -DAPOLLO_BUILD_GAME_MODULE=ON && cmake --build build -j

# 树 3：Examples 树
cmake -S . -B build-examples -DAPOLLO_BUILD_EXAMPLES=ON \
  && cmake --build build-examples -j && ctest --test-dir build-examples
```

## 套件分组（按域，实际注册面）

| 域 | 套件（CTest 名） |
|---|---|
| contract 契约生成（注册于 modules/contract、sdks/gen） | apollo_contract_tests / apollo_contract_gen_compile_test / apollo_gen_golden_check |
| base 基础件 | MemoryTests / StringTests / TerminalTests / ThreadPoolTests / TimeTests |
| core 框架核 | CoreLifecycleTests / CoreLogTests / CoreConfigTests / LogStructuredTests / MetricsTests |
| runtime 进程面 | RuntimeTests / BaseAnchorTests / BaseAppMgrTests |
| net 网络件 | NetTests / ReconnectTests / GatewayRouteTests / ProtocolBootstrapTests |
| game 会话/世界 | SessionWorldTests / WorldHostTests / AvatarTests / PlayerDirectoryTests / DirectoryMirrorTests / FleetRecoveryTests / SupervisorTests / SceneTests / InstanceTests / GameTests |
| battle 玩法 | BattleRuntimeTests / RngSubstreamTests |
| social 社交 | SocialTests |
| discovery 编队发现 | DiscoveryTests |
| data 持久化 | DataTests / PersistJournalTests / PersistenceChainTests / RecoveryTests |
| bigworld 兼容 | BigWorldApiTests |

## 新增测试惯例

1. 测试文件 `tests/test_<域>.cpp`：`bool test_xxx()` 用例 + `TEST_ASSERT(cond, msg)` 失败即返回 false + `main` 注册表（先例：`test_social.cpp` / `test_metrics.cpp`）。
2. `tests/CMakeLists.txt` 挂 `add_executable` + `add_test(NAME ... COMMAND ...)`；模块源直编（不依赖 GAME_MODULE target 的，照 scene_tests / game_tests 先例列相对路径源）；Windows 分支带 `ws2_32`，MSVC 带 `/utf-8`。
3. 依赖条件 target 的注册用 `if(TARGET …)` 守卫（先例：SessionWorldTests / BaseAnchorTests / GatewayRouteTests——缺依赖时静默不注册，门禁计数相应变化需留意）。
4. 集成/环境敏感用例（如真广播 loopback）做能力探测降级：不可达环境打 SKIP 按过计（先例：test_discovery.cpp `broadcast_loopback_reachable`）。
5. 时序敏感断言避免跨两次 sleep 的相对比较（CI 高负载抖动面，先例教训：test_time.cpp `test_timer_reset`）。

## 常用命令

```bash
ctest --test-dir build --output-on-failure     # 全量（失败输出）
ctest --test-dir build -R MetricsTests -V      # 单套件
ctest --test-dir build -N                      # 列出全部套件
```

## 历史

本 README 前身为 Actor 框架测试文档（Actor 模块已移除，见 modules/CMakeLists.txt Level 6 注记）；2026-10-07 随 P4-1 文档修订重写为现状口径。
