# 日志收集程序

logger 是观测工具，不承担玩家、Scene、Instance 或路由的游戏权威。当前约束见 [ADR-013](../architecture/adr.md)。

## 当前功能

业务进程使用本地结构化文件日志；配置 APOLLO_LOG_PUSH_ADDR 后可通过现有 M1 日志推送能力发送到收集端。logger 提供 scan/follow 本地检索与 serve 收集入口，可按进程、级别、时间及类别检索。

收集端不可用时应走既有有界缓冲与降级行为，本地日志保留，不阻塞模拟线程。World 的链路与日志身份统一为 world，程序为 world。

## 实现定位

- apps/logger：收集与检索入口。
- modules/net/logpush：业务进程日志推送。
- modules/core/log：结构化记录与文件输出。
- tests/test_logger_retrieval.cpp、tests/test_log_push.cpp：已有验证入口。

Kafka、ClickHouse、Prometheus/Grafana 外部基建未作为当前完成能力，保持根目录 todo.md 中的需求触发项。具体落盘、查询与失败规则见[日志设计](./logging.md)。
