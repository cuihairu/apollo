[English](q92-log-system.en.md) | [中文](q92-log-system.md)

# Q92: How to Design a Log System?

## Problem Analysis

This question examines understanding of log system design:
- Log categorization
- Log format
- Log storage
- Log search
- KBEngine logging

---

## I. Log System Architecture

### 1.1 System Composition

```
┌─────────────────────────────────────────────────────────────┐
│                    Log System Architecture                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Application Layer (Application):                           │
│  ├── Business logs (operations, trades, combat)             │
│  ├── Error logs (exceptions, failures)                      │
│  ├── Performance logs (latency, bottlenecks)                │
│  └── Audit logs (permissions, security)                     │
│                          │                                  │
│  ▼                                                          │
│  Collection Layer (Collection):                             │
│  ├── Logging library (libfmt, spdlog)                       │
│  ├── Formatting (structured)                                │
│  ├── Context information (thread, entity, player)           │
│  └── Metadata (timestamp, level, source)                    │
│                          │                                  │
│  ▼                                                          │
│  Buffer Layer (Buffer):                                     │
│  ├── Async buffering                                        │
│  ├── Batch writes                                           │
│  └── Memory mapping                                         │
│                          │                                  │
│  ▼                                                          │
│  Storage Layer (Storage):                                   │
│  ├── Local files (rotation by size/time)                    │
│  ├── Log aggregation (Logstash, Fluentd)                    │
│  ├── Search engine (Elasticsearch)                          │
│  └── Long-term storage (S3, OSS)                            │
│                          │                                  │
│  ▼                                                          │
│  Analysis Layer (Analysis):                                 │
│  ├── Search (Kibana)                                        │
│  ├── Visualization (Grafana)                                │
│  └── Alerting (ElastAlert)                                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 Log Levels

```
┌─────────────────────────────────────────────────────────────┐
│                       Log Levels                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  TRACE (Trace):  detailed execution flow                     │
│  ├── Purpose: development debugging                         │
│  ├── Example: function entry/exit, variable values           │
│  └── Production: disabled                                    │
│                                                             │
│  DEBUG (Debug):  debug information                           │
│  ├── Purpose: development debugging                         │
│  ├── Example: state changes, intermediate results           │
│  └── Production: disabled                                    │
│                                                             │
│  INFO (Info):  general information                           │
│  ├── Purpose: normal operation records                      │
│  ├── Example: player login, system startup                  │
│  └── Production: enabled                                     │
│                                                             │
│  WARN (Warning):  warning information                        │
│  ├── Purpose: potential problems                             │
│  ├── Example: slow connections, retries                     │
│  └── Production: enabled                                     │
│                                                             │
│  ERROR (Error):  error information                           │
│  ├── Purpose: error events                                   │
│  ├── Example: request failures, exception catching           │
│  └── Production: enabled                                     │
│                                                             │
│  FATAL (Fatal):  fatal errors                                │
│  ├── Purpose: severe errors causing process exit            │
│  ├── Example: database connection failure, out of memory    │
│  └── Production: enabled                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## II. KBEngine Log System

### 2.1 KBEngine Log Macros

```python
# KBEngine log system (KBEngine 日志系统)

"""
KBEngine log macros (KBEngine 日志宏):

- DEBUG_MSG(): 调试信息 (debug messages)
- INFO_MSG(): 信息日志 (info logs)
- WARNING_MSG(): 警告日志 (warning logs)
- ERROR_MSG(): 错误日志 (error logs)
- CRITICAL_MSG(): 严重错误日志 (critical error logs)
"""

import KBEngine

class Account(KBEngine.Account):
    def __init__(self):
        KBEngine.Account.__init__(self)
        self.login_count = 0

    def onLogin(self, entityType):
        """Login callback (登录回调)"""
        # Record log (记录日志)
        INFO_MSG(f"Account {self.id} login as {entityType}")
        self.login_count += 1

        # Warning: multiple logins (警告: 多次登录)
        if self.login_count > 3:
            WARNING_MSG(f"Account {self.id} login {self.login_count} times")

    def onLogout(self):
        """Logout callback (登出回调)"""
        INFO_MSG(f"Account {self.id} logout after {self.login_count} logins")


class Entity(KBEngine.Entity):
    def onAttack(self, attackerId):
        """Attacked (被攻击)"""
        INFO_MSG(f"Entity {self.id} attacked by {attackerId}")

        # Calculate damage (计算伤害)
        damage = self.calculateDamage(attackerId)

        # Record damage (记录伤害)
        DEBUG_MSG(f"Damage calculated: {damage}, HP: {self.hp}")

        if damage > self.hp:
            ERROR_MSG(f"Entity {self.id} died from {attackerId}")
```

### 2.2 KBEngine Log Configuration

