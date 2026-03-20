# Q88: 如何防止 SQL 注入？

## 问题分析

本题考察对 SQL 注入防护的理解：
- 注入原理
- 参数化查询
- ORM 使用
- 输入验证

---

## 一、SQL 注入原理

### 1.1 常见手法

```
┌─────────────────────────────────────────────────────────────┐
│                    SQL 注入示例                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ❌ 不安全的查询:                                           │
│  query = "SELECT * FROM users WHERE name = '" + name + "'"  │
│                                                             │
│  输入: name = "admin' OR '1'='1"                            │
│  结果: SELECT * FROM users WHERE name = 'admin' OR '1'='1'│
│        → 返回所有用户 (绕过认证)                            │
│                                                             │
│  输入: name = "admin'; DROP TABLE users; --"                │
│  结果: SELECT * FROM users WHERE name = 'admin';           │
│         DROP TABLE users; --'                                │
│        → 删除 users 表                                       │
│                                                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  ✅ 安全的参数化查询:                             │       │
│  │  query = "SELECT * FROM users WHERE name = ?"      │       │
│  │  execute(query, [name])                             │       │
│  │                                                    │       │
│  │  数据库驱动会正确转义输入                           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、防护方法

### 2.1 参数化查询

```cpp
// 参数化查询 (C++)

class SafeDatabase {
public:
    User* getUserByUsername(const std::string& username) {
        // ✅ 参数化查询
        const char* query = "SELECT * FROM users WHERE username = ?";

        PreparedStatement stmt = prepareStatement(query);
        stmt.bindString(1, username);

        ResultSet rs = stmt.executeQuery();

        if (rs.next()) {
            User* user = new User();
            user->id = rs.getInt("id");
            user->username = rs.getString("username");
            user->password = rs.getString("password");
            return user;
        }

        return nullptr;
    }

    // ❌ 不安全的字符串拼接
    User* getUserByUsername_BAD(const std::string& username) {
        std::string query = "SELECT * FROM users WHERE username = '";
        query += username;
        query += "'";

        // SQL 注入风险！
        ResultSet rs = executeQuery(query);
        // ...
    }
};
```

### 2.2 ORM 使用

```python
# 使用 ORM 防止 SQL 注入

from sqlalchemy import create_engine, Column, Integer, String
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker

Base = declarative_base()

class User(Base):
    __tablename__ = 'users'

    id = Column(Integer, primary_key=True)
    username = Column(String(80), unique=True)
    password = Column(String(256))

# ORM 查询 (自动参数化)
def get_user_by_username(username):
    # ✅ ORM 自动处理参数化
    user = session.query(User).filter(User.username == username).first()
    return user

# 等价于: SELECT * FROM users WHERE username = ?
```

### 2.3 输入验证

```cpp
// 输入验证

class InputValidator {
public:
    static std::string sanitizeUsername(const std::string& username) {
        // 1. 检查长度
        if (username.empty() || username.length() > 32) {
            throw std::invalid_argument("Invalid username length");
        }

        // 2. 检查字符
        for (char c : username) {
            if (!isalnum(c) && c != '_' && c != '-') {
                throw std::invalid_argument("Invalid username characters");
            }
        }

        // 3. 检查保留字
        if (isReservedWord(username)) {
            throw std::invalid_argument("Username is reserved");
        }

        return username;
    }

    static int validateId(const std::string& idStr) {
        try {
            int id = std::stoi(idStr);
            if (id <= 0) {
                throw std::invalid_argument("ID must be positive");
            }
            return id;
        } catch (...) {
            throw std::invalid_argument("Invalid ID format");
        }
    }
};
```

---

## 三、总结

### SQL 注入防护

```
防 SQL 注入 = 参数化查询 + ORM + 输入验证 + 最小权限
- 永远使用参数化
- ORM 自动处理
- 验证所有输入
- 数据库用户最小权限
```

---

## 参考资料

- [OWASP SQL Injection](https://owasp.org/www-community/attacks/SQL_Injection)
- [SQLAlchemy](https://www.sqlalchemy.org/)
