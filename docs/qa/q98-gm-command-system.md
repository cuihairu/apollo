# Q98: 如何实现 GM 命令系统？

## 问题分析

本题考察对 GM 命令系统的理解：
- 命令解析
- 权限控制
- 远程执行
- KBEngine Console

---

## 一、GM 命令架构

### 1.1 系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    GM 命令系统架构                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  客户端 (Client):                                          │
│  ├── 命令输入 (/gm command)                                 │
│  ├── 命令发送到服务器                                        │
│  └── 结果显示                                               │
│                          │                                  │
│  ▼                                                          │
│  服务器端 (Server):                                        │
│  ├── 命令解析 (Parser)                                      │
│  ├── 权限验证 (Permission)                                  │
│  ├── 命令执行 (Executor)                                    │
│  ├── 日志记录 (Audit)                                       │
│  └── 结果返回                                               │
│                                                             │
│  命令类型:                                                   │
│  ├── 玩家命令 (who, teleport, kick)                         │
│  ├── 系统命令 (reload, save, shutdown)                      │
│  ├── 调试命令 (debug, profile, watcher)                     │
│  └── 游戏命令 (additem, setlevel, godmode)                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、命令系统实现

### 2.1 命令处理器

```python
# GM 命令系统

import re
from functools import wraps

class GMCommand:
    """GM 命令装饰器"""

    def __init__(self, name, aliases=None, permission_level=1, help_text=""):
        self.name = name
        self.aliases = aliases or []
        self.permission_level = permission_level
        self.help_text = help_text

    def __call__(self, func):
        func.gm_command = True
        func.gm_name = self.name
        func.gm_aliases = self.aliases
        func.gm_permission = self.permission_level
        func.gm_help = self.help_text
        return func


class GMCommandSystem:
    """GM 命令系统"""

    def __init__(self):
        self.commands = {}
        self.permission_cache = {}

    def register_handler(self, handler):
        """注册命令处理器类"""
        for attr_name in dir(handler):
            attr = getattr(handler, attr_name)
            if hasattr(attr, 'gm_command'):
                self.register_command(attr)

    def register_command(self, func):
        """注册命令"""
        cmd = {
            "name": func.gm_name,
            "aliases": func.gm_aliases,
            "permission": func.gm_permission,
            "help": func.gm_help,
            "func": func
        }

        # 注册主命令
        self.commands[func.gm_name] = cmd

        # 注册别名
        for alias in func.gm_aliases:
            self.commands[alias] = cmd

    def execute(self, player, command_str):
        """执行命令"""
        # 解析命令
        parts = command_str.strip().split()
        if not parts:
            return {"success": False, "message": "Empty command"}

        cmd_name = parts[0].lower()
        args = parts[1:] if len(parts) > 1 else []

        # 查找命令
        if cmd_name not in self.commands:
            return {"success": False, "message": f"Unknown command: {cmd_name}"}

        cmd = self.commands[cmd_name]

        # 检查权限
        if not self.check_permission(player, cmd["permission"]):
            return {"success": False, "message": "Permission denied"}

        # 执行命令
        try:
            result = cmd["func"](player, *args)
            return {"success": True, "result": result}
        except Exception as e:
            ERROR_MSG(f"GM command error: {e}")
            return {"success": False, "message": str(e)}

    def check_permission(self, player, required_level):
        """检查权限"""
        player_level = self.get_permission_level(player)
        return player_level >= required_level

    def get_permission_level(self, player):
        """获取权限等级"""
        # 从缓存获取
        if player.id in self.permission_cache:
            return self.permission_cache[player.id]

        # 从数据库或属性获取
        level = getattr(player, "gm_level", 0)

        # 缓存
        self.permission_cache[player.id] = level
        return level

    def list_commands(self, player):
        """列出可用命令"""
        player_level = self.get_permission_level(player)

        commands = []
        for cmd in self.commands.values():
            if cmd["name"] == cmd["name"]:  # 只显示主命令
                if cmd["permission"] <= player_level:
                    commands.append({
                        "name": cmd["name"],
                        "help": cmd["help"]
                    })

        return commands


# 全局命令系统实例
gm_system = GMCommandSystem()


# 命令处理器
class PlayerCommands:
    """玩家相关命令"""

    @GMCommand("who", aliases=["online"], permission_level=1,
               help_text="显示在线玩家列表")
    def cmd_who(self, player):
        """显示在线玩家"""
        online_players = []
        for entity_id, entity in KBEngine.entities.items():
            if hasattr(entity, 'playerName'):
                online_players.append({
                    "id": entity_id,
                    "name": entity.playerName,
                    "level": getattr(entity, 'level', 0),
                    "position": str(entity.position)
                })

        return f"Online players: {len(online_players)}\n" + \
               "\n".join([f"  {p['name']} (Lv{p['level']})" for p in online_players])

    @GMCommand("teleport", aliases=["tp", "goto"], permission_level=1,
               help_text="传送到指定位置或玩家")
    def cmd_teleport(self, player, target=None, x=None, y=None, z=None):
        """传送"""
        if not target:
            return "Usage: /teleport <player_id> or /teleport <x> <y> <z>"

        # 传送到玩家
        if not x:
            target_entity = KBEngine.getEntity(int(target))
            if target_entity:
                player.position = target_entity.position
                player.spaceID = target_entity.spaceID
                return f"Teleported to {target}"
            return f"Player {target} not found"

        # 传送到坐标
        player.position = (float(x), float(y), float(z))
        return f"Teleported to ({x}, {y}, {z})"

    @GMCommand("kick", permission_level=2,
               help_text="踢出玩家")
    def cmd_kick(self, player, player_id):
        """踢出玩家"""
        target = KBEngine.getEntity(int(player_id))
        if not target:
            return f"Player {player_id} not found"

        if hasattr(target, 'client') and target.client:
            target.client.kout()
            return f"Kicked player {player_id}"
        return "Target has no client"

    @GMCommand("ban", permission_level=3,
               help_text="封禁玩家")
    def cmd_ban(self, player, player_id, duration="0"):
        """封禁玩家"""
        duration_sec = int(duration)
        # 实现封禁逻辑
        return f"Banned player {player_id} for {duration_sec} seconds"


class ItemCommands:
    """物品相关命令"""

    @GMCommand("additem", aliases=["item", "add"], permission_level=2,
               help_text="添加物品到背包")
    def cmd_additem(self, player, item_id, count=1):
        """添加物品"""
        item_id = int(item_id)
        count = int(count)

        # 检查物品是否存在
        if not self.item_exists(item_id):
            return f"Item {item_id} does not exist"

        # 添加到背包
        if hasattr(player, 'inventory'):
            player.inventory.addItem(item_id, count)
            return f"Added {count}x item {item_id} to inventory"
        return "Player has no inventory"

    @GMCommand("removeitem", permission_level=2,
               help_text="从背包移除物品")
    def cmd_removeitem(self, player, item_id, count=1):
        """移除物品"""
        item_id = int(item_id)
        count = int(count)

        if hasattr(player, 'inventory'):
            removed = player.inventory.removeItem(item_id, count)
            return f"Removed {removed}x item {item_id}"
        return "Player has no inventory"


class SystemCommands:
    """系统相关命令"""

    @GMCommand("reload", permission_level=3,
               help_text="重新加载脚本")
    def cmd_reload(self, player, module_name=None):
        """重新加载脚本"""
        if module_name:
            # 重新加载特定模块
            import importlib
            import sys

            if module_name in sys.modules:
                importlib.reload(sys.modules[module_name])
                return f"Reloaded module: {module_name}"
            return f"Module {module_name} not loaded"
        else:
            # 触发 KBEngine 脚本重载
            KBEngine.reloadAllScripts()
            return "All scripts reloaded"

    @GMCommand("save", permission_level=2,
               help_text="保存所有实体")
    def cmd_save(self, player):
        """保存所有实体"""
        count = 0
        for entity_id, entity in KBEngine.entities.items():
            if hasattr(entity, 'writeToDB'):
                entity.writeToDB()
                count += 1

        return f"Saving {count} entities..."

    @GMCommand("shutdown", permission_level=4,
               help_text="关闭服务器")
    def cmd_shutdown(self, player, delay="0"):
        """关闭服务器"""
        delay_sec = int(delay)
        if delay_sec > 0:
            # 延迟关机
            shutdown_mgr.schedule_maintenance(delay_sec)
            return f"Server will shutdown in {delay_sec} seconds"
        else:
            # 立即关机
            shutdown_mgr.initiate_shutdown("gm_command")
            return "Server shutting down..."


# 注册所有命令
gm_system.register_handler(PlayerCommands())
gm_system.register_handler(ItemCommands())
gm_system.register_handler(SystemCommands())
```

