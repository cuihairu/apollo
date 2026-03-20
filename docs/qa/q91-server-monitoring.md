# Q91: 如何监控服务器状态？

## 问题分析

本题考察对服务器监控的理解：
- 监控指标
- 监控工具
- 告警机制
- KBEngine 监控

---

## 一、监控指标

### 1.1 系统指标

```
┌─────────────────────────────────────────────────────────────┐
│                    监控指标分类                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  系统资源:                                                   │
│  ├── CPU 使用率                                             │
│  ├── 内存使用量                                             │
│  ├── 磁盘 I/O                                               │
│  ├── 网络带宽                                               │
│  └── 连接数                                                 │
│                                                             │
│  进程状态:                                                   │
│  ├── 进程存活状态                                           │
│  ├── 线程数                                                 │
│  ├── 句柄数                                                 │
│  └── 文件描述符                                             │
│                                                             │
│  游戏业务:                                                   │
│  ├── 在线人数                                               │
│  ├── 请求处理速率 (QPS)                                     │
│  ├── 响应时间 (RT)                                           │
│  ├── 错误率                                                 │
│  └── 实体数量                                               │
│                                                             │
│  KBEngine 特有:                                              │
│  ├── 各组件状态                                             │
│  ├── 消息队列长度                                           │
│  ├── 空间使用情况                                           │
│  └── 数据库连接池                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、KBEngine 监控

### 2.1 KBEngine Watcher 系统

```python
# KBEngine Watcher 监控系统

"""
KBEngine 内置监控机制:

1. Watcher: 对象属性监控
2. Profile: 性能统计
3. Debug Helper: 调试信息
"""

class KBEngineMonitor:
    """KBEngine 监控包装器"""

    @staticmethod
    def get_stats():
        """获取所有统计信息"""
        stats = {}

        # 进程信息
        stats["process"] = KBEngine.getWatcher().get("stats/*")

        # 网络统计
        stats["network"] = KBEngine.getWatcher().get("network/*")

        # 内存统计
        stats["memory"] = KBEngine.getWatcher().get("mem/*")

        # 组件状态
        stats["components"] = KBEngine.getWatcher().get("components/*")

        return stats

    @staticmethod
    def get_entity_count():
        """获取实体数量"""
        return KBEngine.getWatcher().get("entities/count")

    @staticmethod
    def get_space_info():
        """获取空间信息"""
        return KBEngine.getWatcher().get("spaces/*")

    @staticmethod
    def profile_snapshot():
        """性能快照"""
        return KBEngine.profile()


class ComponentMonitor:
    """组件监控"""

    def __init__(self):
        self.components = {}

    def register_component(self, name, component):
        """注册组件"""
        self.components[name] = {
            "start_time": time.time(),
            "status": "running"
        }

    def check_components(self):
        """检查组件状态"""
        status = {}

        for name, info in self.components.items():
            # 检查心跳
            if not self.check_heartbeat(name):
                info["status"] = "down"
                status[name] = info
            else:
                info["status"] = "up"
                status[name] = info

        return status

    def check_heartbeat(self, component_name):
        """检查组件心跳"""
        try:
            # 检查组件是否响应
            return KBEngine.getWatcher().get(f"components/{component_name}/heartbeat")
        except:
            return False


class PerformanceMonitor:
    """性能监控"""

    def __init__(self):
        self.metrics = {}

    def collect_metrics(self):
        """收集性能指标"""
        # 1. 消息处理速率
        self.metrics["messages_per_second"] = self.calculate_mps()

        # 2. 实体更新时间
        self.metrics["entity_update_time"] = self.calculate_entity_update_time()

        # 3. 网络延迟
        self.metrics["network_latency"] = self.calculate_network_latency()

        # 4. 内存使用
        self.metrics["memory_usage"] = self.calculate_memory_usage()

        return self.metrics

    def calculate_mps(self):
        """计算每秒消息数"""
        # KBEngine 内置统计
        stats = KBEngine.getWatcher().get("network/messageIn")
        return stats.get("count", 0)

    def calculate_entity_update_time(self):
        """计算实体更新时间"""
        # 通过 profile 获取
        profile_data = KBEngine.profile()
        return profile_data.get("entityUpdate", 0)

    def calculate_network_latency(self):
        """计算网络延迟"""
        # 通过 round-trip time 计算
        # 需要客户端配合
        return 0  # 占位

    def calculate_memory_usage(self):
        """计算内存使用"""
        mem_stats = KBEngine.getWatcher().get("mem/allocated")
        return mem_stats
