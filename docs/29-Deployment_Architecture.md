# 部署与基础设施架构

> 目的：详细描述 Apollo MMO 各服务在生产环境的部署拓扑、调度方式、配置中心、CI/CD 结构，以及多环境隔离方案，确保设计能够顺利落地。

## 1. 环境划分

| 环境 | 用途 | 规模 | 特点 |
|------|------|------|------|
| Dev | 开发自测 | 单机/小集群 | 快速迭代，允许调试 |
| Test | 集成测试 | 1-2 套 | 自动化 IT/QA |
| Staging | 灰度/压测 | 与 Prod 等规模 | 全量服务，模拟真实数据 |
| Prod | 线上 | 多 Region/Zone | 高可用、容灾 |

每个环境采用独立配置、数据库、Redis；Prod 使用多个 Region，Region 内含多个可用区。

## 2. 基础设施

### 2.1 调度/编排
- 推荐使用 Kubernetes（AKS/EKS/自建）或 HashiCorp Nomad。关键服务（Gate/World/Zone/AOI/DataProxy/LogAgent等）以容器形式运行。
- 使用 Helm/Terraform 管理部署。每个服务有独立 Helm chart，包含 ConfigMap/Secrets/Service/StatefulSet。
- 对于高性能或需裸金属（如数据库、Battle Service），可使用专用节点池。

### 2.2 服务治理
- Consul/Etcd 作为服务发现与配置中心。
- Consul Template/Envoy Sidecar 实现服务发现 + mTLS。
- 使用 HashiCorp Vault 或 KMS 管理密钥/证书，服务启动时动态拉取。

### 2.3 网络拓扑
```
Internet -> (CDN/Anti-DDoS) -> L4 Load Balancer -> GateServer Pods
                                              ↘ 管理入口 (Ops API)
GateServer -> WorldServer -> Zone/AOI/DataProxy (内部 VPC)
Internal Services -> DB/Redis/Kafka (私网，仅白名单访问)
```
- 使用 VPC + 子网隔离：Public 子网（仅 LB）、Private 子网（应用）、Data 子网（DB/Kafka）。
- 安全组限制访问；所有内部通信走 mTLS。

## 3. 服务部署拓扑

| 服务 | 部署模式 | 容器/实例数 | 备注 |
|------|----------|-------------|------|
| GateServer | Deployment (无状态) | 按并发扩缩 | NodePort/LB 暴露 |
| WorldServer | StatefulSet (少量) | 2-3 | 需持有全局状态 |
| Scene Orchestrator | Deployment | 2+ | 可横向扩展 |
| ZoneServer | StatefulSet/Deployment | 按地图/实例数 | Pod 内运行多 Zone 线程 |
| AOI Service | StatefulSet | Shard 数量决定 | 需固定 Pod/IP |
| Battle Service | StatefulSet | 依据场景 | 可有 GPU/特定硬件 |
| DataProxy | Deployment | 2N | 支持水平扩容 |
| Redis/MySQL | 运营商托管或自建 StatefulSet | 集群模式 | 建议托管版（Aurora/MySQL Cluster） |
| Kafka/ClickHouse | StatefulSet | 3+ | 备份/监控 |
| Social Services | Deployment | 2+ | 无状态 |
| LogAgent | DaemonSet | 1/节点 | 收集日志 |

## 4. 配置与 Secrets

### 4.1 配置管理
- 结构：`config/{env}/{service}.yaml` + ConfigMap。
- 支持热加载的服务（NetCore、AOI、DataProxy）从 ConfigMap + Consul 动态拉取。
- 对敏感配置（DB 密码、TLS 证书）使用 Vault + Secret 注入。

### 4.2 配置升级
- CI Pipeline 检查配置格式，部署时通过 Helm 冻结。
- 使用 Feature Flag/Config Toggle 控制灰度。

## 5. CI/CD 流程

### 5.1 Pipeline 步骤
1. **源码提交** -> 触发 CI（Lint/UT/Build）。
2. 生成容器镜像 (Docker) + SBOM。
3. 推送到镜像仓库。
4. 自动部署到 Test 环境，运行集成测试。
5. 人工审批/自动 Gate -> 部署 Staging。
6. 压测/灰度 -> 通过后自动部署 Prod（滚动/蓝绿/金丝雀）。

工具选择：GitHub Actions/GitLab CI/Jenkins + ArgoCD/Fleet 进行 GitOps。

### 5.2 发布策略
- 采用滚动更新和金丝雀发布：
  1. Staging 验证。
  2. Prod 小比例流量（金丝雀）监控指标（RTT、错误）。
  3. 成功后滚动更新全部实例。
- 关键服务（Gate/World/DataProxy）支持回滚（保留前版本镜像/配置）。

## 6. 监控与日志（与 `docs/20` 关联）
- 每个服务暴露 Prometheus `/metrics`。
- 日志通过 LogAgent -> FluentBit -> ELK。
- Trace 使用 OpenTelemetry Collector，导出至 Jaeger/Tempo。
- 告警配置在 Grafana Alert/PagerDuty。

## 7. 容灾与备份
- Prod 多 Region：主 Region + 异地热备，使用数据库双写或半同步复制。
- Kafka/Redis/MySQL 均需跨 Region 备份；ClickHouse 使用复制表。
- 每日备份存储到对象存储（加密）；定期执行恢复演练。

## 8. 资源管理
- Node 池分类：`public`（网关）、`logic`（Zone/AOI/Battle）、`data`（DB/Kafka）、`utility`（LogAgent/CI）。
- 使用自动扩缩（HPA/Cluster Autoscaler）调节 Gate/Zone 等无状态服务。
- Battle Service/AOI 需要保证 CPU/内存绑定，可用 Dedicated Node Pool + CPU pin。

## 9. 安全
- 所有 Pod 运行在非 root 用户，启用 PSP/OPA 限制。
- 使用 Istio/Linkerd（可选）提供 mTLS、策略、观测。
- 可通过 WAF/Cloud Armor/Shield 保护入口。

## 10. Roadmap
1. 搭建 Dev/Test K8s 集群，配置 CI/CD Pipeline。
2. 实现 Helm/ArgoCD 部署流程，覆盖 Gate/World/DataProxy 等服务。
3. 接入 Consul/Vault，统一服务发现和 secrets。
4. 建立监控/日志/追踪栈，配合 `docs/20`。
5. 完成 Staging/Prod 多 Region 架构，执行容灾演练。

---

该部署架构文档为后续基础设施实施提供蓝图，确保服务器设计能在生产环境稳定运行。***
