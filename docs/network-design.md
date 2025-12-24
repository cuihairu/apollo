# Apollo 网络层设计文档

## 1. 概述

Apollo 网络层提供了一个与具体网络库实现无关的抽象接口，支持多种网络后端（SSEngine/SDNet、Boost.Asio、libuv 等），实现网络库的可替换性。

### 1.1 设计目标

1. **抽象性**: 与具体网络库实现解耦，便于切换和测试
2. **性能**: 零拷贝设计，支持高性能异步 I/O
3. **扩展性**: 支持多种数据包格式和协议
4. **易用性**: 简洁的 API 设计，便于业务层使用

### 1.2 支持的网络后端

| 后端 | 平台 | 特性 | 状态 |
|------|------|------|------|
| SSEngine/SDNet | Windows/Linux | IOCP/epoll，高性能 | ✅ 已实现 |
| Boost.Asio | 跨平台 | 成熟稳定，社区活跃 | 🚧 计划中 |
| libuv | 跨平台 | Node.js 同款 | 🚧 计划中 |

## 2. 架构设计

```
┌─────────────────────────────────────────────────────────────────┐
│                        业务层 (Game Logic)                       │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Apollo 网络抽象层                          │
├─────────────────────────────────────────────────────────────────┤
│  NetworkManager  │  Listener  │  Connector  │  Session          │
├─────────────────────────────────────────────────────────────────┤
│  Connection  │  PacketParser  │  SessionFactory                  │
└─────────────────────────────────────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
┌───────────────┐     ┌─────────────────┐     ┌──────────────┐
│ SDNet Adapter │     │ Asio Adapter    │     │ libuv Adapter│
├───────────────┤     ├─────────────────┤     ├──────────────┤
│ ISSConnection │     │ asio::tcp_socket │     │ uv_tcp_t     │
│ ISSListener   │     │ asio::io_context│     │ uv_loop_s    │
│ ISSConnector  │     │                 │     │              │
└───────────────┘     └─────────────────┘     └──────────────┘
```

## 3. 核心接口

### 3.1 Connection (连接接口)

表示一个 TCP 网络连接。

```cpp
class Connection {
    virtual bool isConnected() const = 0;
    virtual int32_t send(const char* data, uint32_t length) = 0;
    virtual bool sendAsync(const char* data, uint32_t length) = 0;
    virtual void disconnect(const char* reason = nullptr) = 0;

    virtual const std::string& getRemoteAddress() const = 0;
    virtual uint16_t getRemotePort() const = 0;
    virtual uint64_t getConnectionId() const = 0;
};
```

### 3.2 Session (会话接口)

表示应用层的逻辑会话，负责处理业务逻辑。

```cpp
class Session {
    virtual void onEstablish(Connection* connection) = 0;
    virtual void onTerminate(TerminateReason reason) = 0;
    virtual uint32_t onRecv(const char* data, uint32_t length) = 0;
    virtual void onError(int32_t errorCode, const char* errorMsg) = 0;

    virtual bool isActive() const = 0;
    virtual void updateHeartbeat() = 0;
};
```

### 3.3 Listener (监听器接口)

负责监听端口，接受传入连接。

```cpp
class Listener {
    virtual bool start(const std::string& ip, uint16_t port) = 0;
    virtual bool stop() = 0;
    virtual bool isRunning() const = 0;

    virtual void setSessionFactory(SessionFactoryPtr factory) = 0;
    virtual void setPacketParser(PacketParserPtr parser) = 0;
    virtual void setCallbacks(const ListenerCallbacks& callbacks) = 0;
};
```

### 3.4 Connector (连接器接口)

负责主动发起连接。

```cpp
class Connector {
    virtual int32_t connect(const std::string& host, uint16_t port) = 0;
    virtual int32_t reconnect() = 0;
    virtual void disconnect() = 0;

    virtual ConnectorState getState() const = 0;
    virtual ConnectionPtr getConnection() const = 0;

    virtual void setCallbacks(const ConnectorCallbacks& callbacks) = 0;
};
```

### 3.5 PacketParser (数据包解析器接口)

处理粘包/半包问题，从字节流中解析完整数据包。

