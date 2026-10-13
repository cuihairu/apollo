# TODO 任务计划（任务书 P0-P4 重排，2026-10-04）

> 重排依据：`docs/rearchitecture/architecture-review-plan.md`（40 节任务书）+ 第一阶段审计七件套。
> 术语纪律：命名优先复用 BigWorld/KBE 既有概念；新词先入 glossary 并给出对照与理由。

## 已完成（审计期 2026-10-04）

- [x] 任务书入库（ce91a739）
- [x] 第一阶段只读审计七件套落库推送（aac84a25）
- [x] 用户审定，解冻进入代码阶段（2026-10-04）

## P0 —— 对象所有权与核心模型重建

- [x] P0-1 执行上下文基础：WorldHost 补编译 + 定帧抽象 + ThreadAffinity/SceneContext 标注（fd61ff7f）
- [x] P0-2 玩家对象模型三件套：Avatar 类 + PlayerAnchor 字段 + PlayerId/EntityId 强类型分型
- [x] P0-3 Scene/Instance 模型重建：Scene 运行时容器 + Instance 八态状态机 + `world.create_scene`/`scene.create_instance`/`instance.enter`
- [x] P0-4 生命周期收口：WorldSession Leaving 可观察 + transfer 异步化与失败回滚 + 应用关闭协议
- [x] P0-5 构建基座加固：6 未编译源挂 target + 零 app 断链
- [x] P0-6 双树收敛预备：LogManager ODR 消除 + 死单例第一批
- [x] PORT-1 跨平台可移植性小批：config_value.h 补 `<cstdint>` + net_util.h setKeepAliveParams 三平台分支

## P1 —— 换幕 / 目录 / AOI / 持久化 / 恢复 / 重连

- [x] P1-1 Scene Transfer/换幕：prepare→detach→attach→state sync→resume 协议 + 失败回滚 + 断线处理
- [x] P1-2 PlayerDirectory/在线目录：modules/game/session PlayerDirectory + 契约 900-905 + 行为契约单测 7 组
- [x] P1-3 AOI 收敛与接入 Scene：SceneAoi 单实现 + ViewerState 水位 + test_game 9 条迁移（15/10 全绿）
- [x] P1-4 持久化栈收敛+最小落盘：五套→一套 + SaveQueue 真落盘 + PersistJournal 骨架 + 写盘→重启→读回
- [x] P1-5 Recovery：三档清单 + RecoveryCoordinator 启动序列 + journal replay + recovery_tests 5 组
- [x] P1-6 Reconnect/保活窗口：契约 906-908 + WorldSession 挂机窗口 + gateway 结构化断连（819cd25e；ON 30/30、OFF 27/27）
- [x] 全量文档对账：README/docs 全站 vs HEAD 真实现

## P2 —— Battle / ECS / Social / Guild / Party

- [x] P2-1 ECS 收敛：五套→场景内一套（2bbae2a3）
- [x] P2-2 Battle Runtime：determinism 四约束 + battle tick 接入 + reward 单向口（battle_runtime_tests 6 + rng_substream_tests 6）
- [x] P2-3 契约系统修订：entities.xml 继承链修文 + 12 条新协议消息 + golden check（15→27）
- [x] P2-4 Social/Guild/Party：Anchor 长期态字段 + Guild/Party 对象 + 契约 18-27（37 msgs；ON/OFF 33/33）

## P3 —— Deployment / Performance / Observability / DevEx

- [x] P3-1 部署与进程形态：baseappmgr 域目录跨进程化 + machined 监督面 + G-1 全链 + instance offload 协议契约 + 改名批（a5750eda）+ 回改批
- [x] P3-2 性能：三模型 benchmark 桩 + 受理面实测 ~0.1µs/意图 + flush 段 + persist 段（934f5df2 / fd46aaf9 / 97b84a54；ON 58/58、OFF 49/49、san 49/49）
- [x] P3-3 可观测性：日志收敛 + LoggerApp + L4 push 通道 + MetricRegistry + metrics admin 通道 + trace 子集 + histogram + 告警 notifier + proc 存活 + 采样装配 + BI 分流（L1-L9；ON 59/59、OFF 50/50、san 50/50）
- [x] P3-4 DevEx：dev_fleet.sh + gateway 三步处方 + net M1 内核 + ingress 接线 + M1 契约 + cell/gateway M1 + 编队成员注册（0de62ea9；ON 56/56）

## P4 —— Documentation / Examples / Benchmark / Tutorial

- [x] P4-1 文档修订：quick-start 实 API + apps 文档 11 项修正 + tests/README 重写 + README 第一屏 + docs/architecture/overview.md + ADR-001..009
- [x] P4-2 最小可运行示例：examples/full_loop_demo.cpp 进程内八步驱动（36 断言门）
- [x] P4-3 Benchmark/Tutorial：docs/guide/tutorial.md 新手上路 + 三模型基准复用
- [x] P4-4 术语承接：全仓用词核对 + nng 底座替换迁移五批全清（9121ebc2；四树 53/53·44/44·51/51·44/44）

## 遗留登记（已关 / 未列主线项，不扩写）

- 旧 9 批次计划（2026-09-29）：批次 1 契约已交付；批次 2-4 重叠者并入 P1-P4；批次 5-8（AI 寻路/脚本/SDK/监控）未列主线，保持现状。
- 审计遗留死件清账：comval 手写 union + attribute_id 290 常量已清（2026-10-09）；LocalServiceDiscovery/queue 族/IPC 旁支保持现状登记。
- G 系列能力缺口：G-1 编队/发现全交付；G-4 定时器轮库件已交付（timer_wheel_tests 12 组）；G-3 停机序列代码面已交付；G-5 检测原语已交付；G-6 契约继承生成期展开已交付；G-2 备份容灾前置设计已立（ADR-010 案 A 先行，standby 不立项，批 0 已交付）。
- 契约差异清欠：entities.xml Player parent 冲突已随 P2-3 消账；WorldSpace 词根随 P0-3 消账；进程名随 P3-1；nng/modules·protocol 已归档（M1 自研内核）。

## 推进纪律（任务书 §37/§40 + improvement-plan §7）

1. 每批完成即重跑四门禁（Build / Unit / Integration / Examples），全绿才提交推送；CI 触发跑一轮。
2. 小步提交推送：每批一个提交，仅含本批文件；先删后建（收敛）与先建后删（立核）批次分开。
3. 术语纪律：命名优先复用 BigWorld/KBE 既有概念；新词先入 glossary 并给出对照与理由。
4. 批内文件范围以 improvement-plan.md 文件级清单为准，超范围改动即上报协调。
