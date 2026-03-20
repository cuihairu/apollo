# C++ 设计模式在 KBEngine 中的实践

## 问题分析

本题考察对设计模式和 KBEngine 架构的深入理解：
- KBEngine 使用了哪些设计模式
- 为什么要这样设计
- 实际代码中的应用
- 设计权衡

---

## 一、KBEngine 核心设计模式

### 1.1 设计模式概览

```
┌─────────────────────────────────────────────────────────────┐
│              KBEngine 核心设计模式                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  创建型模式:                                                 │
│  ├── Singleton (单例) - DBMgr, BaseAppMgr, CellAppMgr       │
│  ├── Factory (工厂) - 实体创建                               │
│  ├── Builder (建造者) - 消息构建                              │
│  └── Object Pool (对象池) - 内存管理                          │
│                                                             │
│  结构型模式:                                                 │
│  ├── Adapter (适配器) - 网络适配                              │
│  ├── Bridge (桥接) - 接口与实现分离                          │
│  ├── Composite (组合) - 空间管理                              │
│  └── Proxy (代理) - 实体代理 (Ghost/Shadow)                 │
│                                                             │
│  行为型模式:                                                 │
│  ├── Observer (观察者) - Watcher 系统                        │
│  ├── Command (命令) - 消息处理                               │
│  ├── Strategy (策略) - 负载均衡                               │
│  ├── Template Method (模板方法) - 实体生命周期               │
│  └── Chain of Responsibility (责任链) - 消息路由             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、单例模式 (Singleton)

### 2.1 KBEngine 应用

**应用场景**: DBMgr, BaseAppMgr, CellAppMgr 等全局唯一组件

```cpp
// KBEngine 单例模式实现
// src/server/dbmgr.cpp

class DBMgr
{
public:
    // 获取单例实例
    static DBMgr& instance()
    {
        static DBMgr instance;
        return instance;
    }

    // 禁止拷贝和赋值
    DBMgr(const DBMgr&) = delete;
    DBMgr& operator=(const DBMgr&) = delete;

private:
    // 私有构造函数
    DBMgr()
        : pNetworkInterface_(NULL)
        , bufferSize_(0)
    {
    }

    // 成员变量
    NetworkInterface* pNetworkInterface_;
    uint32 bufferSize_;
};

// 使用方式
DBMgr& dbmgr = DBMgr::instance();
```

### 2.2 为什么要用单例？

```
┌─────────────────────────────────────────────────────────────┐
│                    单例模式设计原因                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  全局唯一性:                                                 │
│  ├── DBMgr 只能有一个                                       │
│  ├── 避免数据库连接冲突                                     │
│  ├── 确保数据一致性                                         │
│  └── 防止资源竞争                                           │
│                                                             │
│  延迟初始化:                                                 │
│  ├── 只在需要时创建                                         │
│  ├── 减少启动开销                                           │
│  ├── 按需分配资源                                           │
│  └── C++11 保证线程安全                                      │
│                                                             │
│  访问便利:                                                   │
│  ├── 全局访问点                                             │
│  ├── 简化调用代码                                           │
│  ├── 不需要传递引用                                         │
│  └── 代码更清晰                                             │
│                                                             │
│  替代方案对比:                                               │
│  ├── 全局变量: 无法控制初始化顺序                            │
│  ├── 传引用: 函数参数复杂                                    │
│  ├── 依赖注入: 增加耦合                                     │
│  └── 单例: 平衡了简洁性和控制力                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 三、工厂模式 (Factory)

### 3.1 KBEngine 实体工厂

```cpp
// KBEngine 实体工厂
// src/server/entitydef/entitydef.cpp

class EntityDef
{
public:
    // 创建实体
    Entity* createEntity(Entity* pParent, ENTITY_ID entityID)
    {
        // 根据类型创建实体
        Entity* entity = NULL;

        // 通过脚本创建
        if (scriptModuleName_.size() > 0)
        {
            // 调用 Python 脚本创建
            entity = ScriptModule::getInstance()->createEntity(
                scriptModuleName_,
                this,
                pParent,
                entityID
            );
        }
        else
        {
            // C++ 内置实体
            entity = new Entity();
        }

        if (entity)
        {
            // 初始化属性
            entity->initializeProperty(*this);
        }

        return entity;
    }

private:
    std::string scriptModuleName_;  // Python 模块名
    std::map<std::string, PropertyDescription> properties_;
};

// 使用示例
EntityDef* accountDef = EntityDefs::findEntityDef("Account");
Entity* account = accountDef->createEntity(NULL, entityID);
```

