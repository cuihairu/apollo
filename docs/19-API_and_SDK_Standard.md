# API 与 SDK 规范

> 目的：统一对外运营/GM API、内部 gRPC 接口及客户端 SDK 协议，明确命名、认证、版本、幂等策略，替换 `docs/12` 中混合的 REST/gRPC 描述。

## 1. 总体结构

```
┌──────────────────────────────────────────┐
│ 客户端 SDK (Unity/U3D)                   │
│  • TCP/KCP 协议 (NetCore)                │
│  • Protobuf 消息 (玩家实时)               │
└────────────────┬────────────────────────┘
                 │
┌────────────────▼────────────────────────┐
│ Gate/Zone/服务内部 gRPC/TCP 通信          │
│  • Protobuf RPC                          │
│  • Transport (NNG/TCP/IPC)               │
└────────────────┬────────────────────────┘
                 │
┌────────────────▼────────────────────────┐
│ 运维/运营 API (HTTP/gRPC)                │
│  • GM 控制台 / BI / SDK 后台             │
│  • 鉴权/审计/幂等                        │
└──────────────────────────────────────────┘
```

## 2. 内部 gRPC / TCP API

### 2.1 命名与版本
- `package apollo.gameservice.v1` 等形式，按服务划分（player, guild, chat...）。
- Proto 文件位于 `Tool/pb/internal`，生成 C++/Go/Python stub。

### 2.2 认证
- 内网服务通过 mTLS 或 Token（由 ServiceRegistry 下发），所有 RPC 附带 `metadata`（server_id, trace_id）。
- 在 Transport 层内置 ACL，避免未授权服务访问。

### 2.3 幂等和重试
- 请求需包含 `request_id`；服务端可记录最近请求，防止重复执行。
- 返回统一 `Status`（code, message）；错误码在 `base/status.proto` 定义，分 CLIENT/RETRY/FAILURE。

### 2.4 示例
```protobuf
service ZoneControl {
  rpc TransferPlayer(TransferPlayerRequest) returns (TransferPlayerResponse);
  rpc NotifySceneEvent(SceneEventRequest) returns (google.protobuf.Empty);
}
```

## 3. 客户端 SDK 接口

### 3.1 SDK 结构
- `ApolloClient`：管理连接、加密、心跳、断线重连。
- `MessageRouter`：注册消息 ID -> 回调（C# 事件）。
- `Serializer`：使用 Protobuf（`Tool/pb/fugumessage`）。
- 提供 Demo（Unity 包）+ 文档 + 自动化测试。

### 3.2 关键能力
- **连接**：支持 TCP/KCP（默认 TCP）；可选 WebSocket。
- **安全**：握手校验版本、设备、签名；握手后协商 AES/ChaCha。
- **心跳/保活**：SDK 自动发送心跳，超时触发重连。
- **幂等**：客户端发送消息时附带 `seq`，配合服务器 NetCore 去重。
- **灰度**：SDK 可获取 `slot` 信息（例如 A/B 测试），用于配置/资源选择。

### 3.3 API 示例（C#）
```csharp
var client = new ApolloClient(config);
client.OnConnected += ...;
client.RegisterHandler(PlayerMove.MsgId, msg => { ... });
await client.ConnectAsync();
client.Send(PlayerMove.MsgId, new PlayerMove { ... });
```

## 4. 运维/运营 API (HTTP/gRPC)

### 4.1 设计原则
- RESTful 或 gRPC（选定一种作为主接口，可根据场景决定）。对外建议使用 HTTP/JSON + gRPC Gateway。
- 所有接口必须鉴权（OAuth2/JWT/API Key/签名），并有 RBAC 控制。
- 提供统一错误码、请求/响应格式、审计日志。
- 与业务实时链路解耦（单独 Gateway/服务）。

### 4.2 基础结构
- API Gateway（内部部署）负责认证、限流、防刷、路由到具体服务。
- 每个服务提供 Swagger/Proto 描述，自动生成文档。

### 4.3 请求/响应格式
```json
POST /api/v1/players/unlock
Headers:
  Authorization: Bearer <token>
  X-Request-Id: <uuid>
Body:
{
  "player_id": 12345,
  "server_id": "1-2-6-1",
  "reason": "GM_UNLOCK"
}
Response:
{
  "code": 0,
  "message": "OK",
  "request_id": "..."
}
```
- 错误码按照 `StatusCode`（0 成功、1000-1999 客户端、2000+ 服务器）。
- 幂等：要求客户端提供 `X-Idempotency-Key`，服务端存储短期记录。

### 4.4 常见 API 分组
- **账号/封禁**：封号、解封、踢下线。
- **角色管理**：重置副本、修改属性、补发道具。
- **活动配置**：开启/关闭活动、灰度发布。
- **监控/操作**：查看服务器状态、触发脚本、重载配置。

## 5. 命名与版本策略
- Proto/HTTP 版本采用语义化（`v1`, `v1alpha`...），重大变更新增版本，旧版维护一段时间。
- SDK 使用 SemVer，接口变化写入 Release Note。
- 消息 ID（客户端）和 HTTP API 路径遵循统一命名规范，避免冲突。

## 6. 文档与工具
- 使用 `protoc` + `buf` 管理 IDL，生成 C++/C#/Go/Python/TS 等多语言代码。
- REST API 使用 OpenAPI 3.0 描述，自动生成客户端/文档。
- SDK 文档包括集成指南、协议说明、错误码、调试工具、示例场景。

## 7. 安全与审计
- 所有外部 API 请求/响应记录日志（带 request_id、user、时间、参数）。
- 关键操作（例如发放奖励）需二次确认或审批流程。
- TLS/HTTPS 全局开启；内部 gRPC 使用 mTLS。

## 8. Roadmap
1. 重写 `docs/12` API 文档，拆分内部 gRPC / 外部 HTTP ，并生成 Proto/OpenAPI。
2. 完成 Unity SDK 初版（结合 `NetCore` 设计），包括示例项目。
3. 建立认证/授权服务（OAuth2/Keycloak/自研），供 API Gateway 使用。
4. 完成自动化文档（buf/OpenAPI）+ CI 检查。
5. 逐步开放 GM/BI 接口，并集成审计/审批流程。

---

该规范将帮助服务器与客户端、运维 API 形成一致接口标准，支撑对外 SDK 与后台工具接入。***
