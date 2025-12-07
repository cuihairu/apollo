# 专业 MMORPG 服务器架构设计方案 (Reference Architecture)

> **版本**: 1.0
> **作者**: Antigravity (Google Deepmind)
> **适用场景**: 适用于高并发、无缝大地图或大规模 MMORPG 项目。

## 1. 核心架构设计理念

作为专业的游戏架构师，本方案基于**高并发、高可用、易扩展**的原则设计。我们特别针对用户关心的“AOI 独立”、“服务拆分”、“生命周期”和“战斗设计”进行深度展开。

### 1.1 设计哲学

1.  **关注点分离 (Separation of Concerns)**: 将“空间计算”(AOI/Physics)与“业务逻辑”(Skill/Quest)分离。
2.  **存算分离**: 逻辑服务器不直接持有持久化压力，通过 DataProxy/DBServer 层的缓存机制屏蔽底层 DB 抖动。
3.  **最终一致性**: 在非金融类逻辑中，优先保证系统的高响应速度，数据落地采用异步 Write-Back 策略。

---

## 2. 服务拆分与拓扑 (Service Topology)

为了实现真正的无缝世界和弹性伸缩，推荐将 AOI 视为独立的基础设施服务。

### 2.1 推荐的微服务拓扑

```mermaid
graph TD
    Client[客户端] --> |TCP/KCP| Gate[网关集群 (GateServer)]

    subgraph 接入层 (Stateless)
        Gate
        Login[登录认证服 (LoginServer)]
    end

    subgraph 核心逻辑层 (Stateful)
        World[世界服 (WorldServer)] --> |全局管理| Zone
        Zone[场景逻辑服 (ZoneServer)] --> |位置更新| AOI
        Zone --> |战斗计算| Battle[战斗计算服 (可选)]
    end

    subgraph 空间服务层 (Spatial)
        AOI[独立AOI服务集群 (AOI Service)]
        Collision[物理/碰撞服务 (Collision)]
    end

    subgraph 社交与辅助层
        Chat[聊天服]
        Guild[公会服]
        Match[匹配服]
    end

    subgraph 数据层 (Persistence)
        DataProxy[数据代理 (DataProxy)]
        Redis[(Redis Cluster)]
        DB[(MySQL/MongoDB)]
    end

    Gate --> |转发包| Zone
    Gate --> |转发包| Chat
    Zone --> |Save/Load| DataProxy
    DataProxy --> Redis
    DataProxy --> DB
```

### 2.2 关键服务职责详解

#### A. ZoneServer (场景逻辑服)

这是传统的 "GameServer"，但剥离了部分职责。

- **职责**: 实体管理 (Entity Mgmt)、任务系统、NPC 行为树、技能释放判定、Buff 计算。
- **特点**: 有状态，通常绑定 CPU 核心。
- **分线策略**: 如果是无缝地图，一个 Zone 负责一个矩形区域；如果是副本/分线地图，一个 Zone 负责多个 Map 实例。

#### B. AOI Service (独立视野服务 · 重点)

这就是你提到的“AOI 单独做”，在高性能架构中非常推荐。

- **职责**: 仅维护 Entity 的 `(x, y, z)` 和 `AOI Radius`。
- **数据流**:
  1.  ZoneServer 中的玩家移动 -> 发送 `UpdatePosition(uid, pos)` 给 AOI Service。
  2.  AOI Service 计算出 `EnterSet` (新看到谁) 和 `LeaveSet` (看不见谁)。
  3.  AOI Service 打包 `OnEntityEnter/OnEntityLeave` 消息 -> 推送给网关 (直接转发给 Client) 或 ZoneServer (触发逻辑)。
- **优势**:
  - **跨服视野**: 玩家站在 ZoneA 边缘能看到 ZoneB 的人，因为 AOI Service 拥有全局位置信息（或分区的全局）。
  - **计算卸载**: 复杂的十字链表或四叉树重平衡计算不占用逻辑帧时间。

---

## 3. 生命周期管理 (Lifecycle Management)

生命周期是服务器稳定性的基石，分为“节点”、“玩家”、“场景”三个维度。

### 3.1 服务器节点生命周期

一个 Server 进程从启动到销毁的标准状态机：

1.  **Starting (启动中)**: 加载配置、初始化单例、预热缓存。
2.  **Registering (注册中)**: 连接 `Etcd/Consul`，注册服务地址，但此时状态为 `Pending`，不接客。
3.  **Running (运行中)**: 标记为 `Active`，网关和服务发现开始导流。
4.  **Draining (平滑下线)**:
    - 收到 `SIGTERM` 信号。
    - 在服务中心标记状态为 `Draining`。
    - **拒绝新登录/新连接**。
    - 等待现有业务结束 (如副本打完，或数据保存完毕)。
    - 踢出剩余玩家（Save & Kick）。
5.  **Stopping (停止中)**: 销毁资源、关闭 Socket。
6.  **Stopped (已停止)**: 进程退出。

### 3.2 玩家 Session 生命周期 (Player Lifecycle)

这是最容易出 Bug 的地方，建议采用 **FSM (有限状态机)** 管理玩家对象：

- **Init**: 对象创建，内存分配。
- **Loading**:
  - 向 DataProxy 发起 `LoadData` 请求。
  - **关键**: 此时需分布式锁 (Redis Lock) 锁定该 AccountID，防止并发登录导致存盘覆盖。
- **Loaded**: 数据加载完毕，开始反序列化，初始化 Player 身上各模块 (Bag, Quest, Skill)。
- **Entering**: 通知 WorldServer/AOI Service "我上线了"，触发这就职逻辑。
- **Gaming**: 正常游戏状态。
- **Logouting**:
  - 玩家主动点击退出或断线。
  - 标记 flag，停止接收新协议包。