### 3.2 为什么要用工厂？

```
工厂模式优势:

1. 解耦创建和使用
   ├── 调用者不需要知道具体类
   ├── 只需要知道实体名称
   └── 便于替换实现

2. 集中管理创建逻辑
   ├── 统一的创建入口
   ├── 方便添加初始化逻辑
   └── 方便记录日志

3. 支持多态创建
   ├── C++ 实体
   ├── Python 实体
   ├── Lua 实体 (可扩展)
   └── 运行时决定

4. 资源管理
   ├── 对象池复用
   ├── 延迟初始化
   └── 自动回收
```

---

## 四、观察者模式 (Observer)

### 4.1 Watcher 系统

```cpp
// KBEngine Watcher 系统
// src/server/watcher.h

class Watcher
{
public:
    // 注册观察者
    template<typename T>
    void addWatch(const std::string& path, T* watcher)
    {
        watches_[path].push_back(static_cast<void*>(watcher));
    }

    // 移除观察者
    void removeWatch(const std::string& path, void* watcher)
    {
        auto it = watches_.find(path);
        if (it != watches_.end())
        {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), watcher), vec.end());
        }
    }

    // 通知所有观察者
    void notify(const std::string& path, const boost::any& value)
    {
        auto it = watches_.find(path);
        if (it != watches_.end())
        {
            for (void* watcher : it->second)
            {
                static_cast<WatcherInterface*>(watcher)->onWatch(path, value);
            }
        }
    }

    // 获取监控值
    boost::any get(const std::string& path)
    {
        // 解析路径: "stats/cpuUsage"
        // 返回对应的值
        return getValueFromPath(path);
    }

private:
    std::map<std::string, std::vector<void*>> watches_;
};

// 使用示例
class ComponentMonitor : public WatcherInterface
{
public:
    void init()
    {
        Watcher::instance().addWatch("stats/cpuUsage", this);
        Watcher::instance().addWatch("stats/memoryUsage", this);
    }

    void onWatch(const std::string& path, const boost::any& value) override
    {
        if (path == "stats/cpuUsage")
        {
            float cpu = boost::any_cast<float>(value);
            if (cpu > 80.0f)
            {
                WARNING_MSG("High CPU usage: {}", cpu);
            }
        }
    }
};
```

### 4.2 为什么要用观察者？

```
观察者模式优势:

1. 解耦监控和业务
   ├── 监控代码不影响业务逻辑
   ├── 业务代码不需要知道监控存在
   └── 可以动态添加/移除监控

2. 支持多个观察者
   ├── 多个组件监控同一数据
   ├── 独立处理各自逻辑
   └── 互不干扰

3. 延迟注册
   ├── 运行时决定监控内容
   ├── 不同环境不同监控
   └── 方便调试

4. 实时性
   ├── 数据变化立即通知
   ├── 无需轮询
   └── 高效
```

---

## 五、代理模式 (Proxy)

### 5.1 Ghost/Shadow 机制

```cpp
// KBEngine 实体代理
// src/entitydef/entity.h

class Entity
{
public:
    // Base 实体 (客户端代理)
    class BaseEntity : public Entity
    {
    public:
        // 远程调用 Cell 方法
        void callCellMethod(const std::string& method, const MemoryStream& args)
        {
            // 通过网络转发到 CellApp
            NetworkInterface* network = NetworkInterface::instance();
            network->sendToCell(entityID_, method, args);
        }

        // 属性访问
        int32 getHP() const
        {
            // 返回缓存的值
            return cachedHP_;
        }

        void setHP(int32 value)
        {
            cachedHP_ = value;
            // 同步到 Cell
            callCellMethod("setHP", MemoryStream() << value);
        }

    private:
        ENTITY_ID entityID_;
        int32 cachedHP_;  // 缓存的属性值
    };

    // Cell 实体 (真实数据)
    class CellEntity : public Entity
    {
    public:
        int32 getHP() const
        {
            return hp_;  // 真实值
        }

        void setHP(int32 value)
        {
            hp_ = value;
            // 通知 Base 更新
            notifyBase("hp", value);
            // 通知客户端
            if (hasClient())
            {
                sendToClient("onHPChanged", value);
            }
        }

    private:
        int32 hp_;
    };
};
```