```cpp
struct ParseResult {
    bool isValid;
    const char* data;
    uint32_t length;
};

class PacketParser {
    virtual ParseResult parse(const char* data, uint32_t length) = 0;
    virtual void reset() = 0;
    virtual uint32_t getMaxPacketSize() const = 0;
};
```

### 3.6 NetworkManager (网络管理器)

网络层的统一入口，管理所有 Listener 和 Connector。

```cpp
class NetworkManager {
    virtual bool initialize(const NetworkConfig& config) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;

    virtual ListenerPtr createListener(const std::string& name) = 0;
    virtual ConnectorPtr createConnector(const std::string& name) = 0;

    virtual uint32_t broadcast(const char* data, uint32_t length,
                               const std::vector<uint64_t>& excludeIds = {}) = 0;
};
```

## 4. SSEngine/SDNet 适配

### 4.1 接口映射

| SSEngine 接口 | Apollo 抽象接口 | 说明 |
|---------------|-----------------|------|
| `ISSConnection` | `Connection` | TCP 连接 |
| `ISSSession` | `Session` | 会话 |
| `ISSSessionFactory` | `SessionFactory` | 会话工厂 |
| `ISSPacketParser` | `PacketParser` | 数据包解析 |
| `ISSListener` | `Listener` | 监听器 |
| `ISSConnector` | `Connector` | 连接器 |
| `ISSNet` | `NetworkManager` | 网络管理 |

### 4.2 使用方式

```cpp
// 1. 定义会话类型
class GameSession : public adapters::SDNetSession {
protected:
    void handleEstablish() override {
        // 连接建立
    }

    uint32_t handlePacket(const char* data, uint32_t length) override {
        // 处理数据包
        return length;
    }
};

// 2. 创建网络管理器
auto network = adapters::SDNetAdapter::createManager();
network->initialize();
network->start();

// 3. 创建监听器
auto listener = network->createListener("game");
listener->setSessionFactory(std::make_shared<TemplateSessionFactory<GameSession>>());
listener->start("0.0.0.0", 8080);
```

## 5. 数据包格式

### 5.1 默认格式

Apollo 网络层使用简单的包长度前缀格式：

```
┌──────────────┬─────────────────────────────────┐
│   Length     │          Payload Data           │
│   (4 bytes)  │        (Length bytes)           │
└──────────────┴─────────────────────────────────┘
```

- Length: 32位无符号整数，大端序，表示 Payload 长度
- Payload: 实际的业务数据

### 5.2 自定义解析器

可通过实现 `PacketParser` 接口自定义数据包格式：

```cpp
class CustomPacketParser : public PacketParser {
public:
    ParseResult parse(const char* data, uint32_t length) override {
        // 自定义解析逻辑
        // 返回 ParseResult::valid(data, size) 表示解析成功
        // 返回 ParseResult::needMore() 表示需要更多数据
        // 返回 ParseResult::invalid() 表示数据错误
    }
};
```

## 6. 线程模型

### 6.1 SSEngine/SDNet

- Windows: IOCP (I/O Completion Ports)
- Linux: epoll
- 支持多线程轮询

### 6.2 Boost.Asio

- `io_context::run()` 可在多线程中调用
- 通过 `strand` 保证线程安全

### 6.3 libuv

- `uv_run()` 默认单线程
- 可通过工作队列实现多线程

## 7. 性能考虑

1. **零拷贝**: 数据缓冲区使用 `const char*` 传递，避免不必要的复制
2. **对象池**: 会话对象可使用对象池复用
3. **批量发送**: `sendAsync` 支持异步批量发送
4. **连接复用**: 支持连接池管理

## 8. 迁移指南

### 8.1 从 SSEngine 迁移

| 旧代码 | 新代码 |
|--------|--------|
| `ISSConnection*` | `ConnectionPtr` |
| `pConnection->Send()` | `connection->send()` |
| `pConnection->Disconnect()` | `connection->disconnect()` |
| `OnEstablish()` | `session->onEstablish()` |
| `OnTerminate()` | `session->onTerminate()` |
| `OnRecv()` | `session->onRecv()` |

### 8.2 切换网络后端

只需修改一行代码：

```cpp
// 使用 SDNet
auto network = adapters::SDNetAdapter::createManager();

// 切换到 Asio (需要实现 AsioAdapter)
auto network = adapters::AsioAdapter::createManager();
```
