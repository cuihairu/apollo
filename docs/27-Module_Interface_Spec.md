# 核心模块接口与类图

> 目的：在高层设计基础上，进一步描述各核心模块的接口、类结构、状态机与关键交互，便于进入编码阶段。本文件覆盖 NetCore、Transport、AOI Service、DataProxy、Battle Service、插件框架。

## 1. NetCore

### 1.1 类关系
```
NetCore
 ├─ NetListener (io_uring / IOCP backend)
 │    └─ TcpListenerLinux
 │    └─ TcpListenerWindows
 ├─ ConnectionRegistry
 │    └─ NetConnection
 │           ├─ IoUringConnection
 │           └─ IocpConnection
 ├─ SessionManager
 ├─ MessageCodec
 ├─ RateLimiter
 └─ Deduplicator
```

### 1.2 关键接口
```cpp
class INetListener {
public:
    virtual ~INetListener() = default;
    virtual bool Start(const NetConfig& cfg) = 0;
    virtual void Stop() = 0;
};

class INetConnection {
public:
    virtual ~INetConnection() = default;
    virtual bool Send(const ByteBuffer& buffer) = 0;
    virtual void Close(DisconnectReason reason) = 0;
    virtual ConnectionId Id() const = 0;
    virtual Endpoint RemoteEndpoint() const = 0;
    virtual void SetRateLimiter(std::shared_ptr<RateLimiter>) = 0;
};

class SessionManager {
public:
    bool Attach(ConnectionId connId, SessionToken token);
    void Detach(ConnectionId connId);
    Session* GetSession(ConnectionId connId);
};
```

### 1.3 状态机（连接）
```
LISTEN -> CONNECTING -> HANDSHAKE -> ACTIVE -> CLOSING -> CLOSED
                     ↘ (handshake failed) -> CLOSED
```

## 2. Transport 层

### 2.1 类图
```
ITransport
 ├─ NngTcpTransport
 ├─ NngIpcTransport
 └─ (Future) ShmTransport

TransportManager
 ├─ std::vector<ITransport*>
 └─ Dialer (select endpoint by priority)
```

### 2.2 接口详情
```cpp
class ITransport {
public:
    virtual std::string Name() const = 0;
    virtual bool CanDial(const TransportEndpoint& ep) const = 0;
    virtual std::unique_ptr<ITransportConnection> Dial(const TransportEndpoint& ep) = 0;
    virtual std::unique_ptr<ITransportListener> Listen(const TransportEndpoint& ep) = 0;
};

class ITransportConnection {
public:
    virtual bool Send(const Message& msg) = 0;
    virtual void AsyncSend(const Message& msg, CompletionHandler cb) = 0;
    virtual void Receive(ReceiveHandler cb) = 0;
    virtual void Close() = 0;
    virtual const TransportEndpoint& Endpoint() const = 0;
};
```

### 2.3 交互序列（Dial）
```
TransportManager::Dial(target)
  -> fetch endpoints from ServiceRegistry
  -> sort by priority
  -> for each endpoint:
       if transport.CanDial(ep):
           conn = transport.Dial(ep)
           if conn: return conn
  -> throw DialFailure
```

## 3. AOI Service

### 3.1 类图
```
AOIService
 ├─ SpatialIndex
 │    ├─ GridIndex
 │    └─ QuadtreeIndex
 ├─ AOIShardManager
 │    └─ AOIShard (manages subset of scene)
 ├─ AOIEventDispatcher
 ├─ TransportChannel (Zone ↔ AOI)
```

### 3.2 核心接口
```cpp
class AOIService {
public:
    void UpdatePosition(const AOIUpdate& update);   // from Zone
    void RemoveEntity(uint64_t entityId);
    void SubscribeScene(uint64_t sceneId, IAOIListener* listener);
};

class IAOIListener {
public:
    virtual void OnAOIEvent(const AOIEvent& event) = 0;
};
```