### 5.2 Ghost/Shadow 设计图

```
┌─────────────────────────────────────────────────────────────┐
│                    Ghost/Shadow 架构                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  [客户端]                                                    │
│     │                                                       │
│     ▼                                                       │
│  [Shadow Entity] - 客户端预测                                │
│     ├── 本地计算位置                                         │
│     ├── 立即响应输入                                         │
│     └── 显示给玩家                                           │
│                          │                                  │
│                          ▼ 校验                              │
│  [Ghost Entity] - BaseApp 代理                               │
│     ├── 缓存状态数据                                         │
│     ├── 转发消息到 Cell                                      │
│     └── 更新客户端                                           │
│                          │                                  │
│                          ▼ 同步                              │
│  [Real Entity] - CellApp 真实实体                            │
│     ├── 真实逻辑状态                                         │
│     ├── 权威数据源                                           │
│     └── 广播状态变化                                         │
│                                                             │
│  数据流:                                                     │
│  1. 客户端输入 → Shadow (预测) → 显示                         │
│  2. 客户端输入 → Ghost → Cell → 真实计算                      │
│  3. Cell → Ghost → 客户端 → 校正 Shadow                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.3 为什么要用代理？

```
Ghost/Shadow 优势:

1. 网络延迟隐藏
   ├── Shadow 立即响应输入
   ├── 玩家感觉无延迟
   └── Ghost 异步更新

2. 减少网络流量
   ├── 只有同步时才传输
   ├── 避免频繁请求
   └── 节省带宽

3. 安全性
   ├── 客户端只有代理
   ├── 真实逻辑在服务端
   └── 防止作弊

4. 分布式架构
   ├── Base 处理非实时逻辑
   ├── Cell 处理实时逻辑
   └── 各司其职
```

---

## 六、命令模式 (Command)

### 6.1 消息处理

```cpp
// KBEngine 命令模式
// src/network/bundle.h

class MessageHandler
{
public:
    // 抽象命令
    class Command
    {
    public:
        virtual ~Command() {}
        virtual void execute(MemoryStream& stream) = 0;
        virtual size_t getID() const = 0;
    };

    // 具体命令
    class LoginCommand : public Command
    {
    public:
        static size_t ID() { return 100; }

        void execute(MemoryStream& stream) override
        {
            std::string account, password;
            stream >> account >> password;

            // 执行登录逻辑
            Account* pAccount = AccountLogin(account, password);

            // 返回结果
            MemoryStream reply;
            reply << pAccount->getID();
            sendToClient(reply);
        }

        size_t getID() const override { return ID(); }
    };

    // 命令注册器
    void registerCommand(Command* cmd)
    {
        handlers_[cmd->getID()] = cmd;
    }

    // 处理消息
    void processMessage(NetworkInterface* network, const Packet* packet)
    {
        // 解析消息头
        uint16 msgID;
        MemoryStream stream(packet->data(), packet->length());
        stream >> msgID;

        // 查找处理器
        auto it = handlers_.find(msgID);
        if (it != handlers_.end())
        {
            it->second->execute(stream);
        }
        else
        {
            ERROR_MSG("Unknown message ID: {}", msgID);
        }
    }

private:
    std::map<size_t, Command*> handlers_;
};
```

### 6.2 为什么要用命令模式？

```
命令模式优势:

1. 解耦消息处理
   ├── 消息ID和处理逻辑分离
   ├── 易于添加新消息
   └── 便于维护

2. 可扩展性
   ├── 动态注册命令
   ├── 运行时添加
   └── 热更新支持

3. 请求封装
   ├── 参数封装在流中
   ├── 统一序列化
   └── 类型安全

4. 历史记录
   ├── 可记录所有命令
   ├── 用于调试
   └── 用于审计