---

## 三、远程命令

### 3.1 Telnet/HTTP 接口

```python
# 远程 GM 命令

import socket
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler

class GMCommandServer:
    """GM 命令服务器"""

    def __init__(self, host="0.0.0.0", port=9999):
        self.host = host
        self.port = port
        self.password = self._load_password()
        self.sessions = {}

    def _load_password(self):
        """加载密码"""
        # 从配置文件读取
        import os
        return os.getenv("GM_PASSWORD", "admin123")

    def authenticate(self, password):
        """验证密码"""
        return password == self.password


class TelnetGMHandler:
    """Telnet GM 处理器"""

    def __init__(self, conn, addr, gm_server):
        self.conn = conn
        self.addr = addr
        self.gm_server = gm_server
        self.authenticated = False
        self.player = None

    def run(self):
        """运行会话"""
        try:
            # 发送欢迎消息
            self.send("Welcome to GM Command Server")
            self.send("Password: ")

            # 验证密码
            password = self.recv().strip()
            if not self.gm_server.authenticate(password):
                self.send("Authentication failed")
                return

            self.authenticated = True
            self.send("Authentication successful")
            self.send("Type 'help' for available commands")

            # 命令循环
            while True:
                self.send("GM> ", end="")
                cmd = self.recv().strip()

                if not cmd:
                    continue

                if cmd.lower() == "quit" or cmd.lower() == "exit":
                    self.send("Goodbye!")
                    break

                # 执行命令
                result = self.execute_command(cmd)
                self.send(result)

        except Exception as e:
            ERROR_MSG(f"Telnet handler error: {e}")
        finally:
            self.conn.close()

    def send(self, msg, end="\n"):
        """发送消息"""
        self.conn.send((msg + end).encode())

    def recv(self):
        """接收消息"""
        return self.conn.recv(1024).decode()

    def execute_command(self, cmd_str):
        """执行命令"""
        if not self.authenticated:
            return "Not authenticated"

        # 使用虚拟玩家执行
        if not self.player:
            self.player = GMPlayer()

        result = gm_system.execute(self.player, cmd_str)

        if result["success"]:
            return str(result.get("result", "Done"))
        else:
            return f"Error: {result['message']}"


class HTTPGMHandler(BaseHTTPRequestHandler):
    """HTTP GM 处理器"""

    def do_POST(self):
        """处理 POST 请求"""
        if self.path == "/api/gm":
            # 读取请求体
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            import json
            data = json.loads(post_data.decode())

            # 验证
            if not gm_http_server.authenticate(data.get("password")):
                self.send_error(401, "Authentication failed")
                return

            # 执行命令
            player = GMPlayer()
            result = gm_system.execute(player, data.get("command"))

            # 返回结果
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(result).encode())

    def log_message(self, format, *args):
        """禁用默认日志"""
        pass


class GMPlayer:
    """虚拟 GM 玩家"""

    def __init__(self):
        self.id = 0
        self.gm_level = 10  # 最高权限
        self.name = "GM_Console"
```

