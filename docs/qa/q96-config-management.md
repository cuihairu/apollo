# Q96: 如何管理服务器配置？

## 问题分析

本题考察对配置管理的理解：
- 配置文件格式
- 配置热加载
- 环境隔离
- KBEngine 配置系统

---

## 一、配置管理架构

### 1.1 配置层次

```
┌─────────────────────────────────────────────────────────────┐
│                    配置管理层次                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  全局配置 (Global):                                        │
│  ├── 服务器网络配置                                         │
│  ├── 数据库连接                                             │
│  ├── 线程池大小                                             │
│  └── 示例: kbengine_defaults.xml                            │
│                                                             │
│  组件配置 (Component):                                      │
│  ├── BaseApp 配置                                           │
│  ├── CellApp 配置                                           │
│  ├── DBMgr 配置                                             │
│  └── 示例: server.xml                                        │
│                                                             │
│  脚本配置 (Scripts):                                        │
│  ├── 游戏参数表                                             │
│  ├── 平衡性配置                                             │
│  ├── 功能开关                                               │
│  └── 示例: scripts/config/game_settings.xml                  │
│                                                             │
│  运行时配置 (Runtime):                                     │
│  ├── 动态调整                                               │
│  ├── 特权命令                                               │
│  └── 示例: watcher 修改                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、KBEngine 配置系统

### 2.1 KBEngine 配置文件

```xml
<!-- KBEngine 主配置文件: kbengine_defaults.xml -->

<root>
    <!-- 基础配置 -->
    <cellapp>
        <!-- 内部通信端口 -->
        <internalInterface>
            <channelType>3</channelType>  <!-- 0:tcp, 1:udp, 3:tcp_udp -->
        </internalInterface>

        <!-- 实体配置 -->
        <entityDefs>
            <paths>
                <path>./server/entity_defs/</path>
            </paths>
        </entityDefs>
    </cellapp>

    <baseapp>
        <!-- 实体销毁延迟 -->
        <entityDestroyTimeout>
            <seconds>60</seconds>
        </entityDestroyTimeout>

        <!-- 备份周期 -->
        <autoLoadEntitiesThreshold>
            <count>5</count>
        </autoLoadEntitiesThreshold>
    </baseapp>

    <dbmgr>
        <!-- 数据库配置 -->
        <databaseInterface>
            <mysql>
                <host>localhost</host>
                <port>3306</port>
                <database>kbengine</database>
                <username>root</username>
                <password></password>
                <encoding>utf8</encoding>
            </mysql>
        </databaseInterface>

        <!-- 缓冲区大小 -->
        <bufferSize>
            <size>1048576</size>  <!-- 1MB -->
        </bufferSize>
    </dbmgr>

    <!-- 全局配置 -->
    <thread>
        <networkWorkerThreads>
            <count>0</count>  <!-- 0 = CPU核心数 -->
        </networkWorkerThreads>
    </thread>
</root>
```

### 2.2 KBEngine 配置读取

```python
# KBEngine 配置读取

"""
KBEngine 配置系统:

1. XML 格式配置文件
2. 分层配置: defaults > user > commandline
3. 配置热更新支持
"""

import KBEngine

class GameConfig(KBEngine.Entity):
    """游戏配置实体"""

    def __init__(self):
        KBEngine.Entity.__init__(self)

        # 从配置读取参数
        self.load_config()

    def load_config(self):
        """加载配置"""
        # 读取全局配置
        self.max_players = KBEngine.getWatcher().get("game/maxPlayers")
        self.tick_rate = KBEngine.getWatcher().get("game/tickRate")

        # 读取组件配置
        self.cellapp_count = KBEngine.getWatcher().get("components/cellapp/count")

        INFO_MSG(f"Config loaded: max_players={self.max_players}, "
                f"tick_rate={self.tick_rate}")

    def onReload(self):
        """配置重新加载"""
        old_max = self.max_players
        self.load_config()

        if old_max != self.max_players:
            INFO_MSG(f"Max players changed: {old_max} -> {self.max_players}")
```

---

## 三、配置热加载

### 3.1 动态配置更新

```python
# 配置热加载

import xml.etree.ElementTree as ET
import threading
import time

class ConfigManager:
    """配置管理器"""

    def __init__(self, config_path):
        self.config_path = config_path
        self.config = {}
        self.last_mtime = 0
        self.lock = threading.Lock()
        self.watchers = []

        # 初始加载
        self.load_config()

        # 启动监控线程
        self.start_watching()

    def load_config(self):
        """加载配置文件"""
        try:
            tree = ET.parse(self.config_path)
            root = tree.getroot()

            with self.lock:
                self.config = self._parse_config(root)
                self.last_mtime = os.path.getmtime(self.config_path)

            self._notify_watchers()
            INFO_MSG("Config reloaded successfully")

        except Exception as e:
            ERROR_MSG(f"Failed to load config: {e}")

    def _parse_config(self, root):
        """解析配置"""
        config = {}

        # 解析游戏参数
        game = root.find("game")
        if game is not None:
            config["max_players"] = int(game.get("maxPlayers", 1000))
            config["tick_rate"] = float(game.get("tickRate", 10.0))
            config["exp_rate"] = float(game.get("expRate", 1.0))

        # 解析功能开关
        features = root.find("features")
        if features is not None:
            config["features"] = {}
            for feature in features:
                config["features"][feature.tag] = feature.get("enabled") == "true"

        return config

    def start_watching(self):
        """启动配置监控"""
        def watch_loop():
            while True:
                time.sleep(5)  # 每 5 秒检查一次

                try:
                    mtime = os.path.getmtime(self.config_path)
                    if mtime > self.last_mtime:
                        INFO_MSG("Config file changed, reloading...")
                        self.load_config()
                except:
                    pass

        thread = threading.Thread(target=watch_loop, daemon=True)
        thread.start()

    def register_watcher(self, callback):
        """注册配置变化回调"""
        self.watchers.append(callback)

    def _notify_watchers(self):
        """通知观察者"""
        for callback in self.watchers:
            try:
                callback(self.config)
            except Exception as e:
                ERROR_MSG(f"Watcher callback failed: {e}")

    def get(self, key, default=None):
        """获取配置值"""
        with self.lock:
            keys = key.split(".")
            value = self.config

            for k in keys:
                if isinstance(value, dict) and k in value:
                    value = value[k]
                else:
                    return default

            return value