```

---

## 七、对象池模式 (Object Pool)

### 7.1 KBEngine 内存管理

```cpp
// KBEngine 对象池
// src/lib/helpers/objectpool.h

template<typename TYPE, size_t PRE_ALLOC = 64>
class ObjectPool
{
public:
    ObjectPool()
        : pool_(nullptr)
        , numAllocs_(0)
        , numFrees_(0)
    {
    }

    ~ObjectPool()
    {
        clear();
    }

    // 获取对象
    TYPE* alloc()
    {
        TYPE* obj = nullptr;

        if (freeList_.empty())
        {
            // 池为空，创建新对象
            obj = new TYPE();
            ++numAllocs_;
        }
        else
        {
            // 从池中取
            obj = freeList_.back();
            freeList_.pop_back();
        }

        return obj;
    }

    // 释放对象
    void free(TYPE* obj)
    {
        if (obj)
        {
            freeList_.push_back(obj);
            ++numFrees_;
        }
    }

    // 清空池
    void clear()
    {
        for (TYPE* obj : freeList_)
        {
            delete obj;
        }
        freeList_.clear();
    }

    // 统计信息
    size_t getNumAllocs() const { return numAllocs_; }
    size_t getNumFrees() const { return numFrees_; }
    size_t getPoolSize() const { return freeList_.size(); }

private:
    std::vector<TYPE*> freeList_;
    size_t numAllocs_;
    size_t numFrees_;
};

// 使用示例
ObjectPool<Packet, 1024> packetPool;

Packet* packet = packetPool.alloc();
// ... 使用 ...
packetPool.free(packet);
```

### 7.2 为什么要用对象池？

```
对象池优势:

1. 减少内存分配
   ├── 避免频繁 new/delete
   ├── 减少内存碎片
   └── 提高性能

2. 缓存友好
   ├── 连续内存访问
   ├── 提高 CPU 缓存命中率
   └── 减少缺页中断

3. 预分配
   ├── 启动时分配
   ├── 避免运行时分配
   └── 减少分配延迟

4. 可监控
   ├── 知道分配了多少
   ├── 知道池大小
   └── 方便调优

KBEngine 应用:
- Packet 池
- MemoryStream 池
- Entity 池 (部分)
- Channel 池
```

---

## 八、桥接模式 (Bridge)

### 8.1 网络抽象层

```cpp
// KBEngine 网络桥接
// src/lib/network/channel.h

// 抽象接口
class Channel
{
public:
    virtual ~Channel() {}

    virtual bool send(const Packet* packet) = 0;
    virtual Packet* receive() = 0;
    virtual bool isConnected() const = 0;

protected:
    Endpoint* pEndpoint_;
};

// TCP 实现
class TCPChannel : public Channel
{
public:
    bool send(const Packet* packet) override
    {
        // TCP 发送逻辑
        return pEndpoint_->sendtcp(packet);
    }

    Packet* receive() override
    {
        // TCP 接收逻辑
        return pEndpoint_->recvtp();
    }
};

// UDP 实现
class UDPChannel : public Channel
{
public:
    bool send(const Packet* packet) override
    {
        // UDP 发送逻辑
        return pEndpoint_->sendudp(packet);
    }

    Packet* receive() override
    {
        // UDP 接收逻辑
        return pEndpoint_->recvudp();
    }
};

// 信道管理器
class ChannelManager
{
public:
    Channel* createChannel(ProtocolType type, Endpoint* endpoint)
    {
        switch (type)
        {
        case PROTOCOL_TCP:
            return new TCPChannel(endpoint);
        case PROTOCOL_UDP:
            return new UDPChannel(endpoint);
        case PROTOCOL_TCP_UDP:
            return new TCPUDPChannel(endpoint);
        default:
            return nullptr;
        }
    }
};
```

### 8.2 为什么要用桥接？

```
桥接模式优势:

1. 分离抽象和实现
   ├── Channel 接口不变
   ├── TCP/UDP 可替换
   └── 易于扩展新协议

2. 运行时选择
   ├── 配置决定协议类型
   ├── 无需重新编译
   └── 灵活部署

