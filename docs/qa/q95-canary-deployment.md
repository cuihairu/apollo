# Q95: 如何实现灰度发布？

## 问题分析

本题考察对灰度发布的理解：
- 灰度发布策略
- 流量控制
- 自动回滚
- 游戏服务器实践

---

## 一、灰度发布策略

### 1.1 发布模式

```
┌─────────────────────────────────────────────────────────────┐
│                    灰度发布模式                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 按比例灰度:                                              │
│  ├── 10% → 25% → 50% → 100%                               │
│  ├── 逐步增加流量                                            │
│  └── 观察错误率                                               │
│                                                             │
│  2. 按用户灰度:                                              │
│  ├── 白名单用户 → 新版本                                       │
│  ├── VIP 用户 → 新版本                                         │
│  ├── 特定地区 → 新版本                                         │
│  └── 全量推送                                                 │
│                                                             │
│  3. 金丝雀发布 (Canary):                                     │
│  ├── 小流量验证                                              │
│  ├── 监控关键指标                                              │
│  │  ├── 错误率                                               │
│  │  ├── 延迟                                                 │
│  │  └── 资源使用                                             │
│  └── 异常自动回滚                                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、灰度实现

### 2.1 流量分配器

```python
# 灰度发布流量分配

class GrayReleaseManager:
    """灰度发布管理器"""

    def __init__(self):
        self.release_config = {}
        self.player_versions = {}

    def create_release(self, version, config):
        """创建新版本发布"""
        self.release_config[version] = {
            "version": version,
            "status": "canary",  # canary, testing, progressive, production
            "canary_percentage": config.get("canary_percentage", 1),
            "test_percentage": config.get("test_percentage", 10),
            "progressive_percentage": config.get("progressive_percentage", 50),
            "rollout_threshold": config.get("rollout_threshold", 5),
            "rollback_threshold": config.get("rollback_threshold", 10),
            "start_time": time.time(),
            "current_percentage": 0,
            "metrics": {
                "error_count": 0,
                "total_requests": 0,
                "avg_latency": 0
            }
        }

    def get_version_for_player(self, player_id):
        """获取玩家使用的版本"""
        # 检查是否有特定版本分配
        if player_id in self.player_versions:
            return self.player_versions[player_id]

        # 根据灰度配置返回版本
        return self._get_version_by_percentage()

    def _get_version_by_percentage(self):
        """根据百分比获取版本"""
        for version, config in sorted(self.release_config.items(),
                                     key=lambda x: x[1]["version"]):

            if config["status"] == "production":
                return version

            current_pct = config["current_percentage"]

            # 随机决定
            import random
            if random.random() * 100 < current_pct:
                return version

        return "stable"

    def update_metrics(self, version, error, latency):
        """更新版本指标"""
        if version not in self.release_config:
            return

        config = self.release_config[version]
        metrics = config["metrics"]

        metrics["total_requests"] += 1

        if error:
            metrics["error_count"] += 1

        # 更新平均延迟
        metrics["avg_latency"] = (
            (metrics["avg_latency"] * (metrics["total_requests"] - 1) + latency)
            / metrics["total_requests"]
        )

        # 检查是否需要回滚
        error_rate = metrics["error_count"] / metrics["total_requests"]

        if error_rate > config["rollback_threshold"] / 100:
            self.rollback(version, "High error rate")
            return

        # 检查是否需要继续推进
        if error_rate < config["rollout_threshold"] / 100:
            self.advance_release(version)

    def advance_release(self, version):
        """推进发布进度"""
        config = self.release_config[version]

        if config["status"] == "canary":
            # 金丝雀通过，进入测试阶段
            config["status"] = "testing"
            config["current_percentage"] = config["test_percentage"]
            INFO(f"Version {version} advanced to testing phase")

        elif config["status"] == "testing":
            # 测试阶段通过，进入渐进式发布
            config["status"] = "progressive"
            config["current_percentage"] = config["progressive_percentage"]
            INFO(f"Version {version} advanced to progressive phase")

        elif config["status"] == "progressive":
            # 逐步推进到 100%
            config["current_percentage"] = min(100,
                config["current_percentage"] + 10)
            INFO(f"Version {version} advanced to {config['current_percentage']}%")

            if config["current_percentage"] >= 100:
                config["status"] = "production"
                INFO(f"Version {version} is now in production")

    def rollback(self, version, reason):
        """回滚版本"""
        config = self.release_config[version]

        INFO(f"Rolling back version {version}: {reason}")
        config["status"] = "rolled_back"

        # 清理玩家版本分配
        self.player_versions.clear()
