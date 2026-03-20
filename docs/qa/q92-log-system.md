# Q92: 如何设计日志系统？

## 问题分析

本题考察对日志系统设计的理解：
- 日志分类
- 日志格式
- 日志存储
- 日志检索
- KBEngine 日志

---

## 一、日志系统架构

### 1.1 系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    日志系统架构                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  应用层 (Application):                                    │
│  ├── 业务日志 (操作、交易、战斗)                             │
│  ├── 错误日志 (异常、失败)                                   │
│  ├── 性能日志 (耗时、瓶颈)                                   │
│  └── 审计日志 (权限、安全)                                   │
│                          │                                  │
│  ▼                                                          │
│  收集层 (Collection):                                      │
│  ├── 日志库 (libfmt, spdlog)                                 │
│  ├── 格式化 (结构化)                                         │
│  ├── 上下文信息 (线程、实体、玩家)                            │
│  └── 元数据 (时间戳、级别、来源)                              │
│                          │                                  │
│  ▼                                                          │
│  缓冲层 (Buffer):                                            │
│  ├── 异步缓冲                                               │
│  ├── 批量写入                                               │
│  └── 内存映射                                               │
│                          │                                  │
│  ▼                                                          │
│  存储层 (Storage):                                          │
│  ├── 本地文件 (按大小/时间轮转)                              │
│  ├── 日志聚合 (Logstash, Fluentd)                             │
│  ├── 搜索引擎 (Elasticsearch)                                │
│  └── 长期存储 (S3, OSS)                                     │
│                          │                                  │
│  ▼                                                          │
│  分析层 (Analysis):                                         │
│  ├── 检索 (Kibana)                                           │
│  ├── 可视化 (Grafana)                                         │
│  └── 告警 (ElastAlert)                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 日志级别

```
┌─────────────────────────────────────────────────────────────┐
│                    日志级别                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  TRACE (追踪):  详细的执行流程                                │
│  ├── 用途: 开发调试                                         │
│  ├── 示例: 函数进入/退出、变量值                             │
│  └── 生产环境: 关闭                                          │
│                                                             │
│  DEBUG (调试): 调试信息                                        │
│  ├── 用途: 开发调试                                         │
│  ├── 示例: 状态变化、中间结果                                 │
│  └── 生产环境: 关闭                                          │
│                                                             │
│  INFO (信息): 一般信息                                        │
│  ├── 用途: 正常运行记录                                       │
│  ├── 示例: 玩家登录、系统启动                                 │
│  └── 生产环境: 开启                                          │
│                                                             │
│  WARN (警告): 警告信息                                        │
│  ├── 用途: 潜在问题                                           │
│  ├── 示例: 连接慢、重试                                       │
│  └── 生产环境: 开启                                          │
│                                                             │
│  ERROR (错误): 错误信息                                       │
│  ├── 用途: 错误事件                                         │
│  ├── 示例: 请求失败、异常捕获                                 │
│  └── 生产环境: 开启                                          │
│                                                             │
│  FATAL (致命): 致命错误                                       │
│  ├── 用途: 严重错误导致进程退出                                │
│  ├── 示例: 数据库连接失败、内存溢出                           │
│  └── 生产环境: 开启                                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、KBEngine 日志系统

### 2.1 KBEngine 日志宏

```python
# KBEngine 日志系统

"""
KBEngine 日志宏:

- DEBUG_MSG(): 调试信息
- INFO_MSG(): 信息日志
- WARNING_MSG(): 警告日志
- ERROR_MSG(): 错误日志
- CRITICAL_MSG(): 严重错误日志
"""

import KBEngine

class Account(KBEngine.Account):
    def __init__(self):
        KBEngine.Account.__init__(self)
        self.login_count = 0

    def onLogin(self, entityType):
        """登录回调"""
        # 记录日志
        INFO_MSG(f"Account {self.id} login as {entityType}")
        self.login_count += 1

        # 警告: 多次登录
        if self.login_count > 3:
            WARNING_MSG(f"Account {self.id} login {self.login_count} times")

    def onLogout(self):
        """登出回调"""
        INFO_MSG(f"Account {self.id} logout after {self.login_count} logins")


class Entity(KBEngine.Entity):
    def onAttack(self, attackerId):
        """被攻击"""
        INFO_MSG(f"Entity {self.id} attacked by {attackerId}")

        # 计算伤害
        damage = self.calculateDamage(attackerId)

        # 记录伤害
        DEBUG_MSG(f"Damage calculated: {damage}, HP: {self.hp}")

        if damage > self.hp:
            ERROR_MSG(f"Entity {self.id} died from {attackerId}")
