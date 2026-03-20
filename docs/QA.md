# MMORPG 架构 QA

> 本文档是 MMORPG 服务器架构相关问题的索引目录，每个问题都有独立的详细解答文档。

## 一、架构设计篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 1 | 请画出你熟悉的 MMORPG 服务器架构图，说明各组件的职责和通信方式 | ✅ | [q1-server-architecture.md](qa/q1-server-architecture.md) |
| 2 | BigWorld 架构中的 CellApp 和 BaseApp 分别负责什么？为什么这样分离？ | ✅ | [q2-bigworld-cell-base.md](qa/q2-bigworld-cell-base.md) |
| 3 | 什么是 AOI（Area of Interest）？有哪些实现方式？各有什么优缺点？ | ✅ | [q3-aoi-implementation.md](qa/q3-aoi-implementation.md) |
| 4 | 如何实现大地图的无缝切换？如何处理跨服务器的玩家移动？ | ✅ | [q4-seamless-world.md](qa/q4-seamless-world.md) |
| 5 | 服务端如何设计才能支持动态扩容？ | ✅ | [q5-dynamic-scaling.md](qa/q5-dynamic-scaling.md) |
| 6 | 单服架构 vs 分布式架构，如何选择？ | ✅ | [q6-single-vs-distributed.md](qa/q6-single-vs-distributed.md) |
| 7 | 进程内架构 vs 多进程架构，各有什么优劣？ | ✅ | [q7-process-architecture.md](qa/q7-process-architecture.md) |
| 8 | 如何设计才能避免单点故障？ | ✅ | [q8-avoid-spof.md](qa/q8-avoid-spof.md) |
| 9 | CellAppMgr 如果宕机了怎么办？有哪些解决方案？ | ✅ | [q9-cellappmgr-failure.md](qa/q9-cellappmgr-failure.md) |
| 10 | 如何实现跨服功能（如跨服战场、跨服聊天）？ | ✅ | [q10-cross-server.md](qa/q10-cross-server.md) |

## 二、网络通信篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 11 | TCP vs UDP vs KCP，MMO 中各自的使用场景是什么？ | ✅ | [q11-transport-protocol.md](qa/q11-transport-protocol.md) |
| 12 | 如何处理网络抖动和丢包？ | ✅ | [q12-network-jitter.md](qa/q12-network-jitter.md) |
| 13 | 什么是可靠 UDP？如何实现？ | ✅ | [q13-reliable-udp.md](qa/q13-reliable-udp.md) |
| 14 | 如何设计消息协议？Protobuf vs JSON vs 自定义协议？ | ✅ | [q14-message-protocol.md](qa/q14-message-protocol.md) |
| 15 | 如何实现消息压缩？ | ✅ | [q15-message-compression.md](qa/q15-message-compression.md) |
| 16 | 什么是同步问题？客户端和服务端的状态如何同步？ | ✅ | [q16-state-sync.md](qa/q16-state-sync.md) |
| 17 | 如何处理延迟补偿和预测？ | ✅ | [q17-latency-compensation.md](qa/q17-latency-compensation.md) |
| 18 | 什么是 Ghost/Shadow 机制？ | ✅ | [q18-ghost-mechanism.md](qa/q18-ghost-mechanism.md) |
| 19 | 如何防止消息重放攻击？ | ✅ | [q19-replay-attack.md](qa/q19-replay-attack.md) |
| 20 | 长连接如何保持心跳？断线重连如何设计？ | ✅ | [q20-heartbeat-reconnect.md](qa/q20-heartbeat-reconnect.md) |
| 21 | WebSocket 在 MMO 中有什么应用场景？ | ✅ | [q21-websocket-mmo.md](qa/q21-websocket-mmo.md) |
| 22 | 如何处理网络消息的乱序问题？ | ✅ | [q22-out-of-order.md](qa/q22-out-of-order.md) |
| 23 | 如何实现 RPC 调用？ | ✅ | [q23-rpc-implementation.md](qa/q23-rpc-implementation.md) |
| 24 | 如何设计广播机制？如何优化大规模广播？ | ✅ | [q24-broadcast-optimization.md](qa/q24-broadcast-optimization.md) |
| 25 | 连接数上限由什么决定？如何突破 C10K 问题？ | ✅ | [q25-c10k-problem.md](qa/q25-c10k-problem.md) |
| 26 | KBEngine 消息路由机制：Gateway、Proxy、CellApp 之间如何高效转发？ | ✅ | [q26-kbengine-message-routing.md](qa/q26-kbengine-message-routing.md) |
| 27 | Real、Ghost、Shadow Entity 之间如何转换？如何高效同步？ | ✅ | [q27-entity-ghost-shadow-relationship.md](qa/q27-entity-ghost-shadow-relationship.md) |
| 28 | KBEngine 是否存在注册中心？CellApp 如何部署和通信？Actor 模型适用吗？ | ✅ | [q28-kbengine-registry-actor-transport.md](qa/q28-kbengine-registry-actor-transport.md) |
| 29 | CellApp 之间如何通信？如何发现自己的空间位置？ | ✅ | [q29-cellapp-communication-space-discovery.md](qa/q29-cellapp-communication-space-discovery.md) |
| 30 | KBEngine 的 Space 是什么？与物理空间划分有什么区别？ | ✅ | [q30-kbengine-space-concept.md](qa/q30-kbengine-space-concept.md) |
| 31 | KBEngine 如何高效广播？如何保证消息不重复？ | ✅ | [q31-kbengine-broadcast-dedup.md](qa/q31-kbengine-broadcast-dedup.md) |
| 32 | KBEngine CellApp 同机/跨机如何通信？有什么源码证据？ | ✅ | [q32-kbengine-cellapp-communication.md](qa/q32-kbengine-cellapp-communication.md) |
| 33 | KBEngine 为何不做极致性能优化？如果要做该如何改进？ | ✅ | [q33-kbengine-performance-optimization.md](qa/q33-kbengine-performance-optimization.md) |

