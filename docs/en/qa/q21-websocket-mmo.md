
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q21: What are the application scenarios for WebSocket in MMO?

## Core Conclusion

WebSocket is suited to solving the "long-connection bidirectional communication in browser or lightweight client environments" problem, but it is not the general-purpose optimal solution for MMO real-time networking.

If the client mainly runs in a browser, WebSocket is almost the default choice; if it is a native client, a strongly real-time combat scenario, or an extremely bandwidth- and latency-sensitive case, many systems still prefer custom TCP protocols, UDP, KCP, or QUIC.

So the key to judging is not "can WebSocket do MMO", but:

- What is the client runtime environment
- How high is the real-time requirement
- How to trade off access and operations cost

## 1. What WebSocket essentially solves

WebSocket provides:

- A TCP-based long connection
- Bidirectional communication between client and server
- Direct usability in a browser environment
- Better suitability for continuous message exchange than traditional HTTP polling

Its greatest value is not that performance suddenly beats TCP, but that:

- It is usable in browsers
- Gateways, proxies, and access layers are relatively mature
- Access cost is low

Because WebSocket essentially still runs on top of TCP, it inherits many of TCP's characteristics and limitations.

## 2. Which scenarios it is suited to in MMO

### 1. Browser MMO or H5 games

This is the most typical scenario.

Browsers natively support WebSocket, frontend access is simple, and it suits:

- Web games
- H5 MMO
- Socialized lightweight multiplayer scenarios

Such projects usually care more than pure native clients about:

- Fast access
- Cross-platform coverage
- Operational and deployment convenience

### 2. Login, gateway, lobby, and social layers

Even if the whole game's core combat does not use WebSocket, it is commonly used for:

- Login authentication
- Lobby state sync
- Chat
- Matchmaking
- Weak real-time features like friends, guilds, and mail

The reasons are direct:

- Such messages are not that sensitive to tens-of-milliseconds latency
- Access layers often prefer to unify HTTP/WebSocket infrastructure

### 3. Medium and light real-time gameplay

If the gameplay tempo is not extreme, WebSocket can also carry:

- Turn-based combat
- Board and card games
- Light ARPG
- Room-based small-scale battles

But if requirements for high-frequency state sync, packet loss recovery, and extreme weak-network performance are very high, evaluation should be cautious.

## 3. What it is not suited to

### 1. Extremely low-latency real-time combat

Because WebSocket inherits TCP's ordering and reliability semantics, head-of-line blocking typically appears when packet loss occurs.

This means:

- If the previous packet has not arrived, later packets may not be delivered in time even though they have arrived
- Under weak networks the sense of jitter is amplified

For FPS, strong action battles, and extremely high-frequency displacement sync, this is usually not an ideal property.

### 2. Scenarios needing fine-grained control over transport semantics

Many real-time games layer their messages:

- Must be reliable
- May be dropped but must be fresh
- High-frequency state keeps only the latest

WebSocket/TCP is not flexible enough to support such differentiated semantics. To achieve the same effect, you usually need to build another layer of message management at the application layer, and complexity will not be lower.

## 4. The relationship between WebSocket and TCP / UDP / KCP

### 1. Relationship with TCP

WebSocket is not a replacement for TCP, but "a more convenient application-layer protocol for cross-platform access, running on top of TCP".

If it is a native client, using a custom TCP protocol directly is usually lighter and more controllable.

### 2. Relationship with UDP / KCP

UDP or KCP is more suited to:

- High-frequency state sync
- Low-latency input
- Allowing some message loss
- Needing custom reliability strategies

WebSocket is more suited to:

- Easy access
- Easy penetration of existing Web infrastructure
- Browser compatibility first

They solve problems of different priorities.

## 5. How to use it more reasonably in MMO

A common practical division is:

- Login, lobby, chat, announcements, and matchmaking go through WebSocket
- Core combat servers or real-time scene servers go through TCP / UDP / KCP

Some projects unify on WebSocket only, but the premise is usually:

- The gameplay tempo is relatively mild
- The team values development efficiency and unified access more
- Requirements for extreme weak-network performance are not that aggressive

If everything is pushed onto WebSocket, you have to accept its constraints at the high-frequency sync level.

## 6. Several engineering points to note

### 1. Message format still needs to be designed carefully

Using WebSocket does not mean you should directly transmit large JSON packages. Many projects eventually still carry the following in WebSocket binary frames:

- Custom packet headers
- Protobuf
- Compression and encryption information

Otherwise message body bloat grows quickly.

### 2. Heartbeat and reconnection still need to be done yourself

WebSocket is only a connection channel and will not automatically solve:

- Fake connection detection
- Session recovery
- State resend
- Idempotent retry

These remain the application layer's responsibility.

### 3. Gateway and backend must be decoupled

Browser access usually first reaches the WebSocket gateway, and then the gateway forwards to the business process. This makes it easier to handle:

- TLS termination
- Rate limiting
- Connection management
- Protocol adaptation

## 7. Common misconceptions

### 1. WebSocket is more modern, so it must be more suitable for games than TCP

No. It is mainly more friendly in access form; it does not mean real-time performance is necessarily better.

### 2. Since browsers can run WebSocket, large MMO core combat should also use it directly

Not necessarily. Browser limitations and TCP semantics decide that it is suited to some scenarios and unsuited to others.

### 3. Using WebSocket lets you skip the gateway

Usually not. Connection management, authentication, rate limiting, forwarding, and observability still need a dedicated access layer.

## References

- RFC 6455, WebSocket
- RFC 9293, TCP
- Various game gateway and browser long-connection practice materials