3. 独立变化
   ├── 网络层独立升级
   ├── 应用层不受影响
   └── 降低耦合

4. 测试友好
   ├── 可以 Mock Channel
   ├── 单元测试方便
   └── 隔离测试
```

---

## 九、模板方法模式 (Template Method)

### 9.1 实体生命周期

```cpp
// KBEngine 实体生命周期
// src/server/entitydef/entity.h

class Entity
{
public:
    // 模板方法：完整生命周期
    bool initialize(const EntityDef& def)
    {
        // 1. 创建
        if (!onCreate())
        {
            return false;
        }

        // 2. 加载数据
        if (!onLoad(def))
        {
            onDestroy();
            return false;
        }

        // 3. 进入场景
        if (!onEnterSpace())
        {
            onDestroy();
            return false;
        }

        // 4. 完成初始化
        onInitialized();

        return true;
    }

    // 销毁流程
    void destroy()
    {
        // 1. 离开场景
        onLeaveSpace();

        // 2. 销毁
        onDestroy();

        // 3. 清理
        onCleanup();
    }

protected:
    // 钩子方法，子类可重写
    virtual bool onCreate() { return true; }
    virtual bool onLoad(const EntityDef& def) { return true; }
    virtual bool onEnterSpace() { return true; }
    virtual void onInitialized() {}
    virtual void onLeaveSpace() {}
    virtual void onDestroy() {}
    virtual void onCleanup() {}
};

// 使用：Account 实体
class Account : public Entity
{
protected:
    bool onCreate() override
    {
        // 账号特定创建逻辑
        return Entity::onCreate();
    }

    bool onLoad(const EntityDef& def) override
    {
        // 加载账号数据
        return Entity::onLoad(def);
    }
};
```

### 9.2 为什么要用模板方法？

```
模板方法优势:

1. 统一流程
   ├── 所有实体相同生命周期
   ├── 保证初始化步骤一致
   └── 避免遗漏

2. 可扩展
   ├── 子类重写钩子方法
   ├── 不改变主流程
   └── 符合开闭原则

3. 错误处理
   ├── 统一的错误处理
   ├── 失败自动回滚
   └── 保证一致性

4. 清晰的职责
   ├── 框架定义流程
   ├── 子类实现细节
   └── 关注点分离
```

---

## 十、策略模式 (Strategy)

### 10.1 负载均衡策略

```cpp
// KBEngine 负载均衡策略
// src/server/baseappmgr/baseappmgr.cpp

class LoadBalanceStrategy
{
public:
    virtual ~LoadBalanceStrategy() {}

    // 选择 BaseApp
    virtual BaseApp* selectBaseApp(const std::vector<BaseApp*>& apps) = 0;
};

// 轮询策略
class RoundRobinStrategy : public LoadBalanceStrategy
{
public:
    BaseApp* selectBaseApp(const std::vector<BaseApp*>& apps) override
    {
        size_t index = currentIndex_ % apps.size();
        ++currentIndex_;
        return apps[index];
    }

private:
    size_t currentIndex_ = 0;
};

// 最少连接策略
class LeastConnectionStrategy : public LoadBalanceStrategy
{
public:
    BaseApp* selectBaseApp(const std::vector<BaseApp*>& apps) override
    {
        BaseApp* selected = nullptr;
        size_t minConnections = SIZE_MAX;

        for (BaseApp* app : apps)
        {
            if (app->getConnectionCount() < minConnections)
            {
                minConnections = app->getConnectionCount();
                selected = app;
            }
        }

        return selected;
    }
};

// BaseAppMgr 使用策略
class BaseAppMgr
{
public:
    void setStrategy(LoadBalanceStrategy* strategy)
    {
        strategy_.reset(strategy);
    }

    BaseApp* findBestBaseApp()
    {
        return strategy_->selectBaseApp(baseApps_);
    }

private:
    std::unique_ptr<LoadBalanceStrategy> strategy_;
    std::vector<BaseApp*> baseApps_;
};
```

### 10.2 为什么要用策略模式？

```
策略模式优势:

1. 算法可替换
   ├── 不同场景不同策略
   ├── 运行时切换
   └── 无需修改代码

2. 算法解耦
   ├── 负载均衡逻辑独立
   ├── 易于测试
   └── 易于扩展

