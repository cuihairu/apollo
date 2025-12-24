# Server → Apollo 框架抽取工作总结

## 概述

本文档记录了从 `server/trunk/code` 项目中提取游戏服务器框架代码到 `apollo` 项目的进展情况。

## 已完成的抽取工作

### 1. 属性系统 (`apollo/game/attributes/`)

| 源文件 | 目标文件 | 状态 |
|--------|----------|------|
| `GameServer/BaseClass/ComVal.h` | `include/apollo/game/attributes/comval.h` | ✅ 完成 |
| `GameServer/Game/ComValGameDef.h` | `include/apollo/game/attributes/attribute_id.h` | ✅ 完成 |
| `GameServer/Game/GlobeObj.h` (ProperValue) | `include/apollo/game/attributes/attribute_value.h` | ✅ 完成 |
| (实现) | `src/apollo/game/attributes/attribute_value.cpp` | ✅ 完成 |

#### 1.1 ComVal (通用值类型)

**功能:**
- 支持多种数据类型: `bool`, `int8`, `int16`, `int32`, `int64`, `uint16`, `uint32`, `double`, `string`
- 提供类型安全的获取方法
- 支持算术运算和比较运算
- 支持序列化/反序列化

**命名转换:**
| 源命名 | 目标命名 |
|--------|----------|
| `uComVal` | `ComVal` |
| `ECVT_*` | `EComValType::*` |

#### 1.2 AttributeId (属性ID定义)

**功能:**
- 定义所有属性ID常量
- 按类别分组: Object, Creature, Player, Pet, Monster, NPC, Item
- 使用强类型枚举 `enum class`

**命名转换:**
| 源命名 | 目标命名 | 说明 |
|--------|----------|------|
| `ObjectValueDef` | `AttributeId` | 公共对象属性 |
| `CreatureValueDef` | `AttributeId` | 生物属性 |
| `PlayerValueDef` | `AttributeId` | 玩家属性 |
| `Attr_*` | `*_` 或 `*_` | 去除前缀 |

**具体转换示例:**
| 源名称 | 目标名称 |
|--------|----------|
| `Attr_PointId` | `POINT_ID` |
| `Attr_CurHp` | `CUR_HP` |
| `Attr_HpMax` | `HP_MAX` |
| `Attr_AddAtkPer` | `ADD_ATK_PERCENT` |
| `Attr_JinBi` | `GOLD` |
| `Attr_YuanBao` | `CASH` |
| `Attr_Diamond` | `DIAMOND` |

#### 1.3 AttributeContainer (属性容器)

**功能:**
- 管理对象的所有属性
- 变更跟踪(dirty flag机制)
- 属性变更事件通知
- 序列化/反序列化支持

**命名转换:**
| 源命名 | 目标命名 |
|--------|----------|
| `ProperValue` | `AttributeValue` (struct) |
| - | `AttributeContainer` (class) |
| `bUpdate` | `dirty` |

### 2. 配置加载器 (`apollo/config/`)

| 源文件 | 目标文件 | 状态 |
|--------|----------|------|
| `common/comm/BaseLoader.h` | `include/apollo/config/table_loader.h` | ✅ 完成 |
| (实现) | `src/apollo/config/table_loader.cpp` | ✅ 完成 |

**功能:**
- 解析格式化文本表格
- 字段分隔符: `*|*`
- 支持 CRLF 换行符兼容
- 类型安全的字段获取
- 错误定位支持

**命名转换:**
| 源命名 | 目标命名 |
|--------|----------|
| `BaseLoader` | `TableLoader` |
| `FiledInfoStru` | `FieldInfo` |
| `LoadData` | `loadFile` |
| `OnAfterRowRead` | `onRowRead` |
| `UnInit` | `cleanup` |
| `GetFieldData` | `getField` |
| `GetFieldString` | `getFieldString` |

### 3. 网络抽象层 (`apollo/net/`)

