# Unity SDK 设计与接口规范

> 目的：在 NetCore 与 API 规范的基础上，定义 Unity/U3D 客户端 SDK 的模块划分、公共 API、事件、调试工具与资源管理接口，确保客户端团队能够快速接入，并与服务器协议保持一致。

## 1. 模块结构

```
ApolloSDK (Unity Package)
 ├─ Network
 │    ├─ ApolloClient (TCP/KCP/WebSocket)
 │    ├─ ConnectionConfig / RetryPolicy
 │    ├─ PacketEncoder / Decoder
 │    └─ HeartbeatManager
 ├─ Messaging
 │    ├─ MessageRouter
 │    ├─ ProtobufSerializer (Generated via protoc)
 │    └─ HandlerRegistry
 ├─ Session
 │    ├─ AuthManager (token 登录)
 │    ├─ SessionStore (重连)
 │    └─ MetricsCollector
 ├─ Resource
 │    ├─ PatchManager (引用 docs/26)
 │    ├─ ManifestLoader
 │    └─ Storage (cache)
 ├─ Utilities
 │    ├─ Logger
 │    ├─ Diagnostics (抓包/日志上报)
 │    └─ Config (热更新配置)
 └─ Samples / Editor Tools
```

## 2. 初始化流程

```csharp
var config = new ApolloClientConfig {
    ServerAddress = "game.example.com",
    Port = 7700,
    Protocol = TransportProtocol.Tcp,
    HeartbeatInterval = TimeSpan.FromSeconds(15),
    Encryption = EncryptionMode.Aes256,
    ResourceSlot = "A"
};

var client = new ApolloClient(config);
client.RegisterHandler(PlayerMove.MsgId, OnPlayerMove);
client.OnConnected += HandleConnected;
client.OnDisconnected += HandleDisconnected;
await client.ConnectAsync();

await client.LoginAsync(loginRequest); // 自动走 AuthManager
```

## 3. 核心 API

### 3.1 ApolloClient
| 方法 | 说明 |
|------|------|
| `Task ConnectAsync()` | 建立连接（支持 TCP/KCP/WebSocket）。 |
| `Task DisconnectAsync()` | 主动断开。 |
| `Task SendAsync<T>(ushort msgId, T message)` | 序列化并发送 Protobuf 消息。 |
| `void RegisterHandler(ushort msgId, Action<T> handler)` | 注册消息处理。 |
| `void UnregisterHandler(ushort msgId)` | 移除处理器。 |
| `event Action Connected/Disconnected/HeartbeatTimeout` | 连接事件。 |

### 3.2 AuthManager
| 方法 | 说明 |
|------|------|
| `Task<LoginResponse> LoginAsync(LoginRequest request)` | 登录并建立会话。 |
| `Task<TokenRefreshResponse> RefreshTokenAsync()` | 刷新凭证。 |
| `Task LogoutAsync()` | 主动登出。 |

### 3.3 PatchManager
参见 `docs/26`：提供 `CheckUpdate()`, `DownloadAsync()`, `GetBundle(string name)` 等接口，支持灰度。

## 4. 消息路由与 Protobuf

- 使用 `protoc` + `grpc_csharp_plugin` 生成 C# Protobuf 类，生成脚本由 build pipeline 自动运行。
- 消息 ID/枚举来自 `docs/22` 的统一配置；SDK 在构建时引入生成文件（`MessageId.cs`）。
- `MessageRouter` 维护 `Dictionary<ushort, IMessageHandler>`，处理多线程上下文问题（Unity 主线程 vs 网络线程）。

## 5. 连接与重连策略

- `RetryPolicy` 可配置最大重连次数/间隔；支持指数退避。
- 重连时携带 `session_token` 与最后 ack seq，服务器根据 NetCore 去重逻辑恢复。
- 若超过重连次数，触发 `Disconnected` 事件，UI 提示。

## 6. 安全

- SDK 内置 AES/ChaCha 加密模块；握手阶段接收服务器返回的密钥/IV。
- 包头包含 HMAC；SDK 自动计算。
- 资源 Manifest 验证签名，防篡改。

## 7. 调试与诊断

- Logger：提供多级别日志，支持写入文件/上传服务器（可配置）。
- Diagnostics：可启用抓包模式（记录最近 N 条消息）用于问题排查。
- 提供 Unity Editor 工具窗口（`Window -> Apollo SDK Diagnostics`）：显示连接状态、心跳、延迟、消息统计。
- 支持在 Editor 模式下模拟网络状况（延迟、丢包），调用 NetCore 调试接口。

## 8. 示例与文档

- 包含 `Samples` 目录：示例项目演示登录、移动、战斗、资源更新流程。
- 文档（Markdown/HTML）：安装、API 参考、常见问题、错误码（对应服务器 `StatusCode`）。
- 提供 CI pipeline（Unity 构建 + 测试）以验证 SDK。

## 9. 发布与版本

- SDK 采用 SemVer；与服务器版本通过 `sdk_version` 字段校验。
- 发布形式：Unity Package（UPM）和 `.unitypackage`。
- 提供 Release Note，说明兼容服务器版本、变更项。

## 10. Roadmap
1. 实现核心网络层（TCP/KCP/心跳/重连），与 NetCore 对接。
2. 集成 Auth/Patch/Resource 模块。
3. 打通示例场景，完成自动化测试。
4. 发布 1.0 内测版，与服务器联调；收集问题迭代。
5. 提供 CLI/编辑器工具、诊断日志系统。

---

该文档为 Unity 客户端团队提供明确的 SDK 接入蓝图，确保与服务器协议、配置、资源体系保持一致。***
