# Q51: 如何设计 AI 系统？

## 问题分析

本题考察对游戏AI系统的理解：
- AI 架构设计
- 行为树与状态机
- 寻路与导航
- KBEngine 的 AI 实现

---

## 一、AI 系统架构

### 1.1 系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    AI 系统架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  感知层 (Perception):                                      │
│  ├── 视觉检测 (Visual) - 范围内玩家检测                      │
│  ├── 听觉检测 (Auditory) - 声音来源检测                      │
│  ├── 记忆系统 (Memory) - 过往事件记录                        │
│  └── 感知过滤 (Filter) - 忽略不相关信息                     │
│                          │                                  │
│                          ▼                                  │
│  决策层 (Decision):                                        │
│  ├── 状态机 (FSM) - 简单行为切换                           │
│  ├── 行为树 (Behavior Tree) - 复杂决策逻辑                  │
│  ├── 效用系统 (Utility AI) - 多因素决策                     │
│  └── GOAP (目标导向) - 目标规划系统                         │
│                          │                                  │
│                          ▼                                  │
│  行动层 (Action):                                          │
│  ├── 移动 (Movement) - 移动到目标                          │
│  ├── 攻击 (Attack) - 执行攻击技能                          │
│  ├── 防御 (Defense) - 格挡、闪避                            │
│  ├── 逃跑 (Flee) - 撤离战斗                                │
│  ├── 巡逻 (Patrol) - 按路线移动                            │
│  └── 待机 (Idle) - 空闲动画                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 AI 类型

```
┌─────────────────────────────────────────────────────────────┐
│                    AI 类型分类                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 静态 AI (Static)                                       │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 固定位置巡逻                                    │       │
│  │  - 简单的反应逻辑                                  │       │
│  │  - 适合: 低级怪物、NPC                             │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  2. 反应式 AI (Reactive)                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 根据玩家行为做出反应                            │       │
│  │  - 简单的状态机                                    │       │
│  │  - 适合: 中级怪物、小BOSS                           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  3. 规划式 AI (Planning)                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 根据目标规划行动                                │       │
│  │  - 复杂的行为树                                    │       │
│  │  - 适合: 高级怪物、BOSS                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  4. 学习型 AI (Learning)                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 从玩家行为中学习                                │       │
│  │  - 动态调整策略                                    │       │
│  │  - 适合: 特殊BOSS、PVP AI                         │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、状态机 AI

### 2.1 有限状态机

```cpp
// AI 状态机

enum class AIState {
    IDLE = 0,           // 待机
    PATROL = 1,         // 巡逻
    CHASE = 2,          // 追击
    ATTACK = 3,         // 攻击
    FLEE = 4,           // 逃跑
    DEAD = 5,           // 死亡
};

// AI 控制器
class AIController {
public:
    AIController(uint64_t entityId) : entityId_(entityId) {
        state_ = AIState::IDLE;
        targetId_ = 0;
        stateTimer_ = 0;
    }

    // 更新 AI
    void update(uint32 deltaTime) {
        if (state_ == AIState::DEAD) {
            return;
        }

        stateTimer_ += deltaTime;

        switch (state_) {
            case AIState::IDLE:
                updateIdle(deltaTime);
                break;
            case AIState::PATROL:
                updatePatrol(deltaTime);
                break;
            case AIState::CHASE:
                updateChase(deltaTime);
                break;
            case AIState::ATTACK:
                updateAttack(deltaTime);
                break;
            case AIState::FLEE:
                updateFlee(deltaTime);
                break;
        }
    }

    // 感知玩家
    void perceive() {
        // 清除过期目标
        if (targetId_ != 0) {
            auto* target = getEntity(targetId_);
            if (!target || !target->isAlive() ||
                distanceTo(target) > perceptionRange_) {
                targetId_ = 0;
            }
        }

        // 寻找新目标
        if (targetId_ == 0) {
            auto enemies = getEnemiesInRange(perceptionRange_);
            if (!enemies.empty()) {
                // 选择最近的敌人
                targetId_ = enemies[0];
            }
        }
    }