| 源文件 | 目标文件 | 状态 |
|--------|----------|------|
| SSEngine/SDNet 接口 | `include/apollo/net/connection.h` | ✅ 完成 |
| SSEngine/SDNet 接口 | `include/apollo/net/session.h` | ✅ 完成 |
| SSEngine/SDNet 接口 | `include/apollo/net/session_factory.h` | ✅ 完成 |
| SSEngine/SDNet 接口 | `include/apollo/net/packet_parser.h` | ✅ 完成 |
| SSEngine/SDNet 接口 | `include/apollo/net/listener.h` | ✅ 完成 |
| SSEngine/SDNet 接口 | `include/apollo/net/connector.h` | ✅ 完成 |
| SSEngine/SDNet 接口 | `include/apollo/net/network_manager.h` | ✅ 完成 |
| (适配器实现) | `include/apollo/net/adapters/sdnet_adapter.h` | ✅ 完成 |
| (适配器实现) | `src/apollo/net/adapters/sdnet_adapter.cpp` | ✅ 完成 |

**功能:**
- 与网络库实现无关的抽象接口
- 支持 SSEngine/SDNet、Boost.Asio、libuv 等多种后端
- 统一的连接/会话管理
- 数据包解析器接口
- 监听器/连接器抽象

**SSEngine 接口映射:**
| SSEngine 接口 | Apollo 抽象接口 |
|---------------|-----------------|
| `ISSConnection` | `Connection` |
| `ISSSession` | `Session` |
| `ISSSessionFactory` | `SessionFactory` |
| `ISSPacketParser` | `PacketParser` |
| `ISSListener` | `Listener` |
| `ISSConnector` | `Connector` |
| `ISSNet` | `NetworkManager` |

**命名转换:**
| 源命名 | 目标命名 |
|--------|----------|
| `IsConnected()` | `isConnected()` |
| `Send()` | `send()` |
| `Disconnect()` | `disconnect()` |
| `OnEstablish()` | `onEstablish()` |
| `OnTerminate()` | `onTerminate()` |
| `OnRecv()` | `onRecv()` |
| `Start()` (Listener) | `start()` |
| `Stop()` (Listener) | `stop()` |
| `Connect()` (Connector) | `connect()` |

## 目录结构

```
apollo/
├── include/apollo/
│   ├── game/
│   │   └── attributes/
│   │       ├── comval.h            # 通用值类型
│   │       ├── attribute_id.h      # 属性ID定义
│   │       └── attribute_value.h   # 属性容器
│   ├── config/
│   │   └── table_loader.h          # 表格加载器
│   └── net/
│       ├── connection.h            # 连接抽象接口
│       ├── session.h               # 会话抽象接口
│       ├── session_factory.h       # 会话工厂接口
│       ├── packet_parser.h         # 数据包解析器接口
│       ├── listener.h              # 监听器接口
│       ├── connector.h             # 连接器接口
│       ├── network_manager.h       # 网络管理器接口
│       ├── net.h                   # 统一头文件
│       └── adapters/
│           └── sdnet_adapter.h     # SDNet 适配器
│
└── src/apollo/
    ├── game/
    │   └── attributes/
    │       └── attribute_value.cpp # 属性容器实现
    ├── config/
    │   └── table_loader.cpp        # 表格加载器实现
    └── net/
        └── adapters/
            └── sdnet_adapter.cpp   # SDNet 适配器实现
```

## 待抽取的模块

### 高优先级

| 模块 | 源位置 | 目标位置 | 说明 |
|------|--------|----------|------|
| 定时器 | `GameServer/BaseClass/TimerObj.*` | `include/apollo/core/timer/` | 定时器管理 |
| 配置管理 | `common/globalsetting.*` | `include/apollo/config/` | 全局配置 |
| 日志系统 | `common/ApolloLogger/` | `include/apollo/core/log/` | 日志门面 |

### 中优先级

| 模块 | 源位置 | 目标位置 | 说明 |
|------|--------|----------|------|
| 数据库抽象 | `GameServer/db/dbaccess.*` | `include/apollo/database/` | MySQL访问层 |
| Redis封装 | `GameServer/netmodules/RedisCtrl.*` | `include/apollo/database/` | Redis访问层 |
| 协议处理 | `GameServer/protocol/` | `include/apollo/protocol/` | 协议解析 |

## 命名规范

### C++ 命名约定

1. **类名**: PascalCase (大驼峰)
   - 示例: `TableLoader`, `AttributeContainer`

2. **函数/方法**: camelCase (小驼峰)
   - 示例: `getField()`, `loadFile()`, `hasAttribute()`

