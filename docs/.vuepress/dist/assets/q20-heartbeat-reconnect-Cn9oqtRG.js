import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q20-heartbeat-reconnect.html","title":"Q20: 长连接如何保持心跳？断线重连如何设计？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q20-heartbeat-reconnect.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q20-heartbeat-reconnect.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q20-长连接如何保持心跳-断线重连如何设计" tabindex="-1"><a class="header-anchor" href="#q20-长连接如何保持心跳-断线重连如何设计"><span>Q20: 长连接如何保持心跳？断线重连如何设计？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对长连接维护的理解：</p><ul><li>心跳机制的设计与实现</li><li>连接状态检测与超时处理</li><li>断线重连策略</li><li>KBEngine 的心跳和超时机制</li></ul><hr><h2 id="一、心跳机制基础" tabindex="-1"><a class="header-anchor" href="#一、心跳机制基础"><span>一、心跳机制基础</span></a></h2><h3 id="_1-1-为什么需要心跳" tabindex="-1"><a class="header-anchor" href="#_1-1-为什么需要心跳"><span>1.1 为什么需要心跳</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    心跳机制的必要性                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题: TCP 连接的&quot;假死&quot;状态                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  正常情况:                                         │       │</span>
<span class="line">│  │  客户端 ────ping───→ 服务器 ────pong───→ 客户端    │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  假死情况:                                         │       │</span>
<span class="line">│  │  客户端 ────ping───× 服务器 (网络断开)             │       │</span>
<span class="line">│  │  但 TCP 连接状态仍显示 ESTABLISHED                 │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  TCP 假死的原因:                                            │</span>
<span class="line">│  ├── 网络中断 (网线拔掉、WiFi 断开)                         │</span>
<span class="line">│  ├── 路由器重启                                            │</span>
<span class="line">│  ├── NAT 超时清理                                          │</span>
<span class="line">│  ├── 防火墙中断                                            │</span>
<span class="line">│  └── 长时间无数据传输                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  心跳的作用:                                                │</span>
<span class="line">│  ├── 检测连接是否存活                                       │</span>
<span class="line">│  ├── 保持 NAT 映射                                         │</span>
<span class="line">│  ├── 快速发现断线                                           │</span>
<span class="line">│  └── 同步服务器时间                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-心跳类型" tabindex="-1"><a class="header-anchor" href="#_1-2-心跳类型"><span>1.2 心跳类型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    心跳类型对比                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 双向心跳 (推荐)                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  客户端 ────ping───→ 服务器                       │       │</span>
<span class="line">│  │  客户端 ←────pong─── 服务器                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优势:                                            │       │</span>
<span class="line">│  │  ├── 双方都能检测断线                               │       │</span>
<span class="line">│  │  ├── 负载均衡                                       │       │</span>
<span class="line">│  │  └── 确认双向连通                                   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  劣势:                                            │       │</span>
<span class="line">│  │  ├── 带宽开销翻倍                                   │       │</span>
<span class="line">│  │  └── 实现稍复杂                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 单向心跳 (客户端主动)                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  客户端 ────ping───→ 服务器                       │       │</span>
<span class="line">│  │  服务器不回复                                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优势:                                            │       │</span>
<span class="line">│  │  ├── 实现简单                                       │       │</span>
<span class="line">│  │  ├── 带宽开销小                                     │       │</span>
<span class="line">│  │  └── 服务器压力小                                   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  劣势:                                            │       │</span>
<span class="line">│  │  ├── 服务器无法主动检测断线                          │       │</span>
<span class="line">│  │  └── 客户端不知道服务器状态                          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. TCP Keepalive (系统级)                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  由操作系统 TCP 协议栈处理                         │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优势:                                            │       │</span>
<span class="line">│  │  ├── 无需应用层处理                                 │       │</span>
<span class="line">│  │  └── 标准化                                        │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  劣势:                                            │       │</span>
<span class="line">│  │  ├── 时间太长 (默认 2 小时)                         │       │</span>
<span class="line">│  │  ├── 配置复杂                                       │       │</span>
<span class="line">│  │  └── 粒度太粗                                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、心跳实现" tabindex="-1"><a class="header-anchor" href="#二、心跳实现"><span>二、心跳实现</span></a></h2><h3 id="_2-1-客户端心跳" tabindex="-1"><a class="header-anchor" href="#_2-1-客户端心跳"><span>2.1 客户端心跳</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 客户端心跳实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">HeartbeatClient</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">HeartbeatClient</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> serverAddr<span class="token punctuation">,</span> <span class="token keyword">uint16_t</span> port<span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">serverAddr_</span><span class="token punctuation">(</span>serverAddr<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">port_</span><span class="token punctuation">(</span>port<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">          <span class="token function">running_</span><span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">lastPongTime_</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 连接服务器</span></span>
<span class="line">        <span class="token function">connect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 启动心跳线程</span></span>
<span class="line">        heartbeatThread_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>HeartbeatClient<span class="token double-colon punctuation">::</span>heartbeatLoop<span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 启动接收线程</span></span>
<span class="line">        receiveThread_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>HeartbeatClient<span class="token double-colon punctuation">::</span>receiveLoop<span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">~</span><span class="token function">HeartbeatClient</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        running_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>heartbeatThread_<span class="token punctuation">.</span><span class="token function">joinable</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            heartbeatThread_<span class="token punctuation">.</span><span class="token function">join</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>receiveThread_<span class="token punctuation">.</span><span class="token function">joinable</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            receiveThread_<span class="token punctuation">.</span><span class="token function">join</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">heartbeatLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 发送心跳</span></span>
<span class="line">            <span class="token function">sendPing</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 检查是否超时</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">checkTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">onTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">reconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 等待下一个心跳间隔</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>this_thread<span class="token double-colon punctuation">::</span><span class="token function">sleep_for</span><span class="token punctuation">(</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">milliseconds</span><span class="token punctuation">(</span>HEARTBEAT_INTERVAL<span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendPing</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        HeartbeatMessage msg<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>type <span class="token operator">=</span> MSG_PING<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>sequence <span class="token operator">=</span> nextSequence_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送</span></span>
<span class="line">        socket_<span class="token punctuation">.</span><span class="token function">send</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>msg<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">LOG_DEBUG</span><span class="token punctuation">(</span><span class="token string">&quot;Sent ping, seq=&quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>sequence<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">receiveLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint8_t</span> buffer<span class="token punctuation">[</span><span class="token number">4096</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">int</span> len <span class="token operator">=</span> socket_<span class="token punctuation">.</span><span class="token function">recv</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>buffer<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>len <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">handleMessage</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>len <span class="token operator">&lt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 连接错误</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isConnectionError</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">onDisconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token function">reconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Message<span class="token operator">*</span> msg <span class="token operator">=</span> <span class="token punctuation">(</span>Message<span class="token operator">*</span><span class="token punctuation">)</span>data<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>msg<span class="token operator">-&gt;</span>type<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> MSG_PONG<span class="token operator">:</span></span>
<span class="line">                <span class="token function">handlePong</span><span class="token punctuation">(</span><span class="token punctuation">(</span>HeartbeatMessage<span class="token operator">*</span><span class="token punctuation">)</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> MSG_KICK<span class="token operator">:</span></span>
<span class="line">                <span class="token function">handleKick</span><span class="token punctuation">(</span><span class="token punctuation">(</span>KickMessage<span class="token operator">*</span><span class="token punctuation">)</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">default</span><span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 其他业务消息</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handlePong</span><span class="token punctuation">(</span>HeartbeatMessage<span class="token operator">*</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        lastPongTime_ <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        latency_ <span class="token operator">=</span> lastPongTime_ <span class="token operator">-</span> msg<span class="token operator">-&gt;</span>timestamp<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">LOG_DEBUG</span><span class="token punctuation">(</span><span class="token string">&quot;Received pong, seq=&quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>msg<span class="token operator">-&gt;</span>sequence<span class="token punctuation">)</span> <span class="token operator">+</span></span>
<span class="line">                  <span class="token string">&quot;, latency=&quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>latency_<span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token string">&quot;ms&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">checkTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> currentTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 首次连接</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>lastPongTime_ <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查超时</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span>currentTime <span class="token operator">-</span> lastPongTime_<span class="token punctuation">)</span> <span class="token operator">&gt;</span> TIMEOUT_THRESHOLD<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Connection timeout detected&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">onDisconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onDisconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        socket_<span class="token punctuation">.</span><span class="token function">close</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        lastPongTime_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">reconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Attempting to reconnect...&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 指数退避</span></span>
<span class="line">        <span class="token keyword">static</span> <span class="token keyword">int</span> retryCount <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> delay <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">min</span><span class="token punctuation">(</span><span class="token number">1000</span> <span class="token operator">*</span> <span class="token punctuation">(</span><span class="token number">1</span> <span class="token operator">&lt;&lt;</span> retryCount<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token number">30000</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>this_thread<span class="token double-colon punctuation">::</span><span class="token function">sleep_for</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">milliseconds</span><span class="token punctuation">(</span>delay<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">connect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Reconnected successfully&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            retryCount <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            retryCount<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">LOG_ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Reconnect failed, retrying...&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">connect</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> socket_<span class="token punctuation">.</span><span class="token function">connect</span><span class="token punctuation">(</span>serverAddr_<span class="token punctuation">,</span> port_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">HeartbeatMessage</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint8_t</span> type<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> HEARTBEAT_INTERVAL <span class="token operator">=</span> <span class="token number">5000</span><span class="token punctuation">;</span>  <span class="token comment">// 5秒</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> TIMEOUT_THRESHOLD <span class="token operator">=</span> <span class="token number">15000</span><span class="token punctuation">;</span>  <span class="token comment">// 15秒</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string serverAddr_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> port_<span class="token punctuation">;</span></span>
<span class="line">    TCPSocket socket_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>thread heartbeatThread_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>thread receiveThread_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span><span class="token keyword">bool</span><span class="token operator">&gt;</span> running_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> lastPongTime_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> nextSequence_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> latency_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-服务器心跳处理" tabindex="-1"><a class="header-anchor" href="#_2-2-服务器心跳处理"><span>2.2 服务器心跳处理</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 服务器心跳处理</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">HeartbeatServer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">ClientSession</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> clientId<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> lastPingTime<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> lastPongTime<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> latency<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">bool</span> <span class="token function">isTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">uint32_t</span> now <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token punctuation">(</span>now <span class="token operator">-</span> lastPingTime<span class="token punctuation">)</span> <span class="token operator">&gt;</span> CLIENT_TIMEOUT<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token function">HeartbeatServer</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> port<span class="token punctuation">)</span> <span class="token operator">:</span> <span class="token function">port_</span><span class="token punctuation">(</span>port<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 启动服务器</span></span>
<span class="line">        serverSocket_<span class="token punctuation">.</span><span class="token function">bind</span><span class="token punctuation">(</span>port_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        serverSocket_<span class="token punctuation">.</span><span class="token function">listen</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 启动心跳检查线程</span></span>
<span class="line">        checkThread_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>HeartbeatServer<span class="token double-colon punctuation">::</span>checkLoop<span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理客户端 ping</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onPing</span><span class="token punctuation">(</span><span class="token keyword">const</span> TCPConnection<span class="token operator">&amp;</span> conn<span class="token punctuation">,</span> <span class="token keyword">const</span> HeartbeatMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> clientId <span class="token operator">=</span> <span class="token function">getClientId</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新会话</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> session <span class="token operator">=</span> sessions_<span class="token punctuation">[</span>clientId<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        session<span class="token punctuation">.</span>clientId <span class="token operator">=</span> clientId<span class="token punctuation">;</span></span>
<span class="line">        session<span class="token punctuation">.</span>lastPingTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送 pong</span></span>
<span class="line">        HeartbeatMessage pong<span class="token punctuation">;</span></span>
<span class="line">        pong<span class="token punctuation">.</span>type <span class="token operator">=</span> MSG_PONG<span class="token punctuation">;</span></span>
<span class="line">        pong<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> msg<span class="token punctuation">.</span>timestamp<span class="token punctuation">;</span></span>
<span class="line">        pong<span class="token punctuation">.</span>sequence <span class="token operator">=</span> msg<span class="token punctuation">.</span>sequence<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        conn<span class="token punctuation">.</span><span class="token function">send</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>pong<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>pong<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">LOG_DEBUG</span><span class="token punctuation">(</span><span class="token string">&quot;Client &quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token string">&quot; ping, seq=&quot;</span> <span class="token operator">+</span></span>
<span class="line">                  std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>sequence<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理客户端其他消息（更新心跳时间）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onClientMessage</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> clientId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> sessions_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> sessions_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>lastPingTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">checkLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">checkTimeouts</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>this_thread<span class="token double-colon punctuation">::</span><span class="token function">sleep_for</span><span class="token punctuation">(</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">milliseconds</span><span class="token punctuation">(</span>CHECK_INTERVAL<span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">checkTimeouts</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> timeoutClients<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>clientId<span class="token punctuation">,</span> session<span class="token punctuation">]</span> <span class="token operator">:</span> sessions_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>session<span class="token punctuation">.</span><span class="token function">isTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                timeoutClients<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 断开超时客户端</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint32_t</span> clientId <span class="token operator">:</span> timeoutClients<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Client &quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token string">&quot; timeout&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 保存玩家数据</span></span>
<span class="line">            <span class="token function">savePlayerData</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 断开连接</span></span>
<span class="line">            <span class="token function">disconnectClient</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 通知相关系统</span></span>
<span class="line">            <span class="token function">onClientDisconnect</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">disconnectClient</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> clientId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> sessions_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> sessions_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            connections_<span class="token punctuation">[</span>clientId<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">close</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            connections_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            sessions_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">savePlayerData</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> clientId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 自动保存玩家数据</span></span>
<span class="line">        Player<span class="token operator">*</span> player <span class="token operator">=</span> <span class="token function">getPlayer</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>player<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            player<span class="token operator">-&gt;</span><span class="token function">saveToDatabase</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onClientDisconnect</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> clientId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 通知其他客户端</span></span>
<span class="line">        <span class="token function">broadcastPlayerLeave</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知游戏逻辑</span></span>
<span class="line">        <span class="token function">onPlayerLogout</span><span class="token punctuation">(</span>clientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> ClientSession<span class="token operator">&gt;</span> sessions_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> TCPConnection<span class="token operator">&gt;</span> connections_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> CLIENT_TIMEOUT <span class="token operator">=</span> <span class="token number">30000</span><span class="token punctuation">;</span>  <span class="token comment">// 30秒</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> CHECK_INTERVAL <span class="token operator">=</span> <span class="token number">5000</span><span class="token punctuation">;</span>   <span class="token comment">// 5秒</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// KBEngine 风格的心跳处理</span></span>
<span class="line"><span class="token comment">// src/server/baseapp/baseapp_interface.cpp</span></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BaseApp</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 处理客户端消息（更新心跳）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onClientMessage</span><span class="token punctuation">(</span>Network<span class="token double-colon punctuation">::</span>Channel<span class="token operator">*</span> pChannel<span class="token punctuation">,</span> MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 更新客户端心跳时间</span></span>
<span class="line">        pChannel<span class="token operator">-&gt;</span><span class="token function">updateLastRecvTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 处理消息</span></span>
<span class="line">        <span class="token class-name">MessageHandler</span><span class="token double-colon punctuation">::</span><span class="token function">handle</span><span class="token punctuation">(</span>pChannel<span class="token punctuation">,</span> stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 检查超时客户端</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">checkIdleClients</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Network<span class="token double-colon punctuation">::</span>Channel<span class="token operator">*</span><span class="token operator">&gt;</span> timeoutChannels<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> pChannel <span class="token operator">:</span> channels_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>pChannel<span class="token operator">-&gt;</span><span class="token function">isIdle</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                timeoutChannels<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>pChannel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 断开超时连接</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> pChannel <span class="token operator">:</span> timeoutChannels<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Client timeout: &quot;</span> <span class="token operator">+</span></span>
<span class="line">                       pChannel<span class="token operator">-&gt;</span><span class="token function">addrAsString</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 触发断线事件</span></span>
<span class="line">            <span class="token function">onClientDisconnect</span><span class="token punctuation">(</span>pChannel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 关闭连接</span></span>
<span class="line">            pChannel<span class="token operator">-&gt;</span><span class="token function">close</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Network<span class="token double-colon punctuation">::</span>Channel<span class="token operator">*</span><span class="token operator">&gt;</span> channels_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、断线重连设计" tabindex="-1"><a class="header-anchor" href="#三、断线重连设计"><span>三、断线重连设计</span></a></h2><h3 id="_3-1-重连策略" tabindex="-1"><a class="header-anchor" href="#_3-1-重连策略"><span>3.1 重连策略</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    断线重连策略                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  策略 1: 立即重连                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  断线后立即尝试重连                                 │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 响应快                                       │       │</span>
<span class="line">│  │  缺点: 可能频繁重连，浪费资源                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用: 客户端，网络稳定环境                          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  策略 2: 固定延迟重连                                        │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  断线后等待固定时间再重连                           │       │</span>
<span class="line">│  │  例如: 5秒后重连                                    │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 简单                                         │       │</span>
<span class="line">│  │  缺点: 可能还是太频繁                               │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用: 一般网络环境                                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  策略 3: 指数退避重连 (推荐)                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  每次失败后延迟时间翻倍                             │       │</span>
<span class="line">│  │  1s → 2s → 4s → 8s → 16s → 32s (最大)             │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 平衡响应速度和服务器负载                      │       │</span>
<span class="line">│  │  缺点: 实现稍复杂                                   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用: 大部分场景                                    │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  策略 4: 随机抖动重连                                        │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  在指数退避基础上加入随机抖动                       │       │</span>
<span class="line">│  │  delay = baseDelay + random(0, baseDelay * 0.5)   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 避免大量客户端同时重连                       │       │</span>
<span class="line">│  │  缺点: 更复杂                                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用: 服务器重启后大量客户端重连                    │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-重连实现" tabindex="-1"><a class="header-anchor" href="#_3-2-重连实现"><span>3.2 重连实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 完整的断线重连实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ReconnectClient</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">ReconnectClient</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> serverAddr<span class="token punctuation">,</span> <span class="token keyword">uint16_t</span> port<span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">serverAddr_</span><span class="token punctuation">(</span>serverAddr<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">port_</span><span class="token punctuation">(</span>port<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">          <span class="token function">state_</span><span class="token punctuation">(</span>State<span class="token double-colon punctuation">::</span>Disconnected<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">retryCount_</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 连接</span></span>
<span class="line">        <span class="token function">connect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 启动接收线程</span></span>
<span class="line">        receiveThread_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>ReconnectClient<span class="token double-colon punctuation">::</span>receiveLoop<span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">State</span> <span class="token punctuation">{</span></span>
<span class="line">        Disconnected<span class="token punctuation">,</span></span>
<span class="line">        Connecting<span class="token punctuation">,</span></span>
<span class="line">        Connected<span class="token punctuation">,</span></span>
<span class="line">        Reconnecting</span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">connect</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        state_ <span class="token operator">=</span> State<span class="token double-colon punctuation">::</span>Connecting<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Connecting to &quot;</span> <span class="token operator">+</span> serverAddr_ <span class="token operator">+</span> <span class="token string">&quot;:&quot;</span> <span class="token operator">+</span></span>
<span class="line">                 std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>port_<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>socket_<span class="token punctuation">.</span><span class="token function">connect</span><span class="token punctuation">(</span>serverAddr_<span class="token punctuation">,</span> port_<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">onConnected</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">onConnectFailed</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onConnected</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        state_ <span class="token operator">=</span> State<span class="token double-colon punctuation">::</span>Connected<span class="token punctuation">;</span></span>
<span class="line">        retryCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Connected successfully&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送登录请求</span></span>
<span class="line">        <span class="token function">sendLogin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 开始心跳</span></span>
<span class="line">        <span class="token function">startHeartbeat</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onConnectFailed</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">LOG_ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Connection failed&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>state_ <span class="token operator">==</span> State<span class="token double-colon punctuation">::</span>Connecting<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">scheduleReconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onDisconnected</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Disconnected&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        state_ <span class="token operator">=</span> State<span class="token double-colon punctuation">::</span>Disconnected<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">stopHeartbeat</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 安排重连</span></span>
<span class="line">        <span class="token function">scheduleReconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">scheduleReconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        state_ <span class="token operator">=</span> State<span class="token double-colon punctuation">::</span>Reconnecting<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 计算重连延迟</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> delay <span class="token operator">=</span> <span class="token function">calculateReconnectDelay</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Reconnecting in &quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>delay<span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token string">&quot;ms...&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 延迟后重连</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">,</span> delay<span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>this_thread<span class="token double-colon punctuation">::</span><span class="token function">sleep_for</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">milliseconds</span><span class="token punctuation">(</span>delay<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>state_ <span class="token operator">==</span> State<span class="token double-colon punctuation">::</span>Reconnecting<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">connect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">detach</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">calculateReconnectDelay</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 指数退避 + 随机抖动</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> baseDelay <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">min</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token keyword">uint32_t</span><span class="token punctuation">(</span><span class="token number">1000</span> <span class="token operator">*</span> <span class="token punctuation">(</span><span class="token number">1</span> <span class="token operator">&lt;&lt;</span> retryCount_<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            MAX_RETRY_DELAY</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 添加随机抖动 (±25%)</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> jitter <span class="token operator">=</span> baseDelay <span class="token operator">/</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> randomJitter <span class="token operator">=</span> <span class="token function">rand</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">%</span> <span class="token punctuation">(</span><span class="token number">2</span> <span class="token operator">*</span> jitter <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">-</span> jitter<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> baseDelay <span class="token operator">+</span> randomJitter<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">receiveLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint8_t</span> buffer<span class="token punctuation">[</span><span class="token number">4096</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>state_ <span class="token operator">!=</span> State<span class="token double-colon punctuation">::</span>Connected<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>this_thread<span class="token double-colon punctuation">::</span><span class="token function">sleep_for</span><span class="token punctuation">(</span></span>
<span class="line">                    std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">milliseconds</span><span class="token punctuation">(</span><span class="token number">100</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">continue</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">int</span> len <span class="token operator">=</span> socket_<span class="token punctuation">.</span><span class="token function">recv</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>buffer<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>len <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">handleMessage</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>len <span class="token operator">&lt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 连接断开</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>state_ <span class="token operator">==</span> State<span class="token double-colon punctuation">::</span>Connected<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">onDisconnected</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendLogin</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        LoginRequest msg<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>token <span class="token operator">=</span> savedToken_<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>lastSequence <span class="token operator">=</span> lastSequence_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        socket_<span class="token punctuation">.</span><span class="token function">send</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>msg<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onLoginResponse</span><span class="token punctuation">(</span><span class="token keyword">const</span> LoginResponse<span class="token operator">&amp;</span> resp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>resp<span class="token punctuation">.</span>success<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Re-login successful&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 恢复状态</span></span>
<span class="line">            lastSequence_ <span class="token operator">=</span> resp<span class="token punctuation">.</span>serverSequence<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 处理断线期间的消息</span></span>
<span class="line">            <span class="token function">handleMissedMessages</span><span class="token punctuation">(</span>resp<span class="token punctuation">.</span>missedMessages<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Re-login failed: &quot;</span> <span class="token operator">+</span> resp<span class="token punctuation">.</span>reason<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 清空 token，需要重新登录</span></span>
<span class="line">            savedToken_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 通知用户重新登录</span></span>
<span class="line">            <span class="token function">notifyReloginRequired</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleMissedMessages</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Message<span class="token operator">&gt;</span><span class="token operator">&amp;</span> missed<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 处理断线期间服务器缓存的消息</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> msg <span class="token operator">:</span> missed<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">handleMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    State state_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string serverAddr_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> port_<span class="token punctuation">;</span></span>
<span class="line">    TCPSocket socket_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>thread receiveThread_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint32_t</span> retryCount_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string savedToken_<span class="token punctuation">;</span>      <span class="token comment">// 保存的登录 token</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> lastSequence_<span class="token punctuation">;</span>       <span class="token comment">// 最后收到的消息序列号</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> MAX_RETRY_DELAY <span class="token operator">=</span> <span class="token number">30000</span><span class="token punctuation">;</span> <span class="token comment">// 30秒</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-心跳机制" tabindex="-1"><a class="header-anchor" href="#四、kbengine-心跳机制"><span>四、KBEngine 心跳机制</span></a></h2><h3 id="_4-1-kbengine-超时配置" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-超时配置"><span>4.1 KBEngine 超时配置</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 心跳配置</span></span>
<span class="line"><span class="token comment"># kbengine_defaults.xml</span></span>
<span class="line"></span>
<span class="line"><span class="token operator">&lt;</span>root<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 客户端超时时间 <span class="token punctuation">(</span>秒<span class="token punctuation">)</span> <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>timeout<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 默认超时 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>default<span class="token operator">&gt;</span><span class="token number">60</span><span class="token operator">&lt;</span><span class="token operator">/</span>default<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 登录超时 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>login<span class="token operator">&gt;</span><span class="token number">30</span><span class="token operator">&lt;</span><span class="token operator">/</span>login<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 其他特定超时 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span><span class="token operator">/</span>timeout<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 心跳间隔 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>tcp<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 客户端发送心跳间隔 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>heartbeat<span class="token operator">&gt;</span><span class="token number">5</span><span class="token operator">&lt;</span><span class="token operator">/</span>heartbeat<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 服务器检查间隔 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>heartbeatCheck<span class="token operator">&gt;</span><span class="token number">1</span><span class="token operator">&lt;</span><span class="token operator">/</span>heartbeatCheck<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span><span class="token operator">/</span>tcp<span class="token operator">&gt;</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>root<span class="token operator">&gt;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-kbengine-源码分析" tabindex="-1"><a class="header-anchor" href="#_4-2-kbengine-源码分析"><span>4.2 KBEngine 源码分析</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Channel 超时检测</span></span>
<span class="line"><span class="token comment">// src/lib/network/channel.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">ChannelType</span> <span class="token punctuation">{</span></span>
<span class="line">        CHANNEL_NORMAL <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">        CHANNEL_INTERNAL <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">        CHANNEL_CLIENT <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token function">Channel</span><span class="token punctuation">(</span>Network<span class="token double-colon punctuation">::</span>EndPoint<span class="token operator">&amp;</span> endpoint<span class="token punctuation">,</span></span>
<span class="line">            ChannelType type<span class="token punctuation">,</span></span>
<span class="line">            Network<span class="token double-colon punctuation">::</span>Address<span class="token operator">&amp;</span> addr<span class="token punctuation">)</span><span class="token operator">:</span></span>
<span class="line">        <span class="token function">endPoint_</span><span class="token punctuation">(</span>endpoint<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token function">type_</span><span class="token punctuation">(</span>type<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token function">addr_</span><span class="token punctuation">(</span>addr<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token function">lastRecvTime_</span><span class="token punctuation">(</span><span class="token function">timestamp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token function">inactiveTime_</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新最后接收时间</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateLastRecvTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        lastRecvTime_ <span class="token operator">=</span> <span class="token function">timestamp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        inactiveTime_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 检查是否空闲</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isIdle</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> now <span class="token operator">=</span> <span class="token function">timestamp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> inactive <span class="token operator">=</span> now <span class="token operator">-</span> lastRecvTime_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> inactive <span class="token operator">&gt;</span> <span class="token function">getTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取超时时间</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">getTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>type_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> CHANNEL_CLIENT<span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> g_kbeSrvConfig<span class="token punctuation">.</span><span class="token function">timeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 客户端超时</span></span>
<span class="line">            <span class="token keyword">case</span> CHANNEL_INTERNAL<span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token number">300</span><span class="token punctuation">;</span>  <span class="token comment">// 内部连接 5 分钟</span></span>
<span class="line">            <span class="token keyword">default</span><span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token number">60</span><span class="token punctuation">;</span>   <span class="token comment">// 默认 1 分钟</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新空闲时间</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateInactivity</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> now <span class="token operator">=</span> <span class="token function">timestamp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        inactiveTime_ <span class="token operator">=</span> now <span class="token operator">-</span> lastRecvTime_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">inactiveTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> inactiveTime_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    Network<span class="token double-colon punctuation">::</span>EndPoint<span class="token operator">&amp;</span> endPoint_<span class="token punctuation">;</span></span>
<span class="line">    ChannelType type_<span class="token punctuation">;</span></span>
<span class="line">    Network<span class="token double-colon punctuation">::</span>Address addr_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint32_t</span> lastRecvTime_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> inactiveTime_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// KBEngine 超时检查</span></span>
<span class="line"><span class="token comment">// src/server/baseapp/baseapp.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">BaseApp</span><span class="token double-colon punctuation">::</span><span class="token function">processChannels</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 检查所有通道的超时状态</span></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> pChannel <span class="token operator">:</span> channels_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        pChannel<span class="token operator">-&gt;</span><span class="token function">updateInactivity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pChannel<span class="token operator">-&gt;</span><span class="token function">isIdle</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">                <span class="token string">&quot;Channel idle timeout: {}, inactive for {} seconds&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                pChannel<span class="token operator">-&gt;</span><span class="token function">addrAsString</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                pChannel<span class="token operator">-&gt;</span><span class="token function">inactiveTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 触发断线事件</span></span>
<span class="line">            <span class="token function">onChannelTimeout</span><span class="token punctuation">(</span>pChannel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 关闭通道</span></span>
<span class="line">            pChannel<span class="token operator">-&gt;</span><span class="token function">close</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">BaseApp</span><span class="token double-colon punctuation">::</span><span class="token function">onChannelTimeout</span><span class="token punctuation">(</span>Channel<span class="token operator">*</span> pChannel<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span>pChannel<span class="token operator">-&gt;</span><span class="token function">isClient</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 客户端超时</span></span>
<span class="line">        Entity<span class="token operator">*</span> pEntity <span class="token operator">=</span> pChannel<span class="token operator">-&gt;</span><span class="token function">entity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pEntity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 保存玩家数据</span></span>
<span class="line">            pEntity<span class="token operator">-&gt;</span><span class="token function">saveToDatabase</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 触发断线事件</span></span>
<span class="line">            pEntity<span class="token operator">-&gt;</span><span class="token function">onLogout</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 销毁实体</span></span>
<span class="line">            pEntity<span class="token operator">-&gt;</span><span class="token function">destroy</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-kbengine-客户端重连" tabindex="-1"><a class="header-anchor" href="#_4-3-kbengine-客户端重连"><span>4.3 KBEngine 客户端重连</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 客户端重连机制</span></span>
<span class="line"><span class="token comment">// 客户端代码 (Python/C++)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">KBEngineClient</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 服务器断线回调</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onDisconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Disconnected from server&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否需要重连</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">shouldReconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">startReconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">showReconnectUI</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">shouldReconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 自动重连条件：</span></span>
<span class="line">        <span class="token comment">// 1. 不是主动断开</span></span>
<span class="line">        <span class="token comment">// 2. 在游戏中</span></span>
<span class="line">        <span class="token comment">// 3. 未被踢出</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token operator">!</span>isLogout_ <span class="token operator">&amp;&amp;</span> isInGame_ <span class="token operator">&amp;&amp;</span> <span class="token operator">!</span>isKicked_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">startReconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">showReconnectingUI</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 重连线程</span></span>
<span class="line">        reconnectThread_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">int</span> retry <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">while</span> <span class="token punctuation">(</span>retry <span class="token operator">&lt;</span> MAX_RETRIES<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>this_thread<span class="token double-colon punctuation">::</span><span class="token function">sleep_for</span><span class="token punctuation">(</span></span>
<span class="line">                    std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">seconds</span><span class="token punctuation">(</span><span class="token function">calculateRetryDelay</span><span class="token punctuation">(</span>retry<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">tryReconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">onReconnectSuccess</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">                retry<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">updateRetryCount</span><span class="token punctuation">(</span>retry<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token function">onReconnectFailed</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">tryReconnect</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 连接登录服务器</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">connectToLoginApp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 使用保存的 token 重连</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">relogin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 恢复连接</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">restoreConnection</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">relogin</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        ReloginRequest req<span class="token punctuation">;</span></span>
<span class="line">        req<span class="token punctuation">.</span>accountName <span class="token operator">=</span> accountName_<span class="token punctuation">;</span></span>
<span class="line">        req<span class="token punctuation">.</span>token <span class="token operator">=</span> savedToken_<span class="token punctuation">;</span></span>
<span class="line">        req<span class="token punctuation">.</span>lastRecvTime <span class="token operator">=</span> lastRecvTime_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">send</span><span class="token punctuation">(</span>req<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 等待响应</span></span>
<span class="line">        ReloginResponse resp<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">waitForResponse</span><span class="token punctuation">(</span>resp<span class="token punctuation">,</span> <span class="token number">5000</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> resp<span class="token punctuation">.</span>success<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onReconnectSuccess</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Reconnect successful&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">hideReconnectingUI</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 请求断线期间的消息</span></span>
<span class="line">        <span class="token function">requestMissedMessages</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 恢复游戏状态</span></span>
<span class="line">        <span class="token function">restoreGameState</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">calculateRetryDelay</span><span class="token punctuation">(</span><span class="token keyword">int</span> retry<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 指数退避: 1s, 2s, 4s, 8s, 16s</span></span>
<span class="line">        <span class="token keyword">return</span> std<span class="token double-colon punctuation">::</span><span class="token function">min</span><span class="token punctuation">(</span><span class="token number">1</span> <span class="token operator">&lt;&lt;</span> retry<span class="token punctuation">,</span> <span class="token number">16</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_RETRIES <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string accountName_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string savedToken_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> lastRecvTime_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> isLogout_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> isInGame_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> isKicked_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>thread reconnectThread_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、最佳实践" tabindex="-1"><a class="header-anchor" href="#五、最佳实践"><span>五、最佳实践</span></a></h2><h3 id="_5-1-心跳参数配置" tabindex="-1"><a class="header-anchor" href="#_5-1-心跳参数配置"><span>5.1 心跳参数配置</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  心跳参数配置建议                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  心跳间隔:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  5 秒   ← 推荐 (平衡流量和响应速度)               │       │</span>
<span class="line">│  │  3 秒   ← 快速响应 (高频游戏)                     │       │</span>
<span class="line">│  │  10 秒  ← 低流量要求                              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  超时阈值:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  = 心跳间隔 × 3                                   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  心跳间隔 5 秒 → 超时 15 秒                        │       │</span>
<span class="line">│  │  心跳间隔 3 秒 → 超时 9 秒                         │       │</span>
<span class="line">│  │  心跳间隔 10 秒 → 超时 30 秒                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  原则: 超时阈值应该至少是心跳间隔的 2-3 倍                  │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-断线处理流程" tabindex="-1"><a class="header-anchor" href="#_5-2-断线处理流程"><span>5.2 断线处理流程</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              完整的断线处理流程                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  客户端断线处理:                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 检测到断线                                     │       │</span>
<span class="line">│  │  2. 保存本地状态                                   │       │</span>
<span class="line">│  │  3. 显示重连中 UI                                  │       │</span>
<span class="line">│  │  4. 尝试重连                                       │       │</span>
<span class="line">│  │  5. 如果重连成功:                                   │       │</span>
<span class="line">│  │     - 请求断线期间的消息                            │       │</span>
<span class="line">│  │     - 恢复游戏状态                                  │       │</span>
<span class="line">│  │     - 隐藏重连 UI                                   │       │</span>
<span class="line">│  │  6. 如果重连失败:                                   │       │</span>
<span class="line">│  │     - 显示重新登录按钮                              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  服务器断线处理:                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 检测到客户端超时                               │       │</span>
<span class="line">│  │  2. 保存玩家数据到数据库                           │       │</span>
<span class="line">│  │  3. 通知其他玩家玩家离开                           │       │</span>
<span class="line">│  │  4. 缓存玩家状态 (短暂保留，等待重连)               │       │</span>
<span class="line">│  │  5. 销毁/暂停实体                                  │       │</span>
<span class="line">│  │  6. 如果玩家重连成功:                              │       │</span>
<span class="line">│  │     - 恢复实体                                     │       │</span>
<span class="line">│  │     - 发送断线期间的消息                            │       │</span>
<span class="line">│  │  7. 如果超时未重连:                                │       │</span>
<span class="line">│  │     - 完全清理                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-3-状态恢复设计" tabindex="-1"><a class="header-anchor" href="#_5-3-状态恢复设计"><span>5.3 状态恢复设计</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 断线状态恢复</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">GameStateRecovery</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 服务器端保存断线玩家状态</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">saveDisconnectedPlayerState</span><span class="token punctuation">(</span>Player<span class="token operator">*</span> player<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        DisconnectedPlayerState state<span class="token punctuation">;</span></span>
<span class="line">        state<span class="token punctuation">.</span>playerId <span class="token operator">=</span> player<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        state<span class="token punctuation">.</span>accountId <span class="token operator">=</span> player<span class="token operator">-&gt;</span><span class="token function">accountId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        state<span class="token punctuation">.</span>lastPosition <span class="token operator">=</span> player<span class="token operator">-&gt;</span><span class="token function">position</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        state<span class="token punctuation">.</span>lastHP <span class="token operator">=</span> player<span class="token operator">-&gt;</span><span class="token function">hp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        state<span class="token punctuation">.</span>lastMP <span class="token operator">=</span> player<span class="token operator">-&gt;</span><span class="token function">mp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        state<span class="token punctuation">.</span>buffList <span class="token operator">=</span> player<span class="token operator">-&gt;</span><span class="token function">getBuffs</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        state<span class="token punctuation">.</span>inventory <span class="token operator">=</span> player<span class="token operator">-&gt;</span><span class="token function">getInventory</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        state<span class="token punctuation">.</span>disconnectTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 保存到内存 (短暂保留)</span></span>
<span class="line">        disconnectedPlayers_<span class="token punctuation">[</span>player<span class="token operator">-&gt;</span><span class="token function">accountId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">]</span> <span class="token operator">=</span> state<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 同时保存到数据库 (持久化)</span></span>
<span class="line">        player<span class="token operator">-&gt;</span><span class="token function">saveToDatabase</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 玩家重连时恢复状态</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">restorePlayerState</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> accountId<span class="token punctuation">,</span> Player<span class="token operator">*</span> outPlayer<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> disconnectedPlayers_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>accountId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> disconnectedPlayers_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 超时清理，从数据库加载</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token function">loadPlayerFromDatabase</span><span class="token punctuation">(</span>accountId<span class="token punctuation">,</span> outPlayer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">const</span> DisconnectedPlayerState<span class="token operator">&amp;</span> state <span class="token operator">=</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否超时</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> state<span class="token punctuation">.</span>disconnectTime <span class="token operator">&gt;</span> STATE_TIMEOUT<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 超时，从数据库加载</span></span>
<span class="line">            disconnectedPlayers_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token function">loadPlayerFromDatabase</span><span class="token punctuation">(</span>accountId<span class="token punctuation">,</span> outPlayer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 恢复状态</span></span>
<span class="line">        outPlayer<span class="token operator">-&gt;</span><span class="token function">setPosition</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>lastPosition<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        outPlayer<span class="token operator">-&gt;</span><span class="token function">setHP</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>lastHP<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        outPlayer<span class="token operator">-&gt;</span><span class="token function">setMP</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>lastMP<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        outPlayer<span class="token operator">-&gt;</span><span class="token function">restoreBuffs</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>buffList<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        outPlayer<span class="token operator">-&gt;</span><span class="token function">restoreInventory</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>inventory<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 清理临时状态</span></span>
<span class="line">        disconnectedPlayers_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取断线期间的消息</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Message<span class="token operator">&gt;</span> <span class="token function">getMissedMessages</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Message<span class="token operator">&gt;</span> messages<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> messageBuffer_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> messageBuffer_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            messages <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>second<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            messageBuffer_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> messages<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 为断线玩家缓存消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">bufferMessage</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">const</span> Message<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        messageBuffer_<span class="token punctuation">[</span>playerId<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 限制缓冲大小</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>messageBuffer_<span class="token punctuation">[</span>playerId<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> MAX_BUFFERED_MESSAGES<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            messageBuffer_<span class="token punctuation">[</span>playerId<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span></span>
<span class="line">                messageBuffer_<span class="token punctuation">[</span>playerId<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">DisconnectedPlayerState</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> playerId<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> accountId<span class="token punctuation">;</span></span>
<span class="line">        Position lastPosition<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> lastHP<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> lastMP<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Buff<span class="token operator">&gt;</span> buffList<span class="token punctuation">;</span></span>
<span class="line">        Inventory inventory<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> disconnectTime<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> DisconnectedPlayerState<span class="token operator">&gt;</span> disconnectedPlayers_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Message<span class="token operator">&gt;&gt;</span> messageBuffer_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> STATE_TIMEOUT <span class="token operator">=</span> <span class="token number">300</span><span class="token punctuation">;</span> <span class="token comment">// 5分钟</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t MAX_BUFFERED_MESSAGES <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、总结" tabindex="-1"><a class="header-anchor" href="#六、总结"><span>六、总结</span></a></h2><h3 id="心跳重连方案对比" tabindex="-1"><a class="header-anchor" href="#心跳重连方案对比"><span>心跳重连方案对比</span></a></h3><table><thead><tr><th>方案</th><th>心跳间隔</th><th>超时阈值</th><th>重连策略</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>标准</strong></td><td>5秒</td><td>15秒</td><td>指数退避</td><td>大部分 MMO</td></tr><tr><td><strong>快速响应</strong></td><td>3秒</td><td>9秒</td><td>立即重连</td><td>竞技游戏</td></tr><tr><td><strong>低流量</strong></td><td>10秒</td><td>30秒</td><td>固定延迟</td><td>休闲游戏</td></tr><tr><td><strong>高可靠</strong></td><td>5秒</td><td>25秒</td><td>指数+抖动</td><td>关键业务</td></tr></tbody></table><h3 id="kbengine-心跳机制" tabindex="-1"><a class="header-anchor" href="#kbengine-心跳机制"><span>KBEngine 心跳机制</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 默认配置:</span>
<span class="line">- 客户端超时: 60 秒</span>
<span class="line">- 心跳间隔: 5 秒 (可配置)</span>
<span class="line">- 内部连接超时: 300 秒</span>
<span class="line"></span>
<span class="line">特点:</span>
<span class="line">1. 基于 Channel 的超时检测</span>
<span class="line">2. 自动保存玩家数据</span>
<span class="line">3. 支持断线重连</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/lib/network/channel.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - Channel 超时</a></li><li><a href="https://tldp.org/HOWTO/TCP-Keepalive-HOWTO/" target="_blank" rel="noopener noreferrer">TCP Keepalive 详解</a></li><li><a href="https://tools.ietf.org/html/rfc6455" target="_blank" rel="noopener noreferrer">WebSocket 心跳最佳实践</a></li></ul>`,47)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};