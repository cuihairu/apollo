# Q86: 如何设计权限系统？

## 问题分析

本题考察对权限系统的理解：
- 权限模型设计
- RBAC 实现
- 权限验证
- KBEngine 权限

---

## 一、RBAC 模型

### 1.1 基本概念

```
┌─────────────────────────────────────────────────────────────┐
│                    RBAC 模型                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  用户 (User) → 角色 (Role) → 权限 (Permission)              │
│                                                             │
│  示例:                                                      │
│  ┌──────────┐    ┌──────────┐    ┌──────────────┐          │
│  │ Player   │───→│ VIP      │───→│ Chat         │          │
│  └──────────┘    └──────────┘    │ Trade        │          │
│                                   │ Kick Player  │          │
│  ┌──────────┐                   └──────────────┘          │
│  │ GM       │───→┐                                            │
│  └──────────┘    │                                            │
│                  ▼                                            │
│          ┌──────────────┐                                    │
│          │ All Permissions│                                  │
│          │ Ban Player     │                                  │
│          │ Spawn Item     │                                  │
│          └──────────────┘                                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、权限实现

### 2.1 权限定义

```cpp
// 权限定义

enum class Permission {
    // 基础权限
    MOVE,
    ATTACK,
    CHAT,

    // VIP 权限
    VIP_CHAT,
    VIP_TRADE,
    VIP_DUNGEON,

    // GM 权限
    GM_KICK,
    GM_BAN,
    GM_SPAWN,
    GM_TELEPORT,
    GM_INVISIBLE,

    // 管理员权限
    ADMIN_ALL
};

// 权限检查器
class PermissionChecker {
public:
    bool hasPermission(uint64_t playerId, Permission perm) {
        Player* player = getPlayer(playerId);
        if (!player) return false;

        // 管理员拥有所有权限
        if (player->isAdmin()) {
            return true;
        }

        // 检查角色权限
        for (const std::string& role : player->getRoles()) {
            if (roleHasPermission(role, perm)) {
                return true;
            }
        }

        return false;
    }

    bool hasAnyPermission(uint64_t playerId,
                          const std::vector<Permission>& perms) {
        for (Permission perm : perms) {
            if (hasPermission(playerId, perm)) {
                return true;
            }
        }
        return false;
    }

private:
    bool roleHasPermission(const std::string& role, Permission perm) {
        const auto& rolePerms = rolePermissions_[role];
        return rolePerms.count(perm) > 0;
    }

    std::unordered_map<std::string, std::set<Permission>> rolePermissions_;
};
```

### 2.2 命令权限

```cpp
// GM 命令权限系统

class GMCommandSystem {
public:
    struct Command {
        std::string name;
        int requiredLevel;  // 0=Player, 1=VIP, 2=GM, 3=Admin
        std::set<Permission> requiredPerms;
        std::function<void(uint64_t, const std::vector<std::string>&)> handler;
    };

    void registerCommand(const Command& cmd) {
        commands_[cmd.name] = cmd;
    }

    bool executeCommand(uint64_t playerId, const std::string& cmdName,
                       const std::vector<std::string>& args) {
        auto it = commands_.find(cmdName);
        if (it == commands_.end()) {
            sendErrorMessage(playerId, "Unknown command");
            return false;
        }

        const Command& cmd = it->second;

        // 检查权限等级
        Player* player = getPlayer(playerId);
        if (player->getGMLevel() < cmd.requiredLevel) {
            sendErrorMessage(playerId, "Permission denied");
            return false;
        }

        // 检查具体权限
        for (Permission perm : cmd.requiredPerms) {
            if (!permissionChecker_.hasPermission(playerId, perm)) {
                sendErrorMessage(playerId, "Missing permission");
                return false;
            }
        }

        // 执行命令
        cmd.handler(playerId, args);

        // 记录日志
        logGMCommand(playerId, cmdName, args);

        return true;
    }