# 使用示例
game_config = ConfigManager("config/game.xml")

def on_config_changed(config):
    """配置变化回调"""
    INFO_MSG(f"Config changed: {config}")

game_config.register_watcher(on_config_changed)

# 获取配置
max_players = game_config.get("max_players", 1000)
```

---

## 四、环境隔离

### 4.1 多环境配置

```python
# 环境隔离配置

import os

class EnvironmentConfig:
    """环境配置"""

    # 环境类型
    ENV_DEV = "development"
    ENV_TEST = "testing"
    ENV_STAGING = "staging"
    ENV_PROD = "production"

    def __init__(self):
        self.env = self._detect_environment()
        self.config = self._load_env_config()

    def _detect_environment(self):
        """检测环境"""
        # 从环境变量读取
        env = os.getenv("KBENGINE_ENV", "")

        # 从主机名推断
        if not env:
            hostname = os.getenv("HOSTNAME", "")
            if "dev" in hostname:
                return self.ENV_DEV
            elif "test" in hostname:
                return self.ENV_TEST
            elif "stage" in hostname:
                return self.ENV_STAGING

        return self.ENV_PROD

    def _load_env_config(self):
        """加载环境配置"""
        configs = {
            self.ENV_DEV: {
                "db_host": "localhost",
                "db_port": 3306,
                "log_level": "DEBUG",
                "max_players": 100,
            },
            self.ENV_TEST: {
                "db_host": "test-db.internal",
                "db_port": 3306,
                "log_level": "INFO",
                "max_players": 500,
            },
            self.ENV_STAGING: {
                "db_host": "stage-db.internal",
                "db_port": 3306,
                "log_level": "INFO",
                "max_players": 1000,
            },
            self.ENV_PROD: {
                "db_host": "prod-db.internal",
                "db_port": 3306,
                "log_level": "WARN",
                "max_players": 5000,
            },
        }

        return configs.get(self.env, configs[self.ENV_PROD])

    def get(self, key, default=None):
        """获取配置"""
        return self.config.get(key, default)
```

---

## 五、配置验证

### 5.1 配置校验

```python
# 配置验证

class ConfigValidator:
    """配置验证器"""

    @staticmethod
    def validate_config(config):
        """验证配置"""
        errors = []

        # 验证必需字段
        required_fields = ["db_host", "db_port", "db_name"]
        for field in required_fields:
            if field not in config:
                errors.append(f"Missing required field: {field}")

        # 验证数值范围
        if "max_players" in config:
            if config["max_players"] < 1 or config["max_players"] > 100000:
                errors.append("max_players must be between 1 and 100000")

        # 验证端口
        if "db_port" in config:
            port = config["db_port"]
            if port < 1 or port > 65535:
                errors.append(f"Invalid port: {port}")

        # 验证枚举值
        if "log_level" in config:
            valid_levels = ["DEBUG", "INFO", "WARN", "ERROR"]
            if config["log_level"] not in valid_levels:
                errors.append(f"Invalid log_level: {config['log_level']}")

        return errors

    @staticmethod
    def validate_kbengine_config(config_path):
        """验证 KBEngine 配置"""
        try:
            tree = ET.parse(config_path)
            root = tree.getroot()

            errors = []

            # 检查必需组件
            required_components = ["cellapp", "baseapp", "dbmgr"]
            for component in required_components:
                if root.find(component) is None:
                    errors.append(f"Missing component config: {component}")

            # 检查数据库配置
            dbmgr = root.find("dbmgr")
            if dbmgr is not None:
                db = dbmgr.find(".//mysql")
                if db is None:
                    errors.append("Missing MySQL configuration")

            return errors

        except Exception as e:
            return [f"Failed to parse config: {e}"]
```

---

## 六、最佳实践

### 6.1 配置管理建议

| 实践 | 说明 |
|------|------|
| **分层配置** | 默认 < 用户 < 命令行 |
| **版本控制** | 配置文件纳入版本管理 |
| **环境隔离** | 不同环境独立配置 |
| **敏感信息** | 使用密钥管理服务 |
| **变更记录** | 记录配置变更历史 |
| **验证优先** | 启动前验证配置 |

---

## 七、总结

### 配置管理核心

```
配置管理 = 分层加载 + 热更新 + 环境隔离 + 验证机制
- KBEngine XML 配置
- 文件监控实现热更
- 环境变量区分环境
- 启动前验证配置
```

---

## 参考资料

- [KBEngine Configuration](https://kbengine.github.io/docs/en/configuration.html)
- [12-Factor App Config](https://12factor.net/config)
- [Spring Cloud Config](https://spring.io/projects/spring-cloud-config)
