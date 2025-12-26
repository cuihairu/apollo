# Apollo 项目目录结构

## 概述

本文档描述了Apollo MMORPG服务器框架的目录结构设计原则和最佳实践。

## 目录结构设计原则

1. **模块化分离**：不同功能模块的代码物理分离
2. **清晰命名**：目录和文件名要能清楚表达其功能
3. **层次合理**：避免过深的嵌套层次
4. **易于维护**：相关文件集中放置

## 推荐目录结构

```
apollo/
├── include/apollo/                 # 公共头文件
│   ├── framework/                  # 核心框架
│   │   ├── ioc/                   # IoC容器
│   │   │   ├── IComponent.h
│   │   │   ├── ApplicationContext.h
│   │   │   └── ComponentFactory.h
│   │   └── base/                  # 基础类
│   │       ├── BaseComponent.h
│   │       ├── Singleton.h
│   │       └── NonCopyable.h
│   │
│   ├── game/                       # 游戏逻辑
│   │   ├── aoi/                   # 九宫格系统
│   │   │   ├── aoi.hpp
│   │   │   ├── AOIManager.h
│   │   │   └── GridCell.h
│   │   ├── battle/                # 战斗系统
│   │   │   ├── ecs/              # ECS架构
│   │   │   │   ├── ecs.hpp
│   │   │   │   ├── Entity.h
│   │   │   │   ├── Component.h
│   │   │   │   └── System.h
│   │   │   ├── skills/           # 技能系统
│   │   │   │   ├── Skill.h
│   │   │   │   ├── SkillManager.h
│   │   │   │   └── SkillEffect.h
│   │   │   └── buff/              # Buff系统
│   │   │       ├── Buff.h
│   │   │       ├── BuffManager.h
│   │   │       └── BuffEffect.h
│   │   ├── attributes/           # 属性系统
│   │   │   ├── attribute.hpp
│   │   │   ├── AttributeContainer.h
│   │   │   └── AttributeCalculator.h
│   │   ├── scene/                 # 场景系统
│   │   │   ├── Scene.h
│   │   │   ├── SceneManager.h
│   │   │   └── SceneObject.h
│   │   └── player/                # 玩家系统
│   │       ├── Player.h
│   │       ├── PlayerManager.h
│   │       └── PlayerData.h
│   │
│   ├── network/                    # 网络通信
│   │   ├── transport/             # 传输层
│   │   │   ├── socket.hpp
│   │   │   ├── net_common.hpp
│   │   │   ├── reactor.hpp
│   │   │   └── Connection.h
│   │   ├── messaging/             # 消息层
│   │   │   ├── message.hpp
│   │   │   ├── MessageRouter.h
│   │   │   ├── protobuf_message.hpp
│   │   │   └── MessageHandler.h
│   │   ├── rpc/                   # RPC框架
│   │   │   ├── rpc.hpp
│   │   │   ├── RpcClient.h
│   │   │   ├── RpcServer.h
│   │   │   └── RpcService.h
│   │   └── protocol/              # 协议定义
│   │       ├── Protocol.h
│   │       ├── MessageIDs.h
│   │       └── Packet.h
│   │
│   ├── storage/                    # 存储层
│   │   ├── database/               # 数据库
│   │   │   ├── connection.hpp
│   │   │   ├── mysql_connection.hpp
│   │   │   ├── Query.h
│   │   │   ├── Transaction.h
│   │   │   └── ConnectionPool.h
│   │   ├── cache/                  # 缓存
│   │   │   ├── redis.hpp
│   │   │   ├── RedisClient.h
│   │   │   ├── CacheManager.h
│   │   │   └── CacheKey.h
│   │   └── serialization/          # 序列化
│   │       ├── Serializer.h
│   │       ├── JsonSerializer.h
│   │       └── BinarySerializer.h
│   │
│   ├── server/                     # 服务器相关
│   │   ├── core/                   # 核心定义
│   │   │   ├── server_id.hpp
│   │   │   ├── server_types.hpp
│   │   │   └── ServerConfig.h
│   │   ├── gateway/                # 网关
│   │   │   ├── GatewayServer.h
│   │   │   ├── ConnectionManager.h
│   │   │   └── LoadBalancer.h
│   │   ├── login/                  # 登录服务
│   │   │   ├── LoginServer.h
│   │   │   ├── AuthManager.h
│   │   │   └── TokenManager.h
│   │   └── game/                   # 游戏服务
│   │       ├── GameServer.h
│   │       ├── ZoneManager.h
│   │       └── WorldManager.h
│   │
│   └── utils/                       # 工具类
│       ├── logging/               # 日志
│       │   ├── logger.hpp
│       │   ├── LogAppender.h
│       │   ├── LogManager.h
│       │   └── FileAppender.h
│       ├── threading/             # 线程
│       │   ├── thread_pool.hpp
│       │   ├── Thread.h
│       │   ├── Mutex.h
│       │   └── Atomic.h
│       ├── memory/                 # 内存
│       │   ├── memory_pool.hpp
│       │   ├── ObjectPool.h
│       │   ├── SmartPtr.h
│       │   └── Allocator.h
│       ├── time/                   # 时间
│       │   ├── Timer.h
│       │   ├── TimeUtil.h
│       │   └── Clock.h
│       ├── math/                   # 数学
│       │   ├── Vector3.h
│       │   ├── Matrix4.h
│       │   ├── Quaternion.h
│       │   └── MathUtil.h
│       ├── string/                 # 字符串
│       │   ├── StringUtil.h
│       │   ├── StringBuilder.h
│       │   └── Format.h
│       ├── container/              # 容器
│       │   ├── RingBuffer.h
│       │   ├── FixedQueue.h
│       │   └── LRUCache.h
│       ├── config/                 # 配置
│       │   ├── config_loader.hpp
│       │   ├── ConfigManager.h
│       │   ├── JsonConfig.h
│       │   └── XmlConfig.h
│       └── crypto/                 # 加密
│           ├── CryptoUtil.h
│           ├── AES.h
│           ├── MD5.h
│           └── SHA256.h
│
├── src/                            # 实现文件
│   ├── framework/                  # 对应include/apollo/framework
│   ├── game/                       # 对应include/apollo/game
│   ├── network/                    # 对应include/apollo/network
│   ├── storage/                    # 对应include/apollo/storage
│   ├── server/                     # 对应include/apollo/server
│   └── utils/                       # 对应include/apollo/utils
│
├── tests/                          # 测试代码
│   ├── framework/                  # 框架测试
│   ├── game/                       # 游戏逻辑测试
│   ├── network/                    # 网络测试
│   ├── storage/                    # 存储测试
│   └── utils/                       # 工具测试
│
├── examples/                       # 示例代码
│   ├── basic/                      # 基础示例
│   ├── advanced/                   # 高级示例
│   └── tutorials/                  # 教程示例
│
├── skds/                           # SDK (复数形式)
│   ├── unity/                      # Unity SDK
│   ├── cocos/                      # Cocos Creator SDK
│   └── laya/                       # LayaBox SDK
│
├── tools/                          # 工具
│   ├── code_gen/                   # 代码生成器
│   ├── protocol_gen/              # 协议生成器
│   └── config_gen/                 # 配置生成器
│
├── docs/                           # 文档
│   ├── design/                     # 设计文档
│   ├── api/                        # API文档
│   ├── tutorials/                  # 教程文档
│   └── architecture/               # 架构文档
│
├── third_party/                    # 第三方库
│   ├── protobuf/                   # Protobuf库
│   ├── json/                       # JSON库
│   └── testing/                    # 测试框架
│
├── cmake/                          # CMake模块
├── scripts/                        # 脚本
├── assets/                         # 资源文件
├── build/                          # 构建输出
├── CMakeLists.txt                  # 主CMake文件
├── README.md                       # 项目说明
├── LICENSE                          # 许可证
└── .gitignore                      # Git忽略文件
```

