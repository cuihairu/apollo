# Apollo 模块化执行清单

本清单用于跟踪从单体 `apollo` target 迁移到模块化 target 的施工顺序。

## Phase 1: 骨架完成

- [x] 顶层 `CMakeLists.txt` 接入 `modules/`
- [x] 顶层 `CMakeLists.txt` 接入 `apps/`
- [x] 新增 `modules/base`
- [x] 新增 `modules/core`
- [x] 新增 `modules/runtime`
- [x] 新增 `apps/game-server`
- [x] 明确 target 命名规则
- [x] 明确依赖方向

## Phase 2: 基础模块抽取

- [x] 盘点 `src/utils`、`src/apollo/utils` 中可下沉到 `base` 的通用能力
- [x] 抽出 `apollo_base` 第一批真实源码
- [x] 通过独立 smoke build 验证 `apollo_base -> apollo_core -> apollo_runtime -> apollo_game_server`
- [x] 为 `apollo_base` 增加基础单元测试
- [ ] 定义 `base/terminal` 与 `runtime/console` 的边界
- [ ] 将 `Time`、`IdPool` 的现有调用逐步切换到 `apollo/base/*`
- [x] 将 `ThreadPool` 的新公共入口落到 `apollo/base/thread_pool.hpp`

## Phase 3: 核心模块抽取

- [x] 合并并迁移 DI 能力到 `apollo_core`
- [x] 合并并迁移配置管理到 `apollo_core`
- [x] 合并并迁移日志管理到 `apollo_core`
- [ ] 统一生命周期与模块管理模型
- [x] 为 `apollo_core` 增加最小可运行测试

## Phase 4: Runtime 抽取

- [x] 将 `GameServer` 重新定义为 runtime host，而不是框架总入口
- [x] 新增 `ApplicationHost`
- [x] 新增 `ServiceHost`
- [x] 新增 signal / shutdown hook
- [x] 新增 console event loop 抽象边界

## Phase 5: Data 抽取

- [x] 抽出 `apollo_data_core`
- [x] 迁移 ORM / SQL Template / datasource
- [ ] 迁移 RedisTemplate / cache primitive / distributed lock primitive
- [ ] 移除重复的数据库与 redis 路径

## Phase 6: Net 抽取

- [ ] 收敛 `src/network` 与 `src/apollo/net`
- [ ] 抽出 `apollo_net_core`
- [ ] 分离 tcp / http / websocket / rpc

## Phase 7: Game 抽取

- [ ] 抽出 `apollo_game_core`
- [ ] 迁移 AOI / scene / movement 到 `game/world`
- [ ] 迁移 attribute / buff / skill / ecs 到 `game/battle`
- [ ] 识别 social / economy 的独立边界

## Phase 8: Starter 与 App 装配

- [ ] 为 core/net/mysql/redis 提供 starter
- [ ] 将 `apps/game-server` 改造成 starter 组合入口
- [ ] 引入 `apps/gateway-server`
- [ ] 引入 `apps/login-server`

## Exit Criteria

- [ ] 顶层单体 `apollo` target 仅作为兼容聚合层，或完全退役
- [ ] 新 app 全部通过模块 target + starter 组合构建
- [ ] 模块边界稳定，测试覆盖基础路径
