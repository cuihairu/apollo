
# Apollo SDK 目录结构

## 概述

当前仓库里，Unity SDK 代码分布在两棵树：客户端/连接/认证模块在 `skds/unity/ApolloSDK/`，属性系统在 `sdks/unity/ApolloSDK/Attributes/`；其它平台 SDK 工作区保留在 `skds/`（`cocos/`、`laya/`）。

`docs/sdks/` 目录存放 SDK 文档，与服务端框架保持协议和接口一致。

## 目录结构

```
skds/
├── unity/                    # Unity 客户端 SDK
│   └── ApolloSDK/
│       ├── ApolloClient.cs         # 主客户端
│       ├── ApolloClientConfig.cs   # 配置
│       ├── Network/               # 网络模块
│       │   └── NetworkManager.cs
│       └── Session/               # 会话管理
│           └── AuthManager.cs
├── cocos/                   # Cocos Creator SDK
└── laya/                    # LayaBox SDK

sdks/
└── unity/                    # Unity SDK（当前为属性系统）
    └── ApolloSDK/
        └── Attributes/            # 属性系统
            ├── AttributeValue.cs
            ├── AttributeContainer.cs
            ├── AttributeSyncManager.cs
            └── ...
```

## Unity SDK 模块说明

| 模块 | 位置 | 功能 | 状态 |
|------|------|------|------|
| `ApolloClient.cs` | `skds/unity/ApolloSDK/` | 主入口，连接、认证、心跳 | 已有 |
| `ApolloClientConfig.cs` | `skds/unity/ApolloSDK/` | 配置类 | 已有 |
| `Network/NetworkManager.cs` | `skds/unity/ApolloSDK/` | TCP/Socket 连接管理 | 已有 |
| `Session/AuthManager.cs` | `skds/unity/ApolloSDK/` | 登录认证 | 已有 |
| `Attributes/` | `sdks/unity/ApolloSDK/` | 属性同步系统 | 开发中 |
| `Messaging/` | — | Protobuf 消息路由 | 计划中 |
| `Utilities/` | — | 日志、定时器等 | 计划中 |

## 文档规范

- SDK 设计文档放在 `docs/sdks/{platform}/` 目录
- 每个模块一个 `.md` 文件
- 命名规范：`{模块名}_SDK.md`
