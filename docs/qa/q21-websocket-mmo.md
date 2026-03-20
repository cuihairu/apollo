# Q21: WebSocket 在 MMO 中有什么应用场景？

## 问题分析

本题考察对 WebSocket 协议及其在游戏应用中的理解：
- WebSocket 协议特点
- 与 TCP/UDP 的对比
- 在 MMO 中的应用场景
- KBEngine 对 WebSocket 的支持

---

## 一、WebSocket 协议基础

### 1.1 WebSocket 简介

```
┌─────────────────────────────────────────────────────────────┐
│                  WebSocket 协议概述                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  WebSocket 是什么？                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  一种在单个 TCP 连接上进行全双工通信的协议          │       │
│  │                                                   │       │
│  │  RFC 6455 标准                                    │       │
│  │  2011 年发布                                      │       │
│  │  设计用于 Web 浏览器和服务器通信                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  握手流程:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 客户端发送 HTTP Upgrade 请求                  │       │
│  │     GET /chat HTTP/1.1                            │       │
│  │     Host: server.example.com                     │       │
│  │     Upgrade: websocket                           │       │
│  │     Connection: Upgrade                           │       │
│  │     Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==  │       │
│  │     Sec-WebSocket-Version: 13                    │       │
│  │                                                   │       │
│  │  2. 服务器返回 101 Switching Protocols            │       │
│  │     HTTP/1.1 101 Switching Protocols             │       │
│  │     Upgrade: websocket                           │       │
│  │     Connection: Upgrade                           │       │
│  │     Sec-WebSocket-Accept: HSmrc0sMlYUkAGmm5OPpG...│       │
│  │                                                   │       │
│  │  3. 连接升级为 WebSocket                         │       │
│  │  4. 开始双向通信                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 WebSocket 帧格式

```
┌─────────────────────────────────────────────────────────────┐
│                 WebSocket 数据帧格式                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  0                   1                   2                   │
│  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1│
│  +-+-+-+-+-------+-+-------------+-------------------------------+│
│  |F|R|R|R| opcode|M| Payload len |    Extended payload length    ││
│  |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           ││
│  |N|V|V|V|       |S|             |   (if payload len==126/127)   ││
│  | |1|2|3|       |K|             |                               ││
│  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +│
│  |     Extended payload length continued, if payload len == 127  ││
│  + - - - - - - - - - - - - - - - +-------------------------------+│
│  |                               |Masking-key, if MASK set to 1   ││
│  +-------------------------------+-------------------------------+│
│  | Masking-key (continued)       |          Payload Data         ││
│  +-------------------------------- - - - - - - - - - - - - - - - +│
│  |                     Payload Data continued ...                ││
│  +---------------------------------------------------------------+│
│                                                             │
│  字段说明:                                                  │
│  ├── FIN (1 bit): 最后一帧                                    │
│  ├── RSV1-3 (3 bits): 保留                                    │
│  ├── Opcode (4 bits): 帧类型                                   │
│  │   ├── 0x0: 连续帧                                          │
│  │   ├── 0x1: 文本帧                                          │
│  │   ├── 0x2: 二进制帧                                        │
│  │   ├── 0x8: 关闭连接                                        │
│  │   ├── 0x9: Ping                                            │
│  │   └── 0xA: Pong                                            │
│  ├── MASK (1 bit): 是否掩码（客户端必须为1）                   │
│  ├── Payload len (7 bits): 负载长度                            │
│  ├── Masking key (32 bits): 掩码密钥                           │
│  └── Payload data: 实际数据                                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、WebSocket vs 其他协议

### 2.1 协议对比