3. 配置化
   ├── 配置文件选择策略
   ├── 无需重新编译
   └── 灵活部署

4. 性能优化
   ├── 可以针对不同场景优化
   ├── 不影响其他部分
   └── 按需选择
```

---

## 十一、责任链模式 (Chain of Responsibility)

### 11.1 消息路由

```cpp
// KBEngine 消息处理链
// src/server/components/components.cpp

class MessageHandler
{
public:
    using HandlerFunc = std::function<bool(MemoryStream&)>;

    MessageHandler(): next_(nullptr) {}

    // 设置下一个处理器
    void setNext(MessageHandler* next)
    {
        next_ = next;
    }

    // 处理消息
    bool handle(MemoryStream& stream)
    {
        // 尝试自己处理
        if (handleMessage(stream))
        {
            return true;
        }

        // 传递给下一个
        if (next_)
        {
            return next_->handle(stream);
        }

        return false;
    }

    // 注册处理器
    void registerHandler(const std::string& msgName, HandlerFunc func)
    {
        handlers_[msgName] = func;
    }

protected:
    virtual bool handleMessage(MemoryStream& stream)
    {
        std::string msgName;
        stream >> msgName;

        auto it = handlers_.find(msgName);
        if (it != handlers_.end())
        {
            return it->second(stream);
        }

        return false;
    }

private:
    MessageHandler* next_;
    std::map<std::string, HandlerFunc> handlers_;
};

// 使用示例：构建处理链
MessageHandler* buildHandlerChain()
{
    // 创建各级处理器
    MessageHandler* authHandler = new MessageHandler();
    MessageHandler* logicHandler = new MessageHandler();
    MessageHandler* dbHandler = new MessageHandler();

    // 构建链
    authHandler->setNext(logicHandler);
    logicHandler->setNext(dbHandler);

    // 注册处理器
    authHandler->registerHandler("Login", handleLogin);
    logicHandler->registerHandler("Move", handleMove);
    dbHandler->registerHandler("Save", handleSave);

    return authHandler;
}
```

### 11.2 为什么要用责任链？

```
责任链优势:

1. 灵活处理流程
   ├── 可以动态调整处理顺序
   ├── 添加/删除处理器
   └── 不影响其他部分

2. 职责分离
   ├── 每个处理器专注自己的职责
   ├── 验证 → 逻辑 → 持久化
   └── 符合单一职责原则

3. 可扩展
   ├── 新增处理器很容易
   ├── 组合不同链
   └── 支持复杂业务

4. 中断控制
   ├── 处理成功可以中断
   ├── 继续传递给下一个
   └── 灵活控制流程
```

---

## 十二、组合模式 (Composite)

### 12.1 空间管理

```cpp
// KBEngine 空间组合模式
// src/server/cellapp/space.h

class Space
{
public:
    virtual ~Space() {}

    // 添加子空间
    virtual void addSpace(Space* space)
    {
        // 基础空间不实现
    }

    // 移除子空间
    virtual void removeSpace(Space* space)
    {
        // 基础空间不实现
    }

    // 更新空间
    virtual void update(float delta)
    {
        // 更新自身
    }
};

// 复合空间
class CompositeSpace : public Space
{
public:
    void addSpace(Space* space) override
    {
        children_.push_back(space);
    }

    void removeSpace(Space* space) override
    {
        children_.erase(
            std::remove(children_.begin(), children_.end(), space),
            children_.end()
        );
    }

    void update(float delta) override
    {
        // 更新所有子空间
        for (Space* child : children_)
        {
            child->update(delta);
        }

        // 更新自身
        updateSelf(delta);
    }

private:
    void updateSelf(float delta)
    {
        // 自身更新逻辑
    }

    std::vector<Space*> children_;
};

// 叶子空间（实际游戏空间）
class CellSpace : public Space
{
public:
    void update(float delta) override
    {
        // 更新空间内的实体
        updateEntities(delta);
    }

private:
    std::vector<Entity*> entities_;
};
```

### 12.2 为什么要用组合模式？

```
组合模式优势:

1. 树形结构
   ├── 支持嵌套空间
   ├── 递归操作
   └── 简化复杂结构