## 三、数据存储篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 26 | 玩家数据什么时候存数据库？全量保存 vs 增量保存？ | ✅ | [q26-data-persistence.md](qa/q26-data-persistence.md) |
| 27 | 如何设计数据库表结构？ | ✅ | [q27-database-schema.md](qa/q27-database-schema.md) |
| 28 | 如何解决数据一致性问题？ | ✅ | [q28-data-consistency.md](qa/q28-data-consistency.md) |
| 29 | Redis 在 MMO 中有哪些应用场景？ | ✅ | [q29-redis-in-mmo.md](qa/q29-redis-in-mmo.md) |
| 30 | 如何设计排行榜系统？ | ✅ | [q30-leaderboard-system.md](qa/q30-leaderboard-system.md) |
| 31 | 如何设计好友系统？ | ✅ | [q31-friend-system.md](qa/q31-friend-system.md) |
| 32 | 如何设计邮件系统？ | ✅ | [q32-mail-system.md](qa/q32-mail-system.md) |
| 33 | 如何处理热点数据？ | ✅ | [q33-hot-data.md](qa/q33-hot-data.md) |
| 34 | 数据库连接池如何设计？ | ✅ | [q34-connection-pool.md](qa/q34-connection-pool.md) |
| 35 | 如何实现数据库分片？ | ✅ | [q35-database-sharding.md](qa/q35-database-sharding.md) |
| 36 | 如何处理数据库事务？ | ✅ | [q36-database-transaction.md](qa/q36-database-transaction.md) |
| 37 | 如何实现数据的缓存淘汰策略？ | ✅ | [q37-cache-eviction.md](qa/q37-cache-eviction.md) |
| 38 | 如何防止数据被篡改？ | ✅ | [q38-data-integrity.md](qa/q38-data-integrity.md) |

## 四、游戏逻辑篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 39 | 如何设计战斗系统？ | ✅ | [q39-combat-system.md](qa/q39-combat-system.md) |
| 40 | ECS 架构是什么？在游戏中有什么优势？ | ✅ | [q40-ecs-architecture.md](qa/q40-ecs-architecture.md) |
| 41 | 如何设计技能系统？ | ✅ | [q41-skill-system.md](qa/q41-skill-system.md) |
| 42 | 如何设计 Buff/Debuff 系统？ | ✅ | [q42-buff-system.md](qa/q42-buff-system.md) |
| 43 | 如何设计属性系统？ | ✅ | [q43-attribute-system.md](qa/q43-attribute-system.md) |
| 44 | 如何设计背包系统？ | ✅ | [q44-inventory-system.md](qa/q44-inventory-system.md) |
| 45 | 如何设计交易系统？ | ✅ | [q45-trading-system.md](qa/q45-trading-system.md) |
| 46 | 如何设计拍卖行？ | ✅ | [q46-auction-house.md](qa/q46-auction-house.md) |
| 47 | 如何设计公会系统？ | ✅ | [q47-guild-system.md](qa/q47-guild-system.md) |
| 48 | 如何设计任务系统？ | ✅ | [q48-quest-system.md](qa/q48-quest-system.md) |
| 49 | 如何设计副本系统？ | ✅ | [q49-instance-system.md](qa/q49-instance-system.md) |
| 50 | 如何设计匹配系统？ | ✅ | [q50-matchmaking.md](qa/q50-matchmaking.md) |
| 51 | 如何设计 AI 系统？ | ✅ | [q51-ai-system.md](qa/q51-ai-system.md) |
| 52 | 如何设计寻路系统？ | ✅ | [q52-pathfinding.md](qa/q52-pathfinding.md) |
| 53 | 如何设计场景管理？ | ✅ | [q53-scene-management.md](qa/q53-scene-management.md) |
| 54 | 如何处理多人同时抢怪/抢资源？ | ✅ | [q54-loot-contention.md](qa/q54-loot-contention.md) |
| 55 | 如何设计伤害计算？ | ✅ | [q55-damage-calculation.md](qa/q55-damage-calculation.md) |

