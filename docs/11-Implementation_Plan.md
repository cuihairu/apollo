# Apollo MMORPG 服务器实施计划

> **版本**: 2.0  
> **更新日期**: 2024-12-07  
> **项目周期**: 12 个月  
> **团队规模**: 10-14 人

## 1. 项目目标

### 1.1 核心指标
1. **性能**：单服 5000+ CCU，核心操作 P99 < 60 ms，吞吐线性扩展。
2. **可用性**：99.95% 年可用性，具备弹性扩容、自动化运维能力。
3. **易扩展**：模块化架构（NetCore、AOI、DataProxy、插件系统），支持新玩法快速接入。
4. **安全与 SDK**：提供 Unity SDK、GM/运营 API，具备 TLS/mTLS、RBAC、审计。

### 1.2 验收标准
- [ ] NetCore + Unity SDK 在测试环境跑通 5k 虚拟客户端压测。
- [ ] AOI/Scene/Battle 服务具备独立扩缩、故障隔离能力。
- [ ] DataProxy 与 TLog 体系稳定运行，BI 可实时查询核心指标。
- [ ] DevOps/监控/安全体系上线，发布流程可灰度与回滚。

## 2. 团队组织

| 角色 | 人数 | 主要职责 |
|------|------|----------|
| 架构师 | 1 | 总体架构、关键模块把控 |
| 网络/基础设施工程师 | 2 | NetCore、Transport、SDK |
| 服务器逻辑工程师 | 4 | Gate/Zone/AOI/Battle/业务模块 |
| 数据/平台工程师 | 2 | DataProxy、数据库、BI/TLog |
| 运维/DevOps | 2 | CI/CD、监控、发布、安全 |
| QA/测试 | 2 | 自动化测试、压测、SDK 联调 |
| 产品/项目 | 1 | 需求、排期、跨团队协调 |

协作方式：Scrum + 2 周迭代，每个阶段设立里程碑评审；关键模块需要设计评审 + 安全评审。

## 3. 阶段规划

### Phase 1：基础架构搭建 (2025.01 - 2025.03)

| 模块 | 负责人 | 任务要点 | 产出 |
|------|--------|----------|------|
| NetCore & SDK | 架构师 + 网络工程师 | - 基于 io_uring/IOCP 的 NetCore 实现<br>- 消息格式/握手/去重/限流<br>- Unity SDK (C#) 初版 | NetCore 源码、SDK 包、`docs/16` 更新 |
| Transport 层 | 网络工程师 | - 实现 NNG TCP/IPC Transport<br>- ServiceRegistry 集成，动态选择 transport | Transport 库、`docs/13` 实施 |
| Spring-like 基础设施 | 服务器工程师 | - ApplicationContext 2.0（Bean/依赖/事件）<br>- 配置系统升级、命令行解析、事件/任务调度 | `common` 模块、`docs/14` 落地 |
| 插件系统 | 平台工程师 | - SharedLibrary 封装、插件注册接口<br>- MySQL/Redis 插件 PoC | 插件框架、`docs/15` 实施 |
| DevOps 基础 | DevOps 团队 | - CI/CD Pipeline（构建+测试）<br>- Prometheus/Grafana 最小集、日志采集 | CI/CD 配置、`docs/20` Phase1 |

### Phase 2：核心服务开发 (2025.04 - 2025.07)

| 模块 | 负责人 | 任务要点 | 产出 |
|------|--------|----------|------|
| GateServer | 网络 + 服务器工程师 | - 与 NetCore/SDK 对接<br>- 会话管理、限流、防刷、灰度<br>- 监控指标与调试工具 | Gate 模块、压测报告 |
| Scene Orchestrator & ZoneServer | 服务器工程师 | - 场景生命周期、实例池<br>- Zone ECS/战斗主循环 | 实例管理服务、`docs/17` 实施 |
| AOI Service | 服务器工程师 | - AOI 分片、事件协议、容灾<br>- 与 Zone/Gate/Transport 集成 | AOI 服务、协议文档 |
| DataProxy & Hybrid Schema | 数据工程师 | - DataProxy v1（Load/Save/缓存写队列）<br>- Hybrid Schema、分库分表计划<br>- TLog 管线 | DataProxy 服务、`docs/18` 落地 |
| API & SDK 标准 | 平台 + 网络工程师 | - 重写内部 gRPC/外部 REST 规范<br>- SDK/Proto/OpenAPI 生成工具 | `docs/19` 实施、API Gateway |
| 安全基线 | DevOps + 安全 | - TLS/mTLS、HMAC、防重放<br>- OAuth2/Keycloak、RBAC、审计 | 安全设施、`docs/21` Phase1 |

### Phase 3：系统强化与玩法 (2025.08 - 2025.10)

| 模块 | 任务 | 产出 |
|------|------|------|
| Battle Service & 扩展玩法 | - 大型战斗 offload 服务<br>- 统一技能流水线、状态同步 | Battle Service、压测结果 |
| Social/Integration | - Chat/Guild/Match、邮件交易<br>- 与 DataProxy/TLog 一致性检查 | 社交模块 |
| 插件扩展 | - Mongo/ClickHouse/其他数据源插件<br>- 共享内存 transport PoC | 新增插件、性能评估 |
| Observability & Ops | - 完善日志、追踪、自愈<br>- 压测平台、流量复制 | 监控体系完善 |
| 安全进阶 | - 反作弊集成、插件签名、渗透测试 | 安全评估报告 |

### Phase 4：稳定化与上线准备 (2025.11 - 2025.12)

| 模块 | 任务 | 产出 |
|------|------|------|
| 全链路压测 | - 客户端压测（5k+并发）<br>- AOI/Zone/Battle 极限场景 | 压测报告 |
| SDK & API 发布 | - Unity SDK 正式版、文档、示例项目<br>- GM/运营 API 上线（含审批流程） | SDK 交付物、API 门户 |
| 运维与安全演练 | - 故障演练、回滚演练、灾备验证<br>- 安全事件响应演练 | 演练报告 |
| 文档与培训 | - 所有设计/运维文档冻结<br>- 培训开发/运维人员 | 文档库、培训材料 |

## 4. 关键依赖与风险

| 风险 | 描述 | 缓解措施 |
|------|------|----------|
| NetCore 性能风险 | io_uring/IOCP 实现复杂，可能影响进度 | 早期 PoC + 压测，必要时 fallback epoll |
| 数据一致性 | Hybrid Schema + 异步写 复杂 | DataProxy 写队列 + WAL + 测试 |
| AOI/Scene 复杂度 | 多服务交互，容灾难度大 | 明确接口、仿真和压测、容灾演练 |
| 插件/依赖安全 | 动态加载风险 | 插件签名、沙箱、代码审计 |
| SDK 生态 | Unity SDK 需要持续维护 | 提前开放内测，建立反馈机制 |

## 5. 追踪方式
- 每个阶段结束进行里程碑评审，输出：设计文档、代码、测试报告。
- Jira/Issue 跟踪任务；CI/CD 状态驱动质量门禁。
- 指标（性能、上线质量、故障率）纳入 OKR。

---

本实施计划对齐最新架构文档（NetCore、Transport、AOI、DataProxy、API、DevOps、安全等），为未来 12 个月的研发与运营提供执行路线。***
