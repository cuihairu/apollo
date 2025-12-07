# Apollo 进程间通信 Transport 设计

> 目标：抽象服务器之间的通信方式（TCP/IP、IPC、共享内存……），支持能力注册、优先级选择、QoS/背压配置。当前实现基于 NNG，接口预留未来扩展。

## 1. 设计原则
1. **能力协商**：每个进程声明自己支持的 transport 列表（协议、地址、优先级、属性），对端根据本地能力自动选择最佳方案。
2. **统一接口**：业务层只依赖 `ITransport`、`IConnection`、`IListener`，无需关心 NNG/TCP/SHM 的细节。
3. **可扩展**：初期实现 `nng-tcp`、`nng-ipc`，未来可补充共享内存、Relay、KCP 等协议，而无需修改业务代码。
4. **QoS/背压**：连接对象需暴露高水位线、队列长度、统计指标；注册信息可描述带宽/延迟，供调度器参考。

## 2. Transport 接口

```cpp
struct TransportEndpoint {
    std::string protocol;    // e.g. "nng-tcp", "nng-ipc", "shm"
    std::string address;     // "tcp://10.0.0.5:6100", "ipc://zone-1.ipc"
    int priority = 100;      // 越小越优先
    std::map<std::string, std::string> attributes; // latency/bandwidth/locality…
};

class IConnection {
public:
    virtual ~IConnection() = default;
    virtual bool Send(const void* data, size_t len) = 0;
    virtual bool Recv(void* buffer, size_t* len) = 0;
    virtual void Close() = 0;

    // 背压/QoS
    virtual void SetHighWaterMark(size_t messages) = 0;
    virtual void SetPriority(int priority) = 0;
    virtual ConnectionStats GetStats() const = 0;
};

class IListener {
public:
    virtual ~IListener() = default;
    virtual bool Start(std::function<void(std::unique_ptr<IConnection>)> onAccept) = 0;
    virtual void Stop() = 0;
};

class ITransport {
public:
    virtual ~ITransport() = default;
    virtual std::string Name() const = 0;

    virtual bool CanDial(const TransportEndpoint& ep) const = 0;
    virtual std::unique_ptr<IConnection> Dial(const TransportEndpoint& ep) = 0;

    virtual bool CanListen(const TransportEndpoint& ep) const = 0;
    virtual std::unique_ptr<IListener> Listen(const TransportEndpoint& ep) = 0;
};
```

### 默认实现（v1）
- `NNGTcpTransport`：使用 NNG TCP transport (`tcp://host:port`)，跨服务器通信默认使用。
- `NNGIPCTransport`：使用 NNG IPC（Linux: Unix socket；Windows: 命名管道），同机通信优先。
- `ShmTransport`：预留接口，但暂未实现；注册信息中可包含 `protocol: "shm"` 以方便后续。

### 背压/统计
- `SetHighWaterMark` 映射到 NNG 的 `NNG_OPT_SENDBUF/NNG_OPT_RECVBUF` 或共享内存队列容量。
- `ConnectionStats` 包含：发送/接收消息数、队列长度、最后延迟、重连次数等，供监控和调度使用。

## 3. 服务注册与能力描述

### 注册信息结构
```json
{
  "server_id": "1-2-6-1",
  "service": "ZoneServer",
  "host": "10.0.0.5",
  "transports": [
    {
      "protocol": "nng-ipc",
      "address": "ipc://zone-1.ipc",
      "priority": 0,
      "attributes": {
        "locality": "same-host",
        "latency": "low",
        "max_msg_bytes": "1048576"
      }
    },
    {
      "protocol": "nng-tcp",
      "address": "tcp://10.0.0.5:6100",
      "priority": 10,
      "attributes": {
        "bandwidth": "high",
        "latency": "medium"
      }
    }
  ],
  "metadata": {
    "process": "zone_server",
    "version": "1.0.0",
    "heartbeat": 1717830000
  }
}
```

### 注册流程
1. 进程启动后，构建自己的 `TransportEndpoint` 列表（至少一个）。
2. 调用 ServiceRegistry（Consul/Etcd/ZK）注册，并定期刷新心跳。后续可支持本地文件/Envoy LDS 等方式。
3. 若 transport 配置变更（如 IPC 文件重命名、端口变化），需更新注册信息。

### 消费流程
1. 调用 ServiceDiscovery 查询目标服务，获取其 transport 列表。
2. 结合本地 transport 能力和 `priority`/`attributes` 按优先级排序：
   - 同机 → 使用 `nng-ipc`
   - 同机且共享内存可用 → 将来可尝试 `shm`
   - 跨机 → fallback `nng-tcp`
3. 调用对应 `ITransport::Dial` 建立连接；若失败，继续尝试下一候选。
4. 建立成功后，业务层只看 `IConnection` 接口；同时记录采用了哪种 transport 以便监控。

## 4. 能力协商与特性

在 `Dial` 阶段可进行握手，交换如下信息：
- **最大消息大小**：防止发送端超过接收端承受能力。
- **批量/合并包能力**：决定是否开启批量发送或零拷贝。
- **安全属性**：是否启用加密/签名（可选）。
- **扩展特性**：例如压缩、共享内存通道、Relay 支持等。

协商方式：在第一次发送的数据帧中附带 `HandshakeFrame`，双方确认后进入正常数据帧模式。

## 5. 监控与运维
- 每个连接统计项：吞吐、延迟、队列、重连次数、使用的 transport。
- 定期将统计信息上报到监控系统（Prometheus 或自研）。
- 当某 transport 异常（连接失败率高、延迟异常）时可自动降级或标记为 unhealthy，触发 ServiceRegistry 更新。
- 支持动态调整：例如命令行/GM 可在运行时重新设置某进程的可用 transport、优先级等。

## 6. Roadmap
1. **Phase 1**：实现 `nng-tcp`、`nng-ipc` transport；接入服务注册/发现；支持优先级选择和基本背压。
2. **Phase 2**：扩展 `shm` transport（共享内存 ring buffer + 控制通道）；定义握手协议、零拷贝接口。
3. **Phase 3**：加上 Relay/加速 transport、QoS 策略、动态策略下发（如根据业务类型选择不同传输）。
4. **Phase 4**：安全扩展（mutual TLS、ACL）和更完整的监控、调试工具。

---

本设计为后续通信框架的基础。短期内所有进程都使用 NNG transport，同时通过注册能力预留共享内存等更快的通道。下一步可在实施计划中加入“Transport 层开发”任务，并在 NetCore/AOI 等文档引用此设计。***
