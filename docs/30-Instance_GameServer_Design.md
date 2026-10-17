# World 与 Instance 设计

Apollo 面向轻量多人在线玩法。游戏逻辑进程统一称 `World`；几名玩家组队创建、完成刷怪与结算后销毁的副本统一称 `Instance`。术语以[概念与命名](design/concept-glossary.md)为准。

## 进程与对象

```text
Client → Login（认证与凭证）
Client → Gateway（连接与路由）→ World（游戏逻辑进程）
                                ├── Scene（实体、AOI、tick）
                                └── Instance（玩法生命周期，关联 Scene）
```

一个 World 可以管理多个互相隔离的副本。Instance 与 Scene 已有代码实现；Instance 管玩法起止，Scene 管模拟状态，不为相同职责再增加类型。一个进程也可以承载持久场景，是否使用临时 Instance 由玩法决定。

## 临时副本流程

1. 小队发起玩法请求，服务端验证成员、人数、队伍和准入条件。
2. 选择有容量的 World，创建 Instance 及其临时 Scene，绑定本次玩法配置。
3. 成员进入；准备完成后启动玩法，驱动刷怪、战斗和目标判定。
4. 通关或失败后停止推进玩法，提交可去重、可恢复的结算。
5. 玩家离开或被按规则移出，拒绝迟到输入，释放实体、AOI、任务与场景资源，销毁 Instance。

对应现有状态机：`Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed`。无人进入、途中掉线、队伍解散和超时必须有明确处理；不能跳过结算边界直接丢弃有效结果。

## 玩家权威与跨服

玩家持久资料由持久化能力保存，对局实体由所在 World 中的 Scene 权威推进。跨 World 时执行源端冻结、导出、目标确认、路由切换和源端清理；任何时刻仅一个实体能处理玩家输入和权威写入。现有同进程 SceneTransfer 与跨进程权威交接须分别验收。

## 更新

协议更新修改 XML/XSD 契约并重新生成产物，新旧版本兼容与滚动发布仍需完成。仅代码更新保持协议不变：Lua 使用预备、tick 边界提交、观察与回滚原语；C++ 通过进程替换发布。

World 排空时停止分配新副本，等待现有副本完成结算和资源销毁后退出；超时强停需明确失败及恢复策略。Gateway 持有 TCP 连接，多实例不自动迁移既有连接。活动副本跨版本运行还需要状态迁移、兼容检查和失败回滚。

## 实现状态

现有 `World`、`Scene`、`Instance`、`SceneTransfer`、`BattleRuntime` 和 Lua 更新原语可复用。`world-app` 当前创建默认 Scene/Instance，尚未接通完整的小队请求、动态分配、玩法通关结算与资源回收流程；客户端下行广播仍有原型缺口。具体工作见根目录 [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md)。