    // 切换状态
    void switchState(AIState newState) {
        onStateExit(state_);
        state_ = newState;
        stateTimer_ = 0;
        onStateEnter(newState);
    }

private:
    // 待机状态
    void updateIdle(uint32 deltaTime) {
        perceive();

        if (targetId_ != 0) {
            switchState(AIState::CHASE);
            return;
        }

        // 随机进入巡逻
        if (stateTimer_ > 2000) {
            if (rand() % 100 < 30) {
                switchState(AIState::PATROL);
            }
        }
    }

    // 巡逻状态
    void updatePatrol(uint32 deltaTime) {
        perceive();

        if (targetId_ != 0) {
            switchState(AIState::CHASE);
            return;
        }

        // 沿巡逻路线移动
        if (patrolPoints_.empty()) {
            switchState(AIState::IDLE);
            return;
        }

        moveTo(patrolPoints_[currentPatrolPoint_]);

        // 到达巡逻点
        if (isAt(patrolPoints_[currentPatrolPoint_])) {
            currentPatrolPoint_ = (currentPatrolPoint_ + 1) % patrolPoints_.size();
        }
    }

    // 追击状态
    void updateChase(uint32 deltaTime) {
        if (targetId_ == 0) {
            switchState(AIState::PATROL);
            return;
        }

        auto* target = getEntity(targetId_);
        if (!target || !target->isAlive()) {
            targetId_ = 0;
            switchState(AIState::PATROL);
            return;
        }

        float dist = distanceTo(target);

        // 在攻击范围内，开始攻击
        if (dist <= attackRange_) {
            switchState(AIState::ATTACK);
            return;
        }

        // 超出感知范围，放弃追击
        if (dist > perceptionRange_ * 1.5f) {
            targetId_ = 0;
            switchState(AIState::PATROL);
            return;
        }

        // 追逐目标
        chase(target);
    }

    // 攻击状态
    void updateAttack(uint32 deltaTime) {
        if (targetId_ == 0) {
            switchState(AIState::PATROL);
            return;
        }

        auto* target = getEntity(targetId_);
        if (!target || !target->isAlive()) {
            targetId_ = 0;
            switchState(AIState::PATROL);
            return;
        }

        float dist = distanceTo(target);

        // 超出攻击范围，追击
        if (dist > attackRange_) {
            switchState(AIState::CHASE);
            return;
        }

        // 执行攻击
        if (canAttack()) {
            attack(target);
        }

        // 检查血量，是否逃跑
        auto* self = getEntity(entityId_);
        if (self->getHPRatio() < fleeThreshold_) {
            switchState(AIState::FLEE);
        }
    }

    // 逃跑状态
    void updateFlee(uint32 deltaTime) {
        // 远离威胁
        fleeFrom(targetId_);

        // 恢复一定血量后停止逃跑
        auto* self = getEntity(entityId_);
        if (self->getHPRatio() > fleeThreshold_ + 0.2f) {
            targetId_ = 0;
            switchState(AIState::PATROL);
        }
    }

    void onStateEnter(AIState state) {
        switch (state) {
            case AIState::IDLE:
                playAnimation("idle");
                break;
            case AIState::PATROL:
                playAnimation("walk");
                break;
            case AIState::CHASE:
                playAnimation("run");
                break;
            case AIState::ATTACK:
                playAnimation("combat");
                break;
            case AIState::FLEE:
                playAnimation("run");
                break;
        }
    }

    void onStateExit(AIState state) {
        // 状态退出处理
    }

    uint64_t entityId_;
    AIState state_;
    uint64_t targetId_;
    uint32_t stateTimer_;

    // 配置
    float perceptionRange_ = 15.0f;
    float attackRange_ = 3.0f;
    float fleeThreshold_ = 0.2f;  // 20%血量逃跑

    // 巡逻路线
    std::vector<Vector3> patrolPoints_;
    size_t currentPatrolPoint_ = 0;
};
```

---

## 三、行为树 AI

### 3.1 行为树节点

```cpp
// 行为树 AI

