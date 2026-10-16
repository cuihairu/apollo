[English](q109-capacity-assessment.en.md) | [中文](q109-capacity-assessment.md)

# Q109: How Do You Assess Server Capacity?

## Problem Analysis

This question examines performance assessment ability:
- Performance metrics
- Testing methods
- Bottleneck analysis
- Optimization directions

---

## 1. Capacity Metrics

### 1.1 Key Metrics

```
┌─────────────────────────────────────────────────────────────┐
│                    Server Capacity Metrics                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Capacity metrics:                                           │
│  ├── CCU (concurrent users)                                  │
│  ├── PCU (peak concurrent users)                             │
│  ├── ACU (average concurrent users)                         │
│  └── DAU (daily active users)                                │
│                                                             │
│  Performance metrics:                                        │
│  ├── QPS (queries per second)                               │
│  ├── RT (response time)                                     │
│  ├── Error rate                                              │
│  └── Resource usage                                          │
│                                                             │
│  Business metrics:                                           │
│  ├── Player distribution                                     │
│  ├── Operation frequency                                     │
│  ├── Message size                                            │
│  └── Complex scenario proportion                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Benchmark Testing

### 2.1 Single-Server Stress Testing

```python
# 压力测试工具 (stress test tool)

import asyncio
import time
import statistics
from concurrent.futures import ThreadPoolExecutor

class LoadTester:
    """负载测试器 (load tester)"""

    def __init__(self, server_host, server_port):
        self.host = server_host
        self.port = server_port
        self.results = []

    async def single_client(self, client_id, duration=60):
        """单个客户端 (single client)"""
        start_time = time.time()
        requests = 0
        errors = 0
        latencies = []

        while time.time() - start_time < duration:
            try:
                req_start = time.time()

                # 发送请求 (send request)
                await self.send_request(client_id)

                latency = (time.time() - req_start) * 1000
                latencies.append(latency)
                requests += 1

                # 休息 (rest)
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
        """发送请求 (send request)"""
        # 模拟游戏操作 (simulate game operations)
        actions = [
            'move', 'attack', 'use_skill', 'chat', 'trade'
        ]
        action = actions[client_id % len(actions)]

        # 发送到服务器 (send to server)
        # ...

    async def run_test(self, num_clients, duration=60):
        """运行测试 (run test)"""
        start_time = time.time()

        tasks = [
            self.single_client(i, duration)
            for i in range(num_clients)
        ]

        results = await asyncio.gather(*tasks)

        # 统计结果 (collect statistics)
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


# 使用示例 (usage example)
async def main():
    tester = LoadTester('localhost', 9999)

    # 测试不同并发 (test different concurrency levels)
    for clients in [100, 500, 1000, 2000, 3000]:
        print(f"\nTesting with {clients} clients...")
        result = await tester.run_test(clients, duration=30)

        print(f"QPS: {result['qps']:.2f}")
        print(f"Error Rate: {result['error_rate']*100:.2f}%")
        print(f"Avg Latency: {result['avg_latency']:.2f}ms")
        print(f"P95 Latency: {result['p95_latency']:.2f}ms")
        print(f"P99 Latency: {result['p99_latency']:.2f}ms")

        # 判断是否达到瓶颈 (check whether the bottleneck is reached)
        if result['error_rate'] > 0.01 or result['p99_latency'] > 500:
            print(f"Bottleneck reached at {clients} clients")
            break
```

---

## 2. Bottleneck Analysis

### 2.1 System Bottlenecks

```python
# 瓶颈分析 (bottleneck analysis)

