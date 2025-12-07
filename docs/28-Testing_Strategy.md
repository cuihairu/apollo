# 测试与质量保障策略

> 目的：为 Apollo MMORPG 项目建立系统化的测试体系，覆盖单元、集成、端到端、回归与自动化流程，确保快速迭代中保持质量。

## 1. 测试维度概览

| 层级 | 描述 | 主要工具/方法 |
|------|------|---------------|
| 单元测试 (UT) | 独立函数/类、无外部依赖 | Catch2、gTest、GoogleMock |
| 组件测试 (CT) | 模块内子系统（NetCore/AOI/DataProxy等） | 自研模拟器、Mock Transport/DB |
| 集成测试 (IT) | 多服务协同（Gate+Zone+AOI+DataProxy） | Docker Compose/容器集群 |
| 端到端 (E2E) | 客户端 SDK → Gate → Zone → DB 全链路 | Unity 自动化脚本、C++ client |
| 回归/灰度验证 | 更新后对关键场景回归 & 数据对比 | Pipeline 自动回放脚本 |
| 非功能测试 | 性能/压测、安全、容灾 | 参考 `docs/23`、`docs/21` |

## 2. 单元测试

### 2.1 范围
- 算法/容器：AOI 计算、属性计算、序列化。
- 网络编解码：包头、加密、校验。
- DataProxy 序列化/分片逻辑。
- 插件加载、ApplicationContext 生命周期。

### 2.2 策略
- 使用 Catch2 + GoogleMock。
- 覆盖率指标（行/分支）60% 起步，核心模块 80%+。
- 引入 `ctest` 与 CI 集成，失败阻断合并。

## 3. 组件/模块测试

### NetCore
- 搭建模拟客户端（C++），测试连接/重连/心跳/限流。
- 模拟异常（网络抖动、丢包），验证 SessionManager 行为。

### AOI
- 离线构建场景数据（路径/随机移动），回放并检查 Enter/Leave 序列。
- 使用可视化工具（脚本）验证 AOI 视野正确性。

### DataProxy
- 使用内存版 Redis/MySQL（或 Docker 中的真实实例），执行 Load/Save/并发写入。
- 模拟 DB 故障，验证写队列重试、WAL。

### Battle Service
- 构造不同战斗脚本，检查 Pipeline 结果（HP、Buff、事件顺序）。
- 对比客户端模拟器的结算结果，确保 determinism。

## 4. 集成测试

### 4.1 环境
- 通过 Docker Compose 或 Kubernetes 部署 Gate/Zone/AOI/DataProxy/DB/Redis/Transport。
- 每日构建自动部署 Test 环境。

### 4.2 场景
- 登录/创建角色/进入场景。
- 玩家移动 + AOI 广播。
- 战斗流程（施法、伤害、结算）。
- 数据持久化/断线重连。
- 插件加载/卸载、配置热更新。

### 4.3 自动化
- 使用 Python/Golang 写模拟客户端脚本，调用 gRPC/HTTP 接口驱动场景。
- 集成测试后自动收集日志/指标，通过准入条件（如错误=0）控制合并。

## 5. 端到端测试 (E2E)

### 5.1 Unity 自动化
- 使用 Unity Test Runner/PlayMode Tests 驱动 SDK 调用。
- 脚本：登录/移动/战斗/退出/异常网络。
- 可在 CI 上运行（Headless Unity + Linux）。

### 5.2 脚本化客户端
- C++/Go 客户端模拟器，可在无 UI 环境运行，支持协议 fuzz。

### 5.3 数据校验
- 测试完成后，将服务器 TLog/DB 数据与预期对比，确保逻辑正确。

## 6. 回归与发布验证

### 6.1 回放脚本
- 将关键场景录制为脚本（proto/log），每次发布前回放，保证行为一致。
- Script runner 负责在测试环境执行，比较输出/日志。

### 6.2 数据一致性
- 发布后对比旧版本与新版本数据库快照、TLog，确保无异常波动。

### 6.3 灰度验证
- 在灰度环境使用真实玩家/机器人，观察监控/告警指标。

## 7. 自动化流水线

### 7.1 CI 阶段
1. 代码检查（lint/格式/静态分析）。
2. 单元测试。
3. 构建产物。
4. 组件测试（可选，异步）。

### 7.2 CD 阶段
1. 部署 Test 环境，运行集成测试。
2. 运行 E2E/性能测试（按需）。
3. 部署 Staging，执行灰度脚本。
4. 发布 Prod。

### 7.3 工具
- GitHub Actions / GitLab CI / Jenkins。
- Docker/K8s 作为测试环境载体。
- Allure/TestRail 用于报告。

## 8. 缺陷管理与质量门禁
- 缺陷在 Jira/Issue 系统中跟踪，标注影响范围与修复计划。
- 质量门禁：若 UT/IT/E2E 任意环节失败，则阻止合并。
- 所有 bug 需有对应测试用例，防止回归。

## 9. 测试数据与 Mock
- 提供测试数据生成脚本（模拟玩家、装备、任务）。
- 对外部依赖（支付、第三方 SDK）使用 Mock 服务。
- DataProxy 提供“内存模式”以方便测试。

## 10. Roadmap
1. 建立 UT 框架与基础用例。
2. 完成 NetCore/AOI/DataProxy 等模块的组件测试工具。
3. 构建自动化集成环境（Docker Compose/K8s）。
4. 完成 Unity SDK 自动化脚本与 C++ 客户端模拟器。
5. 建立回放脚本、数据校验工具。
6. 将所有测试纳入 CI/CD Pipeline，形成质量门禁。

---

通过该策略，项目可在迭代过程中保持质量可控，问题发现及时，并支撑自动化发布流程。***