```
┌─────────────────────────────────────────────────────────────┐
│              WebSocket vs TCP vs UDP vs HTTP                 │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  HTTP:                                                     │
│  ┌─────────────────────────────────────────────────┐       │
│  │  优点:                                            │       │
│  │  ├── 通用、标准化                                   │       │
│  │  ├── 防火墙友好                                     │       │
│  │  ├── 跨域支持 (CORS)                               │       │
│  │   │                                               │       │
│  │  缺点:                                            │       │
│  │  ├── 半双工（请求-响应模式）                         │       │
│  │  ├── 头部开销大                                     │       │
│  │  ├── 无连接状态                                     │       │
│  │  └── 无法服务器主动推送                              │       │
│  │                                                   │       │
│  │  适用: API 调用、资源加载                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  WebSocket:                                                │
│  ┌─────────────────────────────────────────────────┐       │
│  │  优点:                                            │       │
│  │  ├── 全双工通信                                    │       │
│  │  ├── 低延迟                                        │       │
│  │  ├── 持久连接                                      │       │
│  │  ├── 服务器可主动推送                                │       │
│  │  ├── 浏览器原生支持                                 │       │
│  │  └── 跨域支持                                       │       │
│  │                                                   │       │
│  │  缺点:                                            │       │
│  │  ├── 基于 TCP（受 TCP 缺陷影响）                   │       │
│  │  ├── 帧开销                                        │       │
│  │  ├── 连接状态管理复杂                               │       │
│  │  └── 二进制支持有限                                 │       │
│  │                                                   │       │
│  │  适用: 聊天、实时通知、Web 游戏通信                  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  原生 TCP:                                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  优点:                                            │       │
│  │  ├── 高效、低开销                                   │       │
│  │  ├── 完全可控                                       │       │
│  │  ├── 支持二进制                                     │       │
│  │  └── 可自定义协议                                    │       │
│  │                                                   │       │
│  │  缺点:                                            │       │
│  │  ├── 浏览器不支持                                    │       │
│  │  ├── 需要处理粘包/拆包                                │       │
│  │  ├── 防火墙可能拦截                                   │       │
│  │  └── 开发复杂度高                                    │       │
│  │                                                   │       │
│  │  适用: 原生客户端游戏                                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  UDP:                                                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │  优点:                                            │       │
│  │  ├── 低延迟                                        │       │
│  │  ├── 无连接开销                                    │       │
│  │  ├── 支持广播/组播                                  │       │
│  │  └── 适合实时数据                                   │       │
│  │                                                   │       │
│  │  缺点:                                            │       │
│  │  ├── 无可靠保证                                     │       │
│  │  ├── 丢包问题                                       │       │
│  │  ├── 浏览器支持有限（WebRTC/UDP）                   │       │
│  │  └── 需要实现可靠层                                  │       │
│  │                                                   │       │
│  │  适用: FPS、实时战斗                                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 协议对比表

| 特性 | HTTP | WebSocket | 原生 TCP | UDP |
|------|------|-----------|----------|-----|
| **传输方式** | 半双工 | 全双工 | 全双工 | 无连接 |
| **连接方式** | 短连接 | 长连接 | 长连接 | 无连接 |
| **服务器推送** | ❌ | ✅ | ✅ | ✅ |
| **浏览器支持** | ✅ | ✅ | ❌ | ❌* |
| **延迟** | 高 | 中 | 低 | 最低 |
| **开销** | 高 | 中 | 低 | 低 |
| **可靠性** | ✅ | ✅ | ✅ | ❌ |
| **二进制** | ❌ | ✅ | ✅ | ✅ |
| *UDP 需通过 WebRTC | | | | |

---

## 三、WebSocket 在 MMO 中的应用

### 3.1 应用场景

```
┌─────────────────────────────────────────────────────────────┐
│            WebSocket 在 MMO 中的应用场景                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  场景 1: Web 端聊天系统                                     │
│  ┌─────────────────────────────────────────────────┐       │
│  │  优势:                                            │       │
│  │  ├── 浏览器原生支持，无需插件                        │       │
│  │  ├── 全双工，实时性好                               │       │
│  │  ├── 支持多频道（世界、公会、私聊）                  │       │
│  │  └── 低流量开销                                     │       │
│  │                                                   │       │
│  │  实现:                                            │       │
│  │  客户端 ←WebSocket→ 聊天服务器 ←TCP/KCP→ 游戏服务器  │       │
│  │                                                   │       │
│  │  功能:                                            │       │
│  │  ├── 实时聊天消息推送                                │       │
│  │  ├── 在线人数显示                                   │       │
│  │  ├── 表情/图片发送                                   │       │
│  │  └── 聊天历史记录                                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  场景 2: 实时通知系统                                       │
│  ┌─────────────────────────────────────────────────┐       │
│  │  服务器主动推送通知给玩家                            │       │
│  │                                                   │       │
│  │  通知类型:                                         │       │
│  │  ├── 活动开始提醒                                   │       │
│  │  ├── 系统公告                                       │       │
│  │  ├── 邮件到达提醒                                   │       │
│  │  ├── 好友上线提醒                                   │       │
│  │  ├── 战斗匹配成功                                   │       │
│  │  └── 组队邀请                                       │       │
│  │                                                   │       │
│  │  架构:                                            │       │
│  │  游戏服务器 → 通知服务 → WebSocket → Web/移动客户端 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  场景 3: Web 管理后台                                       │
│  ┌─────────────────────────────────────────────────┐       │
│  │  GM/运维人员通过 Web 管理游戏                       │       │
│  │                                                   │       │
│  │  功能:                                            │       │
│  │  ├── 实时监控服务器状态                             │       │
│  │  ├── 在线玩家列表                                   │       │
│  │  ├── 实时日志查看                                   │       │
│  │  ├── 封禁/解封玩家                                  │       │
│  │  ├── 发送系统公告                                   │       │
│  │  └── 执行 GM 命令                                   │       │
│  │                                                   │       │
│  │  架构:                                            │       │
│  │  Web 后台 ←WebSocket→ 代理服务器 ←TCP→ 游戏服务器  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  场景 4: 社交功能                                           │
│  ┌─────────────────────────────────────────────────┐       │
│  │  移动端 Web App 社交功能                           │       │
│  │                                                   │       │
│  │  功能:                                            │       │
│  │  ├── 查看好友列表                                   │       │
│  │  ├── 查看公会成员                                   │       │
│  │  ├── 好友在线状态                                   │       │
│  │  ├── 发送私聊                                       │       │
│  │  └── 查看排行榜                                     │       │
│  │                                                   │       │
│  │  优势: 随时随地访问，无需下载客户端                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  场景 5: 轻量级 H5 游戏                                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │  简单的网页版游戏客户端                             │       │
│  │                                                   │       │
│  │  功能:                                            │       │
│  │  ├── 简单的战斗模拟                                 │       │
│  │  ├── 查看角色信息                                   │       │
│  │  ├── 背包管理                                       │       │
│  │  ├── 拍卖行浏览                                     │       │
│  │  └── 公会聊天                                       │       │
│  │                                                   │       │
│  │  限制: 不适合复杂战斗操作                           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 混合架构设计

