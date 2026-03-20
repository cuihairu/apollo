# Q109: 如何评估服务器承载能力？

## 问题分析

本题考察性能评估能力：
- 性能指标
- 测试方法
- 瓶颈分析
- 优化方向

---

## 一、承载指标

### 1.1 关键指标

```
┌─────────────────────────────────────────────────────────────┐
│                    服务器承载指标                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  容量指标:                                                   │
│  ├── CCU (同时在线)                                          │
│  ├── PCU (峰值在线)                                          │
│  ├── ACU (平均在线)                                          │
│  └── DAU (日活跃)                                            │
│                                                             │
│  性能指标:                                                   │
│  ├── QPS (每秒请求)                                          │
│  ├── RT (响应时间)                                           │
│  ├── 错误率                                                 │
│  └── 资源使用率                                              │
│                                                             │
│  业务指标:                                                   │
│  ├── 玩家分布                                               │
│  ├── 操作频率                                               │
│  ├── 消息大小                                               │
│  └── 复杂场景占比                                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、基准测试

### 2.1 单机压测

```python
# 压力测试工具

import asyncio
import time
import statistics
from concurrent.futures import ThreadPoolExecutor

class LoadTester:
    """负载测试器"""

    def __init__(self, server_host, server_port):
        self.host = server_host
        self.port = server_port
        self.results = []

    async def single_client(self, client_id, duration=60):
        """单个客户端"""
        start_time = time.time()
        requests = 0
        errors = 0
        latencies = []

        while time.time() - start_time < duration:
            try:
                req_start = time.time()

                # 发送请求
                await self.send_request(client_id)

                latency = (time.time() - req_start) * 1000
                latencies.append(latency)
                requests += 1

                # 休息
                await asyncio.sleep(0.1)

            except Exception as e:
                errors += 1

        return {
            'client_id': client_id,
            'requests': requests,
            'errors': errors,
            'latencies': latencies
        }

    async def send_request(self, client_id):
        """发送请求"""
        # 模拟游戏操作
        actions = [
            'move', 'attack', 'use_skill', 'chat', 'trade'
        ]
        action = actions[client_id % len(actions)]

        # 发送到服务器
        # ...

    async def run_test(self, num_clients, duration=60):
        """运行测试"""
        start_time = time.time()

        tasks = [
            self.single_client(i, duration)
            for i in range(num_clients)
        ]

        results = await asyncio.gather(*tasks)

        # 统计结果
        total_requests = sum(r['requests'] for r in results)
        total_errors = sum(r['errors'] for r in results)

        all_latencies = []
        for r in results:
            all_latencies.extend(r['latencies'])

        elapsed = time.time() - start_time

        return {
            'clients': num_clients,
            'duration': elapsed,
            'total_requests': total_requests,
            'total_errors': total_errors,
            'qps': total_requests / elapsed,
            'error_rate': total_errors / total_requests if total_requests > 0 else 0,
            'avg_latency': statistics.mean(all_latencies) if all_latencies else 0,
            'p95_latency': statistics.quantiles(all_latencies, n=20)[18] if len(all_latencies) > 20 else 0,
            'p99_latency': statistics.quantiles(all_latencies, n=100)[98] if len(all_latencies) > 100 else 0
        }


# 使用示例
async def main():
    tester = LoadTester('localhost', 9999)

    # 测试不同并发
    for clients in [100, 500, 1000, 2000, 3000]:
        print(f"\nTesting with {clients} clients...")
        result = await tester.run_test(clients, duration=30)

        print(f"QPS: {result['qps']:.2f}")
        print(f"Error Rate: {result['error_rate']*100:.2f}%")
        print(f"Avg Latency: {result['avg_latency']:.2f}ms")
        print(f"P95 Latency: {result['p95_latency']:.2f}ms")
        print(f"P99 Latency: {result['p99_latency']:.2f}ms")

        # 判断是否达到瓶颈
        if result['error_rate'] > 0.01 or result['p99_latency'] > 500:
            print(f"Bottleneck reached at {clients} clients")
            break
