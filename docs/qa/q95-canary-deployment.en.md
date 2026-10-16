[English](q95-canary-deployment.en.md) | [中文](q95-canary-deployment.md)

# Q95: How to Implement Canary Deployment?

## Problem Analysis

This question examines understanding of canary deployment:
- Canary deployment strategies
- Traffic control
- Automatic rollback
- Game server practices

---

## I. Canary Deployment Strategies

### 1.1 Deployment Modes

```
┌─────────────────────────────────────────────────────────────┐
│                    Canary Deployment Modes                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. Percentage-based canary:                                │
│  ├── 10% → 25% → 50% → 100%                               │
│  ├── Gradually increase traffic                             │
│  └── Watch the error rate                                   │
│                                                             │
│  2. User-based canary:                                      │
│  ├── Whitelisted users → new version                        │
│  ├── VIP users → new version                                │
│  ├── Specific regions → new version                         │
│  └── Full rollout                                           │
│                                                             │
│  3. Canary deployment (Canary):                             │
│  ├── Small-traffic validation                               │
│  ├── Monitor key metrics                                    │
│  │  ├── Error rate                                          │
│  │  ├── Latency                                             │
│  │  └── Resource usage                                      │
│  └── Automatic rollback on anomaly                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## II. Canary Implementation

### 2.1 Traffic Distributor

```python
# Canary deployment traffic distribution

class GrayReleaseManager:
    """Gray release manager (灰度发布管理器)"""

    def __init__(self):
        self.release_config = {}
        self.player_versions = {}

    def create_release(self, version, config):
        """Create a new version release (创建新版本发布)"""
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
        """Get the version used by the player (获取玩家使用的版本)"""
        # Check for a specific version assignment (检查是否有特定版本分配)
        if player_id in self.player_versions:
            return self.player_versions[player_id]

        # Return version based on gray release config (根据灰度配置返回版本)
        return self._get_version_by_percentage()

    def _get_version_by_percentage(self):
        """Get version by percentage (根据百分比获取版本)"""
        for version, config in sorted(self.release_config.items(),
                                     key=lambda x: x[1]["version"]):

            if config["status"] == "production":
                return version

            current_pct = config["current_percentage"]

            # Random decision (随机决定)
            import random
            if random.random() * 100 < current_pct:
                return version

        return "stable"

    def update_metrics(self, version, error, latency):
        """Update version metrics (更新版本指标)"""
        if version not in self.release_config:
            return

        config = self.release_config[version]
        metrics = config["metrics"]

        metrics["total_requests"] += 1

        if error:
            metrics["error_count"] += 1

        # Update average latency (更新平均延迟)
        metrics["avg_latency"] = (
            (metrics["avg_latency"] * (metrics["total_requests"] - 1) + latency)
            / metrics["total_requests"]
        )

        # Check whether rollback is needed (检查是否需要回滚)
        error_rate = metrics["error_count"] / metrics["total_requests"]

        if error_rate > config["rollback_threshold"] / 100:
            self.rollback(version, "High error rate")
            return

        # Check whether to keep advancing (检查是否需要继续推进)
        if error_rate < config["rollout_threshold"] / 100:
            self.advance_release(version)

    def advance_release(self, version):
        """Advance the release progress (推进发布进度)"""
        config = self.release_config[version]

        if config["status"] == "canary":
            # Canary passed, enter the testing phase (金丝雀通过，进入测试阶段)
            config["status"] = "testing"
            config["current_percentage"] = config["test_percentage"]
            INFO(f"Version {version} advanced to testing phase")

        elif config["status"] == "testing":
            # Testing phase passed, enter progressive release (测试阶段通过，进入渐进式发布)
            config["status"] = "progressive"
            config["current_percentage"] = config["progressive_percentage"]
            INFO(f"Version {version} advanced to progressive phase")

        elif config["status"] == "progressive":
            # Progressively advance to 100% (逐步推进到 100%)
            config["current_percentage"] = min(100,
                config["current_percentage"] + 10)
            INFO(f"Version {version} advanced to {config['current_percentage']}%")

            if config["current_percentage"] >= 100:
                config["status"] = "production"
                INFO(f"Version {version} is now in production")

    def rollback(self, version, reason):
        """Roll back the version (回滚版本)"""
        config = self.release_config[version]

        INFO(f"Rolling back version {version}: {reason}")
        config["status"] = "rolled_back"

        # Clear player version assignments (清理玩家版本分配)
        self.player_versions.clear()
