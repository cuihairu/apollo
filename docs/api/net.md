---
title: Network API
icon: network
prev: /api/README.md
---

# Network API

## apollo::net::tcp::Server

```cpp
namespace apollo::net::tcp {
class Server {
public:
    Server();

    // 启动服务器
    void start(const std::string& host, uint16_t port);

    // 停止服务器
    void stop();

    // 回调
    using ConnectCallback = std::function<void(ConnectionPtr)>;
    using MessageCallback = std::function<void(ConnectionPtr, const Buffer&)>;
    using DisconnectCallback = std::function<void(ConnectionPtr)>;

    void onConnect(ConnectCallback cb);
    void onMessage(MessageCallback cb);
    void onDisconnect(DisconnectCallback cb);

    // 广播
    void broadcast(const Buffer& msg);

    // 获取连接数
    size_t connectionCount() const;
};
}
```

**线程安全**: ⚠️ 回调在网络线程执行

---

## apollo::net::tcp::Client

```cpp
namespace apollo::net::tcp {
class Client {
public:
    Client();

    // 连接
    using ConnectCallback = std::function<void(bool)>;
    void connect(const std::string& host, uint16_t port, ConnectCallback cb);

    // 断开
    void disconnect();

    // 发送
    void send(const Buffer& msg);
    void send(const std::string& msg);

    // 回调
    using MessageCallback = std::function<void(const Buffer&)>;
    void onMessage(MessageCallback cb);

    // 连接状态
    bool isConnected() const;
};
}
```

**线程安全**: ✅

---

## apollo::net::http::Server

```cpp
namespace apollo::net::http {
class Server {
public:
    Server();

    // 启动
    void start(const std::string& host, uint16_t port);

    // 路由
    using Handler = std::function<void(const Request&, Response&)>;
    void get(const std::string& path, Handler handler);
    void post(const std::string& path, Handler handler);
    void put(const std::string& path, Handler handler);
    void delete_(const std::string& path, Handler handler);

    // 停止
    void stop();
};
}
```

**线程安全**: ⚠️ 回调在网络线程执行

---

## apollo::net::http::Request

```cpp
namespace apollo::net::http {
class Request {
public:
    // 方法
    Method method() const;  // GET, POST, PUT, DELETE

    // URL
    std::string url() const;
    std::string path() const;

    // 查询参数
    std::string query(const std::string& key) const;
    template<typename T>
    T param(const std::string& key) const;

    // 头部
    std::string header(const std::string& key) const;

    // Body
    std::string body() const;
    template<typename T>
    T body() const;
};
}
```

---

## apollo::net::http::Response

```cpp
namespace apollo::net::http {
class Response {
public:
    // 状态码
    Response& status(int code);

    // 头部
    Response& set(const std::string& key, const std::string& value);

    // Body
    Response& send(const std::string& body);
    Response& send(const json& body);

    // JSON
    Response& json(const json& data);
};
}
```

---

## apollo::net::http::Client

```cpp
namespace apollo::net::http {
class Client {
public:
    Client();

    // GET
    Response get(const std::string& url);
    Response get(const std::string& url, const Headers& headers);

    // POST
    Response post(const std::string& url, const std::string& body);
    Response post(const std::string& url, const json& body);

    // PUT
    Response put(const std::string& url, const std::string& body);

    // DELETE
    Response delete_(const std::string& url);

    // 设置超时
    void setTimeout(int milliseconds);
};
}
```

**线程安全**: ⚠️ 建议每个线程使用独立的 Client

---

## apollo::net::ws::Server

```cpp
namespace apollo::net::ws {
class Server {
public:
    Server();

    // 启动
    void start(const std::string& host, uint16_t port);

    // 回调
    using ConnectCallback = std::function<void(ConnectionPtr)>;
    using MessageCallback = std::function<void(ConnectionPtr, const std::string&)>;
    using CloseCallback = std::function<void(ConnectionPtr)>;

    void onConnect(ConnectCallback cb);
    void onMessage(MessageCallback cb);
    void onClose(CloseCallback cb);

    // 广播
    void broadcast(const std::string& msg);

    // 停止
    void stop();
};
}
```

**线程安全**: ⚠️ 回调在网络线程执行

---

## apollo::net::ws::Connection

```cpp
namespace apollo::net::ws {
class Connection {
public:
    // 发送
    void send(const std::string& msg);
    void send(const std::vector<uint8_t>& data);

    // 关闭
    void close();

    // 连接信息
    std::string remoteAddress() const;
    uint16_t remotePort() const;
};
}
```

**线程安全**: ✅