```

---

## 二、瓶颈分析

### 2.1 系统瓶颈

```python
# 瓶颈分析

class BottleneckAnalyzer:
    """瓶颈分析器"""

    @staticmethod
    def analyze_cpu(server_stats):
        """分析 CPU 瓶颈"""
        cpu_usage = server_stats['cpu_percent']

        if cpu_usage > 90:
            return {
                'bottleneck': 'CPU',
                'severity': 'critical',
                'recommendation': '增加 CPU 核心或优化算法'
            }
        elif cpu_usage > 70:
            return {
                'bottleneck': 'CPU',
                'severity': 'warning',
                'recommendation': '监控 CPU 使用率，准备扩容'
            }
        return None

    @staticmethod
    def analyze_memory(server_stats):
        """分析内存瓶颈"""
        memory_usage = server_stats['memory_percent']

        if memory_usage > 90:
            return {
                'bottleneck': 'Memory',
                'severity': 'critical',
                'recommendation': '增加内存或优化内存使用'
            }
        elif memory_usage > 70:
            return {
                'bottleneck': 'Memory',
                'severity': 'warning',
                'recommendation': '检查内存泄漏'
            }
        return None

    @staticmethod
    def analyze_network(server_stats):
        """分析网络瓶颈"""
        bandwidth_in = server_stats['network_in_mbps']
        bandwidth_out = server_stats['network_out_mbps']
        max_bandwidth = 1000  # 1 Gbps

        usage_in = bandwidth_in / max_bandwidth
        usage_out = bandwidth_out / max_bandwidth

        if max(usage_in, usage_out) > 0.8:
            return {
                'bottleneck': 'Network',
                'severity': 'critical',
                'recommendation': '增加带宽或优化消息大小'
            }
        return None

    @staticmethod
    def analyze_database(server_stats):
        """分析数据库瓶颈"""
        db_connections = server_stats['db_connections']
        db_query_time = server_stats['avg_query_time']

        issues = []

        if db_connections > 100:
            issues.append('连接数过高')

        if db_query_time > 100:  # 100ms
            issues.append('查询过慢')

        if issues:
            return {
                'bottleneck': 'Database',
                'severity': 'warning',
                'issues': issues,
                'recommendation': '添加索引或使用缓存'
            }
        return None
```

---

## 三、承载计算模型

### 3.1 理论计算

```python
# 承载能力计算

class CapacityCalculator:
    """承载计算器"""

    # 假设参数
    PLAYER_ACTIONS_PER_MINUTE = 60  # 每分钟操作
    AVG_MESSAGE_SIZE = 200  # 字节
    AVG_PROCESSING_TIME = 0.5  # 毫秒

    @staticmethod
    def calculate_by_cpu(cpu_cores, cpu_per_player):
        """基于 CPU 计算"""
        # 假设每个玩家占用 CPU 百分比
        max_players = int((cpu_cores * 100) / cpu_per_player * 0.8)  # 80% 使用率
        return max_players

    @staticmethod
    def calculate_by_memory(total_memory_gb, memory_per_player_mb):
        """基于内存计算"""
        total_memory_mb = total_memory_gb * 1024
        max_players = int((total_memory_mb * 0.7) / memory_per_player_mb)  # 70% 使用率
        return max_players

    @staticmethod
    def calculate_by_network(bandwidth_mbps, message_per_second):
        """基于网络计算"""
        # 带宽利用率 70%
        available_bandwidth = bandwidth_mbps * 0.7 * 1024 / 8  # KB/s

        # 每个玩家每秒消息
        player_bandwidth = message_per_second * 0.2  # 200 字节/消息

        max_players = int(available_bandwidth / player_bandwidth)
        return max_players

    @staticmethod
    def estimate_capacity(server_config):
        """估算承载能力"""
        results = {}

        # CPU 承载
        cpu_capacity = CapacityCalculator.calculate_by_cpu(
            server_config['cpu_cores'],
            server_config.get('cpu_per_player', 0.5)
        )
        results['cpu'] = cpu_capacity

        # 内存承载
        memory_capacity = CapacityCalculator.calculate_by_memory(
            server_config['memory_gb'],
            server_config.get('memory_per_player', 2)
        )
        results['memory'] = memory_capacity

        # 网络承载
        network_capacity = CapacityCalculator.calculate_by_network(
            server_config.get('bandwidth_mbps', 1000),
            server_config.get('messages_per_second', 1)
        )
        results['network'] = network_capacity

        # 取最小值
        max_capacity = min(results.values())

        return {
            'max_capacity': max_capacity,
            'bottleneck': min(results, key=results.get),
            'details': results
        }