// 行为树节点类型
enum class NodeType {
    SEQUENCE,           // 顺序: 子节点依次执行，全部成功才成功
    SELECTOR,           // 选择: 子节点依次执行，一个成功就成功
    PARALLEL,           // 并行: 子节点同时执行
    CONDITION,          // 条件: 检查条件
    ACTION,             // 动作: 执行动作
    DECORATOR,          // 装饰: 修饰子节点行为
};

// 节点执行状态
enum class NodeStatus {
    SUCCESS,
    FAILURE,
    RUNNING,
};

// 行为树节点基类
class BehaviorNode {
public:
    virtual ~BehaviorNode() = default;
    virtual NodeStatus execute(AIController* controller, uint32 deltaTime) = 0;
    virtual void reset() {}

    std::vector<std::shared_ptr<BehaviorNode>> children;
};

// 复合节点 - 顺序
class SequenceNode : public BehaviorNode {
public:
    NodeStatus execute(AIController* controller, uint32 deltaTime) override {
        for (auto& child : children) {
            NodeStatus status = child->execute(controller, deltaTime);
            if (status != NodeStatus::SUCCESS) {
                return status;  // 失败或运行中返回
            }
        }
        return NodeStatus::SUCCESS;  // 全部成功
    }
};

// 复合节点 - 选择
class SelectorNode : public BehaviorNode {
public:
    NodeStatus execute(AIController* controller, uint32 deltaTime) override {
        for (auto& child : children) {
            NodeStatus status = child->execute(controller, deltaTime);
            if (status != NodeStatus::FAILURE) {
                return status;  // 成功或运行中返回
            }
        }
        return NodeStatus::FAILURE;  // 全部失败
    }
};

// 条件节点 - 检查血量
class CheckHPCondition : public BehaviorNode {
public:
    CheckHPCondition(float threshold) : threshold_(threshold) {}

    NodeStatus execute(AIController* controller, uint32 deltaTime) override {
        auto* entity = controller->getEntity();
        if (entity->getHPRatio() < threshold_) {
            return NodeStatus::SUCCESS;
        }
        return NodeStatus::FAILURE;
    }

private:
    float threshold_;
};

// 条件节点 - 检查目标
class HasTargetCondition : public BehaviorNode {
public:
    NodeStatus execute(AIController* controller, uint32 deltaTime) override {
        if (controller->getTargetId() != 0) {
            return NodeStatus::SUCCESS;
        }
        return NodeStatus::FAILURE;
    }
};

// 动作节点 - 寻找目标
class FindTargetAction : public BehaviorNode {
public:
    NodeStatus execute(AIController* controller, uint32 deltaTime) override {
        controller->perceive();
        if (controller->getTargetId() != 0) {
            return NodeStatus::SUCCESS;
        }
        return NodeStatus::FAILURE;
    }
};

// 动作节点 - 追击
class ChaseAction : public BehaviorNode {
public:
    NodeStatus execute(AIController* controller, uint32 deltaTime) override {
        uint64_t targetId = controller->getTargetId();
        if (targetId == 0) {
            return NodeStatus::FAILURE;
        }

        auto* target = getEntity(targetId);
        if (!target || !target->isAlive()) {
            controller->setTargetId(0);
            return NodeStatus::FAILURE;
        }

        controller->chase(target);
        return NodeStatus::RUNNING;
    }
};

// 动作节点 - 攻击
class AttackAction : public BehaviorNode {
public:
    NodeStatus execute(AIController* controller, uint32 deltaTime) override {
        if (!controller->canAttack()) {
            return NodeStatus::RUNNING;
        }

        uint64_t targetId = controller->getTargetId();
        auto* target = getEntity(targetId);
        if (target) {
            controller->attack(target);
            return NodeStatus::SUCCESS;
        }
        return NodeStatus::FAILURE;
    }
};

// 装饰节点 - 反转
class InverterDecorator : public BehaviorNode {
public:
    InverterDecorator(std::shared_ptr<BehaviorNode> child) {
        children.push_back(child);
    }