```
┌─────────────────────────────────────────────────────────────┐
│              MMO 混合网络架构设计                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                    ┌──────────────┐                        │
│                    │   原生客户端   │                        │
│                    │   (TCP/UDP)   │                        │
│                    └──────┬───────┘                        │
│                           │                                 │
│                           ▼                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │              游戏服务器                          │       │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐         │       │
│  │  │ LoginApp│  │ BaseApp │  │ CellApp │         │       │
│  │  └────┬────┘  └────┬────┘  └────┬────┘         │       │
│  │       │            │            │                │       │
│  │       └────────────┴────────────┘                │       │
│  │                   │                              │       │
│  │                   ▼                              │       │
│  │         ┌──────────────────┐                    │       │
│  │         │  WebSocket 网关   │                    │       │
│  │         │  (协议转换层)      │                    │       │
│  │         └────────┬─────────┘                    │       │
│  └──────────────────┼──────────────────────────────┘       │
│                     │                                       │
│         ┌───────────┼───────────┐                           │
│         │           │           │                           │
│         ▼           ▼           ▼                           │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                    │
│  │Web 聊天  │ │Web 管理  │ │移动端    │                    │
│  │  客户端   │ │  后台    │ │  App     │                    │
│  │(WebSocket)│ │(WebSocket)│ │(WebSocket)│                   │
│  └──────────┘ └──────────┘ └──────────┘                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 四、WebSocket 服务器实现

### 4.1 基础 WebSocket 服务器

```cpp
// WebSocket 服务器实现