## 五、性能优化篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 56 | 如何进行性能分析？有哪些工具？ | ✅ | [q56-profiling-tools.md](qa/q56-profiling-tools.md) |
| 57 | 如何优化内存使用？ | ✅ | [q57-memory-optimization.md](qa/q57-memory-optimization.md) |
| 58 | 对象池是什么？如何设计？ | ✅ | [q58-object-pool.md](qa/q58-object-pool.md) |
| 59 | 如何减少锁竞争？ | ✅ | [q59-reduce-lock-contention.md](qa/q59-reduce-lock-contention.md) |
| 60 | 如何设计定时器系统？ | ✅ | [q60-timer-system.md](qa/q60-timer-system.md) |
| 61 | 如何优化日志系统？ | ✅ | [q61-log-optimization.md](qa/q61-log-optimization.md) |
| 62 | 如何进行压力测试？ | ✅ | [q62-stress-testing.md](qa/q62-stress-testing.md) |
| 63 | 如何优化数据库查询？ | ✅ | [q63-database-optimization.md](qa/q63-database-optimization.md) |
| 64 | 如何优化网络带宽？ | ✅ | [q64-bandwidth-optimization.md](qa/q64-bandwidth-optimization.md) |
| 65 | 如何减少 CPU 缓存未命中？ | ✅ | [q65-cache-miss.md](qa/q65-cache-miss.md) |
| 66 | SIMD 在游戏中有哪些应用？ | ✅ | [q66-simd-gaming.md](qa/q66-simd-gaming.md) |
| 67 | 如何进行热点代码优化？ | ✅ | [q67-hotspot-optimization.md](qa/q67-hotspot-optimization.md) |
| 68 | 如何设计内存监控？ | ✅ | [q68-memory-monitoring.md](qa/q68-memory-monitoring.md) |
| 69 | 单服承载 5000 人需要考虑哪些问题？ | ✅ | [q69-5000-ccu.md](qa/q69-5000-ccu.md) |
| 70 | 如何设计高性能的消息队列？ | ✅ | [q70-message-queue.md](qa/q70-message-queue.md) |

## 六、并发与多线程篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 71 | 多线程 vs 多进程，如何选择？ | ✅ | [q71-thread-vs-process.md](qa/q71-thread-vs-process.md) |
| 72 | 如何避免死锁？ | ✅ | [q72-avoid-deadlock.md](qa/q72-avoid-deadlock.md) |
| 73 | Actor 模型是什么？有什么优势？ | ✅ | [q73-actor-model.md](qa/q73-actor-model.md) |
| 74 | 如何设计无锁数据结构？ | ✅ | [q74-lock-free.md](qa/q74-lock-free.md) |
| 75 | 线程池如何设计？任务如何调度？ | ✅ | [q75-thread-pool.md](qa/q75-thread-pool.md) |
| 76 | 如何实现异步 IO？ | ✅ | [q76-async-io.md](qa/q76-async-io.md) |
| 77 | IOCP vs Epoll 有什么区别？ | ✅ | [q77-iocp-epoll.md](qa/q77-iocp-epoll.md) |
| 78 | 如何处理竞态条件？ | ✅ | [q78-race-condition.md](qa/q78-race-condition.md) |
| 79 | 如何设计线程安全的容器？ | ✅ | [q79-thread-safe-container.md](qa/q79-thread-safe-container.md) |
| 80 | 协程在游戏服务器中的应用场景？ | ✅ | [q80-coroutine.md](qa/q80-coroutine.md) |

## 七、安全篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 81 | 如何防止外挂？ | ✅ | [q81-anti-cheat.md](qa/q81-anti-cheat.md) |
| 82 | 如何防止刷物品？ | ✅ | [q82-item-duping.md](qa/q82-item-duping.md) |
| 83 | 如何防止加速挂？ | ✅ | [q83-speed-hack.md](qa/q83-speed-hack.md) |
| 84 | 如何防止内存修改？ | ✅ | [q84-memory-hack.md](qa/q84-memory-hack.md) |
| 85 | 如何防止封包伪造？ | ✅ | [q85-packet-forgery.md](qa/q85-packet-forgery.md) |
| 86 | 如何设计权限系统？ | ✅ | [q86-permission-system.md](qa/q86-permission-system.md) |
| 87 | 敏感数据如何加密传输？ | ✅ | [q87-encryption.md](qa/q87-encryption.md) |
| 88 | 如何防止 SQL 注入？ | ✅ | [q88-sql-injection.md](qa/q88-sql-injection.md) |
| 89 | 如何设计限流和防刷机制？ | ✅ | [q89-rate-limiting.md](qa/q89-rate-limiting.md) |
| 90 | 如何应对 DDoS 攻击？ | ✅ | [q90-ddos-protection.md](qa/q90-ddos-protection.md) |

