
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q91: How do you monitor server status?

## Problem Analysis

This question examines understanding of server monitoring:
- Monitoring metrics
- Monitoring tools
- Alerting mechanisms
- KBEngine monitoring

---

## 1. Monitoring Metrics

### 1.1 System Metrics

```
┌─────────────────────────────────────────────────────────────┐
│                    Monitoring Metric Categories               │
│                    (监控指标分类)                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  System Resources (系统资源):                                 │
│  ├── CPU usage (CPU 使用率)                                   │
│  ├── Memory usage (内存使用量)                                │
│  ├── Disk I/O (磁盘 I/O)                                     │
│  ├── Network bandwidth (网络带宽)                             │
│  └── Connection count (连接数)                                │
│                                                             │
│  Process Status (进程状态):                                   │
│  ├── Process liveness (进程存活状态)                          │
│  ├── Thread count (线程数)                                    │
│  ├── Handle count (句柄数)                                    │
│  └── File descriptors (文件描述符)                            │
│                                                             │
│  Game Business (游戏业务):                                    │
│  ├── Online players (在线人数)                                │
│  ├── Request processing rate (QPS) (请求处理速率)             │
│  ├── Response time (RT) (响应时间)                            │
│  ├── Error rate (错误率)                                      │
│  └── Entity count (实体数量)                                  │
│                                                             │
│  KBEngine Specific (KBEngine 特有):                           │
│  ├── Component status (各组件状态)                            │
│  ├── Message queue length (消息队列长度)                      │
│  ├── Space usage (空间使用情况)                                │
│  └── Database connection pool (数据库连接池)                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. KBEngine Monitoring

### 2.1 KBEngine Watcher System

```python
# KBEngine Watcher 监控系统 (KBEngine Watcher monitoring system)

"""
KBEngine 内置监控机制 (KBEngine built-in monitoring mechanisms):

1. Watcher: 对象属性监控 (object property monitoring)
2. Profile: 性能统计 (performance statistics)
3. Debug Helper: 调试信息 (debug information)
"""

class KBEngineMonitor:
    """KBEngine 监控包装器 (KBEngine monitoring wrapper)"""

    @staticmethod
    def get_stats():
        """获取所有统计信息 (get all statistics)"""
        stats = {}

        # 进程信息 (process information)
        stats["process"] = KBEngine.getWatcher().get("stats/*")

        # 网络统计 (network statistics)
        stats["network"] = KBEngine.getWatcher().get("network/*")

        # 内存统计 (memory statistics)
        stats["memory"] = KBEngine.getWatcher().get("mem/*")

        # 组件状态 (component status)
        stats["components"] = KBEngine.getWatcher().get("components/*")

        return stats

    @staticmethod
    def get_entity_count():
        """获取实体数量 (get entity count)"""
        return KBEngine.getWatcher().get("entities/count")

    @staticmethod
    def get_space_info():
        """获取空间信息 (get space information)"""
        return KBEngine.getWatcher().get("spaces/*")

    @staticmethod
    def profile_snapshot():
        """性能快照 (performance snapshot)"""
        return KBEngine.profile()

class ComponentMonitor:
    """组件监控 (component monitoring)"""

    def __init__(self):
        self.components = {}

    def register_component(self, name, component):
        """注册命令处理器类 (register a component)"""
        self.components[name] = {
            "start_time": time.time(),
            "status": "running"
        }

    def check_components(self):
        """检查组件状态 (check component status)"""
        status = {}

        for name, info in self.components.items():
            # 检查心跳 (check heartbeat)
            if not self.check_heartbeat(name):
                info["status"] = "down"
                status[name] = info
            else:
                info["status"] = "up"
                status[name] = info

        return status

    def check_heartbeat(self, component_name):
        """检查组件心跳 (check component heartbeat)"""
        try:
            # 检查组件是否响应 (check whether the component responds)
            return KBEngine.getWatcher().get(f"components/{component_name}/heartbeat")
        except:
            return False

class PerformanceMonitor:
    """性能监控 (performance monitoring)"""

    def __init__(self):
        self.metrics = {}

    def collect_metrics(self):
        """收集性能指标 (collect performance metrics)"""
        # 1. 消息处理速率 (message processing rate)
        self.metrics["messages_per_second"] = self.calculate_mps()

        # 2. 实体更新时间 (entity update time)
        self.metrics["entity_update_time"] = self.calculate_entity_update_time()

        # 3. 网络延迟 (network latency)
        self.metrics["network_latency"] = self.calculate_network_latency()

        # 4. 内存使用 (memory usage)
        self.metrics["memory_usage"] = self.calculate_memory_usage()

        return self.metrics

    def calculate_mps(self):
        """计算每秒消息数 (calculate messages per second)"""
        # KBEngine 内置统计 (KBEngine built-in statistics)
        stats = KBEngine.getWatcher().get("network/messageIn")
        return stats.get("count", 0)

    def calculate_entity_update_time(self):
        """计算实体更新时间 (calculate entity update time)"""
        # 通过 profile 获取 (obtained via profile)
        profile_data = KBEngine.profile()
        return profile_data.get("entityUpdate", 0)

    def calculate_network_latency(self):
        """计算网络延迟 (calculate network latency)"""
        # 通过 round-trip time 计算 (calculated via round-trip time)
        # 需要客户端配合 (requires client cooperation)
        return 0  # 占位 (placeholder)

    def calculate_memory_usage(self):
        """计算内存使用 (calculate memory usage)"""
        mem_stats = KBEngine.getWatcher().get("mem/allocated")
        return mem_stats