class WebSocketServer {
public:
    WebSocketServer(uint16_t port) : port_(port) {
        // 初始化 WebSocket 库
        initWebSocketLibrary();
    }

    // 启动服务器
    void start() {
        // 创建监听 socket
        listenSocket_ = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        bind(listenSocket_, (struct sockaddr*)&addr, sizeof(addr));
        listen(listenSocket_, 1024);

        // 设置非阻塞
        setNonBlocking(listenSocket_);

        LOG_INFO("WebSocket server started on port " + std::to_string(port_));

        // 事件循环
        eventLoop();
    }

private:
    void eventLoop() {
        while (running_) {
            // 使用 epoll/IOCP 等待事件
            struct epoll_event events[MAX_EVENTS];
            int nfds = epoll_wait(epollFd_, events, MAX_EVENTS, 100);

            for (int i = 0; i < nfds; i++) {
                if (events[i].data.fd == listenSocket_) {
                    acceptNewConnection();
                } else {
                    WebSocketConnection* conn =
                        (WebSocketConnection*)events[i].data.ptr;

                    if (events[i].events & EPOLLIN) {
                        handleRead(conn);
                    }
                    if (events[i].events & EPOLLOUT) {
                        handleWrite(conn);
                    }
                }
            }
        }
    }

    void acceptNewConnection() {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);

        int clientFd = accept(listenSocket_,
                             (struct sockaddr*)&clientAddr,
                             &addrLen);

        if (clientFd < 0) {
            return;
        }

        // 设置非阻塞
        setNonBlocking(clientFd);

        // 创建连接
        auto* conn = new WebSocketConnection(clientFd);
        connections_[clientFd] = conn;