3. **成员变量**: camelCase + 下划线后缀
   - 示例: `objectId_`, `rowCount_`, `fieldMap_`

4. **常量/枚举**: UPPER_CASE 或 PascalCase (enum class)
   - 示例: `MAX_HP`, `AttributeId::LEVEL`

5. **宏定义**: UPPER_CASE
   - 示例: `API_EXPORT`

### 中文拼音 → 英文转换

| 拼音 | 英文 |
|------|------|
| `JinBi` | `Gold` |
| `YuanBao` | `Cash` |
| `BaoBao` | `Bag` |
| `BeiBao` | `Warehouse` |
| `ShangDian` | `Shop` |
| `ZhuangBei` | `Equipment` |
| `JueSe` | `Role` / `Character` |
| `DengJi` | `Level` |
| `JingYan` | `Experience` / `Exp` |
| `Xue` | `HP` / `Health` |
| `Lan` | `MP` / `Mana` |
| `GongJi` | `Attack` |
| `FangYu` | `Defense` |
| `MingZhong` | `Hit` |
| `ShanBi` | `Dodge` |
| `BaoJi` | `Critical` / `Crit` |
| `ZhanDou` | `Battle` |
| `ChuanSong` | `Teleport` |
| `FuHuo` | `Revive` |
| `HuiCheng` | `Return` |

## 使用示例

### 属性容器使用

```cpp
#include "apollo/game/attributes/attribute_value.h"

using namespace apollo::game;

// 创建属性容器
auto container = AttributeContainerManager::instance().getOrCreate(playerId);

// 设置属性
container->setAttribute(AttributeId::LEVEL, ComVal(50));
container->setAttribute(AttributeId::CUR_HP, ComVal(1000));
container->setAttribute(AttributeId::HP_MAX, ComVal(5000));

// 获取属性
int32_t level = container->getInt(AttributeId::LEVEL);
int64_t hp = container->getInt64(AttributeId::CUR_HP);

// 监听变更
container->setChangeListener([](const AttributeChangeEvent& evt) {
    std::cout << "Attr " << evt.attributeId << " changed from "
              << evt.oldValue.toString() << " to " << evt.newValue.toString() << std::endl;
});
```

### 配置加载使用

```cpp
#include "apollo/config/table_loader.h"

using namespace apollo::config;

class MonsterConfigLoader : public TableLoader {
public:
    bool load(const std::string& path) {
        return loadFile(path);
    }

    bool onRowRead() override {
        int32_t id = getField<int32_t>("ID");
        std::string name = getField<std::string>("Name");
        int32_t hp = getField<int32_t>("HP");

        monsters_[id] = {name, hp};
        return true;
    }

private:
    std::unordered_map<int32_t, MonsterInfo> monsters_;
};
```

### 网络层使用

```cpp
#include "apollo/net/net.h"
#include "apollo/net/adapters/sdnet_adapter.h"

using namespace apollo::net;

// 1. 创建网络管理器
auto network = adapters::SDNetAdapter::createManager();
network->initialize();
network->start();

// 2. 创建会话工厂
class GameSession : public adapters::SDNetSession {
protected:
    void handleEstablish() override {
        // 连接建立处理
    }

    uint32_t handlePacket(const char* data, uint32_t length) override {
        // 数据包处理
        return length;
    }

    void handleTerminate(TerminateReason reason) override {
        // 连接断开处理
    }
};

using GameSessionFactory = TemplateSessionFactory<GameSession>;

// 3. 创建监听器
auto listener = network->createListener("game");
listener->setSessionFactory(std::make_shared<GameSessionFactory>());
listener->start("0.0.0.0", 8080);

// 4. 创建连接器(连接到其他服务器)
auto connector = network->createConnector("db");
connector->setCallbacks(ConnectorCallbacks::create(
    [](ConnectionPtr conn) {
        // 连接成功
    },
    [](const char* reason) {
        // 连接断开
    }
));
connector->connect("127.0.0.1", 6379);
```

## 下一步计划

1. **定时器模块** - 提取 TimerObj 定时器管理
2. **完善编译系统** - 更新 CMakeLists.txt
3. **单元测试** - 为抽取的模块添加测试
4. **Boost.Asio 适配器** - 实现 Boost.Asio 后端支持