```

---

## 3. Monitoring Tools

### 3.1 Prometheus Integration

```python
# Prometheus 监控导出 (Prometheus monitoring export)

from prometheus_client import Counter, Gauge, Histogram, start_http_server

class PrometheusMetrics:
    """Prometheus 监控指标 (Prometheus monitoring metrics)"""

    def __init__(self, port=8000):
        # 定义指标 (define metrics)
        self.online_players = Gauge('online_players', 'Online players')
        self.messages_total = Counter('messages_total', 'Total messages', ['direction'])
        self.message_latency = Histogram('message_latency_seconds', 'Message latency')
        self.entity_count = Gauge('entity_count', 'Entity count')

        # 启动 HTTP 服务器 (start the HTTP server)
        start_http_server(port)

    def update_online_players(self, count):
        """更新在线人数 (update online player count)"""
        self.online_players.set(count)

    def inc_messages(self, direction):
        """增加消息计数 (increment message count)"""
        self.messages_total.labels(direction=direction).inc()

    def observe_latency(self, latency):
        """观察延迟 (observe latency)"""
        self.message_latency.observe(latency)

    def update_entity_count(self, count):
        """更新实体数量 (update entity count)"""
        self.entity_count.set(count)

class KBEnginePrometheusExporter:
    """KBEngine Prometheus 导出器 (KBEngine Prometheus exporter)"""

    def __init__(self):
        self.metrics = PrometheusMetrics()
        self.last_update = 0

    def export(self):
        """导出指标到 Prometheus (export metrics to Prometheus)"""
        now = time.time()

        # 每秒更新一次 (update once per second)
        if now - self.last_update >= 1.0:
            self.update_metrics()
            self.last_update = now

    def update_metrics(self):
        """更新所有指标 (update all metrics)"""
        # 在线人数 (online players)
        online_count = len(KBEngine.entities)
        self.metrics.update_online_players(online_count)

        # 实体数量 (entity count)
        self.metrics.update_entity_count(online_count)

        # 消息统计 (需要 KBEngine 内部统计)
        # (message statistics, requires KBEngine internal statistics)
        # 这里简化处理 (simplified here)
```

---

## 4. Alerting System

### 4.1 Alert Rules

```python
# 告警系统 (alerting system)

class AlertManager:
    """告警管理器 (alert manager)"""

    def __init__(self):
        self.alert_rules = [
            AlertRule("high_cpu", "CPU > 80%", self.check_high_cpu),
            AlertRule("high_memory", "Memory > 90%", self.check_high_memory),
            AlertRule("entity_overflow", "Entities > 10000", self.check_entity_overflow),
            AlertRule("process_down", "Process not responding", self.check_process_down),
        ]
        self.alert_handlers = [
            EmailAlertHandler("admin@example.com"),
            SlackAlertHandler("#alerts"),
            SMSAlertHandler("+1234567890"),
        ]

    def check_all_rules(self):
        """检查所有告警规则 (check all alert rules)"""
        alerts = []

        for rule in self.alert_rules:
            if rule.check():
                alerts.append({
                    "rule": rule.name,
                    "message": rule.message,
                    "severity": rule.severity,
                    "timestamp": time.time()
                })

        # 发送告警 (send alerts)
        for alert in alerts:
            self.send_alert(alert)

    def check_high_cpu(self):
        """检查高 CPU 使用率 (check high CPU usage)"""
        cpu_usage = self.get_cpu_usage()
        return cpu_usage > 80.0

    def check_high_memory(self):
        """检查高内存使用 (check high memory usage)"""
        mem_usage = self.get_memory_usage()
        return mem_usage > 90.0

    def check_entity_overflow(self):
        """检查实体数量溢出 (check entity count overflow)"""
        entity_count = len(KBEngine.entities)
        return entity_count > 10000

    def check_process_down(self):
        """检查进程是否存活 (check whether the process is alive)"""
        # 检查关键文件描述符 (check key file descriptors)
        return not self.is_process_responding()

    def send_alert(self, alert):
        """发送告警 (send an alert)"""
        for handler in self.alert_handlers:
            handler.send(alert)

class AlertRule:
    """告警规则 (alert rule)"""

    def __init__(self, name, message, check_func, severity="warning"):
        self.name = name
        self.message = message
        self.check_func = check_func
        self.severity = severity