2. 统一接口
   ├── 单个空间和组合空间相同接口
   ├── 客户端无需区分
   └── 使用简单

3. 易于扩展
   ├── 添加新类型空间很容易
   ├── 不影响现有代码
   └── 符合开闭原则

4. 递归处理
   ├── 更新自动传播到子节点
   ├── 自动管理生命周期
   └── 减少手动管理
```

---

## 十三、设计权衡

### 13.1 KBEngine 设计哲学

```
┌─────────────────────────────────────────────────────────────┐
│              KBEngine 设计权衡与哲学                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  简单性 vs 灵活性:                                           │
│  ├── 选择：偏向简单性                                        │
│  ├── 固定架构，约定优于配置                                   │
│  ├── 降低学习成本                                           │
│  └── 适合快速开发                                           │
│                                                             │
│  性能 vs 可维护性:                                           │
│  ├── Python 脚本牺牲性能                                     │
│  ├── 换取开发效率和可维护性                                  │
│  ├── 热更新成为可能                                         │
│  └── 降低业务逻辑复杂度                                      │
│                                                             │
│  单体 vs 分布式:                                             │
│  ├── 固定组件的单体分布                                      │
│  ├── 组件间明确职责                                          │
│  ├── 避免过度拆分                                           │
│  └── 保持架构可控                                           │
│                                                             │
│  同步 vs 异步:                                               │
│  ├── 单线程 Actor 模型                                      │
│  ├── 避免锁竞争                                             │
│  ├── 消息异步处理                                           │
│  └── 简化并发                                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 13.2 适用场景分析

| 设计模式 | KBEngine 应用 | 适用原因 | 代价 |
|----------|---------------|----------|------|
| Singleton | 组件管理器 | 全局唯一，访问方便 | 测试困难，隐式依赖 |
| Factory | 实体创建 | 解耦类型选择 | 工厂类变复杂 |
| Observer | Watcher | 解耦监控 | 运行时开销 |
| Proxy | Ghost/Shadow | 网络优化 | 复杂度增加 |
| Command | 消息处理 | 解耦消息逻辑 | 类数量增加 |
| Object Pool | 内存管理 | 性能优化 | 内存占用 |
| Bridge | 网络层 | 协议抽象 | 间接层 |
| Template Method | 生命周期 | 流程统一 | 钩子限制 |
| Strategy | 负载均衡 | 算法可换 | 策略类增加 |
| Chain of Responsibility | 消息路由 | 灵活处理 | 调试困难 |
| Composite | 空间管理 | 树形结构 | 递归复杂性 |

---

## 十四、总结

### KBEngine 设计原则

```
KBEngine 设计 = 简单 + 实用 + 可扩展

核心原则:

1. 约定优于配置
   ├── 固定的组件结构
   ├── 清晰的职责划分
   └── 降低理解成本

2. 单一职责
   ├── 每个组件做一件事
   ├── BaseApp: 玩家数据
   ├── CellApp: 游戏逻辑
   └── DBMgr: 数据存储

3. 脚本优先
   ├── C++ 核心框架
   ├── Python 业务逻辑
   └── 热更新支持

4. 渐进优化
   ├── 不过早优化
   ├── 先保证正确
   └── 后优化性能
```

### 学习建议

```
理解 KBEngine 设计:

1. 阅读源码
   ├── src/server/ 核心逻辑
   ├── src/lib/ 基础组件
   └── scripts/ 业务实现

2. 理解模式应用
   ├── 识别每个模式的作用
   ├── 理解为什么用这个模式
   └── 思考替代方案

3. 对比其他框架
   ├── Skynet (Actor 模型)
   ├── Pomelo (Node.js)
   └── 取长补短

4. 实践应用
   ├── 模仿设计
   ├── 改进实现
   └── 总结经验
```

---

## 参考资料

- [KBEngine GitHub](https://github.com/kbengine/kbengine)
- [Design Patterns: Elements of Reusable Object-Oriented Software](https://www.amazon.com/Design-Patterns-Elements-Reusable-Object-Oriented/dp/0201633612)
- [Game Programming Patterns](https://www.gameprogrammingpatterns.com/)