        // 添加到 epoll
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = conn;
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, clientFd, &ev);

        LOG_INFO("New WebSocket connection: " + std::to_string(clientFd));
    }

    void handleRead(WebSocketConnection* conn) {
        uint8_t buffer[4096];
        int len = recv(conn->fd(), buffer, sizeof(buffer), 0);

        if (len <= 0) {
            // 连接关闭
            closeConnection(conn);
            return;
        }

        if (!conn->isHandshakeComplete()) {
            // 处理 WebSocket 握手
            handleHandshake(conn, buffer, len);
        } else {
            // 处理 WebSocket 帧
            handleWebSocketFrame(conn, buffer, len);
        }
    }

    void handleHandshake(WebSocketConnection* conn,
                        const uint8_t* data, size_t len) {
        std::string request((char*)data, len);

        // 检查是否是 WebSocket 握手请求
        if (request.find("Upgrade: websocket") == std::string::npos) {
            // HTTP 请求
            sendHTTPResponse(conn, request);
            return;
        }

        // 提取 Sec-WebSocket-Key
        std::string key = extractWebSocketKey(request);

        // 计算 Sec-WebSocket-Accept
        std::string acceptKey = computeAcceptKey(key);

        // 发送握手响应
        std::string response =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
            "\r\n";

        send(conn->fd(), response.c_str(), response.size(), 0);

        conn->setHandshakeComplete();
        LOG_INFO("WebSocket handshake complete");
    }

    void handleWebSocketFrame(WebSocketConnection* conn,
                             const uint8_t* data, size_t len) {
        size_t offset = 0;

        while (offset < len) {
            // 解析帧头
            uint8_t firstByte = data[offset++];
            uint8_t secondByte = data[offset++];

            bool fin = (firstByte & 0x80) != 0;
            uint8_t opcode = firstByte & 0x0F;
            bool masked = (secondByte & 0x80) != 0;
            uint64_t payloadLen = secondByte & 0x7F;

            // 扩展长度
            if (payloadLen == 126) {
                payloadLen = (data[offset] << 8) | data[offset + 1];
                offset += 2;
            } else if (payloadLen == 127) {
                payloadLen = 0;
                for (int i = 0; i < 8; i++) {
                    payloadLen = (payloadLen << 8) | data[offset++];
                }
            }

            // 掩码密钥
            uint8_t maskKey[4];
            if (masked) {
                memcpy(maskKey, data + offset, 4);
                offset += 4;
            }

            // 负载数据
            std::vector<uint8_t> payload(payloadLen);
            memcpy(payload.data(), data + offset, payloadLen);
            offset += payloadLen;

            // 去掩码
            if (masked) {
                for (size_t i = 0; i < payloadLen; i++) {
                    payload[i] ^= maskKey[i % 4];
                }
            }

            // 处理 opcode
            switch (opcode) {
                case 0x1: // 文本帧
                    handleTextMessage(conn, payload);
                    break;

                case 0x2: // 二进制帧
                    handleBinaryMessage(conn, payload);
                    break;

                case 0x8: // 关闭连接
                    handleClose(conn);
                    break;

                case 0x9: // Ping
                    sendPong(conn, payload);
                    break;

                case 0xA: // Pong
                    break;
            }
        }
    }

    void handleTextMessage(WebSocketConnection* conn,
                          const std::vector<uint8_t>& payload) {
        std::string message(payload.begin(), payload.end());

        LOG_DEBUG("Received text message: " + message);

        // 解析 JSON 消息
        try {
            json msg = json::parse(message);

            std::string type = msg["type"];
            json data = msg["data"];

            if (type == "chat") {
                handleChatMessage(conn, data);
            } else if (type == "login") {
                handleLoginMessage(conn, data);
            } else if (type == "heartbeat") {
                handleHeartbeat(conn);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse message: " + std::string(e.what()));
        }
    }

    void sendFrame(WebSocketConnection* conn,
                  uint8_t opcode,
                  const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> frame;

        // 帧头
        uint8_t firstByte = 0x80 | opcode; // FIN + opcode
        frame.push_back(firstByte);

        // 长度
        size_t payloadLen = payload.size();
        if (payloadLen < 126) {
            frame.push_back(payloadLen);
        } else if (payloadLen < 65536) {
            frame.push_back(126);
            frame.push_back((payloadLen >> 8) & 0xFF);
            frame.push_back(payloadLen & 0xFF);
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; i--) {
                frame.push_back((payloadLen >> (i * 8)) & 0xFF);
            }
        }

        // 负载
        frame.insert(frame.end(), payload.begin(), payload.end());

        // 发送
        send(conn->fd(), frame.data(), frame.size(), 0);
    }

    void sendTextMessage(WebSocketConnection* conn,
                        const std::string& message) {
        std::vector<uint8_t> payload(message.begin(), message.end());
        sendFrame(conn, 0x1, payload); // 文本帧
    }

    std::string extractWebSocketKey(const std::string& request) {
        size_t keyPos = request.find("Sec-WebSocket-Key:");
        if (keyPos == std::string::npos) {
            return "";
        }

        keyPos += 19; // "Sec-WebSocket-Key:" length
        size_t keyEnd = request.find("\r\n", keyPos);
        std::string key = request.substr(keyPos, keyEnd - keyPos);

        // 去除空格
        while (!key.empty() && key[0] == ' ') {
            key.erase(0, 1);
        }

        return key;
    }

    std::string computeAcceptKey(const std::string& key) {
        // WebSocket GUID
        const std::string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string combined = key + guid;

        // SHA-1 哈希
        unsigned char hash[20];
        SHA1((unsigned char*)combined.c_str(), combined.size(), hash);

        // Base64 编码
        return base64Encode(hash, 20);
    }

    int listenSocket_;
    uint16_t port_;
    int epollFd_;

    std::unordered_map<int, WebSocketConnection*> connections_;
    std::atomic<bool> running_{true};

    static constexpr int MAX_EVENTS = 1024;
};

