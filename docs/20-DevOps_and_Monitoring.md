# DevOps 与监控设计

> 目标：定义 Apollo MMO 的构建/测试/发布/监控体系，覆盖 CI/CD、日志、指标、告警、治理流程，确保框架落地后可运维。

## 1. CI/CD 流程

### 1.1 构建
- 使用 CMake Presets + vcpkg manifest；CI 环境（GitHub Actions/GitLab CI）构建 Windows + Linux 版本。
- 静态分析：clang-tidy、cppcheck、clang-format。
- 单元测试：Catch2；集成测试使用 gTest 或自研框架。
- 代码覆盖率：LLVM/GCov，阈值需达成（例如 >70% 核心模块）。

### 1.2 测试阶段
| 阶段 | 内容 |
|------|------|
| UT (Unit Test) | 单元测试、Mock DataProxy/Transport |
| IT (Integration) | NetCore/AOI/DB 模块集成 |
| ST (System) | 端到端测试环境（Gate+Zone+DB） |
| PT (Performance) | 压测工具（wrk、Locust、custom C++ client） |

### 1.3 发布
- 使用 `release manifest`，描述需要部署的服务版本、配置、依赖。
- 灰度发布：支持按大区/机房/玩家段逐步发布。
- 回滚机制：版本+配置可快速回退；数据库迁移需双向脚本。
- 配置管理：版本化（git/配置中心），发布前校验与审批。

## 2. 监控与指标

### 2.1 指标体系
- Prometheus 作为统一收集器，Grafana 展示；各服务暴露 `/metrics`（或 gRPC metrics）。
- 指标类别：
  - **系统**：CPU、内存、IO、FD、线程。
  - **网络**：连接数、带宽、RTT、重传、限流次数（来自 NetCore）。
  - **业务**：在线人数、场景数量、AOI 事件延迟、任务队列长度。
  - **数据层**：DataProxy 队列、Redis 命中率、MySQL QPS、慢查询。
  - **日志**：TLog 写入速率、Kafka backlog。

### 2.2 采集方式
- 使用 Prometheus exporter (自研) 或 `prometheus-cpp`，在每个服务内嵌。
- 系统指标可借助 node_exporter/telegraf。
- 重要定时任务（如场景创建、AOI shard）也通过事件计数器记录。

## 3. 日志与追踪

### 3.1 日志
- spdlog + fmt 封装统一 API（不同级别：trace/debug/info/warn/error/fatal）。
- 日志格式 JSON（包含 timestamp, level, server_id, trace_id, message）。
- 日志轮转、异步写入；输出到本地文件 + FluentBit/Filebeat -> ELK。
- 日志分级：运行日志、访问日志（API）、安全日志（鉴权）、TLog（业务流水）。

### 3.2 分布式追踪
- 使用 OpenTelemetry/Jaeger，埋点 NetCore、Transport、DataProxy、AOI/Zone 等关键链路。
- Trace ID 注入到内部 gRPC/HTTP header；客户端 SDK 也可生成 trace。

## 4. 告警与自愈

### 4.1 告警规则
| 场景 | 告警条件 |
|------|----------|
| 服务不可用 | 健康检查失败/Consul 下线 |
| 网络异常 | 连接丢包率、高 RTT |
| 数据层 | MySQL 延迟、Redis 读写失败、DataProxy 队列堆积 |
| 业务 | 在线人数骤降、AOI 事件延迟超阈、TLog backlog |

### 4.2 告警通道
- Grafana Alert、PagerDuty、企业 WeChat/Slack。
- 分级（P1/P2/P3），不同级别触发不同响应流程。

### 4.3 自愈策略
- 自动重启服务（容器/系统级），结合 Kubernetes/Nomad。
- 数据队列堆积自动扩容消费节点。
- 限流开关：在指标异常时自动开启部分限流/维护模式。

## 5. 运维流程

### 5.1 配置与发布
- 所有配置/脚本由 Git 管理，发布前必须 PR + Review。
- 使用基础设施工具（Ansible/Terraform）部署环境；生产环境自动化程度 > 90%。
- 每次发布记录于 Release Log（版本、操作人、时间、说明）。

### 5.2 故障处置
- 定义 SOP（日志收集、抓取核心指标、回滚/重启步骤）。
- 提供一键打包诊断信息的脚本（日志、配置、核心转储）。
- 定期复盘与问题库，沉淀经验。

## 6. 环境划分
- **Dev**：开发自测，频繁部署。
- **Test**：集成测试 + QA。
- **Staging**：接近生产，执行压测/灰度。
- **Prod**：生产。
- 每个环境维护独立配置/资源，支持一键初始化、数据脱敏。

## 7. 工具与自动化
- **部署**：Kubernetes 或自研调度（Consul + Nomad）。脚本化提供一键启动/停止整个集群。
- **压测平台**：内建压测工具（模拟客户端），支持回放日志/脚本。
- **运营工具**：仪表盘显示在线人数、服务器状态、热点场景等信息。
- **调试工具**：抓包、流量复制、在线参数调整。

## 8. 安全与合规
- 接入审计：所有运维操作（API/SSH）都记录并可追溯。
- 权限管理：RBAC，最小权限原则，运维账号分级。
- 数据保护：敏感数据加密存储/传输；脱敏工具用于测试环境。

## 9. Roadmap
1. 完成 CI/CD Pipeline（构建 + 测试 + 发布）。
2. 集成 Prometheus/Grafana，发布基础指标。
3. 建立日志采集链路（spdlog → FluentBit → ELK）。
4. 引入 OpenTelemetry/Jaeger 追踪。
5. 自动化部署/扩容工具上线，制定灾备演练计划。
6. 完成告警策略和自愈脚本，定期执行演练。

---

该文档为 DevOps/运维团队提供指导，确保 Apollo 框架具备工程化可维护性。***
