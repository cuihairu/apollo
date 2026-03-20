# Q34: 数据库连接池如何设计？

## 问题分析

本题考察对数据库连接池的理解：
- 连接池的作用和原理
- 连接池参数配置
- 连接泄漏预防
- 高性能连接池实现

---

## 一、连接池原理

### 1.1 为什么需要连接池

```
┌─────────────────────────────────────────────────────────────┐
│                连接池的必要性                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  问题: 建立 TCP 连接开销大                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  TCP 三次握手: ~50ms                              │       │
│  │  MySQL 握手: ~50ms                               │       │
│  │  认证授权: ~20ms                                  │       │
│  │  总计: ~100ms+                                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  解决方案: 复用连接                                          │
│  ┌─────────────────────────────────────────────────┐       │
│  │  预先创建一批连接                                    │       │
│  │  使用时从池中获取                                   │       │
│  │  用完后归还池中                                     │       │
│  │                                                   │       │
│  │  优势:                                            │       │
│  │  ├── 减少连接开销 (100ms → <1ms)                   │       │
│  │  ├── 限制连接数，防止 DB 过载                      │       │
│  │  └── 统一管理连接生命周期                          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、连接池实现

### 2.1 基础连接池

```cpp
// 数据库连接池

class ConnectionPool {
public:
    struct Config {
        std::string host;
        uint16_t port = 3306;
        std::string database;
        std::string user;
        std::string password;
        size_t minConnections = 2;
        size_t maxConnections = 10;
        uint32_t connectionTimeout = 5000;
        uint32_t idleTimeout = 300000; // 5分钟
    };

    ConnectionPool(const Config& config) : config_(config) {
        // 初始化连接池
        for (size_t i = 0; i < config_.minConnections; i++) {
            connections_.push_back(createConnection());
        }

        // 启动清理线程
        cleanupThread_ = std::thread(&ConnectionPool::cleanupLoop, this);
    }

    ~ConnectionPool() {
        running_ = false;
        if (cleanupThread_.joinable()) {
            cleanupThread_.join();
        }

        // 关闭所有连接
        for (auto* conn : connections_) {
            delete conn;
        }
    }

    // 获取连接
    Connection* acquire() {
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待可用连接
        condition_.wait(lock, [this] {
            return !idleConnections_.empty() ||
                   connections_.size() < config_.maxConnections;
        });

        // 从空闲连接获取
        if (!idleConnections_.empty()) {
            Connection* conn = idleConnections_.front();
            idleConnections_.pop();
            activeConnections_.insert(conn);

            // 检查连接是否有效
            if (!conn->isAlive()) {
                delete conn;
                return acquire(); // 递归获取新连接
            }

            return conn;
        }

        // 创建新连接
        Connection* conn = createConnection();
        connections_.push_back(conn);
        activeConnections_.insert(conn);

        return conn;
    }

    // 归还连接
    void release(Connection* conn) {
        std::lock_guard<std::mutex> lock(mutex_);

        activeConnections_.erase(conn);
        idleConnections_.push(conn);

        // 通知等待线程
        condition_.notify_one();
    }

private:
    Connection* createConnection() {
        Connection* conn = new Connection();
        conn->connect(config_.host, config_.port,
                      config_.database, config_.user, config_.password);
        return conn;
    }

    void cleanupLoop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(30));

            std::lock_guard<std::mutex> lock(mutex_);

            // 移除过期空闲连接
            size_t beforeSize = idleConnections_.size();
            while (!idleConnections_.empty() &&
                   idleConnections_.size() > config_.minConnections) {
                Connection* conn = idleConnections_.front();
                idleConnections_.pop();

                if (conn->idleTime() > config_.idleTimeout) {
                    connections_.erase(
                        std::find(connections_.begin(), connections_.end(), conn)
                    );
                    delete conn;
                }
            }
        }
    }

    Config config_;
    std::vector<Connection*> connections_;
    std::queue<Connection*> idleConnections_;
    std::unordered_set<Connection*> activeConnections_;

    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread cleanupThread_;
    std::atomic<bool> running_{true};
};
```

### 2.2 RAII 连接管理

```cpp
// RAII 连接管理器

class ScopedConnection {
public:
    ScopedConnection(ConnectionPool& pool)
        : pool_(pool), connection_(pool.acquire()) {}

    ~ScopedConnection() {
        pool_.release(connection_);
    }

    // 箭头操作符
    Connection* operator->() {
        return connection_;
    }