// WebSocket 连接类
class WebSocketConnection {
public:
    WebSocketConnection(int fd) : fd_(fd), handshakeComplete_(false) {}

    int fd() const { return fd_; }
    bool isHandshakeComplete() const { return handshakeComplete_; }
    void setHandshakeComplete() { handshakeComplete_ = true; }

private:
    int fd_;
    bool handshakeComplete_;
};
```

### 4.2 聊天服务器实现

```cpp
// WebSocket 聊天服务器

class ChatWebSocketServer : public WebSocketServer {
public:
    ChatWebSocketServer(uint16_t port) : WebSocketServer(port) {}

private:
    void handleChatMessage(WebSocketConnection* conn, const json& data) {
        std::string channel = data["channel"];
        std::string sender = data["sender"];
        std::string message = data["message"];

        // 创建聊天消息
        json chatMsg;
        chatMsg["type"] = "chat";
        chatMsg["channel"] = channel;
        chatMsg["sender"] = sender;
        chatMsg["message"] = message;
        chatMsg["timestamp"] = getCurrentTime();

        std::string msgStr = chatMsg.dump();

        // 广播给频道内的所有连接
        broadcastToChannel(channel, msgStr);
    }

    void handleLoginMessage(WebSocketConnection* conn, const json& data) {
        std::string token = data["token"];

        // 验证 token
        if (validateToken(token)) {
            conn->setAuthenticated(true);
            conn->setUsername(getUsernameFromToken(token));

            // 加入默认频道
            joinChannel(conn, "world");

            // 发送登录成功
            json response;
            response["type"] = "login";
            response["success"] = true;
            response["channels"] = getJoinedChannels(conn);

            sendTextMessage(conn, response.dump());
        } else {
            json response;
            response["type"] = "login";
            response["success"] = false;
            response["error"] = "Invalid token";

            sendTextMessage(conn, response.dump());
        }
    }

    void handleHeartbeat(WebSocketConnection* conn) {
        conn->updateLastActivity();

        json pong;
        pong["type"] = "pong";
        pong["timestamp"] = getCurrentTime();

        sendTextMessage(conn, pong.dump());
    }

    void joinChannel(WebSocketConnection* conn, const std::string& channel) {
        channels_[channel].insert(conn);
        conn->addChannel(channel);

        // 通知频道内的其他用户
        json notify;
        notify["type"] = "user_joined";
        notify["channel"] = channel;
        notify["user"] = conn->getUsername();

        broadcastToChannel(channel, notify.dump(), conn);
    }

    void broadcastToChannel(const std::string& channel,
                           const std::string& message,
                           WebSocketConnection* exclude = nullptr) {
        auto it = channels_.find(channel);
        if (it == channels_.end()) {
            return;
        }

        for (auto* conn : it->second) {
            if (conn != exclude && conn->isAuthenticated()) {
                sendTextMessage(conn, message);
            }
        }
    }

    std::unordered_map<std::string, std::unordered_set<WebSocketConnection*>> channels_;
};
```

---

## 五、KBEngine WebSocket 集成

### 5.1 WebSocket 网关设计

```cpp
// KBEngine WebSocket 网关
// 将 WebSocket 消息转换为 KBEngine 内部消息

class KBEngineWebSocketGateway {
public:
    // 处理来自 WebSocket 的消息
    void onWebSocketMessage(const WebSocketMessage& wsMsg) {
        // 转换为 KBEngine 消息格式
        KBEngineMessage kbMsg;

        switch (wsMsg.type) {
            case MessageType::Chat:
                kbMsg = convertChatMessage(wsMsg);
                break;

            case MessageType::Move:
                kbMsg = convertMoveMessage(wsMsg);
                break;

            case MessageType::Action:
                kbMsg = convertActionMessage(wsMsg);
                break;

            default:
                break;
        }

        // 路由到对应的 KBEngine App
        routeToKBEngine(kbMsg);
    }

