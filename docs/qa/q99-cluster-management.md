# Q99: 如何管理服务器集群？

## 问题分析

本题考察对集群管理的理解：
- 服务发现
- 负载均衡
- 故障转移
- KBEngine 集群架构

---

## 一、集群架构

### 1.1 KBEngine 集群拓扑

```
┌─────────────────────────────────────────────────────────────┐
│                    KBEngine 集群架构                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  [客户端]                                                    │
│     │                                                       │
│     ▼                                                       │
│  ┌─────────┐                                               │
│  │LoginApp │  登录服务器 (可多实例)                           │
│  └────┬────┘                                               │
│       │                                                     │
│       ▼                                                     │
│  ┌────────────┐                                           │
│  │BaseAppMgr  │  BaseApp 管理器 (单实例, 主节点)               │
│  └────┬───────┘                                           │
│       │                                                     │
│       ├──────────────────────┐                             │
│       ▼                      ▼                             │
│  ┌─────────┐            ┌─────────┐                        │
│  │BaseApp  │  ←─→      │BaseApp  │  基础应用服务器 (可多实例)   │
│  │  #1    │            │  #2    │                         │
│  └────┬────┘            └────┬────┘                         │
│       │                     │                              │
│       └──────────┬──────────┘                              │
│                  ▼                                          │
│          ┌──────────────┐                                  │
│          │CellAppMgr    │  CellApp 管理器 (单实例, 主节点)    │
│          └──────┬───────┘                                  │
│                 │                                           │
│     ┌───────────┼───────────┐                              │
│     ▼           ▼           ▼                              │
│  ┌───────┐  ┌───────┐  ┌───────┐                            │
│  │CellApp│  │CellApp│  │CellApp│  游戏逻辑服务器 (可多实例)   │
│  │  #1   │  │  #2   │  │  #3   │                         │
│  └───────┘  └───────┘  └───────┘                             │
│                 │                                           │
│                 ▼                                           │
│          ┌──────────────┐                                  │
│          │DBMgr         │  数据库管理器 (单实例)              │
│          └──────────────┘                                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、服务发现

### 2.1 组件注册

```python
# KBEngine 组件发现

"""
KBEngine 组件注册机制:

1. 各组件启动时向 Logger/Mgr 注册
2. Mgr 维护组件列表
3. 组件定期发送心跳
4. 心跳超时则标记为不可用
"""

class ComponentRegistry:
    """组件注册表"""

    def __init__(self):
        self.components = {
            "loginapp": [],
            "baseapp": [],
            "cellapp": [],
            "dbmgr": [],
            "baseappmgr": [],
            "cellappmgr": []
        }
        self.heartbeats = {}

    def register(self, component_type, component_id, address, port):
        """注册组件"""
        component = {
            "id": component_id,
            "type": component_type,
            "address": address,
            "port": port,
            "status": "starting",
            "load": 0,
            "register_time": time.time()
        }

        self.components[component_type].append(component)
        self.heartbeats[component_id] = time.time()

        INFO_MSG(f"Registered {component_type} {component_id} at {address}:{port}")

        return component

    def heartbeat(self, component_id):
        """心跳更新"""
        if component_id in self.heartbeats:
            self.heartbeats[component_id] = time.time()
            return True
        return False

    def get_component(self, component_type, component_id):
        """获取组件"""
        for comp in self.components[component_type]:
            if comp["id"] == component_id:
                return comp
        return None

    def get_components_by_type(self, component_type):
        """获取类型的所有组件"""
        return self.components.get(component_type, [])

    def get_healthy_components(self, component_type):
        """获取健康的组件"""
        healthy = []
        current_time = time.time()

        for comp in self.components[component_type]:
            # 检查心跳
            last_heartbeat = self.heartbeats.get(comp["id"], 0)
            if current_time - last_heartbeat < 30:  # 30秒超时
                healthy.append(comp)

        return healthy

    def mark_dead(self, component_id):
        """标记组件死亡"""
        for comp_type, comps in self.components.items():
            for comp in comps:
                if comp["id"] == component_id:
                    comp["status"] = "dead"
                    WARNING_MSG(f"Component {component_id} marked as dead")
                    return True
        return False