    // 注册常用命令
    void registerCommonCommands() {
        registerCommand({"kick", 2, {Permission::GM_KICK},
            [](uint64_t gmId, const auto& args) {
                if (args.empty()) return;
                uint64_t targetId = std::stoull(args[0]);
                kickPlayer(gmId, targetId);
            }
        });

        registerCommand({"ban", 3, {Permission::GM_BAN},
            [](uint64_t gmId, const auto& args) {
                if (args.size() < 2) return;
                uint64_t targetId = std::stoull(args[0]);
                int duration = std::stoi(args[1]);
                banPlayer(gmId, targetId, duration);
            }
        });

        registerCommand({"spawn", 2, {Permission::GM_SPAWN},
            [](uint64_t gmId, const auto& args) {
                if (args.size() < 2) return;
                int itemId = std::stoi(args[0]);
                int count = std::stoi(args[1]);
                spawnItem(gmId, itemId, count);
            }
        });
    }

private:
    std::unordered_map<std::string, Command> commands_;
    PermissionChecker permissionChecker_;
};
```

---

## 三、KBEngine 权限

### 3.1 KBEngine 权限系统

```python
# KBEngine 风格的权限系统

class Permission:
    """权限定义"""

    # 基础权限
    MOVE = "move"
    ATTACK = "attack"
    CHAT = "chat"

    # VIP 权限
    VIP_CHAT = "vip_chat"
    VIP_TRADE = "vip_trade"

    # GM 权限
    GM_KICK = "gm_kick"
    GM_BAN = "gm_ban"
    GM_SPAWN = "gm_spawn"
    GM_TELEPORT = "gm_teleport"

    # 管理员
    ADMIN_ALL = "admin_all"


class Role:
    """角色定义"""

    ROLES = {
        "player": [Permission.MOVE, Permission.ATTACK, Permission.CHAT],
        "vip": [Permission.MOVE, Permission.ATTACK, Permission.CHAT,
                Permission.VIP_CHAT, Permission.VIP_TRADE],
        "gm": [Permission.MOVE, Permission.ATTACK, Permission.CHAT,
               Permission.GM_KICK, Permission.GM_BAN, Permission.GM_SPAWN,
               Permission.GM_TELEPORT],
        "admin": [Permission.ADMIN_ALL]
    }


class PermissionSystem:
    """权限系统"""

    def __init__(self):
        self.player_roles = {}  # player_id -> [roles]
        self.role_permissions = Role.ROLES

    def assign_role(self, player_id, role):
        """分配角色"""
        if player_id not in self.player_roles:
            self.player_roles[player_id] = []

        if role not in self.player_roles[player_id]:
            self.player_roles[player_id].append(role)

    def has_permission(self, player_id, permission):
        """检查权限"""
        roles = self.player_roles.get(player_id, [])

        for role in roles:
            if permission in self.role_permissions.get(role, []):
                return True

        return False

    def check_gm_command(self, player_id, command):
        """检查 GM 命令权限"""
        # GM 命令权限映射
        GM_PERMISSIONS = {
            "/kick": Permission.GM_KICK,
            "/ban": Permission.GM_BAN,
            "/spawn": Permission.GM_SPAWN,
            "/teleport": Permission.GM_TELEPORT,
        }

        perm = GM_PERMISSIONS.get(command)
        if perm:
            return self.has_permission(player_id, perm)

        return False

    def execute_gm_command(self, player_id, command, args):
        """执行 GM 命令"""
        if not self.check_gm_command(player_id, command):
            self.send_error(player_id, "Permission denied")
            return

        # 执行命令
        if command == "/kick":
            self.cmd_kick(player_id, args)
        elif command == "/ban":
            self.cmd_ban(player_id, args)
        # ...

    def cmd_kick(self, gm_id, args):
        """踢人命令"""
        target_id = int(args[0])
        KBEngine.kick(target_id)

        KBEngine.info(f"Player {gm_id} kicked player {target_id}")

    def cmd_ban(self, gm_id, args):
        """封禁命令"""
        target_id = int(args[0])
        duration = int(args[1])

        # 执行封禁
        ban_player(target_id, duration)

        KBEngine.info(f"Player {gm_id} banned player {target_id} for {duration}s")
```

---

## 四、总结

### 权限系统核心

```
权限系统 = RBAC 模型 + 命令检查 + 日志审计
- 角色分配权限
- 最小权限原则
- 所有关键操作检查
- 完整的审计日志
```

---

## 参考资料

- [RBAC NIST Standard](https://csrc.nist.gov/projects/role-based-access-control/)
- [OWASP Access Control](https://owasp.org/www-project-access-control/)
