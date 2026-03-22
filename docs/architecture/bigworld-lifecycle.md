---
title: BigWorld 进程架构与玩家生命周期
icon: flow
order: 3
category:
  - 架构
  - BigWorld
tag:
  - BigWorld
  - 进程架构
  - 玩家生命周期
  - 登录流程
  - 战斗流程
---

# BigWorld 进程架构与玩家生命周期

## 目录
- [BigWorld 进程架构图](#bigworld-进程架构图)
- [进程间通信关系](#进程间通信关系)
- [玩家登录流程](#玩家登录流程)
- [玩家游戏流程](#玩家游戏流程)
- [玩家对战流程](#玩家对战流程)
- [玩家聊天流程](#玩家聊天流程)
- [玩家下线流程](#玩家下线流程)

---

## BigWorld 进程架构图

### 整体架构

先看 BigWorld / KBEngine 核心语义版本：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              客户端 (Client)                                 │
│                    Unity/Unreal/WebGL + 网络层                               │
└────────────────────────────┬────────────────────────────────────────────────┘
                             │ TCP/WebSocket
                             ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           LoginApp (登录服务器)                              │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────────────┐              │
│  │  认证模块  │  │ Base分配   │  │  负载均衡  │  │ 会话票据生成 │              │
│  └───────────┘  └───────────┘  └───────────┘  └────────────┘              │
│  职责: 处理登录认证、选择目标 BaseApp、发放接入票据                            │
└────────────────────────────┬────────────────────────────────────────────────┘
                             │ 返回 BaseApp 地址 + 登录票据
                             ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                   BaseApp (PlayerAnchor + Proxy 宿主)                        │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
│  │ PlayerAnchor │ │ Proxy        │ │ SessionBinding│ │ Reconnect    │       │
│  │ 在线主状态    │ │ 客户端附着点  │ │ 会话归属       │ │ 断线恢复      │       │
│  └──────────────┘ └──────┬───────┘ └──────────────┘ └──────────────┘       │
│                           │                                                  │
│                           │ 分配玩家进入目标 Cell / Space                      │
└───────────────────────────┼──────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        CellApp 游戏逻辑服务器集群                            │
│                                                                              │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐                    │
│  │  CellApp #1   │  │  CellApp #2   │  │  CellApp #N   │                    │
│  │  (主城区域)   │  │  (野外区域)   │  │  (副本区域)   │                    │
│  ├───────────────┤  ├───────────────┤  ├───────────────┤                    │
│  │ AOI系统       │  │ AOI系统       │  │ AOI系统       │                    │
│  │ 实体管理      │  │ 实体管理      │  │ 实体管理      │                    │
│  │ 战斗逻辑      │  │ 战斗逻辑      │  │ 战斗逻辑      │                    │
│  │ Witness/Ghost │  │ Witness/Ghost │  │ Witness/Ghost │                    │
│  │ 跨边界同步    │  │ 跨边界同步    │  │ 跨边界同步    │                    │
│  └───────────────┘  └───────────────┘  └───────────────┘                    │
│                                                                              │
│  职责: 空间实时权威、AOI视野管理、实体状态更新、跨区域迁移                     │
└─────────────────────────────────────────────────────────────────────────────┘
                             ▲
                             │ 数据保存/加载
                    ┌────────┴────────┐
                    │   DBMgr / 持久化层 │
                    │ MySQL + Redis    │
                    └─────────────────┘
```

Apollo 如果需要额外的边缘接入治理，可以在外层再增加独立 `GatewayApp`，形成可选扩展装配：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              客户端 (Client)                                 │
│                    Unity/Unreal/WebGL + 网络层                               │
└────────────────────────────┬────────────────────────────────────────────────┘
                             │ TCP/WebSocket
                             ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           LoginApp (登录服务器)                              │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────────────┐              │
│  │  认证模块  │  │ 入口分配   │  │  负载均衡  │  │ 会话令牌生成 │              │
│  └───────────┘  └───────────┘  └───────────┘  └────────────┘              │
│  职责: 处理登录认证、分配边缘接入入口、创建初始会话                              │
└────────────────────────────┬────────────────────────────────────────────────┘
                             │ 返回 Gateway 地址 + 会话令牌
                             ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         GatewayApp (边缘接入层)                              │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────────────┐              │
│  │ 连接管理   │  │ 消息路由   │  │  心跳检测  │  │  流量控制   │              │
│  └───────────┘  └───────────┘  └───────────┘  └────────────┘              │
│  职责: 维持客户端连接、执行 admission、转发到 BaseApp / ChatApp、做边缘防护       │
└────────┬──────────────────────────────┬─────────────────────────────────────┘
         │                              │
         │ 游戏逻辑消息                  │ 聊天消息
         ▼                              ▼
┌──────────────────────────┐  ┌──────────────────────────┐
│ BaseApp (玩家锚点/Proxy宿主)│  │    ChatApp (聊天服务器)   │
│  ┌───────────┐            │  │  ┌───────────┐           │
│  │ PlayerAnchor│          │  │  │ 频道管理   │           │
│  │ Proxy/Session│         │  │  │ 消息广播   │           │
│  │ Runtime路由  │         │  │  │ 敏感词过滤 │           │
│  │ 重连恢复     │         │  │  │ 历史记录   │           │
│  └───────────┘            │  │  └───────────┘           │
│  职责: 玩家归属、会话锚点  │  │  职责: 聊天频道、广播    │
└────────┬─────────────────┘  └──────────────────────────┘
         │
         │ 分配玩家进入世界 / 协调持久化
         ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        CellApp 游戏逻辑服务器集群                            │
│                                                                              │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐                    │
│  │  CellApp #1   │  │  CellApp #2   │  │  CellApp #N   │                    │
│  │  (主城区域)   │  │  (野外区域)   │  │  (副本区域)   │                    │
│  ├───────────────┤  ├───────────────┤  ├───────────────┤                    │
│  │ AOI系统       │  │ AOI系统       │  │ AOI系统       │                    │
│  │ 实体管理      │  │ 实体管理      │  │ 实体管理      │                    │
│  │ 战斗逻辑      │  │ 战斗逻辑      │  │ 战斗逻辑      │                    │
│  │ 技能系统      │  │ 技能系统      │  │ 技能系统      │                    │
│  │ NPC AI       │  │ NPC AI       │  │ NPC AI       │                    │
│  │ 跨边界同步    │  │ 跨边界同步    │  │ 跨边界同步    │                    │
│  └───────────────┘  └───────────────┘  └───────────────┘                    │
│                                                                              │
│  职责: 游戏核心逻辑、AOI视野管理、实体状态更新、跨区域迁移                     │
└─────────────────────────────────────────────────────────────────────────────┘
                             ▲
                             │ 数据保存/加载
                    ┌────────┴────────┐
                    │   数据库集群      │
                    │ MySQL + Redis    │
                    └─────────────────┘
```

### 进程职责总结

| 进程 | 缩写 | 核心职责 | 通信对象 |
|------|------|----------|----------|
| **LoginApp** | 登录服务器 | 认证、入口分配 | Client, BaseApp |
| **GatewayApp** | 边缘接入层 | 连接管理、消息路由、入口卸载 | Client, BaseApp, ChatApp |
| **BaseApp** | 玩家锚点宿主 | Proxy、会话归属、cell 分配 | LoginApp, GatewayApp, CellApp, DBMgr |
| **CellApp** | 游戏逻辑服务器 | 游戏逻辑、AOI、战斗 | BaseApp, 其他CellApp |
| **ChatApp** | 聊天服务器 | 聊天频道、广播 | BaseApp, GatewayApp |

这里要特别说明：

- 如果讨论 BigWorld / KBEngine 核心语义，主线应看作 `Client -> LoginApp -> BaseApp(Proxy + PlayerAnchor) -> CellApp`
- 如果讨论 Apollo 普通 MMO 或边缘接入装配，才会额外出现独立 `GatewayApp`

---

## 进程间通信关系

### 通信拓扑图

```
                    ┌─────────────┐
                    │    DBMgr    │
                    │(MySQL/Redis)│
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐        ┌─────────────┐
         ┌──────────│   BaseApp   │◄───────│  LoginApp   │
         │          └──────┬──────┘        └──────┬──────┘
         │                 │                      │
         │                 │                      │
┌────────┴────────┐   ┌────▼────┐          ┌────▼────┐
│     Client      │   │GatewayApp│          │GatewayApp│
│  (Unity/Unreal) │◄──┤  Pool    │◄─────────┤  Pool    │
└────────┬────────┘   └────┬────┘          └────┬────┘
         │                 │                      │
         │        ┌────────┴────────┐   ┌────────┴────────┐
         │        │  ┌─────┬─────┐  │   │  ┌─────┬─────┐  │
         └───────►│  │ C#1 │ C#2 │...│   │  │ C#N │...  │  │
                  │  └──┬──┴──┬──┘  │   │  └──┬──┴────┘  │
                  └─────┼─────┼─────┘   └─────┼──────────┘
                        │     │               │
                        └──┬──┴───────┬───────┘
                           │          │
                    ┌──────▼─────┐    │
                    │  ChatApp   │    │
                    └────────────┘    │
                          ▲           │
                          └───────────┘

    Legend:
    C#N = CellApp #N
    ───► 消息流向
    ◄──── 同步/返回
```

---

## 玩家登录流程

### 流程图

```
Client                    LoginApp                BaseApp                GatewayApp              CellApp
  │                          │                       │                       │                       │
  │ 1. 连接请求               │                       │                       │                       │
  ├─────────────────────────►│                       │                       │                       │
  │                          │                       │                       │                       │
  │ 2. 登录请求(账号/密码)     │                       │                       │                       │
  ├─────────────────────────►│                       │                       │                       │
  │                          │                       │                       │                       │
  │                          │ 3. 验证账号密码        │                       │                       │
  │                          ├──────────────────────►│                       │                       │
  │                          │                       │                       │                       │
  │                          │ 4. 激活玩家锚点        │                       │                       │
  │                          │◄──────────────────────┤                       │                       │
  │                          │                       │                       │                       │
  │                          │ 5. 查找最优Gateway    │                       │                       │
  │                          ├──────────────────────────────────────────────►│                       │
  │                          │                       │                       │                       │
  │                          │ 6. 返回Gateway地址    │                       │                       │
  │                          │◄──────────────────────────────────────────────┤                       │
  │                          │                       │                       │                       │
  │ 7. 返回网关地址+会话令牌   │                       │                       │                       │
  │◄─────────────────────────┤                       │                       │                       │
  │                          │                       │                       │                       │
  │ 8. 断开与LoginApp的连接   │                       │                       │                       │
  ├─────────────────────────✘│                       │                       │                       │
  │                          │                       │                       │                       │
  │ 9. 连接到GatewayApp      │                       │                       │                       │
  ├─────────────────────────────────────────────────────────────────────────►│                       │
  │                          │                       │                       │                       │
  │ 10. 进入游戏请求(令牌)    │                       │                       │                       │
  ├─────────────────────────────────────────────────────────────────────────►│                       │
  │                          │                       │                       │                       │
  │                          │ 11. 校验票据并绑定 Proxy│                      │                       │
  │                          ├──────────────────────►│                       │                       │
  │                          │                       │                       │                       │
  │                          │ 12. 分配目标CellApp   │                       │                       │
  │                          ├──────────────────────────────────────────────────────────────────────►│
  │                          │                       │                       │                       │
  │                          │ 13. 创建玩家实体       │                       │                       │
  │                          │◄──────────────────────────────────────────────────────────────────────┤
  │                          │                       │                       │                       │
  │ 15. 返回登录成功          │                       │                       │                       │
  │◄─────────────────────────────────────────────────────────────────────────┤                       │
  │                          │                       │                       │                       │
  │ 16. 开始接收游戏世界数据  │                       │                       │                       │
  │◄═════════════════════════════════════════════════════════════════════════╪═══════════════════════►│
                           │                       │                       │                       │
                           └── 登录流程完成 ─────────┘                       │                       │
```

### 代码示例

```cpp
// LoginApp 处理登录
class LoginApp {
    Response handleLogin(const LoginRequest& req) {
        // 1. 验证账号密码
        auto playerData = baseApp_->verifyAccount(req.username, req.password);
        if (!playerData) {
            return Response::Error("账号或密码错误");
        }

        // 2. 查找最优Gateway (负载最低的)
        auto gateway = gatewayManager_->selectBestGateway();

        // 3. 生成会话令牌
        std::string token = generateSessionToken(playerData->id);

        // 4. 记录会话
        sessionManager_->createSession(token, playerData->id, gateway->address());

        return LoginSuccessResponse{
            .gatewayAddress = gateway->address(),
            .gatewayPort = gateway->port(),
            .sessionToken = token
        };
    }
};

// GatewayApp 处理进入游戏
class GatewayApp {
    Response handleEnterGame(const EnterGameRequest& req, Connection* conn) {
        // 1. 验证令牌
        auto session = loginApp_->verifySession(req.token);
        if (!session) {
            return Response::Error("无效的会话令牌");
        }

        // 2. 向 BaseApp 校验票据并获取玩家路由
        auto route = baseApp_->activatePlayer(session->playerId, req.token);
        if (!route) {
            return Response::Error("激活玩家失败");
        }

        // 3. 根据 BaseApp 返回的 world 路由进入目标 CellApp
        auto cellApp = cellAppManager_->resolve(route->worldShard);

        // 4. 在CellApp中创建玩家实体
        auto entity = cellApp_->createPlayerEntity(*playerData);

        // 5. 创建Proxy (客户端连接代理)
        auto proxy = createProxy(conn, entity->id());

        // 6. 关联Proxy和CellApp
        proxy->bindToCellApp(cellApp);

        return EnterGameSuccessResponse{
            .entityId = entity->id(),
            .position = playerData->lastPosition
        };
    }
};
```

---

## 玩家游戏流程

### 游戏主循环消息流

```
Client                    GatewayApp                CellApp
  │                          │                       │
  │ 1. 移动请求               │                       │
  ├─────────────────────────►│                       │
  │                          │                       │
  │                          │ 2. 转发到CellApp       │
  │                          ├──────────────────────►│
  │                          │                       │
  │                          │                       │ 3. 验证移动合法性
  │                          │                       │ 4. 更新位置
  │                          │                       │ 5. 计算AOI变化
  │                          │                       │
  │                          │                       │ 6. 发现新玩家进入视野
  │                          │                       │    ┌──────────────────┐
  │                          │                       │    │ EntityAppear{e1} │
  │                          │◄──────────────────────┤    │ EntityAppear{e2} │
  │ 7. 周围实体出现           │                       │    └──────────────────┘
  │◄─────────────────────────┤                       │
  │                          │                       │
  │                          │                       │ 8. 广播给周围玩家
  │                          │                       │    ┌──────────────────┐
  │                          │                       │    │ EntityMove{me}   │
  │                          │                       │    └──────────────────┘
  │                          │                       │
  │                          │ 9. 转发给其他Gateway   │
  │                          ├─────────────────────────────────────────►
  │                          │                       │
  │ 10. 其他客户端看到移动    │                       │
  │◄═════════════════════════════════════════════════╪═══════════════════════╪═══════════════════
```

### CellApp 内部处理

```cpp
// CellApp 处理玩家移动
class CellApp {
    void handleMove(EntityID entityId, Position newPos) {
        Entity* entity = getEntity(entityId);
        if (!entity) return;

        Position oldPos = entity->position();

        // 1. 验证移动合法性
        if (!validateMove(entity, oldPos, newPos)) {
            return; // 非法移动，忽略
        }

        // 2. 更新位置
        entity->setPosition(newPos);

        // 3. 检查是否跨格子
        if (getGridId(oldPos) != getGridId(newPos)) {
            handleCrossGridMove(entity, oldPos, newPos);
        }

        // 4. 检查是否跨CellApp
        if (!bounds_.contains(newPos)) {
            handleCrossCellAppMove(entity, newPos);
            return; // 实体已迁移
        }

        // 5. 计算AOI变化
        auto viewChange = aoi_->calculateViewChange(oldPos, newPos);

        // 6. 通知离开视野的玩家
        for (auto* leaver : viewChange.leavers) {
            gateway_->sendToClient(leaver->clientId, EntityLeaveMsg{entityId});
        }

        // 7. 通知进入视野的玩家
        for (auto* enterer : viewChange.enterers) {
            gateway_->sendToClient(enterer->clientId, EntityAppearMsg{entityId, newPos});
        }

        // 8. 广播移动给一直在视野内的玩家
        for (auto* stayer : viewChange.stayers) {
            gateway_->sendToClient(stayer->clientId, EntityMoveMsg{entityId, oldPos, newPos});
        }
    }
};
```

---

## 玩家对战流程

### 战斗消息流

```
Client A (攻击者)         GatewayApp              CellApp              Client B (目标)
  │                          │                       │                       │
  │ 1. 释放技能请求           │                       │                       │
  ├─────────────────────────►│                       │                       │
  │                          │                       │                       │
  │                          │ 2. 转发技能请求        │                       │
  │                          ├──────────────────────►│                       │
  │                          │                       │                       │
  │                          │                       │ 3. 验证技能
  │                          │                       │ 4. 计算伤害
  │                          │                       │ 5. 扣除血量
  │                          │                       │                       │
  │                          │                       │ 6. B收到伤害事件
  │                          │                       │    ┌────────────────┐
  │                          │                       │    │ EntityDamage   │
  │                          │                       │    │ hp: 100→80     │
  │ 7. B受到伤害通知          │                       │    └────────────────┘
  │◄═════════════════════════════════════════════════╪═══════════════════════►│
  │                          │                       │                       │
  │                          │                       │ 8. 播放特效
  │                          │                       │ 9. 广播伤害结果
  │                          │                       │    ┌────────────────┐
  │                          │                       │    │ SkillHitResult │
  │ 10. 周围玩家看到伤害      │                       │    └────────────────┘
  │◄═════════════════════════════════════════════════╪═══════════════════════►│
  │                          │                       │                       │
  │                          │                       │ 11. 如果B死亡
  │                          │                       │    ┌────────────────┐
  │                          │                       │    │ EntityDeath    │
  │                          │                       │    └────────────────┘
  │ 12. 看到B死亡             │                       │                       │
  │◄═════════════════════════════════════════════════╪════════════════════════════════════════════►│
  │                          │                       │                       │
```

### 跨CellApp战斗

```
CellApp A                 CellApp B
(边界附近)                (边界附近)
  │                          │
  │                          │
  │ A区玩家攻击B区目标        │
  │                          │
  │ 1. 在A创建目标的Shadow    │
  │ ◄───────────────────────►│
  │                          │
  │ 2. 计算伤害(A)           │
  │                          │
  │ 3. 同步伤害结果到B        │
  ├─────────────────────────►│
  │                          │
  │                          │ 4. 应用伤害(B)
  │                          │
  │ 5. 同步死亡事件           │
  │◄────────────────────────►│
  │                          │
  ▼                          ▼
```

### 战斗代码示例

```cpp
// CellApp 处理技能释放
class CellApp {
    void handleCastSkill(EntityID casterId, uint32_t skillId, EntityID targetId) {
        Entity* caster = getEntity(casterId);
        Entity* target = getEntity(targetId);

        if (!caster || !target) return;

        // 1. 验证技能条件
        if (!caster->canCastSkill(skillId)) {
            return; // CD中、蓝量不足等
        }

        // 2. 验证目标在射程内
        float distance = calcDistance(caster->position(), target->position());
        if (distance > getSkillRange(skillId)) {
            return; // 超出射程
        }

        // 3. 计算伤害
        int32_t damage = calculateDamage(caster, target, skillId);

        // 4. 应用伤害
        applyDamage(caster, target, damage);

        // 5. 触发技能特效
        spawnSkillEffect(skillId, target->position());

        // 6. 广播技能释放
        aoi_->broadcastToViewers(caster, SkillCastMsg{
            .casterId = casterId,
            .skillId = skillId,
            .targetId = targetId
        });

        // 7. 广播伤害结果
        aoi_->broadcastToViewers(target, SkillHitMsg{
            .targetId = targetId,
            .damage = damage,
            .currentHp = target->hp()
        });

        // 8. 检查死亡
        if (target->hp() <= 0) {
            handleEntityDeath(target);
        }
    }

    void applyDamage(Entity* attacker, Entity* target, int32_t damage) {
        // 如果目标在其他CellApp (边界情况)
        if (target->isShadow()) {
            // 转发伤害到目标所属CellApp
            target->ownerCellApp()->receiveRemoteDamage(target->id(), damage);
        } else {
            // 本地实体，直接应用
            target->takeDamage(damage);
        }
    }
};
```

---

## 玩家聊天流程

### 聊天消息流

```
Client A                  GatewayApp                ChatApp                Client B
  │                          │                       │                       │
  │ 1. 发送聊天消息           │                       │                       │
  │   /say 你好              │                       │                       │
  ├─────────────────────────►│                       │                       │
  │                          │                       │                       │
  │                          │ 2. 验证会话            │                       │
  │                          │ 3. 检查禁言状态        │                       │
  │                          │                       │                       │
  │                          │ 4. 转发到ChatApp      │                       │
  │                          ├──────────────────────►│                       │
  │                          │                       │                       │
  │                          │                       │ 5. 敏感词过滤
  │                          │                       │ 6. 查找频道成员
  │                          │                       │                       │
  │                          │                       │ 7. 确定接收者
  │                          │                       │    ┌──────────────┐
  │                          │                       │    │ A: "你好"     │
  │ 8. 广播给频道成员          │                       │    └──────────────┘
  │◄═════════════════════════════════════════════════╪═══════════════════════►│
  │                          │                       │                       │
```

### 聊天频道类型

| 频道类型 | 范围 | ChatApp处理 | 示例 |
|---------|------|------------|------|
| **当前频道** | AOI内玩家 | 查询CellApp获取AOI内玩家 | `/say 你好` |
| **世界频道** | 全服玩家 | 查询所有在线玩家 | `/world 求组` |
| **私聊频道** | 指定玩家 | 查询目标玩家所在Gateway | `/tell Player 消息` |
| **公会频道** | 公会成员 | 查询公会成员列表 | `/guild 打BOSS` |
| **队伍频道** | 队伍成员 | 查询队伍成员 | `/party 集合` |

```cpp
// ChatApp 处理聊天
class ChatApp {
    void handleChatMessage(const ChatMessage& msg) {
        // 1. 验证发送者
        Player* sender = getPlayer(msg.senderId);
        if (!sender || sender->isMuted()) {
            return; // 玩家不存在或被禁言
        }

        // 2. 敏感词过滤
        std::string filtered = filterSensitiveWords(msg.content);

        // 3. 根据频道类型分发
        switch (msg.channel) {
            case Channel::CURRENT: {
                // 当前频道：发送给AOI内玩家
                auto viewers = cellApp_->getAOIViewers(sender->position());
                broadcastToPlayers(viewers, ChatMsg{
                    .sender = sender->name(),
                    .content = filtered,
                    .channel = Channel::CURRENT
                });
                break;
            }
            case Channel::WORLD: {
                // 世界频道：发送给全服玩家
                auto allPlayers = getAllOnlinePlayers();
                broadcastToPlayers(allPlayers, ChatMsg{
                    .sender = sender->name(),
                    .content = filtered,
                    .channel = Channel::WORLD
                });
                break;
            }
            case Channel::PRIVATE: {
                // 私聊：发送给指定玩家
                Player* receiver = getPlayerByName(msg.targetName);
                if (receiver) {
                    sendToPlayer(receiver, ChatMsg{
                        .sender = sender->name(),
                        .content = filtered,
                        .channel = Channel::PRIVATE
                    });
                }
                break;
            }
            case Channel::GUILD: {
                // 公会频道：发送给公会成员
                auto guildMembers = guildManager_->getMembers(sender->guildId());
                broadcastToPlayers(guildMembers, ChatMsg{
                    .sender = sender->name(),
                    .content = filtered,
                    .channel = Channel::GUILD
                });
                break;
            }
        }

        // 4. 记录聊天历史 (用于审计)
        chatHistory_->record(msg.senderId, msg.channel, filtered);
    }
};
```

---

## 玩家下线流程

### 正常下线

```
Client                    GatewayApp                CellApp                BaseApp
  │                          │                       │                       │
  │ 1. 请求退出游戏           │                       │                       │
  ├─────────────────────────►│                       │                       │
  │                          │                       │                       │
  │                          │ 2. 通知CellApp下线     │                       │
  │                          ├──────────────────────►│                       │
  │                          │                       │                       │
  │                          │                       │ 3. 广播玩家离开
  │                          │                       │    ┌──────────────┐
  │ 4. 周围玩家看到离开       │                       │    │ EntityLeave  │
  │◄═════════════════════════════════════════════════╪═════════════════════►│
  │                          │                       │                       │
  │                          │                       │ 5. 销毁玩家实体       │
  │                          │                       │                       │
  │                          │ 6. 通知 BaseApp 执行收尾│                      │
  │                          ├──────────────────────────────────────────────►│
  │                          │                       │                       │
  │                          │                       │                       │ 7. 协调持久化到 DBMgr
  │                          │                       │                       │   更新缓存/持久化状态
  │                          │                       │                       │
  │                          │ 8. 保存完成            │                       │
  │                          │◄──────────────────────────────────────────────┤
  │                          │                       │                       │
  │ 9. 下线成功               │                       │                       │
  │◄─────────────────────────┤                       │                       │
  │                          │                       │                       │
  │ 10. 断开连接              │                       │                       │
  ├─────────────────────────✘│                       │                       │
```

### 异常掉线处理

```
Client                    GatewayApp                CellApp                BaseApp
  │                          │                       │                       │
  │ 1. 网络断开/超时         │                       │                       │
  ├─────────────────────────✘│                       │                       │
  │                          │                       │                       │
  │                          │ 2. 检测到连接断开      │                       │
  │                          │    (心跳超时)          │                       │
  │                          │                       │                       │
  │                          │ 3. 启动安全下线流程     │                       │
  │                          │    (保留实体N秒)       │                       │
  │                          ├──────────────────────►│                       │
  │                          │                       │                       │
  │                          │                       │ 4. 玩家进入"掉线保护"  │
  │                          │                       │    - 无法移动          │
  │                          │                       │    - 无敌状态          │
  │                          │                       │                       │
  │ 5a. 如果30秒内重连        │                       │                       │
  ├─────────────────────────►│                       │                       │
  │                          │ 6. 验证快速重连        │                       │
  │                          │ 7. 恢复连接            │                       │
  │                          │                       │                       │
  │                          │                       │ 8. 取消掉线保护       │
  │ ◄═════════════════════════════════════════════════╪═══════════════════════►│
  │                          │                       │                       │
  │                          │                       │                       │
  │ 5b. 如果30秒后未重连      │                       │                       │
  │                          │                       │ 9. 执行正常下线流程    │
  │                          │                       │                       │
```

### 下线代码示例

```cpp
// GatewayApp 处理下线
class GatewayApp {
    void handleLogout(Connection* conn) {
        Proxy* proxy = getProxyByConnection(conn);
        if (!proxy) return;

        EntityID playerId = proxy->controlledEntity();

        // 1. 通知CellApp玩家下线
        cellApp_->onPlayerLogout(playerId);

        // 2. 异步保存数据 (不阻塞断开连接)
        baseApp_->savePlayerDataAsync(playerId, [this, conn](bool success) {
            // 保存完成后的回调
            if (!success) {
                logError("保存玩家数据失败");
            }
        });

        // 3. 立即断开连接
        conn->close();
    }

    // 处理异常掉线
    void onConnectionLost(Connection* conn) {
        Proxy* proxy = getProxyByConnection(conn);
        if (!proxy) return;

        EntityID playerId = proxy->controlledEntity();

        // 1. 启动掉线保护定时器
        timer_->setTimeout(30000, [this, playerId]() {
            // 30秒后检查是否重连
            if (!isPlayerReconnected(playerId)) {
                // 未重连，执行安全下线
                performSafeLogout(playerId);
            }
        });

        // 2. 通知CellApp进入掉线保护
        cellApp_->onPlayerDisconnect(playerId);
    }
};

// CellApp 处理掉线
class CellApp {
    void onPlayerDisconnect(EntityID playerId) {
        Entity* entity = getEntity(playerId);
        if (!entity) return;

        // 1. 标记为掉线保护状态
        entity->setFlag(EntityFlag::DISCONNECTED);
        entity->setInvincible(true);  // 无敌
        entity->setMovable(false);     // 不可移动

        // 2. 广播掉线状态 (可选显示"掉线中")
        aoi_->broadcastToViewers(entity, EntityStatusMsg{
            .entityId = playerId,
            .status = EntityStatus::DISCONNECTED
        });
    }

    void onPlayerReconnect(EntityID playerId, Proxy* newProxy) {
        Entity* entity = getEntity(playerId);
        if (!entity) return;

        // 1. 取消掉线保护
        entity->clearFlag(EntityFlag::DISCONNECTED);
        entity->setInvincible(false);
        entity->setMovable(true);

        // 2. 广播重连状态
        aoi_->broadcastToViewers(entity, EntityStatusMsg{
            .entityId = playerId,
            .status = EntityStatus::ONLINE
        });

        // 3. 同步当前状态给客户端
        newProxy->sendFullGameState(entity);
    }
};
```

---

## 完整玩家生命周期图

```
     注册                      登录                      游戏中                        下线
       │                        │                         │                           │
       ▼                        ▼                         ▼                           ▼
┌─────────────┐          ┌─────────────┐          ┌─────────────┐             ┌─────────────┐
│  Client注册  │          │ 连接LoginApp│          │  在CellApp   │             │ 正常/异常    │
│  账号创建    │          │ 验证账号密码 │          │  游戏循环     │             │  下线        │
└──────┬──────┘          └──────┬──────┘          └──────┬──────┘             └──────┬──────┘
       │                        │                         │                           │
       │ 数据存入               │ 返回Gateway地址          │                           │
       ▼ Database              ▼                         ▼                           ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                 BaseApp (玩家锚点 / Proxy / 长期在线归属)                           │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐        │
│  │ 激活玩家   │  │ 绑定会话   │  │ world分配  │  │ 重连恢复   │  │ 持久化协调  │        │
│  └───────────┘  └───────────┘  └───────────┘  └───────────┘  └───────────┘        │
└─────────────────────────────────────────────────────────────────────────────────────┘
       │                        │                         │                           │
       │                        │ 连接Gateway              │                           │
       │                        │ ▼                        │                           │
       │                        │ ┌───────────────────────────────────────────────┐   │
       │                        │ │              GatewayApp (连接管理)             │   │
       │                        │ │  ┌─────────┐  ┌─────────┐  ┌─────────┐       │   │
       │                        │ │  │ 验证令牌 │  │ 消息路由 │  │ 心跳检测 │       │   │
       │                        │ │  └─────────┘  └─────────┘  └─────────┘       │   │
       │                        │ └───────────────────────────────────────────────┘   │
       │                        │                         │                           │
       │                        │ 创建实体                 │                           │
       │                        │ ▼                        │                           │
       │                        │ ┌───────────────────────────────────────────────┐   │
       │                        │ │              CellApp (游戏逻辑)                │   │
       │                        │ │  ┌─────────┐  ┌─────────┐  ┌─────────┐       │   │
       │                        │ │  │ 实体管理 │  │ AOI系统 │  │ 战斗系统 │       │   │
       │                        │ │  │ 技能系统 │  │ NPC AI  │  │ 跨区迁移 │       │   │
       │                        │ │  └─────────┘  └─────────┘  └─────────┘       │   │
       │                        │ └───────────────────────────────────────────────┘   │
       │                        │                         │                           │
       │                        │ 聊天消息                 │                           │
       │                        │ ────────────────────────┼───────                    │
       │                        │                         ▼                           │
       │                        │              ┌─────────────────────┐                │
       │                        │              │     ChatApp         │                │
       │                        │              │  ┌───────────────┐  │                │
       │                        │              │  │ 频道管理       │  │                │
       │                        │              │  │ 消息广播       │  │                │
       │                        │              │  │ 敏感词过滤     │  │                │
       │                        │              │  └───────────────┘  │                │
       │                        │              └─────────────────────┘                │
```

---

## 总结

### 进程间协作模式

| 场景 | 涉及进程 | 协作方式 |
|------|----------|----------|
| **登录** | Client → LoginApp → GatewayApp → BaseApp → CellApp | 入口认证 + 玩家激活 |
| **移动** | Client → GatewayApp → CellApp | 直接路由 |
| **战斗** | Client → GatewayApp → CellApp ↔ CellApp | 直接路由 + 跨App同步 |
| **聊天** | Client → GatewayApp → ChatApp | 直接路由 |
| **下线** | GatewayApp → CellApp → BaseApp | 会话回收 + 持久化协调 |

### 关键设计要点

1. **GatewayApp是无状态的** - 可以水平扩展，负载均衡器随意分配
2. **CellApp按空间分割** - 玩家根据位置自动路由到对应CellApp
3. **BaseApp专注玩家归属** - 长期在线状态、Proxy、会话与 world 路由都应在这一层
4. **ChatApp独立服务** - 聊天流量不影响游戏逻辑性能
5. **掉线保护机制** - 给予玩家重连窗口，提升体验

## 修正说明

这篇文档早期把 `BaseApp` 写成了数据库服务器，这个说法不准确。

更准确的语义应该是：

- `BaseApp`：玩家锚点、Proxy、长期在线归属宿主
- `DBMgr` 或持久化服务：数据库访问与持久化执行者
- `CellApp`：空间内实时权威节点

Apollo 当前仓库里的 `apps/base-app` 只是偏数据服务原型，不能把它直接等同于 BigWorld / KBEngine 语义里的 `BaseApp`。