# 使用示例
config = {
    'cpu_cores': 16,
    'memory_gb': 64,
    'bandwidth_mbps': 1000,
    'cpu_per_player': 0.3,  # 每个玩家占用 0.3% CPU
    'memory_per_player': 2,  # 每个玩家占用 2MB 内存
    'messages_per_second': 2  # 每个玩家每秒 2 条消息
}

capacity = CapacityCalculator.estimate_capacity(config)
print(f"Max capacity: {capacity['max_capacity']} players")
print(f"Bottleneck: {capacity['bottleneck']}")
```

---

## 四、扩展评估

### 4.1 水平扩展

```python
# 扩展能力评估

class ScalingEvaluator:
    """扩展评估器"""

    @staticmethod
    def evaluate_horizontal_scaling(target_ccu, single_server_ccu):
        """评估水平扩展"""
        servers_needed = (target_ccu + single_server_ccu - 1) // single_server_ccu

        return {
            'servers_needed': servers_needed,
            'cost_estimate': servers_needed * 1000,  # 假设每台 1000 元/月
            'complexity': 'medium' if servers_needed < 10 else 'high'
        }

    @staticmethod
    def evaluate_vertical_scaling(current_ccu, target_ccu):
        """评估垂直扩展"""
        scale_factor = target_ccu / current_ccu

        # 估算需要的硬件
        needed_cores = 16 * scale_factor
        needed_memory = 64 * scale_factor

        return {
            'recommended_cores': int(needed_cores),
            'recommended_memory_gb': int(needed_memory),
            'feasibility': 'possible' if scale_factor < 4 else 'limited'
        }
```

---

## 五、实际案例

### 5.1 KBEngine 承载

```
KBEngine 实测承载 (参考):

硬件: Intel Xeon E5-2680 v4, 32GB RAM, SSD

单 BaseApp:
- 玩家数: 2000-3000
- CPU: 60-80%
- 内存: 2-4GB

单 CellApp:
- 实体数: 1000-2000
- CPU: 60-80%
- 内存: 4-8GB

完整集群 (1+2+2 架构):
- CCU: 5000-8000
- CPU 总使用: 70%
- 内存总使用: 30GB
- 网络带宽: 300-500 Mbps
```

---

## 六、最佳实践

### 6.1 评估建议

| 实践 | 说明 |
|------|------|
| **基准测试** | 建立性能基准 |
| **逐步加压** | 递增测试并发 |
| **监控指标** | 全面监控资源 |
| **预留余量** | 预留 20-30% |
| **复现场景** | 模拟真实玩法 |
| **长期观察** | 稳定性测试 |

---

## 七、总结

### 承载评估核心

```
承载评估 = 基准测试 + 瓶颈分析 + 扩展规划
- 压力测试验证
- 多维度分析瓶颈
- 理论计算辅助
- 预留扩展空间
```

---

## 参考资料

- [Load Testing Best Practices](https://loadforge.com/blog/load-testing-best-practices/)
- [Game Server Capacity Planning](https://www.gamedev.net/blogs/entry/2248382-c-and-cpps-game-server-series/)
