# Apollo 测试套件

## 概述

统一测试树：单 cpp 直编惯例（`TEST_ASSERT` 宏 + `bool test_xxx` 用例 + `main` runner），`tests/CMakeLists.txt` 挂 CTest；契约测试三件套注册在 `modules/contract/` 与 `sdks/gen/`，与 tests/ 树共同构成 ctest 全集（本地 ON 树 53 项、CI 腿 64 项——两树注册集不完全重合，见「门禁口径」注记）。

tests/ 下 86 个 `test_*.cpp`，3 个未注册（`test_all_features.cpp` / `test_thread_pool_simple.cpp` / `test_timer_main.cpp`，后者为 0 字节会话语义遗留）。其余挂在 `tests/CMakeLists.txt` 的 92 条 `add_test` 上，**全部位于 :7 的外层 `if(GTest_FOUND AND APOLLO_BUILD_GTESTS)` 门内（块延伸至文件末尾 :1945）**，其中 42 条另受嵌套守卫（`if(TARGET …)` 族、模块开关）支配——本地 ON 树最后一次 configure 仅生效 50 条，被静默跳过的 42 条含整个 game 域族（`avatar_tests` / `scene_tests` / `game_tests` 等二进制已在本地树编出，却未进入 CTestTestfile，ctest 无从感知）。初始导入脚手架批 8 个目标（data_structures / log / network / protobuf / rest_template / utils / id_pool / timer）已于 2026-10-15（305c4d6f）删除、接线同清；buffer_tests 与 channel_tests 现役保留。

## 门禁口径

四树构建 + CTest 全绿（2026-10-16 实测：ON 树 53/53、OFF 树 51/51、Examples 树 51/51、san 树 51/51；数字随批增长，各批交付注记为凭）：

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

san 树（树 4）为独立 sanitizer 配置（`/tmp` 下 ASan+UBSan + `-fno-sanitize-recover=all`），不在上面三条 configure 命令内。

> **注册集分布注记（2026-10-16 实测）**：本地 ON 树 53 项与 CI 腿 64 项**不完全重合**——本地缺整个 game 域族（Avatar/Scene/Instance/PlayerDirectory/Social/Battle/WorldHost 等，即上面 42 条嵌套守卫未生效者），CI 腿含之；本地含 AttrSync/SceneScript/CellScriptHost/M1 族。单看一棵树全绿不足为凭，提交须本地树 + CI 收账双侧确认。CI windows Release 腿另有一例预存红（`ScriptAsync.CallOutsideCoroutineRejected` SEH 0xc0000005，065d1190 起，63/64 绿）——与基线一致的失败不作为回归判据。

## 套件分组（按域，实际注册面）

| 域 | 套件（CTest 名） |
|---|---|
| contract 契约生成（注册于 modules/contract、sdks/gen） | apollo_contract_tests / apollo_contract_gen_compile_test / apollo_gen_golden_check |
| base 基础件 | MemoryTests / StringTests / TerminalTests / ThreadPoolTests / TimeTests / TimerWheelTests |
| core 框架核 | CoreLifecycleTests / CoreLogTests / CoreConfigTests / LogStructuredTests / MetricsTests / LoggerRetrievalTests |
| runtime 进程面 | RuntimeTests / BaseAnchorTests / BaseAppMgrTests |
| net 网络件 | NetTests / ReconnectTests / GatewayRouteTests / ProtocolBootstrapTests / LinkTests（net M1 内核：帧定界/分发/水位/InterServerLink/TCP 底座/管理器） |
| game 会话/世界 | SessionWorldTests / WorldHostTests / AvatarTests / PlayerDirectoryTests / DirectoryMirrorTests / FleetRecoveryTests / SupervisorTests / SceneTests / InstanceTests / CellAcceptanceTests / GameTests |
| battle 玩法 | BattleRuntimeTests / RngSubstreamTests / BattleOffloadTests |
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
