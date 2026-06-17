# Apollo 项目目录结构

## 概述

这份文档描述当前仓库的实际目录布局。

## 顶层目录

| 目录 | 说明 |
|------|------|
| `apps/` | 可执行服务与进程壳 |
| `modules/` | 模块化实现 |
| `docs/` | 文档站点 |
| `examples/` | 示例代码 |
| `tests/` | 测试代码 |
| `include/` | 公共头文件与兼容层头文件 |
| `src/` | 公共实现与兼容层实现 |
| `sdks/` | Unity 客户端 SDK |
| `skds/` | Cocos / Laya / 历史 SDK 工作区 |
| `cmake/` | CMake 辅助文件 |
| `scripts/` | 脚本 |
| `build*/` | 构建输出 |

## apps/

| 目录 | 说明 |
|------|------|
| `login-app/` | 登录入口 |
| `gateway-app/` | 边缘接入层 |
| `base-app/` | 玩家锚点宿主 |
| `cell-app/` | 世界运行时原型 |
| `game-server/` | 兼容或演示型游戏服务 |

## modules/

| 目录 | 说明 |
|------|------|
| `base/` | 基础工具与底座能力 |
| `core/` | 核心框架能力 |
| `data/` | 数据访问与缓存抽象 |
| `game/` | 游戏逻辑域 |
| `net/` | 网络与协议 |
| `protocol/` | 协议层 |
| `runtime/` | 运行时与宿主 |
| `starter/` | 装配与启动 |
| `bigworld/` | BigWorld 兼容层 |

## docs/

| 目录 | 说明 |
|------|------|
| `.vitepress/` | 站点配置与主题 |
| `guide/` | 入门指南 |
| `architecture/` | 架构文档 |
| `apps/` | 应用设计文档 |
| `modules/` | 模块设计文档 |
| `api/` | API 参考 |
| `qa/` | 问答索引 |
| `sdks/` | SDK 文档 |
| `public/` | 静态资源 |

## 备注

仓库里同时保留了 `sdks/` 和 `skds/` 两个目录名，其中 `sdks/` 是当前 Unity SDK 主目录，`skds/` 是多平台 SDK 工作区与历史目录。文档按实际用途描述，不再按旧版单树结构编排。