```

### 2.2 KBEngine 日志配置

```python
# KBEngine 日志配置

"""
日志配置文件: kbengine_defs.xml

<root>
    <logging>
        <!-- 日志级别: DEBUG, INFO, WARNING, ERROR, CRITICAL -->
        <defaultLevel>INFO</defaultLevel>

        <!-- 日志路径 -->
        <logPath>logs/</logPath>

        <!-- 单个日志文件大小限制 (MB) -->
        <logSize>100</logSize>

        <!-- 日志文件数量 -->
        <logCount>10</logCount>

        <!-- 组件日志 -->
        <components>
            <!-- 全局日志 -->
            <default>
                <level>INFO</level>
            </default>

            <!-- DBMgr 单独日志 -->
            <DBMgr>
                <level>DEBUG</level>
            </DBMgr>

            <!-- BaseApp 单独日志 -->
            <BaseApp>
                <level>INFO</level>
            </BaseApp>

            <!-- CellApp 单独日志 -->
            <CellApp>
                <level>INFO</level>
            </CellApp>
        </components>
    </logging>
</root>
"""
```

---

## 三、结构化日志

### 3.1 JSON 格式日志

```python
# 结构化 JSON 日志

import json
import time
from datetime import datetime

class StructuredLogger:
    """结构化日志记录器"""

    def __init__(self, component):
        self.component = component

    def log(self, level, message, **context):
        """记录结构化日志"""
        log_entry = {
            "timestamp": datetime.utcnow().isoformat(),
            "level": level,
            "component": self.component,
            "message": message,
            "context": context
        }

        # 输出到文件
        self.write_log(log_entry)

    def write_log(self, log_entry):
        """写入日志文件"""
        log_file = f"logs/{self.component}.log"

        with open(log_file, 'a') as f:
            f.write(json.dumps(log_entry) + '\n')


# 使用示例
class GameLogger(StructuredLogger):
    """游戏日志记录器"""

    def __init__(self):
        super().__init__("game")

    def log_trade(self, player_a, player_b, items):
        """记录交易日志"""
        self.log("INFO", "Trade completed",
                 player_a=player_a,
                 player_b=player_b,
                 items=items,
                 trade_id=self.generate_trade_id())

    def log_combat(self, entity_id, damage, attacker_id):
        """记录战斗日志"""
        self.log("INFO", "Entity took damage",
                 entity_id=entity_id,
                 damage=damage,
                 attacker_id=attacker_id,
                 remaining_hp=self.get_entity_hp(entity_id))

    def log_chat(self, player_id, channel, message):
        """记录聊天日志"""
        self.log("INFO", "Chat message",
                 player_id=player_id,
                 channel=channel,
                 message=message,
                 timestamp=int(time.time()))
```

---

## 四、日志收集

### 4.1 Filebeat 收集

```yaml
# filebeat 配置

filebeat.inputs:
  # KBEngine 日志文件
  - type: log
    enabled: true
    paths:
      - /path/to/kbengine/logs/*.log
    fields:
      service: kbengine
      env: production
    fields_under_root: true
    multiline:
      pattern: '^['
      negate: true
      match: after

    # 解析 JSON 日志
    json.keys_under_root: true

output.elasticsearch:
  hosts: ["localhost:9200"]
  indices:
    - "kbengine-%{+yyyy.MM.dd}"
```

---

## 五、日志检索

### 5.1 Kibana 查询

```python
# KBEngine 日志查询示例

"""
Elasticsearch/Kibana 查询示例:

1. 查询所有错误日志:
   level: "ERROR"

2. 查询特定玩家的操作:
   context.player_id: "12345"

3. 查询特定时间范围:
   timestamp: [2024-01-01 TO 2024-01-31]

4. 查询包含特定关键词:
   message: *login*

5. 聚合查询:
   level: "ERROR" AND message: *database*
"""
```

---

## 六、最佳实践

### 6.1 日志设计建议

| 实践 | 说明 |
|------|------|
| **结构化** | JSON 格式便于解析 |
| **上下文** | 包含关键信息 |
| **分级记录** | 合理设置日志级别 |
| **定期轮转** | 避免单个文件过大 |
| **异步写入** | 不阻塞主线程 |

---

## 七、总结

### 日志系统核心

```
日志系统 = 分级分类 + 结构化格式 + 异步写入 + 集中存储
- 合理的日志级别
- JSON 结构化
- 文件轮转
- Elasticsearch 存储
```

---

## 参考资料

- [KBEngine Logging](https://kbengine.github.io/docs/)
- [ELK Stack](https://www.elastic.co/what-is/elk)
- [Python Logging](https://docs.python.org/3/library/logging.html)