---

## 四、审计日志

### 4.1 命令审计

```python
# GM 命令审计

import time
import json

class GMAuditLogger:
    """GM 审计日志"""

    def __init__(self, log_path="logs/gm_audit.log"):
        self.log_path = log_path

    def log_command(self, player, command_str, result):
        """记录命令"""
        log_entry = {
            "timestamp": time.time(),
            "gm_id": player.id,
            "gm_name": getattr(player, 'name', 'unknown'),
            "command": command_str,
            "result": str(result),
            "ip": self.get_client_ip(player)
        }

        # 写入日志文件
        with open(self.log_path, 'a') as f:
            f.write(json.dumps(log_entry) + '\n')

        # 同时写入数据库
        self.log_to_database(log_entry)

    def get_client_ip(self, player):
        """获取客户端 IP"""
        # 从 KBEngine 获取
        if hasattr(player, 'clientAddr'):
            return player.clientAddr[0]
        return "unknown"

    def log_to_database(self, entry):
        """记录到数据库"""
        # 异步写入数据库
        pass


# 修改 GMCommandSystem 以支持审计
class AuditedGMCommandSystem(GMCommandSystem):

    def __init__(self):
        super().__init__()
        self.audit_logger = GMAuditLogger()

    def execute(self, player, command_str):
        """执行命令（带审计）"""
        result = super().execute(player, command_str)

        # 记录审计日志
        self.audit_logger.log_command(player, command_str, result)

        return result
```