# KBEngine 组件发现扩展
class KBEngineComponentDiscovery:
    """KBEngine 组件发现"""

    def __init__(self):
        self.registry = ComponentRegistry()
        self.watcher_update_interval = 5  # 秒

    def start_discovery(self):
        """启动发现"""
        # 定期更新组件状态
        KBEngine.addTimer(self.watcher_update_interval, 0,
                          self.update_component_status)

    def update_component_status(self):
        """更新组件状态"""
        # 从 KBEngine Watcher 获取组件状态
        self.update_baseapps()
        self.update_cellapps()

    def update_baseapps(self):
        """更新 BaseApp 状态"""
        # 获取所有 BaseApp
        baseapps = KBEngine.getWatcher().get("components/baseapp")

        for baseapp_id, baseapp_data in baseapps.items():
            # 检查是否已注册
            comp = self.registry.get_component("baseapp", baseapp_id)

            if comp is None:
                # 新组件，注册
                self.registry.register(
                    "baseapp",
                    baseapp_id,
                    baseapp_data.get("address", "localhost"),
                    baseapp_data.get("port", 0)
                )
            else:
                # 更新状态
                comp["load"] = baseapp_data.get("load", 0)
                comp["entity_count"] = baseapp_data.get("entityCount", 0)
                self.registry.heartbeat(baseapp_id)

    def update_cellapps(self):
        """更新 CellApp 状态"""
        cellapps = KBEngine.getWatcher().get("components/cellapp")

        for cellapp_id, cellapp_data in cellapps.items():
            comp = self.registry.get_component("cellapp", cellapp_id)

            if comp is None:
                self.registry.register(
                    "cellapp",
                    cellapp_id,
                    cellapp_data.get("address", "localhost"),
                    cellapp_data.get("port", 0)
                )
            else:
                comp["load"] = cellapp_data.get("load", 0)
                comp["entity_count"] = cellapp_data.get("entityCount", 0)
                comp["space_count"] = cellapp_data.get("spaceCount", 0)
                self.registry.heartbeat(cellapp_id)
```

---

## 三、负载均衡

### 3.1 BaseApp 负载均衡

```python
# BaseApp 负载均衡

class BaseAppLoadBalancer:
    """BaseApp 负载均衡器"""

    # 负载均衡策略
    STRATEGY_ROUND_ROBIN = "round_robin"
    STRATEGY_LEAST_CONNECTIONS = "least_connections"
    STRATEGY_WEIGHTED = "weighted"
    STRATEGY_CONSISTENT_HASH = "consistent_hash"

    def __init__(self, strategy=STRATEGY_LEAST_CONNECTIONS):
        self.strategy = strategy
        self.round_robin_index = 0

    def select_baseapp(self, player_id=None):
        """选择 BaseApp"""
        baseapps = self.get_healthy_baseapps()

        if not baseapps:
            ERROR_MSG("No healthy BaseApps available")
            return None

        if self.strategy == self.STRATEGY_ROUND_ROBIN:
            return self._round_robin_select(baseapps)
        elif self.strategy == self.STRATEGY_LEAST_CONNECTIONS:
            return self._least_connections_select(baseapps)
        elif self.strategy == self.STRATEGY_WEIGHTED:
            return self._weighted_select(baseapps)
        elif self.strategy == self.STRATEGY_CONSISTENT_HASH:
            return self._consistent_hash_select(baseapps, player_id)

        return baseapps[0]

    def get_healthy_baseapps(self):
        """获取健康的 BaseApp"""
        # 从 KBEngine 获取
        baseapps = []
        baseapp_list = KBEngine.getWatcher().get("components/baseapp")

        for baseapp_id, data in baseapp_list.items():
            if data.get("state", "") == "running":
                baseapps.append({
                    "id": baseapp_id,
                    "load": data.get("load", 0),
                    "connections": data.get("connections", 0),
                    "weight": data.get("weight", 1)
                })

        return baseapps

    def _round_robin_select(self, baseapps):
        """轮询选择"""
        selected = baseapps[self.round_robin_index % len(baseapps)]
        self.round_robin_index += 1
        return selected

    def _least_connections_select(self, baseapps):
        """最少连接选择"""
        return min(baseapps, key=lambda x: x["connections"])

    def _weighted_select(self, baseapps):
        """加权选择"""
        # 计算总权重
        total_weight = sum(b["weight"] for b in baseapps)

        if total_weight == 0:
            return baseapps[0]

        # 随机选择
        import random
        rand = random.uniform(0, total_weight)

        current = 0
        for baseapp in baseapps:
            current += baseapp["weight"]
            if rand <= current:
                return baseapp

        return baseapps[-1]

    def _consistent_hash_select(self, baseapps, player_id):
        """一致性哈希选择"""
        if player_id is None:
            return baseapps[0]

        # 简单哈希
        hash_val = hash(player_id) % len(baseapps)
        return baseapps[hash_val]