## 八、运维与监控篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 91 | 如何监控服务器状态？ | ✅ | [q91-server-monitoring.md](qa/q91-server-monitoring.md) |
| 92 | 如何设计日志系统？ | ✅ | [q92-log-system.md](qa/q92-log-system.md) |
| 93 | 如何快速定位线上问题？ | ✅ | [q93-troubleshooting.md](qa/q93-troubleshooting.md) |
| 94 | 如何实现热更新？ | ✅ | [q94-hot-reload.md](qa/q94-hot-reload.md) |
| 95 | 如何实现灰度发布？ | ✅ | [q95-canary-deployment.md](qa/q95-canary-deployment.md) |
| 96 | 如何设计配置管理？ | ✅ | [q96-config-management.md](qa/q96-config-management.md) |
| 97 | 如何实现服务器重启不丢数据？ | ✅ | [q97-graceful-shutdown.md](qa/q97-graceful-shutdown.md) |
| 98 | 如何设计 GM 命令系统？ | ✅ | [q98-gm-command-system.md](qa/q98-gm-command-system.md) |
| 99 | 如何实现服务器集群管理？ | ✅ | [q99-cluster-management.md](qa/q99-cluster-management.md) |
| 100 | 如何进行容量规划？ | ✅ | [q100-capacity-planning.md](qa/q100-capacity-planning.md) |

## 九、脚本系统篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 101 | 为什么要嵌入脚本语言？Lua vs Python 如何选择？ | ✅ | [q101-script-language.md](qa/q101-script-language.md) |
| 102 | C++ 如何调用 Lua？Lua 如何调用 C++？ | ✅ | [q102-cpp-lua-binding.md](qa/q102-cpp-lua-binding.md) |
| 103 | 如何实现脚本热更新？ | ✅ | [q103-script-hot-reload.md](qa/q103-script-hot-reload.md) |
| 104 | 如何限制脚本的执行时间？ | ✅ | [q104-script-timeout.md](qa/q104-script-timeout.md) |
| 105 | 如何调试脚本？ | ✅ | [q105-script-debug.md](qa/q105-script-debug.md) |

## 十、实战经验篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 106 | 你遇到过最难的技术问题是什么？如何解决的？ | ✅ | [q106-toughest-problem.md](qa/q106-toughest-problem.md) |
| 107 | 线上出过什么严重事故？如何处理的？ | ✅ | [q107-production-incident.md](qa/q107-production-incident.md) |
| 108 | 从 0 到 1 搭建一个 MMO 服务器，你的思路是什么？ | ✅ | [q108-mmo-from-scratch.md](qa/q108-mmo-from-scratch.md) |
| 109 | 如何评估服务器承载能力？ | ✅ | [q109-capacity-assessment.md](qa/q109-capacity-assessment.md) |
| 110 | 你认为 MMORPG 服务器最难的部分是什么？ | ✅ | [q110-hardest-part.md](qa/q110-hardest-part.md) |
| 111 | 如何与客户端同学协作？ | ✅ | [q111-client-collab.md](qa/q111-client-collab.md) |
| 112 | 如何与策划同学沟通技术方案？ | ✅ | [q112-designer-comm.md](qa/q112-designer-comm.md) |
| 113 | 如何平衡代码质量和开发速度？ | ✅ | [q113-quality-vs-speed.md](qa/q113-quality-vs-speed.md) |
| 114 | 你对哪些开源游戏服务器框架有了解？各有什么特点？ | ✅ | [q114-open-source-frameworks.md](qa/q114-open-source-frameworks.md) |
| 115 | 如果让你重新设计，你会如何改进当前框架？ | ✅ | [q115-framework-redesign.md](qa/q115-framework-redesign.md) |
| 116 | C++ 设计模式在 KBEngine 中有哪些实践？为什么这样设计？ | ✅ | [q116-cpp-design-patterns-in-kbengine.md](qa/q116-cpp-design-patterns-in-kbengine.md) |

---

## 十一、深入问题篇

| # | 问题 | 状态 | 链接 |
|---|------|------|------|
| 117 | 如果你已经回答了 MMO 基础题，接下来还应该继续思考哪些深入问题？ | ✅ | [q117-interviewer-follow-up.md](qa/q117-interviewer-follow-up.md) |

---

**进度: 117/117 完成 ✅**

*共 117 题，覆盖 MMO 框架开发的核心领域*