    // 处理来自 KBEngine 的消息
    void onKBEngineMessage(const KBEngineMessage& kbMsg) {
        // 转换为 WebSocket 消息格式
        WebSocketMessage wsMsg;

        switch (kbMsg.msgId) {
            case KBEngineMsgId::OnRemoteCallMethod:
                wsMsg = convertRemoteCall(kbMsg);
                break;

            case KBEngineMsgId::OnUpdateData:
                wsMsg = convertUpdateData(kbMsg);
                break;

            case KBEngineMsgId::OnEntityEnter:
                wsMsg = convertEntityEnter(kbMsg);
                break;

            case KBEngineMsgId::OnEntityLeave:
                wsMsg = convertEntityLeave(kbMsg);
                break;

            default:
                break;
        }

        // 发送到 WebSocket 客户端
        sendToWebSocketClient(wsMsg);
    }

private:
    KBEngineMessage convertChatMessage(const WebSocketMessage& wsMsg) {
        KBEngineMessage kbMsg;
        kbMsg.msgId = KBEngineMsgId::Chat;
        kbMsg.entityId = wsMsg.playerId;

        // 将 JSON 数据转换为 KBEngine Binary
        Bundle* bundle = Bundle::create();
        (*bundle) << wsMsg.data["channel"].get<std::string>();
        (*bundle) << wsMsg.data["message"].get<std::string>();

        kbMsg.bundle = bundle;
        return kbMsg;
    }

    WebSocketMessage convertEntityEnter(const KBEngineMessage& kbMsg) {
        WebSocketMessage wsMsg;
        wsMsg.type = MessageType::EntityEnter;

        // 将 KBEngine Binary 转换为 JSON
        json data;
        data["entityId"] = kbMsg.entityId;
        data["entityType"] = kbMsg.params["entityType"];
        data["position"] = {
            kbMsg.params["x"],
            kbMsg.params["y"],
            kbMsg.params["z"]
        };

        wsMsg.data = data;
        return wsMsg;
    }

    void routeToKBEngine(const KBEngineMessage& kbMsg) {
        // 根据消息类型路由到不同的 App
        switch (kbMsg.msgId) {
            case KBEngineMsgId::Chat:
                // 聊天消息路由到 BaseApp
                sendToBaseApp(kbMsg);
                break;

            case KBEngineMsgId::Move:
            case KBEngineMsgId::Action:
                // 游戏操作路由到 CellApp
                sendToCellApp(kbMsg);
                break;

            default:
                break;
        }
    }

    void sendToBaseApp(const KBEngineMessage& kbMsg) {
        // 通过内部 TCP 连接发送到 BaseApp
        baseAppConnection_->send(kbMsg.bundle);
    }

    void sendToCellApp(const KBEngineMessage& kbMsg) {
        // 通过内部 TCP 连接发送到 CellApp
        cellAppConnection_->send(kbMsg.bundle);
    }

    void sendToWebSocketClient(const WebSocketMessage& wsMsg) {
        // 根据玩家 ID 找到对应的 WebSocket 连接
        auto it = clientConnections_.find(wsMsg.playerId);
        if (it != clientConnections_.end()) {
            it->second->send(wsMsg.toJson());
        }
    }