# KBEngine BaseAppMgr 中的负载均衡
class BaseAppManager(KBEngine.Entity):
    """BaseApp 管理器"""

    def __init__(self):
        KBEngine.Entity.__init__(self)
        self.load_balancer = BaseAppLoadBalancer(
            BaseAppLoadBalancer.STRATEGY_LEAST_CONNECTIONS
        )
        self.baseapps = {}

    def onPlayerLogin(self, player_id):
        """玩家登录"""
        # 选择 BaseApp
        baseapp = self.load_balancer.select_baseapp(player_id)

        if baseapp is None:
            ERROR_MSG("No BaseApp available for login")
            return None

        INFO_MSG(f"Player {player_id} assigned to BaseApp {baseapp['id']}")

        # 返回 BaseApp 地址
        return {
            "address": baseapp["address"],
            "port": baseapp["port"]
        }
```

---

## 四、故障转移

### 4.1 组件故障检测

```python
# 故障检测和转移

class FailoverManager:
    """故障转移管理器"""

    def __init__(self):
        self.check_interval = 10  # 秒
        self.failure_threshold = 3  # 连续失败次数
        self.failure_counts = {}
        self.recovery_callbacks = {}

    def start_monitoring(self):
        """启动监控"""
        KBEngine.addTimer(self.check_interval, 0, self.check_all_components)

    def check_all_components(self):
        """检查所有组件"""
        self.check_baseapps()
        self.check_cellapps()
        self.check_dbmgr()

    def check_baseapps(self):
        """检查 BaseApp"""
        baseapps = KBEngine.getWatcher().get("components/baseapp")

        for baseapp_id, data in baseapps.items():
            # 检查状态
            if data.get("state") != "running":
                self.handle_failure("baseapp", baseapp_id)
            else:
                self.handle_recovery("baseapp", baseapp_id)

    def check_cellapps(self):
        """检查 CellApp"""
        cellapps = KBEngine.getWatcher().get("components/cellapp")

        for cellapp_id, data in cellapps.items():
            if data.get("state") != "running":
                self.handle_failure("cellapp", cellapp_id)
            else:
                self.handle_recovery("cellapp", cellapp_id)

    def check_dbmgr(self):
        """检查 DBMgr"""
        dbmgr = KBEngine.getWatcher().get("components/dbmgr")

        if dbmgr.get("state") != "running":
            CRITICAL_MSG("DBMgr is down!")
            # DBMgr 故障需要特殊处理
            self.handle_dbmgr_failure()

    def handle_failure(self, component_type, component_id):
        """处理组件故障"""
        key = f"{component_type}:{component_id}"

        # 增加失败计数
        self.failure_counts[key] = self.failure_counts.get(key, 0) + 1

        # 检查是否超过阈值
        if self.failure_counts[key] >= self.failure_threshold:
            ERROR_MSG(f"Component {component_type} {component_id} failed!")

            # 执行故障转移
            if component_type == "baseapp":
                self.failover_baseapp(component_id)
            elif component_type == "cellapp":
                self.failover_cellapp(component_id)

    def handle_recovery(self, component_type, component_id):
        """处理组件恢复"""
        key = f"{component_type}:{component_id}"

        if key in self.failure_counts:
            del self.failure_counts[key]
            INFO_MSG(f"Component {component_type} {component_id} recovered")

            # 执行恢复回调
            if key in self.recovery_callbacks:
                self.recovery_callbacks[key]()
                del self.recovery_callbacks[key]

    def failover_baseapp(self, failed_id):
        """BaseApp 故障转移"""
        ERROR_MSG(f"Failing over BaseApp {failed_id}")

        # 1. 获取受影响的玩家
        affected_players = self.get_baseapp_players(failed_id)

        # 2. 将玩家迁移到其他 BaseApp
        for player_id in affected_players:
            self.migrate_player(player_id, failed_id)

        # 3. 标记 BaseApp 为不可用
        self.mark_component_unavailable("baseapp", failed_id)

    def failover_cellapp(self, failed_id):
        """CellApp 故障转移"""
        ERROR_MSG(f"Failing over CellApp {failed_id}")

        # 1. 获取受影响的空间
        affected_spaces = self.get_cellapp_spaces(failed_id)

        # 2. 迁移空间到其他 CellApp
        for space_id in affected_spaces:
            self.migrate_space(space_id, failed_id)

        # 3. 标记 CellApp 为不可用
        self.mark_component_unavailable("cellapp", failed_id)

    def handle_dbmgr_failure(self):
        """DBMgr 故障处理"""
        CRITICAL_MSG("DBMgr failure detected!")

        # DBMgr 是单点，需要特殊处理
        # 1. 尝试重启 DBMgr
        # 2. 如果重启失败，切换到备用数据库
        # 3. 通知所有组件等待恢复

    def migrate_player(self, player_id, from_baseapp):
        """迁移玩家"""
        # 选择新的 BaseApp
        load_balancer = BaseAppLoadBalancer()
        new_baseapp = load_balancer.select_baseapp(player_id)

        if new_baseapp:
            # 通知玩家重连到新 BaseApp
            player = KBEngine.getEntity(player_id)
            if player and hasattr(player, 'client'):
                player.client.onBaseAppChange({
                    "new_address": new_baseapp["address"],
                    "new_port": new_baseapp["port"]
                })

            INFO_MSG(f"Player {player_id} migrated from BaseApp {from_baseapp} "
                    f"to {new_baseapp['id']}")

    def migrate_space(self, space_id, from_cellapp):
        """迁移空间"""
        # 选择新的 CellApp
        cellapps = [c for c in KBEngine.getWatcher().get("components/cellapp").items()
                   if c[0] != from_cellapp and c[1].get("state") == "running"]

        if not cellapps:
            ERROR_MSG(f"No available CellApp for space {space_id} migration")
            return

        new_cellapp_id, new_cellapp = cellapps[0]

        # 迁移空间
        INFO_MSG(f"Migrating space {space_id} from CellApp {from_cellapp} "
                f"to {new_cellapp_id}")

        # KBEngine 提供空间迁移接口
        # KBEngine.migrateSpace(space_id, new_cellapp_id)