    NodeStatus execute(AIController* controller, uint32 deltaTime) override {
        NodeStatus status = children[0]->execute(controller, deltaTime);
        if (status == NodeStatus::SUCCESS) return NodeStatus::FAILURE;
        if (status == NodeStatus::FAILURE) return NodeStatus::SUCCESS;
        return status;
    }
};

// 构建行为树
std::shared_ptr<BehaviorNode> createBehaviorTree() {
    // 主选择器: 战斗 || 逃跑 || 巡逻
    auto root = std::make_shared<SelectorNode>();

    // 战斗行为: 检测敌人 -> 追击 -> 攻击
    auto combatSequence = std::make_shared<SequenceNode>();
    combatSequence->children.push_back(std::make_shared<FindTargetAction>());
    combatSequence->children.push_back(std::make_shared<ChaseAction>());
    combatSequence->children.push_back(std::make_shared<AttackAction>());

    // 逃跑行为: 血量低 -> 逃跑
    auto fleeSequence = std::make_shared<SequenceNode>();
    fleeSequence->children.push_back(std::make_shared<CheckHPCondition>(0.2f));
    fleeSequence->children.push_back(std::make_shared<FleeAction>());

    // 巡逻行为
    auto patrolAction = std::make_shared<PatrolAction>();

    root->children.push_back(combatSequence);
    root->children.push_back(fleeSequence);
    root->children.push_back(patrolAction);

    return root;
}
```

---

## 四、KBEngine AI 实现

### 4.1 KBEngine 怪物 AI

```python
# KBEngine 怪物 AI

# scripts/entities/monster.py
import KBEngine
from KBEDebug import *