```

---

## 三、游戏服务器灰度

### 3.1 服务器灰度

```python
# 游戏服务器灰度发布

class ServerGrayRelease:
    """服务器灰度发布"""

    def __init__(self):
        # 服务器分组
        self.server_groups = {
            "canary": ["cellapp-01"],
            "testing": ["cellapp-01", "cellapp-02"],
            "progressive": ["cellapp-01", "cellapp-02", "cellapp-03"],
            "production": ["cellapp-01", "cellapp-02", "cellapp-03",
                          "cellapp-04", "cellapp-05"]
        }

        self.current_phase = "canary"
        self.phase_mappings = {
            "canary": "v2.0.0-canary",
            "testing": "v2.0.0-testing",
            "progressive": "v2.0.0",
            "production": "v2.0.0"
        }

    def route_player(self, player_id):
        """根据灰度阶段路由玩家"""
        # 获取玩家应该连接的服务器组
        server_group = self._get_server_group_for_player(player_id)

        # 返回服务器地址
        return self.get_server_address(server_group)

    def _get_server_group_for_player(self, player_id):
        """获取玩家应该连接的服务器组"""
        # 白名单玩家总是使用新版本
        if self._is_whitelisted(player_id):
            return self.current_phase

        # 随机分配到当前阶段
        servers = self.server_groups[self.current_phase]
        return random.choice(servers)

    def _is_whitelisted(self, player_id):
        """检查是否在白名单"""
        return is_player_in_whitelist(player_id)

    def advance_phase(self):
        """推进到下一阶段"""
        phases = ["canary", "testing", "progressive", "production"]
        current_index = phases.index(self.current_phase)

        if current_index + 1 < len(phases):
            self.current_phase = phases[current_index + 1]

            INFO(f"Gray release advanced to {self.current_phase}")
            return True

        return False

    def rollback(self):
        """回滚到旧版本"""
        self.current_phase = "canary"
        INFO("Gray release rolled back")
```

---

## 四、自动化发布

### 4.1 CI/CD 集成

```yaml
# 自动化灰度发布流程

stages:
  # 1. 构建阶段
  - name: Build and Test
    run: |
      # 编译新版本
      ./build.sh
      # 运行测试
      ./run_tests.sh

  # 2. 金丝雀发布
  - name: Canary Release
    run: |
      # 部署到金丝雀服务器
      ./deploy.sh canary servers
      # 等待 5 分钟观察
      sleep 300
      # 检查错误率
      ./check_error_rate.sh canary

  # 3. 测试阶段
  - name: Test Release
    run: |
      # 扩大范围
      ./deploy.sh testing servers
      # 等待观察
      sleep 600

  # 4. 渐进式发布
  - name: Progressive Release
    run: |
      # 每次增加 25%
      for percentage in [25, 50, 75, 100]:
        ./deploy.sh progressive servers --percentage $percentage
        sleep 300

  # 5. 全量发布
  - name: Full Release
    run: |
      ./deploy.sh all servers

  # 6. 监控和回滚
  - name: Monitor and Rollback
    run: |
      # 监控关键指标
      ./monitor_metrics.sh

      # 如果错误率过高，自动回滚
      if ./check_error_rate.sh all | grep "HIGH"; then
        ./rollback.sh all
        exit 1
      fi
```

---

## 五、最佳实践

### 5.1 灰度发布建议

| 实践 | 说明 |
|------|------|
| **小步快跑** | 每次小增量，快速迭代 |
| **充分测试** | 每个阶段充分验证 |
| **监控到位** | 实时监控关键指标 |
| **快速回滚** | 异常时立即回滚 |
| **功能开关** | 功能开关控制新特性 |

---

## 六、总结

### 灰度发布核心

```
灰度发布 = 流量控制 + 自动回滚 + 监控验证 + 快速迭代
- 金丝雀验证
- 逐步扩大范围
- 实时监控指标
- 异常自动回滚
```

---

## 参考资料

- [Canary Deployment](https://martinfowler.com/canary/)
- [Blue-Green Deployment](https://martinfowler.com/bluegreen/)
- [Feature Flags](https://www.atlassian.com/software/jira/software/best-practices/feature-flags)