class BottleneckAnalyzer:
    """瓶颈分析器 (bottleneck analyzer)"""

    @staticmethod
    def analyze_cpu(server_stats):
        """分析 CPU 瓶颈 (analyze CPU bottleneck)"""
        cpu_usage = server_stats['cpu_percent']

        if cpu_usage > 90:
            return {
                'bottleneck': 'CPU',
                'severity': 'critical',
                'recommendation': '增加 CPU 核心或优化算法 (add CPU cores or optimize algorithms)'
            }
        elif cpu_usage > 70:
            return {
                'bottleneck': 'CPU',
                'severity': 'warning',
                'recommendation': '监控 CPU 使用率，准备扩容 (monitor CPU usage, prepare to scale out)'
            }
        return None

    @staticmethod
    def analyze_memory(server_stats):
        """分析内存瓶颈 (analyze memory bottleneck)"""
        memory_usage = server_stats['memory_percent']

        if memory_usage > 90:
            return {
                'bottleneck': 'Memory',
                'severity': 'critical',
                'recommendation': '增加内存或优化内存使用 (add memory or optimize memory usage)'
            }
        elif memory_usage > 70:
            return {
                'bottleneck': 'Memory',
                'severity': 'warning',
                'recommendation': '检查内存泄漏 (check for memory leaks)'
            }
        return None

    @staticmethod
    def analyze_network(server_stats):
        """分析网络瓶颈 (analyze network bottleneck)"""
        bandwidth_in = server_stats['network_in_mbps']
        bandwidth_out = server_stats['network_out_mbps']
        max_bandwidth = 1000  # 1 Gbps

        usage_in = bandwidth_in / max_bandwidth
        usage_out = bandwidth_out / max_bandwidth

        if max(usage_in, usage_out) > 0.8:
            return {
                'bottleneck': 'Network',
                'severity': 'critical',
                'recommendation': '增加带宽或优化消息大小 (increase bandwidth or optimize message size)'
            }
        return None

    @staticmethod
    def analyze_database(server_stats):
        """分析数据库瓶颈 (analyze database bottleneck)"""
        db_connections = server_stats['db_connections']
        db_query_time = server_stats['avg_query_time']

        issues = []

        if db_connections > 100:
            issues.append('连接数过高 (too many connections)')

        if db_query_time > 100:  # 100ms
            issues.append('查询过慢 (queries too slow)')

        if issues:
            return {
                'bottleneck': 'Database',
                'severity': 'warning',
                'issues': issues,
                'recommendation': '添加索引或使用缓存 (add indexes or use a cache)'
            }
        return None
```

---

## 3. Capacity Calculation Model

### 3.1 Theoretical Calculation

```python
# 承载能力计算 (capacity calculation)

class CapacityCalculator:
    """承载计算器 (capacity calculator)"""

    # 假设参数 (assumed parameters)
    PLAYER_ACTIONS_PER_MINUTE = 60  # 每分钟操作 (actions per minute)
    AVG_MESSAGE_SIZE = 200  # 字节 (bytes)
    AVG_PROCESSING_TIME = 0.5  # 毫秒 (milliseconds)

    @staticmethod
    def calculate_by_cpu(cpu_cores, cpu_per_player):
        """基于 CPU 计算 (CPU-based calculation)"""
        # 假设每个玩家占用 CPU 百分比 (assume CPU percentage occupied per player)
        max_players = int((cpu_cores * 100) / cpu_per_player * 0.8)  # 80% 使用率 (80% usage)
        return max_players

    @staticmethod
    def calculate_by_memory(total_memory_gb, memory_per_player_mb):
        """基于内存计算 (memory-based calculation)"""
        total_memory_mb = total_memory_gb * 1024
        max_players = int((total_memory_mb * 0.7) / memory_per_player_mb)  # 70% 使用率 (70% usage)
        return max_players

    @staticmethod
    def calculate_by_network(bandwidth_mbps, message_per_second):
        """基于网络计算 (network-based calculation)"""
        # 带宽利用率 70% (70% bandwidth utilization)
        available_bandwidth = bandwidth_mbps * 0.7 * 1024 / 8  # KB/s

        # 每个玩家每秒消息 (messages per player per second)
        player_bandwidth = message_per_second * 0.2  # 200 字节/消息 (200 bytes/message)

        max_players = int(available_bandwidth / player_bandwidth)
        return max_players

    @staticmethod
    def estimate_capacity(server_config):
        """估算承载能力 (estimate capacity)"""
        results = {}

        # CPU 承载 (CPU capacity)
        cpu_capacity = CapacityCalculator.calculate_by_cpu(
            server_config['cpu_cores'],
            server_config.get('cpu_per_player', 0.5)
        )
        results['cpu'] = cpu_capacity

        # 内存承载 (memory capacity)
        memory_capacity = CapacityCalculator.calculate_by_memory(
            server_config['memory_gb'],
            server_config.get('memory_per_player', 2)
        )
        results['memory'] = memory_capacity

        # 网络承载 (network capacity)
        network_capacity = CapacityCalculator.calculate_by_network(
            server_config.get('bandwidth_mbps', 1000),
            server_config.get('messages_per_second', 1)
        )
        results['network'] = network_capacity

        # 取最小值 (take the minimum)
        max_capacity = min(results.values())

        return {
            'max_capacity': max_capacity,
            'bottleneck': min(results, key=results.get),
            'details': results
        }


