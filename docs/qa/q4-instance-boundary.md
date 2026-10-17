# Q4：副本如何创建、完成与销毁？

Apollo 使用 World 承载游戏逻辑，Instance 表示几名玩家组队创建的临时副本。World 内的 Scene 拥有实体、AOI 和 tick，Instance 负责玩法生命周期。

小队发起请求后，服务端校验成员并选择 World，创建 Instance 与临时 Scene。玩家进入后完成刷怪等目标，服务端提交幂等结算；玩家离开后释放实体、任务、脚本及场景资源，销毁副本。无人进入、全员退出、失败和超时也需明确收尾。

World、Scene、Instance 状态机已有实现，完整的小队请求、通关结算和自动销毁接线尚未完成。跨 World 玩家转移需单独验证权威交接与 Gateway 路由更新。详见[World 与 Instance 设计](../30-Instance_GameServer_Design.md)和根目录 [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md)。
