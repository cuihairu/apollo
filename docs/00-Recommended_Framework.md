# Apollo MMORPG 自研框架（建议版）

## 1. 目标与原则
- **统一口径**：文档、指标、实施计划围绕同一套技术栈与服务拆分，避免再出现 SDNet 等历史遗留描述。
- **跨平台网络内核**：Linux 采用 `io_uring` (fallback `epoll`)，Windows 采用 IOCP；对上提供统一的 NetCore。
- **性能目标**：单服 5000+ CCU，核心操作 P99 < 60 ms，99.95% 可用；每份文档均以此为 SLO。
- **关注点分离**：AOI、Battle、Social、DataProxy 等服务各司其职；存储/计算/日志链路互不阻塞。
- **工程化**：C++20 + CMake Presets + vcpkg；CI 覆盖构建、单元/压力测试；Prometheus/Grafana 观测；Kafka→ClickHouse TLog。

## 2. 技术栈
| 层级 | 技术 |
|------|------|
| 语言 & 标准 | C++20，MSVC 2022 / GCC 11+，`std::pmr` + RAII |
| 构建 | CMake Presets，vcpkg manifest，clang-format/tidy |
| 网络 | 自研 NetCore：Linux `io_uring` + fallback `epoll`，Windows IOCP；支持 TCP/KCP/WebSocket |
| 序列化 | Protobuf/gRPC（内部），REST/JSON（对外运营/GM） |
| 配置 | Excel→Lua/JSON pipeline，配置中心 + FileWatcher + 版本治理 |
| 数据 | MySQL 8.0 主从 + 分库分表；Redis Cluster；Kafka + ClickHouse（BI/TLog） |
| 逻辑 | Lua 5.4 + sol2，热更沙箱；ECS 架构 + 属性容器 |
| 运维 | Consul/Etcd 服务发现，Prometheus + Grafana，Jaeger Trace，ELK 日志 |

## 3. 推荐架构
```
┌───────────────────────── 玩家客户端 ─────────────────────────┐
│ Unity / UE / Mobile                                         │
└───────────────┬─────────────────────────────────────────────┘
                │TCP/KCP + Protobuf
        ┌───────▼───────┐
        │ CDN / WAF     │
        └───────┬───────┘
                │L4 LB
┌───────────────▼──────────────── 接入层 ─────────────────────┐
│ GateServer Cluster                                           │
│ • 连接/心跳 • 加密/防刷 • 消息路由 • 限流/灰度 • Session     │
└───────┬────────────────────────────────────────────────────┘
        │NetCore (TCP/KCP + Proto)
┌───────▼────────┐
│ WorldServer     │  玩家流量主干
└─┬───────────────┘
  │玩家生命周期 / 场景调度 / Consul
  ▼
┌───────────────┐          ┌───────────────────┐
│ Scene Orchestr.│─spawn→  │ ZoneServer xN      │ (ECS/战斗/任务)
└────┬──────────┘          └────┬──────────────┘
     │实例生命周期                  │调用
     │                             ▼
     │                     ┌──────────────┐
     │                     │ AOI Service  │ (独立位置/广播)
     │                     └────┬─────────┘
     │                           │Enter/Leave
     │                           ▼
     │                     ┌──────────────┐
     │                     │ Battle Service│ (可选大型战斗)
     │                     └────┬─────────┘
     │                           │技能结算
     ▼                           ▼
┌──────────────┐        ┌─────────────────────┐
│ Social Svc   │        │ DataProxy            │
│ (Chat/Guild/ │<------>│ (Redis/MySQL 封装)   │
│ Match/Rank…) │        └────┬────────────────┘
└────┬─────────┘             │SQL/Cache
     │                        ▼
     │                ┌────────────────────────┐
     │                │ MySQL Cluster / Redis  │
     │                └────────────────────────┘
     │日志
     ▼
┌──────────────────────────────┐
│ LogAgent → Kafka → ClickHouse│
└──────────────────────────────┘

┌───────────────────────── 运维/运营 ─────────────────────────┐
│ GM / BI / 数据服务 / SDK                                   │
└────┬───────────────────────────────────────────────────────┘
     │HTTP/gRPC
┌────▼──────────┐
│ Ops API Svc   │ (可选，负责 GM、BI、工具接入)
└────┬──────────┘
     │内部 gRPC
     ▼
┌──────────────┐
│ Social / Data │ (复用 DataProxy / Social 模块能力)
└──────────────┘
```

## 4. 核心模块职责
- **GateServer**：基于 NetCore 的连接管理、限流、加密、消息去重、路由；支持灰度和流量复制。与 WorldServer 直接协作，其他后台接口不经过 Gate。
- **WorldServer**：全局场景索引、跨服活动、账号登录态、玩家迁移 FSM；与 Consul/Etcd 交互控制调度。
- **Scene Orchestrator + ZoneServer**：ZoneServer 负责具体场景逻辑与 ECS/战斗；Orchestrator 负责实例生命周期管理、资源池、平滑扩容。对于固定地图或塔防类玩法，可提供 “Compact GameServer” 形态，将 Scene/AOI/Battle 合并在单进程运行，仍复用 NetCore、DataProxy、Transport 等基础设施，后续需要扩展时可平滑切换回分层架构。
- **AOI Service**：持有实体位置和 AOI 半径，输出 Enter/Leave/Sync 事件，支持 AOI 分片和跨区广播；在 Compact 模式下可内嵌于单进程逻辑层。
- **Battle Service（可选）**：为大型团战/副本提供独立结算流水线，减轻 ZoneServer；塔防或轻量战斗可直接使用 Compact GameServer 内置战斗模块。
- **DataProxy**：封装 Redis/MySQL，提供缓存一致性框架、写回策略、TLog 触发；供 Zone/AOI/Social 使用。
- **Social Services**：Chat/Guild/Match/Rank/Mail/Trade 等按业务拆分，统一通过 DataProxy 访问数据，可同时为玩家实时链路与运维/GM API 提供能力。
- **LogAgent/TLog**：从各模块收集行为日志→Kafka→实时监控(Flink)/离线分析(ClickHouse)。

## 5. 缺失文档 & 下一步
（已在当前仓库中逐步补齐：NetCore、Transport、AOI、DataProxy、API、实施计划等文档。后续若引入新模块，再按需更新此列表。）

确认此框架后，可逐章替换现有 docs，并把实施计划/需求池同步更新，再展开具体模块的技术细节讨论。