### 3.3 状态流程
```
ZoneServer -> (Transport) AOIService.UpdatePosition
  -> AOIShard finds affected entities
  -> compute enter/leave/sync sets
  -> AOIEventDispatcher pushes event to listener (Zone/Gate)
```

## 4. DataProxy

### 4.1 类图
```
DataProxy
 ├─ CacheLayer (RedisClient)
 ├─ StorageLayer (MySqlClient, ShardRouter)
 ├─ WriteQueue (AsyncWriter)
 ├─ TLogPublisher
 └─ Serializer (Protobuf)
```

### 4.2 主要接口
```cpp
class IDataProxy {
public:
    virtual PlayerSnapshot LoadPlayer(uint64_t playerId) = 0;
    virtual void SavePlayer(const PlayerSnapshot& player) = 0;
    virtual void UpdateEconomy(uint64_t playerId, CurrencyDelta delta, TransactionContext ctx) = 0;
    virtual QueryResult QueryPlayers(const QueryRequest& request) = 0;
};

struct PlayerSnapshot {
    uint64_t playerId;
    PlayerProfile profile;      // Proto
    AttributeMap attributes;
    uint64_t version;
};
```

### 4.3 序列图（LoadPlayer）
```
ZoneServer -> DataProxy.LoadPlayer(playerId)
  -> CacheLayer.Get(playerId)
      -> if hit: return snapshot
      -> else: StorageLayer.Query(shard)
              -> Serializer.Deserialize(blob)
              -> CacheLayer.Set
              -> return snapshot
```
（Compact GameServer 模式可直接将 CacheLayer/StorageLayer 嵌入单进程，接口保持一致。）

## 5. Battle Service

### 5.1 类图
```
BattleService
 ├─ Matchmaker
 ├─ BattleInstanceManager
 │    └─ BattleInstance
 │          ├─ EntityRegistry
 │          ├─ ComponentManager
 │          ├─ SystemPipeline
 │          └─ EventBus
 ├─ CommandGateway
 ├─ ResultDispatcher
```

### 5.2 关键接口
```cpp
class IBattleService {
public:
    virtual BattleInstanceId CreateInstance(const BattleConfig& cfg) = 0;
    virtual void DestroyInstance(BattleInstanceId id) = 0;
    virtual void SubmitCommand(BattleInstanceId id, const BattleCommand& cmd) = 0;
    virtual void RegisterListener(BattleInstanceId id, IBattleListener* listener) = 0;
};

class IBattleListener {
public:
    virtual void OnBattleEvent(const BattleEvent& event) = 0;
    virtual void OnBattleResult(const BattleResult& result) = 0;
};
```

### 5.3 状态机（Battle Instance）
```
CREATED -> PREPARING -> RUNNING -> (WIN/LOSE/DRAW) -> RESULT -> DESTROYED
                         ↘ error -> ABORTED -> DESTROYED
```

## 6. 插件框架

### 6.1 类图
```
PluginManager
 ├─ SharedLibrary
 ├─ PluginDescriptor
 └─ LoadedPlugin
       ├─ RegisterBeans(ApplicationContext&)
       └─ OnUnload()
```

### 6.2 接口
```cpp
class PluginManager {
public:
    void LoadPlugin(const std::filesystem::path& path);
    void UnloadPlugin(const std::string& name);
private:
    struct LoadedPlugin {
        std::string name;
        SharedLibrary library;
        std::function<bool(ApplicationContext&)> registerFunc;
    };
    std::unordered_map<std::string, LoadedPlugin> plugins_;
};

extern "C" bool RegisterBeans(ApplicationContext& ctx, const PluginConfig& cfg);
```

### 6.3 加载流程
```
Application startup:
  -> scan plugin directory
  -> for each so/dll:
       SharedLibrary.Load
       fetch RegisterBeans symbol
       RegisterBeans(ctx, cfg)
  -> ApplicationContext.Startup()
```

---

这些接口与类图为后续编码提供参考，具体实现可参考各设计文档 (`docs/16-25`). 在开发过程中，应根据实际需求进一步细化方法参数、异常处理和并发策略。***