---

## 五、KBEngine Console

### 5.1 KBEngine 内置控制台

```python
# KBEngine Console 扩展

"""
KBEngine 内置控制台:

1. Python Console: 连接到 CellApp/BaseApp 的 Python 接口
2. 可以直接执行 Python 代码
3. 通过 kbengine/python_console 实现
"""

class KBEngineConsole:
    """KBEngine 控制台扩展"""

    @staticmethod
    def register_console_commands():
        """注册控制台命令"""

        # 注册自定义命令
        KBEngine.registerConsoleCommand("players", KBEngineConsole.list_players)
        KBEngine.registerConsoleCommand("spaces", KBEngineConsole.list_spaces)
        KBEngine.registerConsoleCommand("find", KBEngineConsole.find_entity)

    @staticmethod
    def list_players():
        """列出所有玩家"""
        players = []
        for eid, entity in KBEngine.entities.items():
            if hasattr(entity, 'playerName'):
                players.append(f"{eid}: {entity.playerName}")
        return "\n".join(players)

    @staticmethod
    def list_spaces():
        """列出所有空间"""
        spaces = []
        for sid, space in KBEngine.spaces.items():
            spaces.append(f"{sid}: {space.spaceName}")
        return "\n".join(spaces)

    @staticmethod
    def find_entity(name):
        """查找实体"""
        for eid, entity in KBEngine.entities.items():
            entity_name = getattr(entity, 'playerName', '')
            if name.lower() in entity_name.lower():
                return f"{eid}: {entity_name} at {entity.position}"
        return "Not found"


# 在脚本初始化时注册
def onInit(isReload):
    """初始化"""
    if not isReload:
        KBEngineConsole.register_console_commands()
```

---

## 六、最佳实践

### 6.1 GM 命令建议

| 实践 | 说明 |
|------|------|
| **权限分级** | 至少 4 级权限 |
| **审计日志** | 记录所有 GM 操作 |
| **安全认证** | 密码/双因素认证 |
| **命令限制** | 生产环境谨慎使用 |
| **敏感命令** | 二次确认 |
| **远程访问** | 限制 IP 白名单 |

---

## 七、总结

### GM 命令系统核心

```
GM 命令 = 命令解析 + 权限验证 + 审计日志 + 远程接口
- 装饰器注册命令
- 分级权限控制
- 所有操作可审计
- 支持 Telnet/HTTP
```

---

## 参考资料

- [KBEngine Console](https://kbengine.github.io/docs/en/programming-guide/console.html)
- [Game Master Systems](https://www.gamedev.net/articles/programming/engines-and-middleware/
- [Command Pattern](https://en.wikipedia.org/wiki/Command_pattern)
