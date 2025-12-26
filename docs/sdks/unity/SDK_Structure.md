# Apollo SDK 目录结构

## 概述

`skds/` 目录存放各平台的客户端 SDK，与服务端框架保持协议和接口一致。

## 目录结构

```
skds/
├── unity/                    # Unity SDK
│   └── ApolloSDK/
│       ├── ApolloClient.cs         # 主客户端
│       ├── ApolloClientConfig.cs   # 配置
│       ├── Network/               # 网络模块
│       │   └── NetworkManager.cs
│       ├── Session/               # 会话管理
│       │   └── AuthManager.cs
│       ├── Attributes/            # 属性系统
│       │   ├── AttributeValue.cs
│       │   ├── AttributeContainer.cs
│       │   ├── AttributeSyncManager.cs
│       │   └── ...
│       ├── Messaging/             # 消息处理
│       └── Utilities/             # 工具类
│
├── cocos/                   # Cocos Creator SDK
│   └── ApolloSDK/
│
├── laya/                    # LayaBox SDK
│   └── ApolloSDK/
│   └── ApolloSDK/
│
└── web/                     # Web SDK (TODO)
    └── ApolloSDK/
```

## Unity SDK 模块说明

| 模块 | 功能 | 状态 |
|------|------|------|
| `ApolloClient.cs` | 主入口，连接、认证、心跳 | ✅ 已有 |
| `ApolloClientConfig.cs` | 配置类 | ✅ 已有 |
| `Network/NetworkManager.cs` | TCP/Socket 连接管理 | ✅ 已有 |
| `Session/AuthManager.cs` | 登录认证 | ✅ 已有 |
| `Attributes/` | 属性同步系统 | 🚧 开发中 |
| `Messaging/` | Protobuf 消息路由 | 📋 计划中 |
| `Utilities/` | 日志、定时器等 | 📋 计划中 |

## 文档规范

- SDK 设计文档放在 `docs/sdks/{platform}/` 目录
- 每个模块一个 `.md` 文件
- 命名规范：`{模块名}_SDK.md`