- **Saving**: 序列化数据，发送 `SaveData` 给 DataProxy。
  - **重试机制**: 必须确保 Save 成功，失败需无限重试或写入本地容灾文件 (WAL)。
- **Destroyed**: 内存回收，Redis Lock 解锁。

### 3.3 场景 生命周期 (Scene Lifecycle)

- **Static Scene (主城/野外)**: 跟随 Server 启动而创建，常驻内存。
- **Dynamic Scene (副本)**:
  - **Create**: 收到 WorldServer 分配请求，Pool 中拿出一个空闲 Scene。
  - **Prepare**: 加载静态地图数据 (NavMesh)，刷入第一波怪。
  - **Active**: 玩家进入，AI 线程启动。
  - **EmptyCheck**: 玩家全部离开后，启动 `KeepAlive` 计时器 (如保留 5 分钟，防玩家掉线重连)。
  - **Recycle**: 重置状态，怪物清除，放回 Pool，防止内存碎片。

---

## 4. 战斗系统设计 (Battle Design)

战斗是 MMO 的核心。为了高性能和可扩展性，建议采用 **组件化 + Pipeline** 模式。

### 4.1 战斗实体架构 (Entity Structure)

不建议用深继承 (Player -> Character -> Unit -> Object)，建议用组合 (Composition):

```cpp
class BattleEntity {
    uint64_t id;
    TransformComponent* transform; // 位置、朝向
    AttributeComponent* attr;      // 属性 (HP, ATK, Def)
    SkillComponent* skill;         // 技能管理器
    BuffComponent* buff;           // Buff 容器
    StateComponent* state;         // 状态机 (Stunned, Casting, Moving)
};
```

### 4.2 技能施法流程 (The Skill Pipeline)

将技能释放抽象为一个流式管道，每一步都可以被 Check 拦截：

1.  **Request**: 客户端请求 `CastSkill(skillID, targetID)`。
2.  **Validator**:
    - 状态检查 (是否眩晕? 是否沉默?)
    - 消耗检查 (MP 够吗? Item 够吗?)
    - CD 检查 (冷却了吗?)
    - 距离检查 (目标在射程内吗?)
3.  **Cost**: 扣除 MP，设置 CD，进入 `Casting` 状态 (若是吟唱技能)。
4.  **Action**:
    - 广播动画包 `PlayAnimation(caster, skillID)` 给 AOI。
    - 这里的**关键点**: **服务端不跑动画，但跑时间轴 (Timeline)**。
5.  **Effect Trigger** (时间轴触发):
    - 比如动画 0.5s 后产生判定框。
    - 服务端 Timer 触发 -> 进行空间查询 (AOI 或 碰撞检测) 找目标。
6.  **Settlement (结算)**:
    - `CalculateDamage(caster, target)`
    - 应用伤害 (Modify HP)。
    - 触发被动 (OnHit, OnDamage)。
7.  **Result**: 广播飘字/血条变化。

### 4.3 状态同步策略

- **属性同步**: 采用**脏标记 (Dirty Mask)** 模式。每一帧(或每 100ms) 将变化的属性 (HP, MP) 打包成一个 Protobuf 数组下发。不要改一个值发一个包。
- **位置同步**:
  - 客户端预测移动 (Client Prediction)。
  - 服务端验证 (Server Authoritative) + 插值广播。

---

## 5. 数据存储与一致性 (Persistence & Consistency)

### 5.1 数据模型设计

不要把所有数据塞进一个 Blob。建议拆表：

- `t_account`: 账号基础信息 (LastLogin, BanStatus)。
- `t_player`: 角色基础 (Name, Job, Level, Skin)。
- `t_item`: 背包数据 (独立的行，方便查询 logs)。
- `t_quest`: 任务数据 (通常 JSONBlob 存，因为太碎)。

### 5.2 缓存策略 (Look-aside + Write-back)

1.  **读取**: ZoneServer -> DataProxy -> Redis (Hit?) -> MySQL.
2.  **写入**:
    - ZoneServer 修改内存。
    - **定时回写**: 每隔 3-5 分钟，将 Dirty 数据 Dump 到 Redis，并发送 MQ 消息给 DBWriter 写入 MySQL。
    - **离线回写**: 玩家下线瞬间，**同步**写入 Redis，**异步**写入 MySQL。

### 5.3 极端情况的数据保护

- **停服存盘**: 停服时，通过“踢人”流程触发正常的 Save 逻辑。
- **Crash 保护**: 使用 **共享内存 (Shared Memory)** 存放 Player 对象。如果 ZoneServer 进程崩溃，守护进程 (Monitor) 可以重启 ZoneServer 并从共享内存恢复玩家状态，实现“无感宕机恢复”（高级技术，难度较大）。或者利用 `Redis` 的高频快照作为 Crash 恢复点。

---

## 6. 总结建议

1.  **上不上独立 AOI？**: 如果你的目标是单服 2000 人以上，且地图较大，**上**。单独的 AOI 服务能极大降低逻辑服的延迟抖动。
2.  **战斗怎么做？**: 不要迷信 ECS，除非你在写 Unity DOTS。服务器端用精简的 Component 模式足矣。重要的是**技能配表**和**Timeline**的抽象。
3.  **开发顺序**:
    1.  网关 + 登录 + 消息转发 (跑通 Ping/Pong)。
    2.  AOI/场景管理 + 玩家移动。
    3.  基础战斗 (属性 + 简单攻击)。
    4.  数据落地 + 容灾。

这份文档为高标准的架构方案，可根据团队规模进行裁剪。对于 5-10 人的开发团队，建议适当合并服务(如将 Chat, Guild 合并进 World; AOI 合并进 Zone)以降低运维复杂度。
