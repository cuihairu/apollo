---
title: Network API
icon: network
prev: /api/README.md
---

# Network API

> 2026-10-04 audit: this page was rewritten against the actual state of HEAD.
> The old draft's `apollo::net::tcp::Server/Client` (`net/tcp/*.hpp`) does not
> exist; the HTTP surface is parsing and routing primitives (no built-in
> `http::Server` class); the WebSocket namespace is `websocket` (not `ws`).

## apollo::net::Connection (TCP connection abstraction)

```cpp
namespace apollo::net {
class Connection {
public:
    virtual ~Connection() = default;
    bool isConnected() const;
    ConnectionState getState() const;
    // synchronous/buffered send
    int32_t send(const char* data, uint32_t length);
    bool sendAsync(const char* data, uint32_t length);
    void disconnect(const char* reason = nullptr);
    void close();
    const std::string& getRemoteAddress() const;
};
}
```

---

## apollo::net::Listener (listening interface)

```cpp
namespace apollo::net {
class Listener {
public:
    virtual ~Listener() = default;
    virtual bool start(const std::string& ip, uint16_t port) = 0;  // empty ip = INADDR_ANY
    virtual bool stop() = 0;
    virtual bool isRunning() const = 0;
    virtual uint32_t getConnectionCount() const = 0;
    virtual void setMaxConnections(uint32_t maxConn) = 0;
};
}
```

---

## apollo::net::EventLoop

```cpp
namespace apollo::net {
class EventLoop {
public:
    bool init(const EventLoopConfig& config = {});
    bool run();          // or runInThread() to run in the background
    void stop();
    void wakeup();
    bool isRunning() const;
    void executeInLoop(Task task);
};
}
```

The same file also provides `Timer`, `HeartbeatManager`, `ReconnectManager`.

---

## apollo::net::http (HTTP primitives)

`include/apollo/net/http.h`: parsing and routing primitives — there is **no**
built-in `http::Server` class (serving HTTP = assembling Listener/EventLoop +
RequestParser/Router yourself).

```cpp
namespace apollo::net::http {
struct Request {
    Method method = Method::Get;
    std::string uri;
    Version version;
    Headers headers;
    std::string body;
    std::unordered_map<std::string, std::string> queryParams;
    std::unordered_map<std::string, std::string> pathParams;
    std::string remoteAddress;
    uint16_t remotePort = 0;
    std::string fullPath() const;
};

struct Response { /* status line/header/body fields */ };

class RequestParser { /* streaming parse -> Request */ };
class Router { /* path -> Handler matching (incl. pathParams extraction) */ };
}
```

`http/rest_client.h` additionally provides REST client primitives.

---

## apollo::net::websocket::Server

```cpp
namespace apollo::net::websocket {
class Server {
public:
    virtual ~Server() = default;
    virtual bool start(const std::string& host, uint16_t port,
                       const Callbacks& callbacks, const Config& config = {}) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual void broadcast(const Message& message) = 0;
    virtual void broadcastText(const std::string& text) = 0;
    virtual size_t getConnectionCount() const = 0;
    virtual ServerConnection* getConnection(uint64_t connId) const = 0;
    using RouteHandler = std::function<bool(ServerConnection*, const std::string& path)>;
    virtual void setRoute(const std::string& path, RouteHandler handler) = 0;
};
}
```

---

## apollo::net::RpcServer

`include/apollo/net/rpc.h`: inter-process RPC (service registration + calls),
see the header for details.

---

## modules/net/protocol (apollo::net::protocol)

Service-discovery metadata (in service): `endpoint.{hpp,cpp}` — the `Protocol`
enum (Ipc/Tcp/InProc/Ws/Auto) + the `Endpoint` struct (service name/host/port),
discovery-surface vocabulary, decoupled from transport implementations.
`channel.hpp` and `apollo/protocol/socket.hpp` do not exist (old-draft
references, removed with the nng Channel retirement, P4-4 batch 5 9121ebc2).

---

## net M1 kernel submodules

The self-built four-layer kernel (converged 2026-10-12); API details are in
the respective headers:

- `modules/net/link/` (`apollo::net_link`) — frame delimiting / `Dispatcher`
  contract routing / `Watermark` backpressure watermark / `InterServerLink`
  unicast topology (reliable seq dedup + cumulative ack).
- `modules/net/gameconnection/` (`apollo::net_gameconnection`) — the
  `GameConnection` L3 facade (net-abstraction.md §2/§3), the single entry
  point for game logic.
  - `enum class Channel { kReliable, kDroppable }` / `Priority { kLow, kNormal, kHigh }` / `SessionState { kHandshaking, kActive, kClosed }` / `SendCode { kAccepted, kBackPressured }`.
  - `send(u16 msg_id, payload, Channel, Priority) -> SendCode` (two overloads: `vector<u8>` / `const u8* + size`).
  - `subscribe(u16 msg_id, Handler)` / `set_route_table(const RouteTable*)` / `allow_domain(string)`.
  - `state() -> SessionState` / `pump(now_ms) -> bool` / `attach(transport, now_ms)` / `close(reason)`.
  - Holds an internal `InterServerLink`; `link()` / `stats()` access the link layer directly.
  - L2 session deltas (four-channel queue / `TRIMMED_LOW` / `Resuming` / resume callbacks / scene-thread delivery) land in later batches; `Priority` is currently semantic-only.
- `modules/net/linktrace/` (`apollo::net_linktrace`) — W3C traceparent context + structured span line emission + the 932 internal_envelope codec seam.
- `modules/net/linkrpc/` (`apollo::net_linkrpc`) — contract 940/941 correlation envelope + corr_id correlation + the `call_async`/`poll`/`wait` bridge.
- `modules/net/remoteentity/` (`apollo::net_remoteentity`) — `RemoteEntityRef` / `RemoteMethodSchema` / `RouteResolver` / `RpcTrace` (the remaining pieces of the remote-entity-call four-piece set, landed 2026-10-13).
- `modules/net/contract_wire/` (`apollo::net_contract_wire`) — contract payload codec driven by generated MsgMeta headers.

---

**Not implemented**: `apollo::net::tcp::Server/Client` (an old-draft API that
never existed), built-in `http::Server/Client` classes; these old-draft
interfaces get cleaned up as P4-1 rewrites pages against real APIs.