```

---

## 三、监控工具

### 3.1 Prometheus 集成

```python
# Prometheus 监控导出

from prometheus_client import Counter, Gauge, Histogram, start_http_server

class PrometheusMetrics:
    """Prometheus 监控指标"""

    def __init__(self, port=8000):
        # 定义指标
        self.online_players = Gauge('online_players', 'Online players')
        self.messages_total = Counter('messages_total', 'Total messages', ['direction'])
        self.message_latency = Histogram('message_latency_seconds', 'Message latency')
        self.entity_count = Gauge('entity_count', 'Entity count')

        # 启动 HTTP 服务器
        start_http_server(port)

    def update_online_players(self, count):
        """更新在线人数"""
        self.online_players.set(count)

    def inc_messages(self, direction):
        """增加消息计数"""
        self.messages_total.labels(direction=direction).inc()

    def observe_latency(self, latency):
        """观察延迟"""
        self.message_latency.observe(latency)

    def update_entity_count(self, count):
        """更新实体数量"""
        self.entity_count.set(count)


class KBEnginePrometheusExporter:
    """KBEngine Prometheus 导出器"""

    def __init__(self):
        self.metrics = PrometheusMetrics()
        self.last_update = 0

    def export(self):
        """导出指标到 Prometheus"""
        now = time.time()

        # 每秒更新一次
        if now - self.last_update >= 1.0:
            self.update_metrics()
            self.last_update = now

    def update_metrics(self):
        """更新所有指标"""
        # 在线人数
        online_count = len(KBEngine.entities)
        self.metrics.update_online_players(online_count)

        # 实体数量
        self.metrics.update_entity_count(online_count)

        # 消息统计 (需要 KBEngine 内部统计)
        # 这里简化处理
```

---

## 四、告警系统

### 4.1 告警规则

```python
# 告警系统

class AlertManager:
    """告警管理器"""

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
        """检查所有告警规则"""
        alerts = []

        for rule in self.alert_rules:
            if rule.check():
                alerts.append({
                    "rule": rule.name,
                    "message": rule.message,
                    "severity": rule.severity,
                    "timestamp": time.time()
                })

        # 发送告警
        for alert in alerts:
            self.send_alert(alert)

    def check_high_cpu(self):
        """检查高 CPU 使用率"""
        cpu_usage = self.get_cpu_usage()
        return cpu_usage > 80.0

    def check_high_memory(self):
        """检查高内存使用"""
        mem_usage = self.get_memory_usage()
        return mem_usage > 90.0

    def check_entity_overflow(self):
        """检查实体数量溢出"""
        entity_count = len(KBEngine.entities)
        return entity_count > 10000

    def check_process_down(self):
        """检查进程是否存活"""
        # 检查关键文件描述符
        return not self.is_process_responding()

    def send_alert(self, alert):
        """发送告警"""
        for handler in self.alert_handlers:
            handler.send(alert)


class AlertRule:
    """告警规则"""

    def __init__(self, name, message, check_func, severity="warning"):
        self.name = name
        self.message = message
        self.check_func = check_func
        self.severity = severity


class EmailAlertHandler:
    """邮件告警处理器"""

    def __init__(self, to_address):
        self.to_address = to_address

    def send(self, alert):
        """发送邮件"""
        import smtplib
        from email.mime.text import MIMEText

        msg = MIMEText(f"Alert: {alert['message']}")
        msg['Subject'] = f"[{alert['severity'].upper()}] {alert['rule']}"
        msg['To'] = self.to_address

        # 发送邮件
        # ...