```

---

## 五、集群配置

### 5.1 集群配置管理

```xml
<!-- KBEngine 集群配置 -->

<root>
    <!-- 机器配置 -->
    <machines>
        <machine>
            <!-- 机器 1 -->
            <ip>192.168.1.10</ip>
            <internal>
                <hostname>server-1</hostname>
            </internal>
            <external>
                <hostname>192.168.1.10</hostname>
            </external>
            <!-- 该机器上运行的组件 -->
            <components>
                <baseapp>
                    <count>2</count>
                </baseapp>
                <cellapp>
                    <count>3</count>
                </cellapp>
            </components>
        </machine>

        <machine>
            <!-- 机器 2 -->
            <ip>192.168.1.11</ip>
            <internal>
                <hostname>server-2</hostname>
            </internal>
            <external>
                <hostname>192.168.1.11</hostname>
            </external>
            <components>
                <baseapp>
                    <count>2</count>
                </baseapp>
                <cellapp>
                    <count>3</count>
                </cellapp>
            </components>
        </machine>
    </machines>

    <!-- 全局组件 -->
    <globalComponents>
        <dbmgr>
            <count>1</count>
        </dbmgr>
        <baseappmgr>
            <count>1</count>
        </baseappmgr>
        <cellappmgr>
            <count>1</count>
        </cellappmgr>
        <loginapp>
            <count>2</count>
        </loginapp>
    </globalComponents>
</root>
```

---

## 六、最佳实践

### 6.1 集群管理建议

| 实践 | 说明 |
|------|------|
| **高可用** | 关键组件多实例 |
| **故障检测** | 定期心跳检查 |
| **自动转移** | 故障自动迁移 |
| **负载均衡** | 合理的负载分配 |
| **监控告警** | 及时发现问题 |
| **容量规划** | 预留扩展空间 |

---

## 七、总结

### 集群管理核心

```
集群管理 = 服务发现 + 负载均衡 + 故障转移 + 配置管理
- KBEngine 组件自动注册
- BaseApp/CellApp 负载均衡
- 组件故障自动转移
- 分布式部署配置
```

---

## 参考资料

- [KBEngine Cluster Deployment](https://kbengine.github.io/docs/en/deployment/cluster.html)
- [Distributed Systems Patterns](https://www.amazon.com/Distributed-Systems-Patterns-Handbook-Software-Architects/dp/1119655274)
- [Consistent Hashing](https://www.tom-e-white.com/2007/11/consistent-hashing.html)