    std::unordered_map<uint32_t, WebSocketConnection*> clientConnections_;
    TCPConnection* baseAppConnection_;
    TCPConnection* cellAppConnection_;
};
```

### 5.2 与 KBEngine 集成

```
┌─────────────────────────────────────────────────────────────┐
│          KBEngine + WebSocket 集成架构                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  客户端层:                                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │  原生客户端   │  │  Web 客户端  │  │  移动 H5    │         │
│  │  (TCP)      │  │ (WebSocket) │  │ (WebSocket) │         │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │
│         │                │                │                 │
│         ▼                ▼                ▼                 │
│  ┌──────────────────────────────────────────────────┐      │
│  │              WebSocket 网关                       │      │
│  │  ┌─────────────────────────────────────────┐     │      │
│  │  │  协议转换层                                  │     │      │
│  │  │  - WebSocket ←→ KBEngine Bundle           │     │      │
│  │  │  - JSON ←→ Binary                         │     │      │
│  │  │  - 客户端会话管理                            │     │      │
│  │  └─────────────────────────────────────────┘     │      │
│  └─────────────────────────┬────────────────────────┘      │
│                            │                                │
│                            ▼                                │
│  ┌──────────────────────────────────────────────────┐      │
│  │              KBEngine 服务器                      │      │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐           │      │
│  │  │LoginApp │  │BaseApp  │  │CellApp  │           │      │
│  │  └─────────┘  └─────────┘  └─────────┘           │      │
│  └──────────────────────────────────────────────────┘      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 六、最佳实践

### 6.1 WebSocket 性能优化

```
┌─────────────────────────────────────────────────────────────┐
│              WebSocket 性能优化技巧                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 消息压缩                                                │
│     ├── 文本消息使用 gzip 压缩                               │
│     ├── 二进制消息使用自定义压缩                              │
│     └── 小于 100 字节的消息不压缩                            │
│                                                             │
│  2. 批量发送                                                │
│     ├── 合并多个小消息                                       │
│     ├── 减少系统调用次数                                     │
│     └── 设置 flush 阈值                                      │
│                                                             │
│  3. 连接复用                                                │
│     ├── 同一页面只使用一个连接                                │
│     ├── 使用频道/订阅模式                                    │
│     └── 减少连接数                                           │
│                                                             │
│  4. 心跳优化                                                │
│     ├── 使用 WebSocket Ping/Pong                            │
│     ├── 间隔 30-60 秒                                        │
│     └── 避免频繁心跳                                         │
│                                                             │
│  5. 二进制协议                                              │
│     ├── 使用二进制帧而非文本帧                                │
│     ├── 自定义序列化格式                                      │
│     └── 减少数据传输量                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 安全考虑

```
┌─────────────────────────────────────────────────────────────┐
│              WebSocket 安全措施                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 握手验证                                                │
│     ├── 验证 Origin 头                                       │
│     ├── 检查 Referer                                         │
│     └── 使用 Token 验证                                      │
│                                                             │
│  2. 数据加密                                                │
│     ├── 使用 TLS/WSS                                         │
│     ├── 应用层加密                                           │
│     └── 消息签名                                            │
│                                                             │
│  3. 速率限制                                                │
│     ├── 限制消息频率                                         │
│     ├── 限制消息大小                                         │
│     └── 检测异常行为                                         │
│                                                             │
│  4. 认证授权                                                │
│     ├── 握手后立即验证 Token                                  │
│     ├── 定期刷新 Token                                       │
│     └── 按频道/操作授权                                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 七、总结

### WebSocket 应用场景总结

| 场景 | 优先级 | 复杂度 | 效果 |
|------|--------|--------|------|
| **Web 聊天** | 高 | 低 | 优秀 |
| **实时通知** | 高 | 中 | 良好 |
| **管理后台** | 中 | 中 | 良好 |
| **社交功能** | 中 | 低 | 良好 |
| **H5 游戏** | 低 | 高 | 一般 |

### 架构选择建议

```
纯 WebSocket:
- 适合轻量级 Web 游戏
- 不适合复杂战斗

TCP + WebSocket:
- 原生客户端用 TCP
- Web/H5 用 WebSocket
- 通过网关协议转换

UDP + WebSocket:
- 战斗用 UDP (原生客户端)
- 聊天用 WebSocket
- 最佳用户体验组合
```

---

## 参考资料

- [RFC 6455 - WebSocket Protocol](https://tools.ietf.org/html/rfc6455)
- [WebSocket API - MDN](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)
- [KBEngine GitHub - 网络层实现](https://github.com/kbengine/kbengine/tree/master/kbe/src/lib/network)