```

---

## 五、实时监控面板

### 5.1 Web 监控面板

```python
# Flask 监控面板

from flask import Flask, render_template, jsonify

app = Flask(__name__)

class MonitorDashboard:
    """监控仪表盘"""

    @app.route('/')
    def dashboard():
        """主面板"""
        return render_template('dashboard.html')

    @app.route('/api/stats')
    def get_stats():
        """获取统计数据"""
        monitor = KBEngineMonitor()
        return jsonify(monitor.get_stats())

    @app.route('/api/metrics')
    def get_metrics():
        """获取性能指标"""
        perf_monitor = PerformanceMonitor()
        return jsonify(perf_monitor.collect_metrics())

    @app.route('/api/components')
    def get_component_status():
        """获取组件状态"""
        comp_monitor = ComponentMonitor()
        return jsonify(comp_monitor.check_components())

    @app.route('/api/topology')
    def get_topology():
        """获取拓扑结构"""
        return jsonify({
            "loginapp": {"status": "up", "address": "localhost:20013"},
            "baseapp1": {"status": "up", "address": "localhost:20011"},
            "baseapp2": {"status": "up", "address": "localhost:20012"},
            "cellapp1": {"status": "up", "address": "localhost:20021"},
            "dbmgr": {"status": "up", "address": "localhost:20004"},
        })


# HTML 模板 (简化)
DASHBOARD_HTML = """
<!DOCTYPE html>
<html>
<head>
    <title>KBEngine Monitor</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <h1>KBEngine 服务器监控</h1>

    <div id="stats"></div>
    <canvas id="memoryChart"></canvas>
    <canvas id="cpuChart"></canvas>

    <script>
    // 定期更新
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

## 六、日志分析

### 6.1 日志监控

```python
# 日志分析监控

class LogMonitor:
    """日志监控"""

    def __init__(self):
        self.error_patterns = [
            r"ERROR",
            r"FATAL",
            r"Exception",
            r"Failed to",
            r"Timeout",
        ]

    def analyze_logs(self):
        """分析日志"""
        # 读取最近日志
        logs = self.get_recent_logs()

        stats = {
            "error_count": 0,
            "warning_count": 0,
            "errors_by_type": {},
        }

        for log in logs:
            if any(pattern in log for pattern in self.error_patterns):
                stats["error_count"] += 1

                # 统计错误类型
                for pattern in self.error_patterns:
                    if pattern in log:
                        stats["errors_by_type"][pattern] = \
                            stats["errors_by_type"].get(pattern, 0) + 1

        return stats

    def get_recent_logs(self):
        """获取最近的日志"""
        # 从日志文件读取最近的日志行
        log_file = "kbengine.log"

        try:
            with open(log_file, 'r') as f:
                # 读取最后 100 行
                lines = f.readlines()[-100:]
                return [line.strip() for line in lines]
        except:
            return []
```

---

## 七、最佳实践

### 7.1 监控建议

| 实践 | 说明 |
|------|------|
| **分层监控** | 系统/进程/业务三层 |
| **可视化** | 图表展示趋势 |
| **实时告警** | 及时发现问题 |
| **日志分析** | 深入分析问题 |
| **容量规划** | 基于数据做预测 |

---

## 八、总结

### 服务器监控核心

```
服务器监控 = 系统指标 + 业务指标 + 告警机制 + 可视化
- CPU/内存/网络
- 在线人数/QPS/延迟
- KBEngine Watcher
- Prometheus + Grafana
```

---

## 参考资料

- [Prometheus Best Practices](https://prometheus.io/docs/practices/)
- [KBEngine Monitoring](https://kbengine.github.io/docs/)
- [Grafana Dashboards](https://grafana.com/docs/)