```python
# KBEngine log configuration (KBEngine 日志配置)

"""
Log configuration file: kbengine_defs.xml

<root>
    <logging>
        <!-- 日志级别: DEBUG, INFO, WARNING, ERROR, CRITICAL (log level) -->
        <defaultLevel>INFO</defaultLevel>

        <!-- 日志路径 (log path) -->
        <logPath>logs/</logPath>

        <!-- 单个日志文件大小限制 (MB) (single log file size limit, MB) -->
        <logSize>100</logSize>

        <!-- 日志文件数量 (log file count) -->
        <logCount>10</logCount>

        <!-- 组件日志 (component logs) -->
        <components>
            <!-- 全局日志 (global log) -->
            <default>
                <level>INFO</level>
            </default>

            <!-- DBMgr 单独日志 (DBMgr separate log) -->
            <DBMgr>
                <level>DEBUG</level>
            </DBMgr>

            <!-- BaseApp 单独日志 (BaseApp separate log) -->
            <BaseApp>
                <level>INFO</level>
            </BaseApp>

            <!-- CellApp 单独日志 (CellApp separate log) -->
            <CellApp>
                <level>INFO</level>
            </CellApp>
        </components>
    </logging>
</root>
"""
```

---

## III. Structured Logging

### 3.1 JSON Format Logs

```python
# Structured JSON logs (结构化 JSON 日志)

import json
import time
from datetime import datetime

class StructuredLogger:
    """Structured logger (结构化日志记录器)"""

    def __init__(self, component):
        self.component = component

    def log(self, level, message, **context):
        """Record a structured log (记录结构化日志)"""
        log_entry = {
            "timestamp": datetime.utcnow().isoformat(),
            "level": level,
            "component": self.component,
            "message": message,
            "context": context
        }

        # Write to file (输出到文件)
        self.write_log(log_entry)

    def write_log(self, log_entry):
        """Write the log file (写入日志文件)"""
        log_file = f"logs/{self.component}.log"

        with open(log_file, 'a') as f:
            f.write(json.dumps(log_entry) + '\n')


# Usage example (使用示例)
class GameLogger(StructuredLogger):
    """Game logger (游戏日志记录器)"""

    def __init__(self):
        super().__init__("game")

    def log_trade(self, player_a, player_b, items):
        """Record a trade log (记录交易日志)"""
        self.log("INFO", "Trade completed",
                 player_a=player_a,
                 player_b=player_b,
                 items=items,
                 trade_id=self.generate_trade_id())

    def log_combat(self, entity_id, damage, attacker_id):
        """Record a combat log (记录战斗日志)"""
        self.log("INFO", "Entity took damage",
                 entity_id=entity_id,
                 damage=damage,
                 attacker_id=attacker_id,
                 remaining_hp=self.get_entity_hp(entity_id))

    def log_chat(self, player_id, channel, message):
        """Record a chat log (记录聊天日志)"""
        self.log("INFO", "Chat message",
                 player_id=player_id,
                 channel=channel,
                 message=message,
                 timestamp=int(time.time()))
```

---

## IV. Log Collection

### 4.1 Filebeat Collection

```yaml
# filebeat configuration (filebeat 配置)

filebeat.inputs:
  # KBEngine log files (KBEngine 日志文件)
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

    # Parse JSON logs (解析 JSON 日志)
    json.keys_under_root: true

output.elasticsearch:
  hosts: ["localhost:9200"]
  indices:
    - "kbengine-%{+yyyy.MM.dd}"
```

---

## V. Log Search

### 5.1 Kibana Queries

```python
# KBEngine log query examples (KBEngine 日志查询示例)

"""
Elasticsearch/Kibana query examples (Elasticsearch/Kibana 查询示例):

1. 查询所有错误日志 (query all error logs):
   level: "ERROR"

2. 查询特定玩家的操作 (query a specific player's operations):
   context.player_id: "12345"

3. 查询特定时间范围 (query a specific time range):
   timestamp: [2024-01-01 TO 2024-01-31]

4. 查询包含特定关键词 (query containing a specific keyword):
   message: *login*

5. 聚合查询 (aggregation query):
   level: "ERROR" AND message: *database*
"""
```

---

## VI. Best Practices

### 6.1 Log Design Recommendations

| Practice | Description |
|------|------|
| **Structured** | JSON format is easy to parse |
| **Context** | Include key information |
| **Leveled logging** | Set log levels reasonably |
| **Periodic rotation** | Avoid overly large single files |
| **Async writes** | Do not block the main thread |

---

## VII. Summary

### Log System Core

```
Log system = leveled categorization + structured format + async writes + centralized storage
- Reasonable log levels
- JSON structuring
- File rotation
- Elasticsearch storage
```

---

## References

- [KBEngine Logging](https://kbengine.github.io/docs/)
- [ELK Stack](https://www.elastic.co/what-is/elk)
- [Python Logging](https://docs.python.org/3/library/logging.html)
