---
home: true
title: Apollo
heroImage: /apollo.png
heroText: Apollo
tagline: 高性能 MMORPG 服务器框架
actions:
  - text: 快速开始
    link: /guide/quick-start
    type: primary
  - text: 架构概述
    link: /architecture/overview
    type: secondary

features:
  - title: 🚀 高性能
    details: 基于 C++20 和现代架构设计，支持百万级并发连接
  - title: 🎮 游戏驱动
    details: 专为 MMORPG 设计，内置 AOI、战斗、属性等游戏系统
  - title: 🧩 模块化
    details: 清晰的分层架构，按需组装，易于扩展和维护
  - title: 🔄 BigWorld 兼容
    details: 提供完整的 BigWorld API 兼容层，平滑迁移现有代码
  - title: 📡 Actor 模型
    details: 基于 Actor 的并发模型，安全高效的分布式通信
  - title: 🛠️ 开发友好
    details: 完善的日志、配置、生命周期管理，专注业务逻辑

footer: MIT Licensed | Copyright © 2024-present Apollo Team
---

## 简介

Apollo 是一个专为 **大型多人在线角色扮演游戏 (MMORPG)** 设计的高性能服务器框架。它采用现代化的 **C++20** 标准编写，融合了多种成熟的架构模式。

## 核心特性

### 分层模块架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        Applications                              │
│  gateway-server | login-server | game-server | chat-server     │
└─────────────────────────────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────┐
│                    BigWorld Compatibility                        │
└─────────────────────────────────────────────────────────────────┘
                                ↓
┌──────┬──────┬──────┬──────┬──────────┬──────────┬────────────┐
│  net │ data │ game │ actor│ framework│  runtime │   core     │
└──────┴──────┴──────┴──────┴──────────┴──────────┴────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────┐
│                        Base                                      │
│  Time | ThreadPool | IdPool | Memory | String                   │
└─────────────────────────────────────────────────────────────────┘
```

### BigWorld 架构支持

- **CellApp** - 空间分割的游戏逻辑服务器，支持水平扩展
- **BaseApp** - 专用数据库服务器，异步 IO 设计
- **LoginApp** - 登录认证服务器
- **GatewayApp** - 高性能网关，支持连接管理和消息路由

### AOI 九宫格系统

- O(1) 复杂度的视野查询
- 高效的消息去重机制
- 支持跨 CellApp 边界处理

## 快速开始

```bash
# 克隆仓库
git clone https://github.com/cuihairu/apollo.git
cd apollo

# 安装依赖 (使用 vcpkg)
vcpkg install openssl:x64-windows protobuf:x64-windows

# 构建项目
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# 运行游戏服务器
./build/bin/game-server
```

## 文档导航

| 章节 | 描述 | 链接 |
|------|------|------|
| **快速开始** | 安装、构建、运行 | [指南](/guide/) |
| **架构设计** | BigWorld、AOI、进程架构 | [架构](/architecture/) |
| **模块说明** | 各模块详细文档 | [模块](/modules/) |
| **服务器应用** | GatewayApp、LoginApp、BaseApp、CellApp | [应用](/apps/) |
| **API 参考** | 接口与类说明 | [API](/api/) |

## 社区

- **GitHub**: [https://github.com/cuihairu/apollo](https://github.com/cuihairu/apollo)
- **Issues**: [https://github.com/cuihairu/apollo/issues](https://github.com/cuihairu/apollo/issues)
- **Discussions**: [https://github.com/cuihairu/apollo/discussions](https://github.com/cuihairu/apollo/discussions)

## 许可证

[MIT](LICENSE) License