```

---

## III. Game Server Canary

### 3.1 Server Canary

```python
# Game server canary deployment

class ServerGrayRelease:
    """Server canary deployment (服务器灰度发布)"""

    def __init__(self):
        # Server groups (服务器分组)
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
        """Route the player based on the canary phase (根据灰度阶段路由玩家)"""
        # Get the server group the player should connect to (获取玩家应该连接的服务器组)
        server_group = self._get_server_group_for_player(player_id)

        # Return the server address (返回服务器地址)
        return self.get_server_address(server_group)

    def _get_server_group_for_player(self, player_id):
        """Get the server group the player should connect to (获取玩家应该连接的服务器组)"""
        # Whitelisted players always use the new version (白名单玩家总是使用新版本)
        if self._is_whitelisted(player_id):
            return self.current_phase

        # Randomly assign to the current phase (随机分配到当前阶段)
        servers = self.server_groups[self.current_phase]
        return random.choice(servers)

    def _is_whitelisted(self, player_id):
        """Check whether in the whitelist (检查是否在白名单)"""
        return is_player_in_whitelist(player_id)

    def advance_phase(self):
        """Advance to the next phase (推进到下一阶段)"""
        phases = ["canary", "testing", "progressive", "production"]
        current_index = phases.index(self.current_phase)

        if current_index + 1 < len(phases):
            self.current_phase = phases[current_index + 1]

            INFO(f"Gray release advanced to {self.current_phase}")
            return True

        return False

    def rollback(self):
        """Roll back to the old version (回滚到旧版本)"""
        self.current_phase = "canary"
        INFO("Gray release rolled back")
```

---

## IV. Automated Release

### 4.1 CI/CD Integration

```yaml
# Automated canary deployment pipeline (自动化灰度发布流程)

stages:
  # 1. Build phase (构建阶段)
  - name: Build and Test
    run: |
      # Compile the new version (编译新版本)
      ./build.sh
      # Run the tests (运行测试)
      ./run_tests.sh

  # 2. Canary release (金丝雀发布)
  - name: Canary Release
    run: |
      # Deploy to the canary servers (部署到金丝雀服务器)
      ./deploy.sh canary servers
      # Wait 5 minutes for observation (等待 5 分钟观察)
      sleep 300
      # Check the error rate (检查错误率)
      ./check_error_rate.sh canary

  # 3. Test phase (测试阶段)
  - name: Test Release
    run: |
      # Expand the scope (扩大范围)
      ./deploy.sh testing servers
      # Wait for observation (等待观察)
      sleep 600

  # 4. Progressive release (渐进式发布)
  - name: Progressive Release
    run: |
      # Increase by 25% each time (每次增加 25%)
      for percentage in [25, 50, 75, 100]:
        ./deploy.sh progressive servers --percentage $percentage
        sleep 300

  # 5. Full release (全量发布)
  - name: Full Release
    run: |
      ./deploy.sh all servers

  # 6. Monitoring and rollback (监控和回滚)
  - name: Monitor and Rollback
    run: |
      # Monitor key metrics (监控关键指标)
      ./monitor_metrics.sh

      # If the error rate is too high, roll back automatically (如果错误率过高，自动回滚)
      if ./check_error_rate.sh all | grep "HIGH"; then
        ./rollback.sh all
        exit 1
      fi
```

---

## V. Best Practices

### 5.1 Canary Deployment Recommendations

| Practice | Description |
|------|------|
| **Small steps, fast iteration** | Small increments each time, iterate quickly |
| **Full testing** | Validate fully at each phase |
| **Monitoring in place** | Monitor key metrics in real time |
| **Fast rollback** | Roll back immediately on anomaly |
| **Feature flags** | Use feature flags to control new features |

---

## VI. Summary

### Canary Deployment Core

```
Canary deployment = traffic control + automatic rollback + monitoring verification + fast iteration
- Canary validation
- Gradually expand scope
- Real-time metric monitoring
- Automatic rollback on anomaly
```

---

## References

- [Canary Deployment](https://martinfowler.com/canary/)
- [Blue-Green Deployment](https://martinfowler.com/bluegreen/)
- [Feature Flags](https://www.atlassian.com/software/jira/software/best-practices/feature-flags)