# 使用示例 (usage example)
config = {
    'cpu_cores': 16,
    'memory_gb': 64,
    'bandwidth_mbps': 1000,
    'cpu_per_player': 0.3,  # 每个玩家占用 0.3% CPU (0.3% CPU per player)
    'memory_per_player': 2,  # 每个玩家占用 2MB 内存 (2MB memory per player)
    'messages_per_second': 2  # 每个玩家每秒 2 条消息 (2 messages per player per second)
}

capacity = CapacityCalculator.estimate_capacity(config)
print(f"Max capacity: {capacity['max_capacity']} players")
print(f"Bottleneck: {capacity['bottleneck']}")
```

---

## 4. Scaling Assessment

### 4.1 Horizontal Scaling

```python
# 扩展能力评估 (scaling capability assessment)

class ScalingEvaluator:
    """扩展评估器 (scaling evaluator)"""

    @staticmethod
    def evaluate_horizontal_scaling(target_ccu, single_server_ccu):
        """评估水平扩展 (evaluate horizontal scaling)"""
        servers_needed = (target_ccu + single_server_ccu - 1) // single_server_ccu

        return {
            'servers_needed': servers_needed,
            'cost_estimate': servers_needed * 1000,  # 假设每台 1000 元/月 (assume 1000 CNY/month per server)
            'complexity': 'medium' if servers_needed < 10 else 'high'
        }

    @staticmethod
    def evaluate_vertical_scaling(current_ccu, target_ccu):
        """评估垂直扩展 (evaluate vertical scaling)"""
        scale_factor = target_ccu / current_ccu

        # 估算需要的硬件 (estimate required hardware)
        needed_cores = 16 * scale_factor
        needed_memory = 64 * scale_factor

        return {
            'recommended_cores': int(needed_cores),
            'recommended_memory_gb': int(needed_memory),
            'feasibility': 'possible' if scale_factor < 4 else 'limited'
        }
```

---

## 5. Practical Cases

### 5.1 KBEngine Capacity

```
KBEngine 实测承载 (参考) (KBEngine measured capacity (reference)):

硬件 (Hardware): Intel Xeon E5-2680 v4, 32GB RAM, SSD

单 BaseApp (single BaseApp):
- 玩家数 (players): 2000-3000
- CPU: 60-80%
- 内存 (memory): 2-4GB

单 CellApp (single CellApp):
- 实体数 (entities): 1000-2000
- CPU: 60-80%
- 内存 (memory): 4-8GB

完整集群 (1+2+2 架构) (full cluster (1+2+2 architecture)):
- CCU: 5000-8000
- CPU 总使用 (total CPU usage): 70%
- 内存总使用 (total memory usage): 30GB
- 网络带宽 (network bandwidth): 300-500 Mbps
```

---

## 6. Best Practices

### 6.1 Assessment Recommendations

| Practice | Description |
|------|------|
| **基准测试 (Benchmark testing)** | 建立性能基准 (establish a performance baseline) |
| **逐步加压 (Gradual load increase)** | 递增测试并发 (increase test concurrency progressively) |
| **监控指标 (Monitor metrics)** | 全面监控资源 (comprehensively monitor resources) |
| **预留余量 (Reserve headroom)** | 预留 20-30% (reserve 20-30%) |
| **复现场景 (Reproduce scenarios)** | 模拟真实玩法 (simulate real gameplay) |
| **长期观察 (Long-term observation)** | 稳定性测试 (stability testing) |

---

## 7. Summary

### Capacity Assessment Essentials

```
承载评估 = 基准测试 + 瓶颈分析 + 扩展规划
(Capacity assessment = benchmark testing + bottleneck analysis + scaling planning)
- 压力测试验证 (stress test verification)
- 多维度分析瓶颈 (multi-dimensional bottleneck analysis)
- 理论计算辅助 (theoretical calculation as aid)
- 预留扩展空间 (reserve scaling headroom)
```

---

## References

- [Load Testing Best Practices](https://loadforge.com/blog/load-testing-best-practices/)
- [Game Server Capacity Planning](https://www.gamedev.net/blogs/entry/2248382-c-and-cpps-game-server-series/)