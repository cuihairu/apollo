# Apollo NetCore 网络模块设计

> 目标：替换旧的 SDNet/SSEngine 依赖，提供跨平台（Linux io_uring/epoll、Windows IOCP）的统一网络层，覆盖客户端链路、内部服务通信，并为 Unity/U3D SDK 提供稳定协议。

## 1. 设计目标
1. **统一抽象**：上层只依赖 `NetCore` 接口，底层在 Linux 选择 `io_uring`（fallback `epoll`），在 Windows 使用 IOCP。
2. **消息规范**：定义标准包头、序列号、压缩、加密、去重、QoS；客户端 SDK 与服务器共享 IDL。
3. **扩展能力**：内置限流、灰度、流量复制、负载均衡 Hook；支持 KCP/WebSocket 等扩展。
4. **监控/调试**：暴露连接状态、带宽、RTT、错误等指标；提供抓包/注入工具。
5. **与 Transport 集成**：内网服务间通信可以复用 `Transport` 层（NNG/SharedMemory），NetCore 专注于客户端链路与高性能 TCP/KCP。

## 2. 模块分层

```
┌────────────────────────────────────────────┐
│ NetCore API                                │
│  • Connection / Listener / SessionManager  │
│  • MessageCodec / PacketBuilder            │
│  • RateLimiter / QoS / Deduplicator        │
└──────────────┬─────────────────────────────┘
               │
┌──────────────▼──────────────┐   ┌──────────────────────────────┐
│ Linux Backend                │   │ Windows Backend              │
│  io_uring Executor           │   │ IOCP Executor                │
│  epoll (compat)              │   │ WSAPoll (compat)             │
└──────────────┬──────────────┘   └──────────────┬──────────────┘
               │                                   │
        ┌──────▼──────┐                      ┌─────▼──────┐
        │ TCP Driver  │                      │ TCP Driver │
        ├─────────────┤                      ├────────────┤
        │ KCP/UDP     │ (可选)               │ WSAPacket  │
        │ WebSocket   │ (可选)               │ WinHTTP(可选) │
        └─────────────┘                      └────────────┘
```

## 3. 核心接口

```cpp
struct NetConfig {
    uint32_t workerThreads = std::thread::hardware_concurrency();
    size_t sendBuffer = 64 * 1024;
    size_t recvBuffer = 64 * 1024;
    bool enableTLS = false;
    bool enableKCP = false;
    bool enableWebSocket = false;
    uint32_t maxConnections = 20000;
    std::string certificatePath;
    std::string privateKeyPath;
};

class INetListener {
public:
    virtual ~INetListener() = default;
    virtual bool Start(const NetConfig&) = 0;
    virtual void Stop() = 0;
};

class INetConnection {
public:
    virtual ~INetConnection() = default;
    virtual bool Send(const void* data, size_t len) = 0;
    virtual void Close() = 0;
    virtual ConnectionMetrics GetMetrics() const = 0;
};
```

### SessionManager
- 维护连接 ID、心跳、断线重连（支持 token 验证）。
- 对接 MessageDispatcher（客户端协议）与 Transport（内网协议）。

### MessageCodec / PacketBuilder
- 包头（16 bytes）：`uint16 length`, `uint16 msg_id`, `uint32 seq`, `uint32 ts`, `uint16 flags`, `uint16 checksum`.
- Flags 包含：压缩、加密、重传标记、保留位。
- 支持 Protobuf 序列化、批量包合并、分片。

### RateLimiter / Deduplicator
- 参考 `docs/01` 中的去重、限流策略，整合到 NetCore 层（带可配置阈值）。
- QoS：优先级队列 + 速率控制（令牌桶/漏桶），配合传输层背压机制。

## 4. Linux Backend 设计