class EmailAlertHandler:
    """邮件告警处理器 (email alert handler)"""

    def __init__(self, to_address):
        self.to_address = to_address

    def send(self, alert):
        """发送邮件 (send an email)"""
        import smtplib
        from email.mime.text import MIMEText

        msg = MIMEText(f"Alert: {alert['message']}")
        msg['Subject'] = f"[{alert['severity'].upper()}] {alert['rule']}"
        msg['To'] = self.to_address

        # 发送邮件 (send the email)
        # ...
```

---

## 5. Real-Time Monitoring Dashboard

### 5.1 Web Monitoring Dashboard

```python
# Flask 监控面板 (Flask monitoring dashboard)

from flask import Flask, render_template, jsonify

app = Flask(__name__)

class MonitorDashboard:
    """监控仪表盘 (monitoring dashboard)"""

    @app.route('/')
    def dashboard():
        """主面板 (main dashboard)"""
        return render_template('dashboard.html')

    @app.route('/api/stats')
    def get_stats():
        """获取统计数据 (get statistics)"""
        monitor = KBEngineMonitor()
        return jsonify(monitor.get_stats())

    @app.route('/api/metrics')
    def get_metrics():
        """获取性能指标 (get performance metrics)"""
        perf_monitor = PerformanceMonitor()
        return jsonify(perf_monitor.collect_metrics())

    @app.route('/api/components')
    def get_component_status():
        """获取组件状态 (get component status)"""
        comp_monitor = ComponentMonitor()
        return jsonify(comp_monitor.check_components())

    @app.route('/api/topology')
    def get_topology():
        """获取拓扑结构 (get topology)"""
        return jsonify({
            "loginapp": {"status": "up", "address": "localhost:20013"},
            "baseapp1": {"status": "up", "address": "localhost:20011"},
            "baseapp2": {"status": "up", "address": "localhost:20012"},
            "cellapp1": {"status": "up", "address": "localhost:20021"},
            "dbmgr": {"status": "up", "address": "localhost:20004"},
        })

# HTML 模板 (简化) (HTML template, simplified)
DASHBOARD_HTML = """
<!DOCTYPE html>
<html>
<head>
    <title>KBEngine Monitor</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <h1>KBEngine 服务器监控 (KBEngine Server Monitoring)</h1>

    <div id="stats"></div>
    <canvas id="memoryChart"></canvas>
    <canvas id="cpuChart"></canvas>

    <script>
        // 定期更新 (periodic update)
        setInterval(function() {
            fetch('/api/stats')
            .then(r => r.json())
            .then(data => {
                document.getElementById('stats').innerText =
                    JSON.stringify(data, null, 2);
            });
        }, 5000);
    </script>
</body>
</html>
"""
```

---

## 6. Log Analysis

### 6.1 Log Monitoring

```python
# 日志分析监控 (log analysis monitoring)

class LogMonitor:
    """日志监控 (log monitoring)"""

    def __init__(self):
        self.error_patterns = [
            r"ERROR",
            r"FATAL",
            r"Exception",
            r"Failed to",
            r"Timeout",
        ]

    def analyze_logs(self):
        """分析日志 (analyze logs)"""
        # 读取最近日志 (read recent logs)
        logs = self.get_recent_logs()

        stats = {
            "error_count": 0,
            "warning_count": 0,
            "errors_by_type": {},
        }

        for log in logs:
            if any(pattern in log for pattern in self.error_patterns):
                stats["error_count"] += 1

                # 统计错误类型 (count error types)
                for pattern in self.error_patterns:
                    if pattern in log:
                        stats["errors_by_type"][pattern] = \
                            stats["errors_by_type"].get(pattern, 0) + 1

        return stats

    def get_recent_logs(self):
        """获取最近的日志 (get recent logs)"""
        # 从日志文件读取最近的日志行
        # (read recent log lines from the log file)
        log_file = "kbengine.log"

        try:
            with open(log_file, 'r') as f:
                # 读取最后 100 行 (read the last 100 lines)
                lines = f.readlines()[-100:]
                return [line.strip() for line in lines]
        except:
            return []
```

---

## 7. Best Practices

### 7.1 Monitoring Recommendations

| Practice | Description |
|------|------|
| **Layered monitoring** (分层监控) | System/process/business three layers (系统/进程/业务三层) |
| **Visualization** (可视化) | Charts showing trends (图表展示趋势) |
| **Real-time alerting** (实时告警) | Discover problems in time (及时发现问题) |
| **Log analysis** (日志分析) | Deep-dive into problems (深入分析问题) |
| **Capacity planning** (容量规划) | Forecast based on data (基于数据做预测) |

---

## 8. Summary

### Server Monitoring Core

```
Server monitoring = system metrics + business metrics + alerting mechanism + visualization
(服务器监控 = 系统指标 + 业务指标 + 告警机制 + 可视化)
- CPU/memory/network (CPU/内存/网络)
- Online players/QPS/latency (在线人数/QPS/延迟)
- KBEngine Watcher
- Prometheus + Grafana
```

---

## References

- [Prometheus Best Practices](https://prometheus.io/docs/practices/)
- [KBEngine Monitoring](https://kbengine.github.io/docs/)
- [Grafana Dashboards](https://grafana.com/docs/)
