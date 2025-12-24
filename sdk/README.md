# Apollo SDK

Apollo 游戏服务器框架的多平台客户端 SDK。

## 支持的平台

- ✅ **Unity** - C# SDK
- ✅ **Cocos Creator** - TypeScript SDK
- ✅ **LayaBox/Air** - TypeScript SDK

## 目录结构

```
sdk/
├── unity/          # Unity SDK
├── cocos/          # Cocos Creator SDK
└── laya/           # LayaBox SDK
```

## 快速开始

### Unity SDK

```csharp
using ApolloSDK;

// 创建配置
var config = new ApolloClientConfig {
    ServerAddress = "localhost",
    Port = 8080
};

// 创建客户端
var client = new ApolloClient(config);

// 连接服务器
await client.ConnectAsync();

// 登录
var loginRequest = new LoginRequest {
    Username = "player1",
    Password = "password123"
};
var response = await client.LoginAsync(loginRequest);
```

### Cocos Creator SDK

```typescript
import { ApolloClient, ApolloClientConfig } from './ApolloSDK';

// 创建配置
const config = new ApolloClientConfig({
    serverAddress: 'localhost',
    port: 8080
});

// 创建客户端
const client = new ApolloClient(config);

// 连接服务器
await client.connect();

// 登录
const loginResponse = await client.login({
    username: 'player1',
    password: 'password123'
});
```

### LayaBox SDK

```typescript
import { ApolloClient, ApolloClientConfig } from './ApolloSDK';

// 创建配置
const config = new ApolloClientConfig({
    serverAddress: 'localhost',
    port: 8080
});

// 创建客户端
const client = new ApolloClient(config);

// 连接服务器
this.client.connect().then((success) => {
    if (success) {
        // 登录
        this.client.login({
            username: 'player1',
            password: 'password123'
        }).then((response) => {
            console.log('Login result:', response);
        });
    }
});
```

## 功能特性

### 核心功能

- ✅ WebSocket 连接管理
- ✅ 自动重连机制
- ✅ 心跳保活
- ✅ 消息编解码（支持 Protobuf 和 JSON）
- ✅ 消息路由和分发
- ✅ 用户认证
- ✅ 网络统计

### 事件系统

```typescript
// 连接事件
client.on('connected', () => {
    console.log('Connected to server');
});

client.on('disconnected', () => {
    console.log('Disconnected from server');
});

client.on('error', (error) => {
    console.error('Network error:', error);
});

client.on('heartbeatTimeout', () => {
    console.warn('Heartbeat timeout');
});
```

### 消息发送和接收

```typescript
// 发送消息
await client.send(messageId, messageData);

// 注册消息处理器
client.registerHandler(messageId, (message) => {
    console.log('Received message:', message);
});
```

## 配置选项

| 选项 | 类型 | 默认值 | 描述 |
|------|------|--------|------|
| `serverAddress` | string | 'localhost' | 服务器地址 |
| `port` | number | 8080 | 服务器端口 |
| `useSSL` | boolean | false | 是否使用 WSS |
| `connectTimeout` | number | 10000 | 连接超时(ms) |
| `heartbeatInterval` | number | 30000 | 心跳间隔(ms) |
| `reconnectAttempts` | number | 3 | 重连尝试次数 |
| `reconnectInterval` | number | 5000 | 重连间隔(ms) |
| `logLevel` | LogLevel | INFO | 日志级别 |
| `enableAutoReconnect` | boolean | true | 自动重连 |

## 文档

详细文档请查看各平台 SDK 目录：
- [Unity SDK 文档](./unity/README.md)
- [Cocos SDK 文档](./cocos/README.md)
- [LayaBox SDK 文档](./laya/README.md)

## 许可证

MIT License
