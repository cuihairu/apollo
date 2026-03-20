import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q21-websocket-mmo.html","title":"Q21: WebSocket 在 MMO 中有什么应用场景？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q21-websocket-mmo.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q21-websocket-mmo.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q21-websocket-在-mmo-中有什么应用场景" tabindex="-1"><a class="header-anchor" href="#q21-websocket-在-mmo-中有什么应用场景"><span>Q21: WebSocket 在 MMO 中有什么应用场景？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对 WebSocket 协议及其在游戏应用中的理解：</p><ul><li>WebSocket 协议特点</li><li>与 TCP/UDP 的对比</li><li>在 MMO 中的应用场景</li><li>KBEngine 对 WebSocket 的支持</li></ul><hr><h2 id="一、websocket-协议基础" tabindex="-1"><a class="header-anchor" href="#一、websocket-协议基础"><span>一、WebSocket 协议基础</span></a></h2><h3 id="_1-1-websocket-简介" tabindex="-1"><a class="header-anchor" href="#_1-1-websocket-简介"><span>1.1 WebSocket 简介</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  WebSocket 协议概述                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  WebSocket 是什么？                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  一种在单个 TCP 连接上进行全双工通信的协议          │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  RFC 6455 标准                                    │       │</span>
<span class="line">│  │  2011 年发布                                      │       │</span>
<span class="line">│  │  设计用于 Web 浏览器和服务器通信                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  握手流程:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 客户端发送 HTTP Upgrade 请求                  │       │</span>
<span class="line">│  │     GET /chat HTTP/1.1                            │       │</span>
<span class="line">│  │     Host: server.example.com                     │       │</span>
<span class="line">│  │     Upgrade: websocket                           │       │</span>
<span class="line">│  │     Connection: Upgrade                           │       │</span>
<span class="line">│  │     Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==  │       │</span>
<span class="line">│  │     Sec-WebSocket-Version: 13                    │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  2. 服务器返回 101 Switching Protocols            │       │</span>
<span class="line">│  │     HTTP/1.1 101 Switching Protocols             │       │</span>
<span class="line">│  │     Upgrade: websocket                           │       │</span>
<span class="line">│  │     Connection: Upgrade                           │       │</span>
<span class="line">│  │     Sec-WebSocket-Accept: HSmrc0sMlYUkAGmm5OPpG...│       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  3. 连接升级为 WebSocket                         │       │</span>
<span class="line">│  │  4. 开始双向通信                                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-websocket-帧格式" tabindex="-1"><a class="header-anchor" href="#_1-2-websocket-帧格式"><span>1.2 WebSocket 帧格式</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                 WebSocket 数据帧格式                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  0                   1                   2                   │</span>
<span class="line">│  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1│</span>
<span class="line">│  +-+-+-+-+-------+-+-------------+-------------------------------+│</span>
<span class="line">│  |F|R|R|R| opcode|M| Payload len |    Extended payload length    ││</span>
<span class="line">│  |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           ││</span>
<span class="line">│  |N|V|V|V|       |S|             |   (if payload len==126/127)   ││</span>
<span class="line">│  | |1|2|3|       |K|             |                               ││</span>
<span class="line">│  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +│</span>
<span class="line">│  |     Extended payload length continued, if payload len == 127  ││</span>
<span class="line">│  + - - - - - - - - - - - - - - - +-------------------------------+│</span>
<span class="line">│  |                               |Masking-key, if MASK set to 1   ││</span>
<span class="line">│  +-------------------------------+-------------------------------+│</span>
<span class="line">│  | Masking-key (continued)       |          Payload Data         ││</span>
<span class="line">│  +-------------------------------- - - - - - - - - - - - - - - - +│</span>
<span class="line">│  |                     Payload Data continued ...                ││</span>
<span class="line">│  +---------------------------------------------------------------+│</span>
<span class="line">│                                                             │</span>
<span class="line">│  字段说明:                                                  │</span>
<span class="line">│  ├── FIN (1 bit): 最后一帧                                    │</span>
<span class="line">│  ├── RSV1-3 (3 bits): 保留                                    │</span>
<span class="line">│  ├── Opcode (4 bits): 帧类型                                   │</span>
<span class="line">│  │   ├── 0x0: 连续帧                                          │</span>
<span class="line">│  │   ├── 0x1: 文本帧                                          │</span>
<span class="line">│  │   ├── 0x2: 二进制帧                                        │</span>
<span class="line">│  │   ├── 0x8: 关闭连接                                        │</span>
<span class="line">│  │   ├── 0x9: Ping                                            │</span>
<span class="line">│  │   └── 0xA: Pong                                            │</span>
<span class="line">│  ├── MASK (1 bit): 是否掩码（客户端必须为1）                   │</span>
<span class="line">│  ├── Payload len (7 bits): 负载长度                            │</span>
<span class="line">│  ├── Masking key (32 bits): 掩码密钥                           │</span>
<span class="line">│  └── Payload data: 实际数据                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、websocket-vs-其他协议" tabindex="-1"><a class="header-anchor" href="#二、websocket-vs-其他协议"><span>二、WebSocket vs 其他协议</span></a></h2><h3 id="_2-1-协议对比" tabindex="-1"><a class="header-anchor" href="#_2-1-协议对比"><span>2.1 协议对比</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              WebSocket vs TCP vs UDP vs HTTP                 │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  HTTP:                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  优点:                                            │       │</span>
<span class="line">│  │  ├── 通用、标准化                                   │       │</span>
<span class="line">│  │  ├── 防火墙友好                                     │       │</span>
<span class="line">│  │  ├── 跨域支持 (CORS)                               │       │</span>
<span class="line">│  │   │                                               │       │</span>
<span class="line">│  │  缺点:                                            │       │</span>
<span class="line">│  │  ├── 半双工（请求-响应模式）                         │       │</span>
<span class="line">│  │  ├── 头部开销大                                     │       │</span>
<span class="line">│  │  ├── 无连接状态                                     │       │</span>
<span class="line">│  │  └── 无法服务器主动推送                              │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用: API 调用、资源加载                            │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  WebSocket:                                                │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  优点:                                            │       │</span>
<span class="line">│  │  ├── 全双工通信                                    │       │</span>
<span class="line">│  │  ├── 低延迟                                        │       │</span>
<span class="line">│  │  ├── 持久连接                                      │       │</span>
<span class="line">│  │  ├── 服务器可主动推送                                │       │</span>
<span class="line">│  │  ├── 浏览器原生支持                                 │       │</span>
<span class="line">│  │  └── 跨域支持                                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  缺点:                                            │       │</span>
<span class="line">│  │  ├── 基于 TCP（受 TCP 缺陷影响）                   │       │</span>
<span class="line">│  │  ├── 帧开销                                        │       │</span>
<span class="line">│  │  ├── 连接状态管理复杂                               │       │</span>
<span class="line">│  │  └── 二进制支持有限                                 │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用: 聊天、实时通知、Web 游戏通信                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  原生 TCP:                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  优点:                                            │       │</span>
<span class="line">│  │  ├── 高效、低开销                                   │       │</span>
<span class="line">│  │  ├── 完全可控                                       │       │</span>
<span class="line">│  │  ├── 支持二进制                                     │       │</span>
<span class="line">│  │  └── 可自定义协议                                    │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  缺点:                                            │       │</span>
<span class="line">│  │  ├── 浏览器不支持                                    │       │</span>
<span class="line">│  │  ├── 需要处理粘包/拆包                                │       │</span>
<span class="line">│  │  ├── 防火墙可能拦截                                   │       │</span>
<span class="line">│  │  └── 开发复杂度高                                    │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用: 原生客户端游戏                                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  UDP:                                                      │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  优点:                                            │       │</span>
<span class="line">│  │  ├── 低延迟                                        │       │</span>
<span class="line">│  │  ├── 无连接开销                                    │       │</span>
<span class="line">│  │  ├── 支持广播/组播                                  │       │</span>
<span class="line">│  │  └── 适合实时数据                                   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  缺点:                                            │       │</span>
<span class="line">│  │  ├── 无可靠保证                                     │       │</span>
<span class="line">│  │  ├── 丢包问题                                       │       │</span>
<span class="line">│  │  ├── 浏览器支持有限（WebRTC/UDP）                   │       │</span>
<span class="line">│  │  └── 需要实现可靠层                                  │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用: FPS、实时战斗                                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-协议对比表" tabindex="-1"><a class="header-anchor" href="#_2-2-协议对比表"><span>2.2 协议对比表</span></a></h3><table><thead><tr><th>特性</th><th>HTTP</th><th>WebSocket</th><th>原生 TCP</th><th>UDP</th></tr></thead><tbody><tr><td><strong>传输方式</strong></td><td>半双工</td><td>全双工</td><td>全双工</td><td>无连接</td></tr><tr><td><strong>连接方式</strong></td><td>短连接</td><td>长连接</td><td>长连接</td><td>无连接</td></tr><tr><td><strong>服务器推送</strong></td><td>❌</td><td>✅</td><td>✅</td><td>✅</td></tr><tr><td><strong>浏览器支持</strong></td><td>✅</td><td>✅</td><td>❌</td><td>❌*</td></tr><tr><td><strong>延迟</strong></td><td>高</td><td>中</td><td>低</td><td>最低</td></tr><tr><td><strong>开销</strong></td><td>高</td><td>中</td><td>低</td><td>低</td></tr><tr><td><strong>可靠性</strong></td><td>✅</td><td>✅</td><td>✅</td><td>❌</td></tr><tr><td><strong>二进制</strong></td><td>❌</td><td>✅</td><td>✅</td><td>✅</td></tr><tr><td>*UDP 需通过 WebRTC</td><td></td><td></td><td></td><td></td></tr></tbody></table><hr><h2 id="三、websocket-在-mmo-中的应用" tabindex="-1"><a class="header-anchor" href="#三、websocket-在-mmo-中的应用"><span>三、WebSocket 在 MMO 中的应用</span></a></h2><h3 id="_3-1-应用场景" tabindex="-1"><a class="header-anchor" href="#_3-1-应用场景"><span>3.1 应用场景</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│            WebSocket 在 MMO 中的应用场景                       │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 1: Web 端聊天系统                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  优势:                                            │       │</span>
<span class="line">│  │  ├── 浏览器原生支持，无需插件                        │       │</span>
<span class="line">│  │  ├── 全双工，实时性好                               │       │</span>
<span class="line">│  │  ├── 支持多频道（世界、公会、私聊）                  │       │</span>
<span class="line">│  │  └── 低流量开销                                     │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  实现:                                            │       │</span>
<span class="line">│  │  客户端 ←WebSocket→ 聊天服务器 ←TCP/KCP→ 游戏服务器  │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  功能:                                            │       │</span>
<span class="line">│  │  ├── 实时聊天消息推送                                │       │</span>
<span class="line">│  │  ├── 在线人数显示                                   │       │</span>
<span class="line">│  │  ├── 表情/图片发送                                   │       │</span>
<span class="line">│  │  └── 聊天历史记录                                    │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 2: 实时通知系统                                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  服务器主动推送通知给玩家                            │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  通知类型:                                         │       │</span>
<span class="line">│  │  ├── 活动开始提醒                                   │       │</span>
<span class="line">│  │  ├── 系统公告                                       │       │</span>
<span class="line">│  │  ├── 邮件到达提醒                                   │       │</span>
<span class="line">│  │  ├── 好友上线提醒                                   │       │</span>
<span class="line">│  │  ├── 战斗匹配成功                                   │       │</span>
<span class="line">│  │  └── 组队邀请                                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  架构:                                            │       │</span>
<span class="line">│  │  游戏服务器 → 通知服务 → WebSocket → Web/移动客户端 │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 3: Web 管理后台                                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  GM/运维人员通过 Web 管理游戏                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  功能:                                            │       │</span>
<span class="line">│  │  ├── 实时监控服务器状态                             │       │</span>
<span class="line">│  │  ├── 在线玩家列表                                   │       │</span>
<span class="line">│  │  ├── 实时日志查看                                   │       │</span>
<span class="line">│  │  ├── 封禁/解封玩家                                  │       │</span>
<span class="line">│  │  ├── 发送系统公告                                   │       │</span>
<span class="line">│  │  └── 执行 GM 命令                                   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  架构:                                            │       │</span>
<span class="line">│  │  Web 后台 ←WebSocket→ 代理服务器 ←TCP→ 游戏服务器  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 4: 社交功能                                           │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  移动端 Web App 社交功能                           │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  功能:                                            │       │</span>
<span class="line">│  │  ├── 查看好友列表                                   │       │</span>
<span class="line">│  │  ├── 查看公会成员                                   │       │</span>
<span class="line">│  │  ├── 好友在线状态                                   │       │</span>
<span class="line">│  │  ├── 发送私聊                                       │       │</span>
<span class="line">│  │  └── 查看排行榜                                     │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优势: 随时随地访问，无需下载客户端                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 5: 轻量级 H5 游戏                                      │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  简单的网页版游戏客户端                             │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  功能:                                            │       │</span>
<span class="line">│  │  ├── 简单的战斗模拟                                 │       │</span>
<span class="line">│  │  ├── 查看角色信息                                   │       │</span>
<span class="line">│  │  ├── 背包管理                                       │       │</span>
<span class="line">│  │  ├── 拍卖行浏览                                     │       │</span>
<span class="line">│  │  └── 公会聊天                                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  限制: 不适合复杂战斗操作                           │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-混合架构设计" tabindex="-1"><a class="header-anchor" href="#_3-2-混合架构设计"><span>3.2 混合架构设计</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              MMO 混合网络架构设计                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│                    ┌──────────────┐                        │</span>
<span class="line">│                    │   原生客户端   │                        │</span>
<span class="line">│                    │   (TCP/UDP)   │                        │</span>
<span class="line">│                    └──────┬───────┘                        │</span>
<span class="line">│                           │                                 │</span>
<span class="line">│                           ▼                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │              游戏服务器                          │       │</span>
<span class="line">│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐         │       │</span>
<span class="line">│  │  │ LoginApp│  │ BaseApp │  │ CellApp │         │       │</span>
<span class="line">│  │  └────┬────┘  └────┬────┘  └────┬────┘         │       │</span>
<span class="line">│  │       │            │            │                │       │</span>
<span class="line">│  │       └────────────┴────────────┘                │       │</span>
<span class="line">│  │                   │                              │       │</span>
<span class="line">│  │                   ▼                              │       │</span>
<span class="line">│  │         ┌──────────────────┐                    │       │</span>
<span class="line">│  │         │  WebSocket 网关   │                    │       │</span>
<span class="line">│  │         │  (协议转换层)      │                    │       │</span>
<span class="line">│  │         └────────┬─────────┘                    │       │</span>
<span class="line">│  └──────────────────┼──────────────────────────────┘       │</span>
<span class="line">│                     │                                       │</span>
<span class="line">│         ┌───────────┼───────────┐                           │</span>
<span class="line">│         │           │           │                           │</span>
<span class="line">│         ▼           ▼           ▼                           │</span>
<span class="line">│  ┌──────────┐ ┌──────────┐ ┌──────────┐                    │</span>
<span class="line">│  │Web 聊天  │ │Web 管理  │ │移动端    │                    │</span>
<span class="line">│  │  客户端   │ │  后台    │ │  App     │                    │</span>
<span class="line">│  │(WebSocket)│ │(WebSocket)│ │(WebSocket)│                   │</span>
<span class="line">│  └──────────┘ └──────────┘ └──────────┘                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、websocket-服务器实现" tabindex="-1"><a class="header-anchor" href="#四、websocket-服务器实现"><span>四、WebSocket 服务器实现</span></a></h2><h3 id="_4-1-基础-websocket-服务器" tabindex="-1"><a class="header-anchor" href="#_4-1-基础-websocket-服务器"><span>4.1 基础 WebSocket 服务器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// WebSocket 服务器实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">WebSocketServer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">WebSocketServer</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> port<span class="token punctuation">)</span> <span class="token operator">:</span> <span class="token function">port_</span><span class="token punctuation">(</span>port<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 初始化 WebSocket 库</span></span>
<span class="line">        <span class="token function">initWebSocketLibrary</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 启动服务器</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">start</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 创建监听 socket</span></span>
<span class="line">        listenSocket_ <span class="token operator">=</span> <span class="token function">socket</span><span class="token punctuation">(</span>AF_INET<span class="token punctuation">,</span> SOCK_STREAM<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">struct</span> <span class="token class-name">sockaddr_in</span> addr<span class="token punctuation">;</span></span>
<span class="line">        addr<span class="token punctuation">.</span>sin_family <span class="token operator">=</span> AF_INET<span class="token punctuation">;</span></span>
<span class="line">        addr<span class="token punctuation">.</span>sin_addr<span class="token punctuation">.</span>s_addr <span class="token operator">=</span> INADDR_ANY<span class="token punctuation">;</span></span>
<span class="line">        addr<span class="token punctuation">.</span>sin_port <span class="token operator">=</span> <span class="token function">htons</span><span class="token punctuation">(</span>port_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">bind</span><span class="token punctuation">(</span>listenSocket_<span class="token punctuation">,</span> <span class="token punctuation">(</span><span class="token keyword">struct</span> <span class="token class-name">sockaddr</span><span class="token operator">*</span><span class="token punctuation">)</span><span class="token operator">&amp;</span>addr<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>addr<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">listen</span><span class="token punctuation">(</span>listenSocket_<span class="token punctuation">,</span> <span class="token number">1024</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 设置非阻塞</span></span>
<span class="line">        <span class="token function">setNonBlocking</span><span class="token punctuation">(</span>listenSocket_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;WebSocket server started on port &quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>port_<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 事件循环</span></span>
<span class="line">        <span class="token function">eventLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">eventLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 使用 epoll/IOCP 等待事件</span></span>
<span class="line">            <span class="token keyword">struct</span> <span class="token class-name">epoll_event</span> events<span class="token punctuation">[</span>MAX_EVENTS<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">int</span> nfds <span class="token operator">=</span> <span class="token function">epoll_wait</span><span class="token punctuation">(</span>epollFd_<span class="token punctuation">,</span> events<span class="token punctuation">,</span> MAX_EVENTS<span class="token punctuation">,</span> <span class="token number">100</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> nfds<span class="token punctuation">;</span> i<span class="token operator">++</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>events<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>data<span class="token punctuation">.</span>fd <span class="token operator">==</span> listenSocket_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">acceptNewConnection</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                    WebSocketConnection<span class="token operator">*</span> conn <span class="token operator">=</span></span>
<span class="line">                        <span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span><span class="token punctuation">)</span>events<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>data<span class="token punctuation">.</span>ptr<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                    <span class="token keyword">if</span> <span class="token punctuation">(</span>events<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>events <span class="token operator">&amp;</span> EPOLLIN<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                        <span class="token function">handleRead</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token punctuation">}</span></span>
<span class="line">                    <span class="token keyword">if</span> <span class="token punctuation">(</span>events<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>events <span class="token operator">&amp;</span> EPOLLOUT<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                        <span class="token function">handleWrite</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token punctuation">}</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">acceptNewConnection</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">struct</span> <span class="token class-name">sockaddr_in</span> clientAddr<span class="token punctuation">;</span></span>
<span class="line">        socklen_t addrLen <span class="token operator">=</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>clientAddr<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">int</span> clientFd <span class="token operator">=</span> <span class="token function">accept</span><span class="token punctuation">(</span>listenSocket_<span class="token punctuation">,</span></span>
<span class="line">                             <span class="token punctuation">(</span><span class="token keyword">struct</span> <span class="token class-name">sockaddr</span><span class="token operator">*</span><span class="token punctuation">)</span><span class="token operator">&amp;</span>clientAddr<span class="token punctuation">,</span></span>
<span class="line">                             <span class="token operator">&amp;</span>addrLen<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>clientFd <span class="token operator">&lt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 设置非阻塞</span></span>
<span class="line">        <span class="token function">setNonBlocking</span><span class="token punctuation">(</span>clientFd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 创建连接</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> conn <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">WebSocketConnection</span><span class="token punctuation">(</span>clientFd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        connections_<span class="token punctuation">[</span>clientFd<span class="token punctuation">]</span> <span class="token operator">=</span> conn<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 添加到 epoll</span></span>
<span class="line">        <span class="token keyword">struct</span> <span class="token class-name">epoll_event</span> ev<span class="token punctuation">;</span></span>
<span class="line">        ev<span class="token punctuation">.</span>events <span class="token operator">=</span> EPOLLIN <span class="token operator">|</span> EPOLLET<span class="token punctuation">;</span></span>
<span class="line">        ev<span class="token punctuation">.</span>data<span class="token punctuation">.</span>ptr <span class="token operator">=</span> conn<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">epoll_ctl</span><span class="token punctuation">(</span>epollFd_<span class="token punctuation">,</span> EPOLL_CTL_ADD<span class="token punctuation">,</span> clientFd<span class="token punctuation">,</span> <span class="token operator">&amp;</span>ev<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;New WebSocket connection: &quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>clientFd<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleRead</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint8_t</span> buffer<span class="token punctuation">[</span><span class="token number">4096</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> len <span class="token operator">=</span> <span class="token function">recv</span><span class="token punctuation">(</span>conn<span class="token operator">-&gt;</span><span class="token function">fd</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> buffer<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>buffer<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>len <span class="token operator">&lt;=</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 连接关闭</span></span>
<span class="line">            <span class="token function">closeConnection</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>conn<span class="token operator">-&gt;</span><span class="token function">isHandshakeComplete</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 处理 WebSocket 握手</span></span>
<span class="line">            <span class="token function">handleHandshake</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> buffer<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 处理 WebSocket 帧</span></span>
<span class="line">            <span class="token function">handleWebSocketFrame</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> buffer<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleHandshake</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">,</span></span>
<span class="line">                        <span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string <span class="token function">request</span><span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token keyword">char</span><span class="token operator">*</span><span class="token punctuation">)</span>data<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否是 WebSocket 握手请求</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>request<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span><span class="token string">&quot;Upgrade: websocket&quot;</span><span class="token punctuation">)</span> <span class="token operator">==</span> std<span class="token double-colon punctuation">::</span>string<span class="token double-colon punctuation">::</span>npos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// HTTP 请求</span></span>
<span class="line">            <span class="token function">sendHTTPResponse</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> request<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 提取 Sec-WebSocket-Key</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string key <span class="token operator">=</span> <span class="token function">extractWebSocketKey</span><span class="token punctuation">(</span>request<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 计算 Sec-WebSocket-Accept</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string acceptKey <span class="token operator">=</span> <span class="token function">computeAcceptKey</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送握手响应</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string response <span class="token operator">=</span></span>
<span class="line">            <span class="token string">&quot;HTTP/1.1 101 Switching Protocols\\r\\n&quot;</span></span>
<span class="line">            <span class="token string">&quot;Upgrade: websocket\\r\\n&quot;</span></span>
<span class="line">            <span class="token string">&quot;Connection: Upgrade\\r\\n&quot;</span></span>
<span class="line">            <span class="token string">&quot;Sec-WebSocket-Accept: &quot;</span> <span class="token operator">+</span> acceptKey <span class="token operator">+</span> <span class="token string">&quot;\\r\\n&quot;</span></span>
<span class="line">            <span class="token string">&quot;\\r\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">send</span><span class="token punctuation">(</span>conn<span class="token operator">-&gt;</span><span class="token function">fd</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> response<span class="token punctuation">.</span><span class="token function">c_str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> response<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        conn<span class="token operator">-&gt;</span><span class="token function">setHandshakeComplete</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;WebSocket handshake complete&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleWebSocketFrame</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">,</span></span>
<span class="line">                             <span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t offset <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>offset <span class="token operator">&lt;</span> len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 解析帧头</span></span>
<span class="line">            <span class="token keyword">uint8_t</span> firstByte <span class="token operator">=</span> data<span class="token punctuation">[</span>offset<span class="token operator">++</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">uint8_t</span> secondByte <span class="token operator">=</span> data<span class="token punctuation">[</span>offset<span class="token operator">++</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">bool</span> fin <span class="token operator">=</span> <span class="token punctuation">(</span>firstByte <span class="token operator">&amp;</span> <span class="token number">0x80</span><span class="token punctuation">)</span> <span class="token operator">!=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">uint8_t</span> opcode <span class="token operator">=</span> firstByte <span class="token operator">&amp;</span> <span class="token number">0x0F</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">bool</span> masked <span class="token operator">=</span> <span class="token punctuation">(</span>secondByte <span class="token operator">&amp;</span> <span class="token number">0x80</span><span class="token punctuation">)</span> <span class="token operator">!=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">uint64_t</span> payloadLen <span class="token operator">=</span> secondByte <span class="token operator">&amp;</span> <span class="token number">0x7F</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 扩展长度</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>payloadLen <span class="token operator">==</span> <span class="token number">126</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                payloadLen <span class="token operator">=</span> <span class="token punctuation">(</span>data<span class="token punctuation">[</span>offset<span class="token punctuation">]</span> <span class="token operator">&lt;&lt;</span> <span class="token number">8</span><span class="token punctuation">)</span> <span class="token operator">|</span> data<span class="token punctuation">[</span>offset <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">                offset <span class="token operator">+=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>payloadLen <span class="token operator">==</span> <span class="token number">127</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                payloadLen <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> <span class="token number">8</span><span class="token punctuation">;</span> i<span class="token operator">++</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    payloadLen <span class="token operator">=</span> <span class="token punctuation">(</span>payloadLen <span class="token operator">&lt;&lt;</span> <span class="token number">8</span><span class="token punctuation">)</span> <span class="token operator">|</span> data<span class="token punctuation">[</span>offset<span class="token operator">++</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 掩码密钥</span></span>
<span class="line">            <span class="token keyword">uint8_t</span> maskKey<span class="token punctuation">[</span><span class="token number">4</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>masked<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">memcpy</span><span class="token punctuation">(</span>maskKey<span class="token punctuation">,</span> data <span class="token operator">+</span> offset<span class="token punctuation">,</span> <span class="token number">4</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                offset <span class="token operator">+=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 负载数据</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> <span class="token function">payload</span><span class="token punctuation">(</span>payloadLen<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">memcpy</span><span class="token punctuation">(</span>payload<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> data <span class="token operator">+</span> offset<span class="token punctuation">,</span> payloadLen<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            offset <span class="token operator">+=</span> payloadLen<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 去掩码</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>masked<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> payloadLen<span class="token punctuation">;</span> i<span class="token operator">++</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    payload<span class="token punctuation">[</span>i<span class="token punctuation">]</span> <span class="token operator">^=</span> maskKey<span class="token punctuation">[</span>i <span class="token operator">%</span> <span class="token number">4</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 处理 opcode</span></span>
<span class="line">            <span class="token keyword">switch</span> <span class="token punctuation">(</span>opcode<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">case</span> <span class="token number">0x1</span><span class="token operator">:</span> <span class="token comment">// 文本帧</span></span>
<span class="line">                    <span class="token function">handleTextMessage</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> payload<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">case</span> <span class="token number">0x2</span><span class="token operator">:</span> <span class="token comment">// 二进制帧</span></span>
<span class="line">                    <span class="token function">handleBinaryMessage</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> payload<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">case</span> <span class="token number">0x8</span><span class="token operator">:</span> <span class="token comment">// 关闭连接</span></span>
<span class="line">                    <span class="token function">handleClose</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">case</span> <span class="token number">0x9</span><span class="token operator">:</span> <span class="token comment">// Ping</span></span>
<span class="line">                    <span class="token function">sendPong</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> payload<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">case</span> <span class="token number">0xA</span><span class="token operator">:</span> <span class="token comment">// Pong</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleTextMessage</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">,</span></span>
<span class="line">                          <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> payload<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string <span class="token function">message</span><span class="token punctuation">(</span>payload<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> payload<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">LOG_DEBUG</span><span class="token punctuation">(</span><span class="token string">&quot;Received text message: &quot;</span> <span class="token operator">+</span> message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 解析 JSON 消息</span></span>
<span class="line">        <span class="token keyword">try</span> <span class="token punctuation">{</span></span>
<span class="line">            json msg <span class="token operator">=</span> json<span class="token double-colon punctuation">::</span><span class="token function">parse</span><span class="token punctuation">(</span>message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>string type <span class="token operator">=</span> msg<span class="token punctuation">[</span><span class="token string">&quot;type&quot;</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            json data <span class="token operator">=</span> msg<span class="token punctuation">[</span><span class="token string">&quot;data&quot;</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>type <span class="token operator">==</span> <span class="token string">&quot;chat&quot;</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">handleChatMessage</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>type <span class="token operator">==</span> <span class="token string">&quot;login&quot;</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">handleLoginMessage</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>type <span class="token operator">==</span> <span class="token string">&quot;heartbeat&quot;</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">handleHeartbeat</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">catch</span> <span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>exception<span class="token operator">&amp;</span> e<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Failed to parse message: &quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">string</span><span class="token punctuation">(</span>e<span class="token punctuation">.</span><span class="token function">what</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendFrame</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">,</span></span>
<span class="line">                  <span class="token keyword">uint8_t</span> opcode<span class="token punctuation">,</span></span>
<span class="line">                  <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> payload<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> frame<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 帧头</span></span>
<span class="line">        <span class="token keyword">uint8_t</span> firstByte <span class="token operator">=</span> <span class="token number">0x80</span> <span class="token operator">|</span> opcode<span class="token punctuation">;</span> <span class="token comment">// FIN + opcode</span></span>
<span class="line">        frame<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>firstByte<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 长度</span></span>
<span class="line">        size_t payloadLen <span class="token operator">=</span> payload<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>payloadLen <span class="token operator">&lt;</span> <span class="token number">126</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            frame<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>payloadLen<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>payloadLen <span class="token operator">&lt;</span> <span class="token number">65536</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            frame<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token number">126</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            frame<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">(</span>payloadLen <span class="token operator">&gt;&gt;</span> <span class="token number">8</span><span class="token punctuation">)</span> <span class="token operator">&amp;</span> <span class="token number">0xFF</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            frame<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>payloadLen <span class="token operator">&amp;</span> <span class="token number">0xFF</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            frame<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token number">127</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span> i <span class="token operator">&gt;=</span> <span class="token number">0</span><span class="token punctuation">;</span> i<span class="token operator">--</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                frame<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">(</span>payloadLen <span class="token operator">&gt;&gt;</span> <span class="token punctuation">(</span>i <span class="token operator">*</span> <span class="token number">8</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token operator">&amp;</span> <span class="token number">0xFF</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 负载</span></span>
<span class="line">        frame<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>frame<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> payload<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> payload<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送</span></span>
<span class="line">        <span class="token function">send</span><span class="token punctuation">(</span>conn<span class="token operator">-&gt;</span><span class="token function">fd</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> frame<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> frame<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendTextMessage</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">,</span></span>
<span class="line">                        <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> message<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> <span class="token function">payload</span><span class="token punctuation">(</span>message<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> message<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">sendFrame</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> <span class="token number">0x1</span><span class="token punctuation">,</span> payload<span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token comment">// 文本帧</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">extractWebSocketKey</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> request<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t keyPos <span class="token operator">=</span> request<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span><span class="token string">&quot;Sec-WebSocket-Key:&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>keyPos <span class="token operator">==</span> std<span class="token double-colon punctuation">::</span>string<span class="token double-colon punctuation">::</span>npos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token string">&quot;&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        keyPos <span class="token operator">+=</span> <span class="token number">19</span><span class="token punctuation">;</span> <span class="token comment">// &quot;Sec-WebSocket-Key:&quot; length</span></span>
<span class="line">        size_t keyEnd <span class="token operator">=</span> request<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span><span class="token string">&quot;\\r\\n&quot;</span><span class="token punctuation">,</span> keyPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string key <span class="token operator">=</span> request<span class="token punctuation">.</span><span class="token function">substr</span><span class="token punctuation">(</span>keyPos<span class="token punctuation">,</span> keyEnd <span class="token operator">-</span> keyPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 去除空格</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>key<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> key<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span> <span class="token operator">==</span> <span class="token char">&#39; &#39;</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            key<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> key<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">computeAcceptKey</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// WebSocket GUID</span></span>
<span class="line">        <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string guid <span class="token operator">=</span> <span class="token string">&quot;258EAFA5-E914-47DA-95CA-C5AB0DC85B11&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string combined <span class="token operator">=</span> key <span class="token operator">+</span> guid<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// SHA-1 哈希</span></span>
<span class="line">        <span class="token keyword">unsigned</span> <span class="token keyword">char</span> hash<span class="token punctuation">[</span><span class="token number">20</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">SHA1</span><span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token keyword">unsigned</span> <span class="token keyword">char</span><span class="token operator">*</span><span class="token punctuation">)</span>combined<span class="token punctuation">.</span><span class="token function">c_str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> combined<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> hash<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// Base64 编码</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">base64Encode</span><span class="token punctuation">(</span>hash<span class="token punctuation">,</span> <span class="token number">20</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">int</span> listenSocket_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> port_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> epollFd_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">int</span><span class="token punctuation">,</span> WebSocketConnection<span class="token operator">*</span><span class="token operator">&gt;</span> connections_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span><span class="token keyword">bool</span><span class="token operator">&gt;</span> running_<span class="token punctuation">{</span><span class="token boolean">true</span><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_EVENTS <span class="token operator">=</span> <span class="token number">1024</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// WebSocket 连接类</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">WebSocketConnection</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">WebSocketConnection</span><span class="token punctuation">(</span><span class="token keyword">int</span> fd<span class="token punctuation">)</span> <span class="token operator">:</span> <span class="token function">fd_</span><span class="token punctuation">(</span>fd<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">handshakeComplete_</span><span class="token punctuation">(</span><span class="token boolean">false</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">fd</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> fd_<span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isHandshakeComplete</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> handshakeComplete_<span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setHandshakeComplete</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span> handshakeComplete_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">int</span> fd_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> handshakeComplete_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-聊天服务器实现" tabindex="-1"><a class="header-anchor" href="#_4-2-聊天服务器实现"><span>4.2 聊天服务器实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// WebSocket 聊天服务器</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ChatWebSocketServer</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">WebSocketServer</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">ChatWebSocketServer</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> port<span class="token punctuation">)</span> <span class="token operator">:</span> <span class="token function">WebSocketServer</span><span class="token punctuation">(</span>port<span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleChatMessage</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">,</span> <span class="token keyword">const</span> json<span class="token operator">&amp;</span> data<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string channel <span class="token operator">=</span> data<span class="token punctuation">[</span><span class="token string">&quot;channel&quot;</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string sender <span class="token operator">=</span> data<span class="token punctuation">[</span><span class="token string">&quot;sender&quot;</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string message <span class="token operator">=</span> data<span class="token punctuation">[</span><span class="token string">&quot;message&quot;</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 创建聊天消息</span></span>
<span class="line">        json chatMsg<span class="token punctuation">;</span></span>
<span class="line">        chatMsg<span class="token punctuation">[</span><span class="token string">&quot;type&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token string">&quot;chat&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        chatMsg<span class="token punctuation">[</span><span class="token string">&quot;channel&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> channel<span class="token punctuation">;</span></span>
<span class="line">        chatMsg<span class="token punctuation">[</span><span class="token string">&quot;sender&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> sender<span class="token punctuation">;</span></span>
<span class="line">        chatMsg<span class="token punctuation">[</span><span class="token string">&quot;message&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> message<span class="token punctuation">;</span></span>
<span class="line">        chatMsg<span class="token punctuation">[</span><span class="token string">&quot;timestamp&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string msgStr <span class="token operator">=</span> chatMsg<span class="token punctuation">.</span><span class="token function">dump</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 广播给频道内的所有连接</span></span>
<span class="line">        <span class="token function">broadcastToChannel</span><span class="token punctuation">(</span>channel<span class="token punctuation">,</span> msgStr<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleLoginMessage</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">,</span> <span class="token keyword">const</span> json<span class="token operator">&amp;</span> data<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string token <span class="token operator">=</span> data<span class="token punctuation">[</span><span class="token string">&quot;token&quot;</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 验证 token</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">validateToken</span><span class="token punctuation">(</span>token<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            conn<span class="token operator">-&gt;</span><span class="token function">setAuthenticated</span><span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            conn<span class="token operator">-&gt;</span><span class="token function">setUsername</span><span class="token punctuation">(</span><span class="token function">getUsernameFromToken</span><span class="token punctuation">(</span>token<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 加入默认频道</span></span>
<span class="line">            <span class="token function">joinChannel</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> <span class="token string">&quot;world&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 发送登录成功</span></span>
<span class="line">            json response<span class="token punctuation">;</span></span>
<span class="line">            response<span class="token punctuation">[</span><span class="token string">&quot;type&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token string">&quot;login&quot;</span><span class="token punctuation">;</span></span>
<span class="line">            response<span class="token punctuation">[</span><span class="token string">&quot;success&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            response<span class="token punctuation">[</span><span class="token string">&quot;channels&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token function">getJoinedChannels</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token function">sendTextMessage</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> response<span class="token punctuation">.</span><span class="token function">dump</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            json response<span class="token punctuation">;</span></span>
<span class="line">            response<span class="token punctuation">[</span><span class="token string">&quot;type&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token string">&quot;login&quot;</span><span class="token punctuation">;</span></span>
<span class="line">            response<span class="token punctuation">[</span><span class="token string">&quot;success&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            response<span class="token punctuation">[</span><span class="token string">&quot;error&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token string">&quot;Invalid token&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token function">sendTextMessage</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> response<span class="token punctuation">.</span><span class="token function">dump</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleHeartbeat</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        conn<span class="token operator">-&gt;</span><span class="token function">updateLastActivity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        json pong<span class="token punctuation">;</span></span>
<span class="line">        pong<span class="token punctuation">[</span><span class="token string">&quot;type&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token string">&quot;pong&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        pong<span class="token punctuation">[</span><span class="token string">&quot;timestamp&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">sendTextMessage</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> pong<span class="token punctuation">.</span><span class="token function">dump</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">joinChannel</span><span class="token punctuation">(</span>WebSocketConnection<span class="token operator">*</span> conn<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> channel<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        channels_<span class="token punctuation">[</span>channel<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        conn<span class="token operator">-&gt;</span><span class="token function">addChannel</span><span class="token punctuation">(</span>channel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知频道内的其他用户</span></span>
<span class="line">        json notify<span class="token punctuation">;</span></span>
<span class="line">        notify<span class="token punctuation">[</span><span class="token string">&quot;type&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token string">&quot;user_joined&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        notify<span class="token punctuation">[</span><span class="token string">&quot;channel&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> channel<span class="token punctuation">;</span></span>
<span class="line">        notify<span class="token punctuation">[</span><span class="token string">&quot;user&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> conn<span class="token operator">-&gt;</span><span class="token function">getUsername</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">broadcastToChannel</span><span class="token punctuation">(</span>channel<span class="token punctuation">,</span> notify<span class="token punctuation">.</span><span class="token function">dump</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcastToChannel</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> channel<span class="token punctuation">,</span></span>
<span class="line">                           <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> message<span class="token punctuation">,</span></span>
<span class="line">                           WebSocketConnection<span class="token operator">*</span> exclude <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> channels_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>channel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> channels_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> conn <span class="token operator">:</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>conn <span class="token operator">!=</span> exclude <span class="token operator">&amp;&amp;</span> conn<span class="token operator">-&gt;</span><span class="token function">isAuthenticated</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">sendTextMessage</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>unordered_set<span class="token operator">&lt;</span>WebSocketConnection<span class="token operator">*</span><span class="token operator">&gt;&gt;</span> channels_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、kbengine-websocket-集成" tabindex="-1"><a class="header-anchor" href="#五、kbengine-websocket-集成"><span>五、KBEngine WebSocket 集成</span></a></h2><h3 id="_5-1-websocket-网关设计" tabindex="-1"><a class="header-anchor" href="#_5-1-websocket-网关设计"><span>5.1 WebSocket 网关设计</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine WebSocket 网关</span></span>
<span class="line"><span class="token comment">// 将 WebSocket 消息转换为 KBEngine 内部消息</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">KBEngineWebSocketGateway</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 处理来自 WebSocket 的消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onWebSocketMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> WebSocketMessage<span class="token operator">&amp;</span> wsMsg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 转换为 KBEngine 消息格式</span></span>
<span class="line">        KBEngineMessage kbMsg<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>wsMsg<span class="token punctuation">.</span>type<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> MessageType<span class="token double-colon punctuation">::</span>Chat<span class="token operator">:</span></span>
<span class="line">                kbMsg <span class="token operator">=</span> <span class="token function">convertChatMessage</span><span class="token punctuation">(</span>wsMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> MessageType<span class="token double-colon punctuation">::</span>Move<span class="token operator">:</span></span>
<span class="line">                kbMsg <span class="token operator">=</span> <span class="token function">convertMoveMessage</span><span class="token punctuation">(</span>wsMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> MessageType<span class="token double-colon punctuation">::</span>Action<span class="token operator">:</span></span>
<span class="line">                kbMsg <span class="token operator">=</span> <span class="token function">convertActionMessage</span><span class="token punctuation">(</span>wsMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">default</span><span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 路由到对应的 KBEngine App</span></span>
<span class="line">        <span class="token function">routeToKBEngine</span><span class="token punctuation">(</span>kbMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理来自 KBEngine 的消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onKBEngineMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> KBEngineMessage<span class="token operator">&amp;</span> kbMsg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 转换为 WebSocket 消息格式</span></span>
<span class="line">        WebSocketMessage wsMsg<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>kbMsg<span class="token punctuation">.</span>msgId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> KBEngineMsgId<span class="token double-colon punctuation">::</span>OnRemoteCallMethod<span class="token operator">:</span></span>
<span class="line">                wsMsg <span class="token operator">=</span> <span class="token function">convertRemoteCall</span><span class="token punctuation">(</span>kbMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> KBEngineMsgId<span class="token double-colon punctuation">::</span>OnUpdateData<span class="token operator">:</span></span>
<span class="line">                wsMsg <span class="token operator">=</span> <span class="token function">convertUpdateData</span><span class="token punctuation">(</span>kbMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> KBEngineMsgId<span class="token double-colon punctuation">::</span>OnEntityEnter<span class="token operator">:</span></span>
<span class="line">                wsMsg <span class="token operator">=</span> <span class="token function">convertEntityEnter</span><span class="token punctuation">(</span>kbMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> KBEngineMsgId<span class="token double-colon punctuation">::</span>OnEntityLeave<span class="token operator">:</span></span>
<span class="line">                wsMsg <span class="token operator">=</span> <span class="token function">convertEntityLeave</span><span class="token punctuation">(</span>kbMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">default</span><span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送到 WebSocket 客户端</span></span>
<span class="line">        <span class="token function">sendToWebSocketClient</span><span class="token punctuation">(</span>wsMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    KBEngineMessage <span class="token function">convertChatMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> WebSocketMessage<span class="token operator">&amp;</span> wsMsg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        KBEngineMessage kbMsg<span class="token punctuation">;</span></span>
<span class="line">        kbMsg<span class="token punctuation">.</span>msgId <span class="token operator">=</span> KBEngineMsgId<span class="token double-colon punctuation">::</span>Chat<span class="token punctuation">;</span></span>
<span class="line">        kbMsg<span class="token punctuation">.</span>entityId <span class="token operator">=</span> wsMsg<span class="token punctuation">.</span>playerId<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 将 JSON 数据转换为 KBEngine Binary</span></span>
<span class="line">        Bundle<span class="token operator">*</span> bundle <span class="token operator">=</span> <span class="token class-name">Bundle</span><span class="token double-colon punctuation">::</span><span class="token function">create</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>bundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> wsMsg<span class="token punctuation">.</span>data<span class="token punctuation">[</span><span class="token string">&quot;channel&quot;</span><span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>bundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> wsMsg<span class="token punctuation">.</span>data<span class="token punctuation">[</span><span class="token string">&quot;message&quot;</span><span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        kbMsg<span class="token punctuation">.</span>bundle <span class="token operator">=</span> bundle<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> kbMsg<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    WebSocketMessage <span class="token function">convertEntityEnter</span><span class="token punctuation">(</span><span class="token keyword">const</span> KBEngineMessage<span class="token operator">&amp;</span> kbMsg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        WebSocketMessage wsMsg<span class="token punctuation">;</span></span>
<span class="line">        wsMsg<span class="token punctuation">.</span>type <span class="token operator">=</span> MessageType<span class="token double-colon punctuation">::</span>EntityEnter<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 将 KBEngine Binary 转换为 JSON</span></span>
<span class="line">        json data<span class="token punctuation">;</span></span>
<span class="line">        data<span class="token punctuation">[</span><span class="token string">&quot;entityId&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> kbMsg<span class="token punctuation">.</span>entityId<span class="token punctuation">;</span></span>
<span class="line">        data<span class="token punctuation">[</span><span class="token string">&quot;entityType&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> kbMsg<span class="token punctuation">.</span>params<span class="token punctuation">[</span><span class="token string">&quot;entityType&quot;</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        data<span class="token punctuation">[</span><span class="token string">&quot;position&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            kbMsg<span class="token punctuation">.</span>params<span class="token punctuation">[</span><span class="token string">&quot;x&quot;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            kbMsg<span class="token punctuation">.</span>params<span class="token punctuation">[</span><span class="token string">&quot;y&quot;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            kbMsg<span class="token punctuation">.</span>params<span class="token punctuation">[</span><span class="token string">&quot;z&quot;</span><span class="token punctuation">]</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        wsMsg<span class="token punctuation">.</span>data <span class="token operator">=</span> data<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> wsMsg<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">routeToKBEngine</span><span class="token punctuation">(</span><span class="token keyword">const</span> KBEngineMessage<span class="token operator">&amp;</span> kbMsg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 根据消息类型路由到不同的 App</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>kbMsg<span class="token punctuation">.</span>msgId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> KBEngineMsgId<span class="token double-colon punctuation">::</span>Chat<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 聊天消息路由到 BaseApp</span></span>
<span class="line">                <span class="token function">sendToBaseApp</span><span class="token punctuation">(</span>kbMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> KBEngineMsgId<span class="token double-colon punctuation">::</span>Move<span class="token operator">:</span></span>
<span class="line">            <span class="token keyword">case</span> KBEngineMsgId<span class="token double-colon punctuation">::</span>Action<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 游戏操作路由到 CellApp</span></span>
<span class="line">                <span class="token function">sendToCellApp</span><span class="token punctuation">(</span>kbMsg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">default</span><span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendToBaseApp</span><span class="token punctuation">(</span><span class="token keyword">const</span> KBEngineMessage<span class="token operator">&amp;</span> kbMsg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 通过内部 TCP 连接发送到 BaseApp</span></span>
<span class="line">        baseAppConnection_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>kbMsg<span class="token punctuation">.</span>bundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendToCellApp</span><span class="token punctuation">(</span><span class="token keyword">const</span> KBEngineMessage<span class="token operator">&amp;</span> kbMsg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 通过内部 TCP 连接发送到 CellApp</span></span>
<span class="line">        cellAppConnection_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>kbMsg<span class="token punctuation">.</span>bundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendToWebSocketClient</span><span class="token punctuation">(</span><span class="token keyword">const</span> WebSocketMessage<span class="token operator">&amp;</span> wsMsg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 根据玩家 ID 找到对应的 WebSocket 连接</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> clientConnections_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>wsMsg<span class="token punctuation">.</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> clientConnections_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            it<span class="token operator">-&gt;</span>second<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>wsMsg<span class="token punctuation">.</span><span class="token function">toJson</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> WebSocketConnection<span class="token operator">*</span><span class="token operator">&gt;</span> clientConnections_<span class="token punctuation">;</span></span>
<span class="line">    TCPConnection<span class="token operator">*</span> baseAppConnection_<span class="token punctuation">;</span></span>
<span class="line">    TCPConnection<span class="token operator">*</span> cellAppConnection_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-与-kbengine-集成" tabindex="-1"><a class="header-anchor" href="#_5-2-与-kbengine-集成"><span>5.2 与 KBEngine 集成</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│          KBEngine + WebSocket 集成架构                        │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  客户端层:                                                  │</span>
<span class="line">│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │</span>
<span class="line">│  │  原生客户端   │  │  Web 客户端  │  │  移动 H5    │         │</span>
<span class="line">│  │  (TCP)      │  │ (WebSocket) │  │ (WebSocket) │         │</span>
<span class="line">│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │</span>
<span class="line">│         │                │                │                 │</span>
<span class="line">│         ▼                ▼                ▼                 │</span>
<span class="line">│  ┌──────────────────────────────────────────────────┐      │</span>
<span class="line">│  │              WebSocket 网关                       │      │</span>
<span class="line">│  │  ┌─────────────────────────────────────────┐     │      │</span>
<span class="line">│  │  │  协议转换层                                  │     │      │</span>
<span class="line">│  │  │  - WebSocket ←→ KBEngine Bundle           │     │      │</span>
<span class="line">│  │  │  - JSON ←→ Binary                         │     │      │</span>
<span class="line">│  │  │  - 客户端会话管理                            │     │      │</span>
<span class="line">│  │  └─────────────────────────────────────────┘     │      │</span>
<span class="line">│  └─────────────────────────┬────────────────────────┘      │</span>
<span class="line">│                            │                                │</span>
<span class="line">│                            ▼                                │</span>
<span class="line">│  ┌──────────────────────────────────────────────────┐      │</span>
<span class="line">│  │              KBEngine 服务器                      │      │</span>
<span class="line">│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐           │      │</span>
<span class="line">│  │  │LoginApp │  │BaseApp  │  │CellApp  │           │      │</span>
<span class="line">│  │  └─────────┘  └─────────┘  └─────────┘           │      │</span>
<span class="line">│  └──────────────────────────────────────────────────┘      │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-websocket-性能优化" tabindex="-1"><a class="header-anchor" href="#_6-1-websocket-性能优化"><span>6.1 WebSocket 性能优化</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              WebSocket 性能优化技巧                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 消息压缩                                                │</span>
<span class="line">│     ├── 文本消息使用 gzip 压缩                               │</span>
<span class="line">│     ├── 二进制消息使用自定义压缩                              │</span>
<span class="line">│     └── 小于 100 字节的消息不压缩                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 批量发送                                                │</span>
<span class="line">│     ├── 合并多个小消息                                       │</span>
<span class="line">│     ├── 减少系统调用次数                                     │</span>
<span class="line">│     └── 设置 flush 阈值                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 连接复用                                                │</span>
<span class="line">│     ├── 同一页面只使用一个连接                                │</span>
<span class="line">│     ├── 使用频道/订阅模式                                    │</span>
<span class="line">│     └── 减少连接数                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 心跳优化                                                │</span>
<span class="line">│     ├── 使用 WebSocket Ping/Pong                            │</span>
<span class="line">│     ├── 间隔 30-60 秒                                        │</span>
<span class="line">│     └── 避免频繁心跳                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 二进制协议                                              │</span>
<span class="line">│     ├── 使用二进制帧而非文本帧                                │</span>
<span class="line">│     ├── 自定义序列化格式                                      │</span>
<span class="line">│     └── 减少数据传输量                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-安全考虑" tabindex="-1"><a class="header-anchor" href="#_6-2-安全考虑"><span>6.2 安全考虑</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              WebSocket 安全措施                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 握手验证                                                │</span>
<span class="line">│     ├── 验证 Origin 头                                       │</span>
<span class="line">│     ├── 检查 Referer                                         │</span>
<span class="line">│     └── 使用 Token 验证                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 数据加密                                                │</span>
<span class="line">│     ├── 使用 TLS/WSS                                         │</span>
<span class="line">│     ├── 应用层加密                                           │</span>
<span class="line">│     └── 消息签名                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 速率限制                                                │</span>
<span class="line">│     ├── 限制消息频率                                         │</span>
<span class="line">│     ├── 限制消息大小                                         │</span>
<span class="line">│     └── 检测异常行为                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 认证授权                                                │</span>
<span class="line">│     ├── 握手后立即验证 Token                                  │</span>
<span class="line">│     ├── 定期刷新 Token                                       │</span>
<span class="line">│     └── 按频道/操作授权                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="websocket-应用场景总结" tabindex="-1"><a class="header-anchor" href="#websocket-应用场景总结"><span>WebSocket 应用场景总结</span></a></h3><table><thead><tr><th>场景</th><th>优先级</th><th>复杂度</th><th>效果</th></tr></thead><tbody><tr><td><strong>Web 聊天</strong></td><td>高</td><td>低</td><td>优秀</td></tr><tr><td><strong>实时通知</strong></td><td>高</td><td>中</td><td>良好</td></tr><tr><td><strong>管理后台</strong></td><td>中</td><td>中</td><td>良好</td></tr><tr><td><strong>社交功能</strong></td><td>中</td><td>低</td><td>良好</td></tr><tr><td><strong>H5 游戏</strong></td><td>低</td><td>高</td><td>一般</td></tr></tbody></table><h3 id="架构选择建议" tabindex="-1"><a class="header-anchor" href="#架构选择建议"><span>架构选择建议</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">纯 WebSocket:</span>
<span class="line">- 适合轻量级 Web 游戏</span>
<span class="line">- 不适合复杂战斗</span>
<span class="line"></span>
<span class="line">TCP + WebSocket:</span>
<span class="line">- 原生客户端用 TCP</span>
<span class="line">- Web/H5 用 WebSocket</span>
<span class="line">- 通过网关协议转换</span>
<span class="line"></span>
<span class="line">UDP + WebSocket:</span>
<span class="line">- 战斗用 UDP (原生客户端)</span>
<span class="line">- 聊天用 WebSocket</span>
<span class="line">- 最佳用户体验组合</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://tools.ietf.org/html/rfc6455" target="_blank" rel="noopener noreferrer">RFC 6455 - WebSocket Protocol</a></li><li><a href="https://developer.mozilla.org/en-US/docs/Web/API/WebSocket" target="_blank" rel="noopener noreferrer">WebSocket API - MDN</a></li><li><a href="https://github.com/kbengine/kbengine/tree/master/kbe/src/lib/network" target="_blank" rel="noopener noreferrer">KBEngine GitHub - 网络层实现</a></li></ul>`,49)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};