### 4.1 io_uring
- 使用 `IORING_SETUP_SQPOLL` + `IORING_SETUP_COOP_TASKRUN` 提高效率。
- 提交队列处理：`accept`, `readv`, `writev`, `timeout`, `recvmsg` (KCP)。
- Buffer 管理：采用 `FixedBuffer` + `provided buffers`（`IORING_OP_PROVIDE_BUFFERS`），减少内存拷贝。
- 若内核版本 < 5.10，则 fallback 到 `epoll + eventfd + thread pool`。

### 4.2 线程模型
- `IOThread`：少量线程（1-2）负责 `io_uring_enter`。
- `WorkerThread`：处理业务回调（消息解码、分发），可以复用全局线程池。
- CPU 绑定：高负载场景可 pin 线程到固定核心。

## 5. Windows Backend 设计

### 5.1 IOCP
- `CreateIoCompletionPort` + 工作线程池。
- 每个连接使用 `WSABUF` + Overlapped；配合 `AcceptEx`, `ConnectEx`.
- 支持 `DisconnectEx` 平滑断开、超时处理。

### 5.2 KCP/WebSocket (可选)
- 在 Windows 上封装 KCP（用户态 UDP），或使用 WinSock UDP + io_uring-style overlapped。
- WebSocket 可基于 WinHTTP/WebSocket APIs 或第三方库（libwebsockets），但与 NetCore 解耦，作为可选模块。

## 6. 客户端 SDK 接入规范

### 6.1 SDK 特性
- Unity/U3D C# SDK 提供：连接/重连、心跳、包头封装、Protobuf 序列化、加密（AES/ChaCha20）与签名、压缩（LZ4）。
- 支持 TCP(默认)、KCP(低延迟)、WebSocket(可选)。
- 提供 Hook：网络事件（连接成功/失败、断线重连）、消息回调、调试日志。

### 6.2 握手流程
1. Client → Gate：`Hello` 包（ClientVersion、DeviceID、Token、Transport特性）。
2. Gate 返回 `HelloAck`（会话ID、加密参数、心跳间隔）。
3. 后续消息携带 `session_id`/`seq`，心跳包用于保活；断线后客户端可使用会话 Token 重连。

### 6.3 协议规范
- 使用同一套 Protobuf IDL（`Tool/pb`）。
- 消息 ID 与枚举由 IDL 自动生成，客户端 SDK 通过 stub 访问。
- SDK 提供 `Send<T>(msg_id, message)` API，内部封包、压缩、加密。

## 7. 安全与加密
- **传输层**：可选 TLS（OpenSSL / SChannel），在 GateServer 与客户端之间启用。
- **应用层**：握手后协商对称密钥（AES-256-GCM / ChaCha20-Poly1305），消息级别加密。
- **签名/防篡改**：包头包含 HMAC，防止中间人篡改。
- **防刷/限流**：GateServer 结合 IP/QPS 黑名单、令牌桶。

## 8. 灰度与流量复制
- NetCore 对每个连接维护标签（版本、区域、设备），可由 GateServer 的策略器决定转发到不同 GameServer。
- 支持“流量复制”模式：将真实客户端流量复制一份给测试服务器（仅透传，不影响主链路）。

## 9. 监控与调试
- 暴露指标：连接数、带宽、消息速率、RTT、重传、失败率、KCP 滑窗等。
- 提供调试端口：可抓取近期包内容（脱敏），或注入测试包。
- 日志：关键事件（连接建立/断开、异常、限流触发）写入统一日志模块。

## 10. 项目拆分
1. **阶段 1**：TCP (io_uring/IOCP) + 基础包格式 + SessionManager + Unity SDK 初版。
2. **阶段 2**：KCP/WebSocket 可选模块、TLS/加密、流量复制、限流。
3. **阶段 3**：零拷贝优化、共享内存直通、SDK 工具链（抓包、调试 UI）。
4. **阶段 4**：安全增强（HMAC、动态密钥）、自动化压测工具、与 Transport 层联动。

---

本设计替代旧有 SDNet 方案，为所有服务和 SDK 提供统一的网络基础，并为后续 AOI/Battle/Transport 等模块提供稳定的数据通道。***