    // 禁止拷贝
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

private:
    ConnectionPool& pool_;
    Connection* connection_;
};

// 使用示例
void queryPlayerData(uint64_t playerId) {
    ConnectionPool& pool = ConnectionPool::instance();
    ScopedConnection conn(pool);

    auto result = conn->query(fmt::format(
        "SELECT * FROM player WHERE id = {}", playerId
    ));

    // 自动归还连接
}
```

---

## 三、连接池配置

### 3.1 参数说明

```
┌─────────────────────────────────────────────────────────────┐
│                连接池参数配置                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  minConnections (最小连接数):                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  保持的最小空闲连接数                               │       │
│  │  建议: 核心数 / 2                                │       │
│  │  例如: 8核 → 4                                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  maxConnections (最大连接数):                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  最大连接数，包括空闲和活跃                         │       │
│  │  建议: 核心数 × 2 ~ 4                            │       │
│  │  例如: 8核 → 16-32                                │       │
│  │                                                   │       │
│  │  注意: 不能超过数据库 max_connections            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  connectionTimeout (连接超时):                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  获取连接的最长等待时间                             │       │
│  │  建议: 1-5 秒                                     │       │
│  │  超时返回错误，避免无限等待                          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  idleTimeout (空闲超时):                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  空闲连接超时时间                                   │       │
│  │  建议: 5-10 分钟                                   │       │
│  │  超时的连接会被回收                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 KBEngine 连接池

```cpp
// KBEngine 数据库连接池
// src/server/dbmgr/dbmgr.cpp

namespace KBEngine {

class DBMgr {
public:
    // 初始化连接池
    bool initialize(const std::string& host, uint16_t port,
                    const std::string& database,
                    const std::string& user, const std::string& password) {
        // 配置连接参数
        db_host_ = host;
        db_port_ = port;
        db_name_ = database;
        db_user_ = user;
        db_password_ = password;

        // 创建连接池
        size_t poolSize = g_kbeSrvConfig.dbDeadLockRetries();
        for (size_t i = 0; i < poolSize; i++) {
            MySQLConnection* conn = createConnection();
            if (conn) {
                connectionPool_.push_back(conn);
            }
        }

        return !connectionPool_.empty();
    }

private:
    MySQLConnection* createConnection() {
        MySQLConnection* conn = new MySQLConnection();

        if (conn->connect(db_host_, db_port_, db_name_,
                         db_user_, db_password_)) {
            return conn;
        }

        delete conn;
        return nullptr;
    }

    std::vector<MySQLConnection*> connectionPool_;
};

} // namespace KBEngine
```

---

## 四、最佳实践

### 4.1 连接池最佳实践

| 实践 | 说明 |
|------|------|
| **设置合理上限** | 不超过数据库 max_connections |
| **定期检查连接** | 清理无效连接 |
| **使用 RAII** | 自动归还连接 |
| **监控连接数** | 避免连接泄漏 |
| **设置超时** | 避免永久等待 |

### 4.2 连接泄漏预防

```cpp
// 连接泄漏检测

class ConnectionPoolLeakDetector {
public:
    ~ConnectionPoolLeakDetector() {
        // 检查是否有未归还的连接
        std::lock_guard<std::mutex> lock(mutex_);

        if (!activeConnections_.empty()) {
            LOG_ERROR("Connection leak detected! "
                      + std::to_string(activeConnections_.size()) +
                      " connections not released");

            // 打印调用栈
            for (const auto& [conn, stack] : activeConnections_) {
                LOG_ERROR("Leaked connection allocated at:\n" + stack);
            }
        }
    }

    void registerAcquisition(Connection* conn) {
        std::lock_guard<std::mutex> lock(mutex_);

        activeConnections_[conn] = getCurrentStackTrace();
    }

    void registerRelease(Connection* conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        activeConnections_.erase(conn);
    }

private:
    std::unordered_map<Connection*, std::string> activeConnections_;
    std::mutex mutex_;
};
```

---

## 五、总结

### 连接池关键参数

| 参数 | 推荐值 | 说明 |
|------|--------|------|
| 最小连接 | 核心数/2 | 保持基线连接 |
| 最大连接 | 核心数×2-4 | 峰值承载 |
| 连接超时 | 1-5秒 | 避免永久等待 |
| 空闲超时 | 5-10分钟 | 回收空闲连接 |

---

## 参考资料

- [MySQL 连接池最佳实践](https://dev.mysql.com/doc/)
- [HikariCP 连接池](https://github.com/brettwooldridge/HikariCP)