class Monster(KBEngine.Entity):
    def __init__(self):
        KBEngine.Entity.__init__(self)

        # AI 状态
        self.state = "idle"  # idle, patrol, chase, attack, flee, dead
        self.target = None
        self.patrolPoints = []
        self.currentPatrolIndex = 0
        self.patrolTimer = 0

        # AI 参数
        self.perceptionRange = 15.0
        self.attackRange = 3.0
        self.fleeThreshold = 0.2

        # 注册 AI 更新
        self.addTimer(1.0, 1.0, 0, "onAITick")

    def onAITick(self):
        """AI 心跳"""
        if self.state == "dead":
            return

        if self.state == "idle":
            self.updateIdle()
        elif self.state == "patrol":
            self.updatePatrol()
        elif self.state == "chase":
            self.updateChase()
        elif self.state == "attack":
            self.updateAttack()
        elif self.state == "flee":
            self.updateFlee()

    def perceive(self):
        """感知周围"""
        # 清除无效目标
        if self.target:
            targetEntity = KBEngine.entities.get(self.target)
            if not targetEntity or not targetEntity.isAlive:
                self.target = None

        # 寻找新目标
        if not self.target:
            enemies = self.getEnemiesInRange(self.perceptionRange)
            if enemies:
                # 选择最近的敌人
                self.target = enemies[0].id

    def updateIdle(self):
        """待机状态"""
        self.perceive()

        if self.target:
            self.changeState("chase")
            return

        # 随机巡逻
        if rand() % 100 < 30:
            self.changeState("patrol")

    def updatePatrol(self):
        """巡逻状态"""
        self.perceive()

        if self.target:
            self.changeState("chase")
            return

        if not self.patrolPoints:
            self.changeState("idle")
            return

        # 移动到巡逻点
        targetPos = self.patrolPoints[self.currentPatrolIndex]
        self.moveTo(targetPos)

        # 到达巡逻点
        if self.position.distanceTo(targetPos) < 1.0:
            self.currentPatrolIndex = (self.currentPatrolIndex + 1) % len(self.patrolPoints)

    def updateChase(self):
        """追击状态"""
        if not self.target:
            self.changeState("patrol")
            return

        targetEntity = KBEngine.entities.get(self.target)
        if not targetEntity or not targetEntity.isAlive:
            self.target = None
            self.changeState("patrol")
            return

        distance = self.position.distanceTo(targetEntity.position)

        # 进入攻击范围
        if distance <= self.attackRange:
            self.changeState("attack")
            return

        # 超出感知范围
        if distance > self.perceptionRange * 1.5:
            self.target = None
            self.changeState("patrol")
            return

        # 追击
        self.moveTo(targetEntity.position)

    def updateAttack(self):
        """攻击状态"""
        if not self.target:
            self.changeState("patrol")
            return

        targetEntity = KBEngine.entities.get(self.target)
        if not targetEntity or not targetEntity.isAlive:
            self.target = None
            self.changeState("patrol")
            return

        distance = self.position.distanceTo(targetEntity.position)

        # 超出攻击范围
        if distance > self.attackRange:
            self.changeState("chase")
            return

        # 执行攻击
        if self.canAttack():
            self.attack(targetEntity)

        # 检查血量
        if self.HP / self.maxHP < self.fleeThreshold:
            self.changeState("flee")

    def updateFlee(self):
        """逃跑状态"""
        if self.target:
            targetEntity = KBEngine.entities.get(self.target)
            if targetEntity:
                # 远离目标
                fleeDir = self.position - targetEntity.position
                fleeDir.normalize()
                fleePos = self.position + fleeDir * 10.0
                self.moveTo(fleePos)

        # 恢复血量后停止逃跑
        if self.HP / self.maxHP > self.fleeThreshold + 0.2:
            self.target = None
            self.changeState("patrol")

    def changeState(self, newState):
        """切换状态"""
        INFO(f"{self.className} {self.id} state: {self.state} -> {newState}")
        self.state = newState

        # 播放动画
        if newState == "idle":
            self.playAnimation("idle")
        elif newState == "patrol":
            self.playAnimation("walk")
        elif newState == "chase":
            self.playAnimation("run")
        elif newState == "attack":
            self.playAnimation("combat")
        elif newState == "flee":
            self.playAnimation("run")

    def getEnemiesInRange(self, range):
        """获取范围内的敌人"""
        enemies = []
        for entityID, entity in KBEngine.entities.items():
            if entity.__class__.__name__ == "Avatar":
                if entity.isAlive and self.position.distanceTo(entity.position) <= range:
                    enemies.append(entity)
        return sorted(enemies, key=lambda e: self.position.distanceTo(e.position))

    def canAttack(self):
        """是否可以攻击"""
        # 检查攻击冷却
        return True  # 简化实现

    def attack(self, target):
        """执行攻击"""
        INFO(f"{self.id} attack {target.id}")
        # 发送攻击请求到服务器
        self.cell.callRemote("attack", target.id)

    def moveTo(self, position):
        """移动到指定位置"""
        self.cell.callRemote("moveTo", position)

    def playAnimation(self, animName):
        """播放动画"""
        self.cell.callRemote("playAnimation", animName)
```

---

## 五、最佳实践

### 5.1 AI 系统设计建议

| 实践 | 说明 |
|------|------|
| **分层架构** | 感知-决策-行动分离 |
| **状态有限** | 避免过多状态导致维护困难 |
| **性能优化** | 降低更新频率 |
| **调试工具** | 可视化 AI 状态 |
| **数据驱动** | AI 配置参数化 |

### 5.2 性能优化

```
优化策略:
1. 降低 AI 更新频率 (0.5-1秒)
2. 视锥检测代替圆形检测
3. 距离剔除 (只更新附近 AI)
4. 状态缓存避免重复计算
5. LOD (远处AI简化逻辑)
```

---

## 六、总结

### AI 系统核心

```
AI 系统 = 感知 + 决策 + 行动
- 感知获取环境信息
- 决策选择行动策略
- 行动执行具体操作
- 状态机/行为树实现逻辑
```

---

## 参考资料

- [KBEngine AI 文档](https://kbengine.github.io/docs/)
- [游戏 AI 编程](https://www.gamedev.net/)
- [行为树教程](https://www.behaviortree.com/)