## 目录说明

### framework/
核心框架代码，包含IoC容器、组件系统等基础设施。

### game/
游戏相关的所有逻辑代码，包括AOI、战斗、属性、场景等。

### network/
网络通信相关代码，从底层的Socket传输到高层的RPC框架。

### storage/
数据存储相关代码，包括数据库操作、缓存管理、序列化等。

### server/
服务器相关代码，包括各类服务器的实现。

### utils/
通用工具类，不依赖具体业务逻辑的可复用代码。

## 命名规范

1. **目录名**：小写字母，使用下划线分隔
2. **文件名**：小写字母+下划线，或使用驼峰命名（根据项目约定）
3. **类名**：大驼峰命名（PascalCase）
4. **函数名**：小驼峰命名（camelCase）
5. **常量**：全大写字母+下划线

## 文件组织原则

1. **头文件与实现文件分离**：.h/.hpp 与 .cpp/.cc 对应
2. **单职责原则**：每个文件只负责一个功能模块
3. **依赖清晰**：避免循环依赖
4. **接口优先**：先定义接口，再实现具体功能

## 注意事项

1. 避免过深的目录嵌套（建议不超过4层）
2. 相关功能的文件要放在一起
3. 公共头文件放在合适的公共目录
4. 私有实现放在src目录对应位置

## 最佳实践

1. 按功能模块组织代码，而不是按文件类型
2. 使用清晰的包/命名空间结构
3. 提供统一的公共头文件（如apollo.h）
4. 定期重构和整理目录结构
5. 保持文档和代码同步更新