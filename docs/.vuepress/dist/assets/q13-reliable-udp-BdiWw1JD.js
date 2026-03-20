import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q13-reliable-udp.html","title":"Q13: 什么是可靠 UDP？如何实现？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q13-reliable-udp.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q13-reliable-udp.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q13-什么是可靠-udp-如何实现" tabindex="-1"><a class="header-anchor" href="#q13-什么是可靠-udp-如何实现"><span>Q13: 什么是可靠 UDP？如何实现？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对可靠 UDP 的理解：</p><ul><li>为什么需要可靠 UDP</li><li>可靠 UDP 的实现原理</li><li>KBEngine 的可靠 UDP 实现</li><li>与 TCP 的对比</li></ul><hr><h2 id="一、可靠-udp-概述" tabindex="-1"><a class="header-anchor" href="#一、可靠-udp-概述"><span>一、可靠 UDP 概述</span></a></h2><h3 id="_1-1-为什么需要可靠-udp" tabindex="-1"><a class="header-anchor" href="#_1-1-为什么需要可靠-udp"><span>1.1 为什么需要可靠 UDP</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  TCP vs UDP 的困境                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  TCP 的问题：                                                │</span>
<span class="line">│  ├── HEAD-OF-LINE BLOCKING - 丢包阻塞后续数据               │</span>
<span class="line">│  ├── 拥塞控制保守 - 发送速率受限                            │</span>
<span class="line">│  ├── 连接建立开销 - 三次握手                                │</span>
<span class="line">│  └── 固定重传超时 - 延迟高                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  UDP 的问题：                                                │</span>
<span class="line">│  ├── 不可靠 - 数据可能丢失                                  │</span>
<span class="line">│  ├── 无序 - 数据可能乱序                                    │</span>
<span class="line">│  ├── 无流量控制 - 可能淹没接收方                            │</span>
<span class="line">│  └── 无拥塞控制 - 可能导致网络拥塞                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  解决方案：可靠 UDP                                          │</span>
<span class="line">│  ├── 保留 UDP 的低延迟特性                                  │</span>
<span class="line">│  ├── 在应用层实现可靠性                                     │</span>
<span class="line">│  ├── 可根据场景定制策略                                     │</span>
<span class="line">│  └── 避免 TCP 的固有缺陷                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-可靠-udp-的设计目标" tabindex="-1"><a class="header-anchor" href="#_1-2-可靠-udp-的设计目标"><span>1.2 可靠 UDP 的设计目标</span></a></h3><table><thead><tr><th>特性</th><th>TCP</th><th>UDP</th><th>可靠 UDP</th></tr></thead><tbody><tr><td><strong>延迟</strong></td><td>高</td><td>最低</td><td>低</td></tr><tr><td><strong>可靠性</strong></td><td>高</td><td>无</td><td>高</td></tr><tr><td><strong>顺序</strong></td><td>保证</td><td>无</td><td>保证</td></tr><tr><td><strong>拥塞控制</strong></td><td>内置</td><td>无</td><td>可选</td></tr><tr><td><strong>灵活性</strong></td><td>低</td><td>高</td><td>高</td></tr></tbody></table><hr><h2 id="二、可靠-udp-实现原理" tabindex="-1"><a class="header-anchor" href="#二、可靠-udp-实现原理"><span>二、可靠 UDP 实现原理</span></a></h2><h3 id="_2-1-核心机制" tabindex="-1"><a class="header-anchor" href="#_2-1-核心机制"><span>2.1 核心机制</span></a></h3>`,13),i(d,{code:`eJxLy8kvT85ILCpR8AniUgCC4tKk9KLEggyFoNSczMSknNRQl4Bopaf961/OXaAAZCvFgpWBgGP00139TzumP+3fjhB0in6+cN2LdUuezdn1tGObgqOzN0LOOfrFttZn07e9bO99smeBQlCIP0LOJfpp326gBELENfrJzo1AG4CCz/omIcTdop9tbXzZ3v+sbznQBoh4al4KF5jhqKCra6fgBCadwSIuYLYrmO0MZrtxAQDRhVZ4`}),o[1]||=e(`<h3 id="_2-2-数据包格式" tabindex="-1"><a class="header-anchor" href="#_2-2-数据包格式"><span>2.2 数据包格式</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  可靠 UDP 数据包格式                         │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  UDP 首部 (8 字节)                               │       │</span>
<span class="line">│  │  ┌────────┬────────┬──────┬──────┐              │       │</span>
<span class="line">│  │  │源端口  │目标端口│ 长度  │校验和 │              │       │</span>
<span class="line">│  │  └────────┴────────┴──────┴──────┘              │       │</span>
<span class="line">│  ├─────────────────────────────────────────────────┤       │</span>
<span class="line">│  │  可靠层 首部                                    │       │</span>
<span class="line">│  │  ┌──────────┬──────────┬──────────┬──────────┐ │       │</span>
<span class="line">│  │  │ 序列号    │ 确认号    │ 标志     │ 窗口     │ │       │</span>
<span class="line">│  │  │ (16bit)  │ (16bit)  │ (8bit)   │ (16bit)  │ │       │</span>
<span class="line">│  │  └──────────┴──────────┴──────────┴──────────┘ │       │</span>
<span class="line">│  ├─────────────────────────────────────────────────┤       │</span>
<span class="line">│  │  数据负载                                        │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  标志位：                                                  │</span>
<span class="line">│  ├── bit 0: SYN (同步)                                     │</span>
<span class="line">│  ├── bit 1: ACK (确认)                                     │</span>
<span class="line">│  ├── bit 2: FIN (结束)                                     │</span>
<span class="line">│  └── bit 3: NACK (负确认)                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-状态机" tabindex="-1"><a class="header-anchor" href="#_2-3-状态机"><span>2.3 状态机</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  可靠 UDP 连接状态机                        │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│    ┌─────────┐                                             │</span>
<span class="line">│    │ CLOSED  │                                             │</span>
<span class="line">│    └────┬────┘                                             │</span>
<span class="line">│         │ 发送 SYN                                          │</span>
<span class="line">│         ▼                                                   │</span>
<span class="line">│    ┌─────────┐   收到 SYN/ACK   ┌─────────┐                │</span>
<span class="line">│    │ SYN_SENT │ ───────────────►│ ESTAB   │                │</span>
<span class="line">│    └─────────┘                  └────┬────┘                │</span>
<span class="line">│         ▲                              │                    │</span>
<span class="line">│         │    收到 SYN                 │ 发送 FIN            │</span>
<span class="line">│         └─────────────────────────────┼─────────────────┐  │</span>
<span class="line">│                                        ▼                 │  │</span>
<span class="line">│                                  ┌─────────┐             │  │</span>
<span class="line">│                                  │FIN_WAIT │             │  │</span>
<span class="line">│                                  └────┬────┘             │  │</span>
<span class="line">│                                       │ 收到 FIN/ACK      │  │</span>
<span class="line">│                                       ▼                  │  │</span>
<span class="line">│                                  ┌─────────┐             │  │</span>
<span class="line">│                                  │ CLOSED  │◄────────────┘  │</span>
<span class="line">│                                  └─────────┘                │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、核心机制实现" tabindex="-1"><a class="header-anchor" href="#三、核心机制实现"><span>三、核心机制实现</span></a></h2><h3 id="_3-1-序列号与确认" tabindex="-1"><a class="header-anchor" href="#_3-1-序列号与确认"><span>3.1 序列号与确认</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 可靠 UDP 实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ReliableUDP</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 发送窗口</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">SendWindow</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">struct</span> <span class="token class-name">Slot</span> <span class="token punctuation">{</span></span>
<span class="line">            Packet packet<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">bool</span> acked<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">uint64_t</span> sendTime<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">int</span> retries<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>array<span class="token operator">&lt;</span>Slot<span class="token punctuation">,</span> <span class="token number">256</span><span class="token operator">&gt;</span> slots<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> base<span class="token punctuation">;</span>      <span class="token comment">// 窗口起始序列号</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> next<span class="token punctuation">;</span>      <span class="token comment">// 下一个待发送序列号</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> size<span class="token punctuation">;</span>      <span class="token comment">// 窗口大小</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收窗口</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">RecvWindow</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">struct</span> <span class="token class-name">Slot</span> <span class="token punctuation">{</span></span>
<span class="line">            Packet packet<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">bool</span> received<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>array<span class="token operator">&lt;</span>Slot<span class="token punctuation">,</span> <span class="token number">256</span><span class="token operator">&gt;</span> slots<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> base<span class="token punctuation">;</span>      <span class="token comment">// 期望接收序列号</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> size<span class="token punctuation">;</span>      <span class="token comment">// 窗口大小</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送数据</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">send</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>sendWindow_<span class="token punctuation">.</span>next <span class="token operator">-</span> sendWindow_<span class="token punctuation">.</span>base <span class="token operator">&gt;=</span> sendWindow_<span class="token punctuation">.</span>size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 发送窗口已满</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 创建数据包</span></span>
<span class="line">        Packet packet<span class="token punctuation">;</span></span>
<span class="line">        packet<span class="token punctuation">.</span>seq <span class="token operator">=</span> sendWindow_<span class="token punctuation">.</span>next<span class="token punctuation">;</span></span>
<span class="line">        packet<span class="token punctuation">.</span>data<span class="token punctuation">.</span><span class="token function">assign</span><span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token keyword">char</span><span class="token operator">*</span><span class="token punctuation">)</span>data<span class="token punctuation">,</span> <span class="token punctuation">(</span><span class="token keyword">char</span><span class="token operator">*</span><span class="token punctuation">)</span>data <span class="token operator">+</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 存入发送窗口</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> slot <span class="token operator">=</span> sendWindow_<span class="token punctuation">.</span>slots<span class="token punctuation">[</span>packet<span class="token punctuation">.</span>seq <span class="token operator">%</span> <span class="token number">256</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        slot<span class="token punctuation">.</span>packet <span class="token operator">=</span> packet<span class="token punctuation">;</span></span>
<span class="line">        slot<span class="token punctuation">.</span>acked <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        slot<span class="token punctuation">.</span>sendTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        slot<span class="token punctuation">.</span>retries <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送</span></span>
<span class="line">        udpSocket_<span class="token operator">-&gt;</span><span class="token function">sendTo</span><span class="token punctuation">(</span>packet<span class="token punctuation">.</span>data<span class="token punctuation">,</span> destAddr_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        sendWindow_<span class="token punctuation">.</span>next<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收数据</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onPacketReceived</span><span class="token punctuation">(</span><span class="token keyword">const</span> Packet<span class="token operator">&amp;</span> packet<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> seq <span class="token operator">=</span> packet<span class="token punctuation">.</span>seq<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否在接收窗口内</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>seq <span class="token operator">-</span> recvWindow_<span class="token punctuation">.</span>base <span class="token operator">&lt;</span> recvWindow_<span class="token punctuation">.</span>size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> slot <span class="token operator">=</span> recvWindow_<span class="token punctuation">.</span>slots<span class="token punctuation">[</span>seq <span class="token operator">%</span> <span class="token number">256</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            slot<span class="token punctuation">.</span>packet <span class="token operator">=</span> packet<span class="token punctuation">;</span></span>
<span class="line">            slot<span class="token punctuation">.</span>received <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 发送 ACK</span></span>
<span class="line">            <span class="token function">sendAck</span><span class="token punctuation">(</span>seq<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 处理可交付的数据</span></span>
<span class="line">            <span class="token function">deliverPackets</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理 ACK</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onAckReceived</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> ack<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 标记已确认</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint16_t</span> seq <span class="token operator">=</span> sendWindow_<span class="token punctuation">.</span>base<span class="token punctuation">;</span></span>
<span class="line">             seq <span class="token operator">!=</span> ack<span class="token punctuation">;</span></span>
<span class="line">             seq <span class="token operator">=</span> <span class="token punctuation">(</span>seq <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">&amp;</span> <span class="token number">0xFFFF</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> slot <span class="token operator">=</span> sendWindow_<span class="token punctuation">.</span>slots<span class="token punctuation">[</span>seq <span class="token operator">%</span> <span class="token number">256</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            slot<span class="token punctuation">.</span>acked <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 滑动窗口</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>sendWindow_<span class="token punctuation">.</span>base <span class="token operator">!=</span> ack<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> slot <span class="token operator">=</span> sendWindow_<span class="token punctuation">.</span>slots<span class="token punctuation">[</span>sendWindow_<span class="token punctuation">.</span>base <span class="token operator">%</span> <span class="token number">256</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>slot<span class="token punctuation">.</span>acked<span class="token punctuation">)</span> <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            sendWindow_<span class="token punctuation">.</span>base<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 交付已接收的数据</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">deliverPackets</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>recvWindow_<span class="token punctuation">.</span>base <span class="token operator">&lt;</span> recvWindow_<span class="token punctuation">.</span>base <span class="token operator">+</span> recvWindow_<span class="token punctuation">.</span>size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> slot <span class="token operator">=</span> recvWindow_<span class="token punctuation">.</span>slots<span class="token punctuation">[</span>recvWindow_<span class="token punctuation">.</span>base <span class="token operator">%</span> <span class="token number">256</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>slot<span class="token punctuation">.</span>received<span class="token punctuation">)</span> <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 交付给应用层</span></span>
<span class="line">            <span class="token function">onDataReceived</span><span class="token punctuation">(</span>slot<span class="token punctuation">.</span>packet<span class="token punctuation">.</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            recvWindow_<span class="token punctuation">.</span>base<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    UDPSocket<span class="token operator">*</span> udpSocket_<span class="token punctuation">;</span></span>
<span class="line">    SendWindow sendWindow_<span class="token punctuation">;</span></span>
<span class="line">    RecvWindow recvWindow_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-超时重传" tabindex="-1"><a class="header-anchor" href="#_3-2-超时重传"><span>3.2 超时重传</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 超时重传实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RetransmissionManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// RTO (Retransmission Timeout) 计算</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint64_t</span> MIN_RTO <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span>   <span class="token comment">// 100ms</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint64_t</span> MAX_RTO <span class="token operator">=</span> <span class="token number">3000</span><span class="token punctuation">;</span>  <span class="token comment">// 3s</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">float</span> G <span class="token operator">=</span> <span class="token number">0.125f</span><span class="token punctuation">;</span>          <span class="token comment">// 增益因子</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint64_t</span> srtt_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>   <span class="token comment">// 平滑 RTT</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> rttvar_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> <span class="token comment">// RTT 变化量</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> rto_ <span class="token operator">=</span> MIN_RTO<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新 RTT 估算</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateRTT</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> measuredRTT<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>srtt_ <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 第一次测量</span></span>
<span class="line">            srtt_ <span class="token operator">=</span> measuredRTT<span class="token punctuation">;</span></span>
<span class="line">            rttvar_ <span class="token operator">=</span> measuredRTT <span class="token operator">/</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 更新平滑 RTT</span></span>
<span class="line">            rttvar_ <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token number">3</span> <span class="token operator">*</span> rttvar_ <span class="token operator">+</span> <span class="token function">abs</span><span class="token punctuation">(</span>measuredRTT <span class="token operator">-</span> srtt_<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">            srtt_ <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token number">7</span> <span class="token operator">*</span> srtt_ <span class="token operator">+</span> measuredRTT<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">8</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 计算 RTO</span></span>
<span class="line">        rto_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">clamp</span><span class="token punctuation">(</span>srtt_ <span class="token operator">+</span> <span class="token number">4</span> <span class="token operator">*</span> rttvar_<span class="token punctuation">,</span> MIN_RTO<span class="token punctuation">,</span> MAX_RTO<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 检查超时重传</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">checkTimeouts</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> currentTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint16_t</span> seq <span class="token operator">=</span> sendWindow_<span class="token punctuation">.</span>base<span class="token punctuation">;</span></span>
<span class="line">             seq <span class="token operator">!=</span> sendWindow_<span class="token punctuation">.</span>next<span class="token punctuation">;</span></span>
<span class="line">             seq <span class="token operator">=</span> <span class="token punctuation">(</span>seq <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">&amp;</span> <span class="token number">0xFFFF</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> slot <span class="token operator">=</span> sendWindow_<span class="token punctuation">.</span>slots<span class="token punctuation">[</span>seq <span class="token operator">%</span> <span class="token number">256</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>slot<span class="token punctuation">.</span>acked<span class="token punctuation">)</span> <span class="token keyword">continue</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>currentTime <span class="token operator">-</span> slot<span class="token punctuation">.</span>sendTime <span class="token operator">&gt;=</span> rto_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 超时，重传</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>slot<span class="token punctuation">.</span>retries <span class="token operator">&lt;</span> MAX_RETRIES<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">retransmit</span><span class="token punctuation">(</span>slot<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    slot<span class="token punctuation">.</span>retries<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">                    slot<span class="token punctuation">.</span>sendTime <span class="token operator">=</span> currentTime<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                    <span class="token comment">// 指数退避</span></span>
<span class="line">                    rto_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">min</span><span class="token punctuation">(</span>rto_ <span class="token operator">*</span> <span class="token number">2</span><span class="token punctuation">,</span> MAX_RTO<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token comment">// 超过最大重传次数，连接超时</span></span>
<span class="line">                    <span class="token function">onTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">retransmit</span><span class="token punctuation">(</span>SendWindow<span class="token double-colon punctuation">::</span>Slot<span class="token operator">&amp;</span> slot<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 重传数据包</span></span>
<span class="line">        udpSocket_<span class="token operator">-&gt;</span><span class="token function">sendTo</span><span class="token punctuation">(</span>slot<span class="token punctuation">.</span>packet<span class="token punctuation">.</span>data<span class="token punctuation">,</span> destAddr_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_RETRIES <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-快速重传" tabindex="-1"><a class="header-anchor" href="#_3-3-快速重传"><span>3.3 快速重传</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 快速重传（类似 TCP）</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">FastRetransmit</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 重复 ACK 计数</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint16_t</span><span class="token punctuation">,</span> <span class="token keyword">int</span><span class="token operator">&gt;</span> dupAckCount_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理 ACK</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onAck</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> ack<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>ack <span class="token operator">==</span> lastAck_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 重复 ACK</span></span>
<span class="line">            dupAckCount_<span class="token punctuation">[</span>ack<span class="token punctuation">]</span><span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 3 次重复 ACK，立即重传</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>dupAckCount_<span class="token punctuation">[</span>ack<span class="token punctuation">]</span> <span class="token operator">&gt;=</span> <span class="token number">3</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">fastRetransmit</span><span class="token punctuation">(</span>ack<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                dupAckCount_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 新 ACK</span></span>
<span class="line">            lastAck_ <span class="token operator">=</span> ack<span class="token punctuation">;</span></span>
<span class="line">            dupAckCount_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 快速重传</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">fastRetransmit</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> ack<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 重传从 ack 开始的未确认数据</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint16_t</span> seq <span class="token operator">=</span> ack<span class="token punctuation">;</span></span>
<span class="line">             seq <span class="token operator">!=</span> sendWindow_<span class="token punctuation">.</span>next<span class="token punctuation">;</span></span>
<span class="line">             seq <span class="token operator">=</span> <span class="token punctuation">(</span>seq <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">&amp;</span> <span class="token number">0xFFFF</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> slot <span class="token operator">=</span> sendWindow_<span class="token punctuation">.</span>slots<span class="token punctuation">[</span>seq <span class="token operator">%</span> <span class="token number">256</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>slot<span class="token punctuation">.</span>acked<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">retransmit</span><span class="token punctuation">(</span>slot<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span>  <span class="token comment">// 只重传一个包</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> lastAck_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-的可靠-udp" tabindex="-1"><a class="header-anchor" href="#四、kbengine-的可靠-udp"><span>四、KBEngine 的可靠 UDP</span></a></h2><h3 id="_4-1-kbengine-实现" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-实现"><span>4.1 KBEngine 实现</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine 源码</a>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 可靠 UDP 实现</span></span>
<span class="line"><span class="token comment">// src/server/network/reliable_udp.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ReliableUDP</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Channel</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 数据包标志</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">PacketFlags</span> <span class="token punctuation">{</span></span>
<span class="line">        FLAG_HAS_RECV_PACKET <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span>    <span class="token comment">// 已接收包</span></span>
<span class="line">        FLAG_IS_SENDING <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span>         <span class="token comment">// 发送中</span></span>
<span class="line">        FLAG_HAS_SEND_PACKET <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">,</span>    <span class="token comment">// 已发送包</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送数据包</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">send</span><span class="token punctuation">(</span>Packet<span class="token operator">*</span> pPacket<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pPacket<span class="token operator">-&gt;</span><span class="token function">isReliable</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 可靠包需要处理</span></span>
<span class="line">            pPacket<span class="token operator">-&gt;</span>seq <span class="token operator">=</span> sequence_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 添加到发送队列</span></span>
<span class="line">            sendQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>pPacket<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 立即发送</span></span>
<span class="line">            pPacket<span class="token operator">-&gt;</span>flags <span class="token operator">|=</span> FLAG_IS_SENDING<span class="token punctuation">;</span></span>
<span class="line">            pPacket<span class="token operator">-&gt;</span>sentTimes <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">            pPacket<span class="token operator">-&gt;</span>sentTime <span class="token operator">=</span> <span class="token function">getTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            channel_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>pPacket<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 不可靠包直接发送</span></span>
<span class="line">            channel_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>pPacket<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理 ACK</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processAck</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> seq<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 从发送队列移除已确认的包</span></span>
<span class="line">        <span class="token keyword">auto</span> iter <span class="token operator">=</span> sendQueue_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>iter <span class="token operator">!=</span> sendQueue_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token operator">*</span>iter<span class="token punctuation">)</span><span class="token operator">-&gt;</span>seq <span class="token operator">==</span> seq<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">delete</span> <span class="token operator">*</span>iter<span class="token punctuation">;</span></span>
<span class="line">                iter <span class="token operator">=</span> sendQueue_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>iter<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token operator">++</span>iter<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 超时重传</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">checkSendTimeOut</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> now <span class="token operator">=</span> <span class="token function">getTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> timeout <span class="token operator">=</span> resendTimeout_ <span class="token operator">*</span> <span class="token number">1000</span><span class="token punctuation">;</span>  <span class="token comment">// 微秒</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> pPacket <span class="token operator">:</span> sendQueue_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>pPacket<span class="token operator">-&gt;</span>flags <span class="token operator">&amp;</span> FLAG_IS_SENDING<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>now <span class="token operator">-</span> pPacket<span class="token operator">-&gt;</span>sentTime <span class="token operator">&gt;=</span> timeout<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token comment">// 超时重传</span></span>
<span class="line">                    <span class="token keyword">if</span> <span class="token punctuation">(</span>pPacket<span class="token operator">-&gt;</span>sentTimes <span class="token operator">&lt;</span> maxResendTimes_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                        channel_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>pPacket<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                        pPacket<span class="token operator">-&gt;</span>sentTimes<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">                        pPacket<span class="token operator">-&gt;</span>sentTime <span class="token operator">=</span> now<span class="token punctuation">;</span></span>
<span class="line">                    <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                        <span class="token comment">// 超过最大重传次数</span></span>
<span class="line">                        <span class="token function">onPacketLoss</span><span class="token punctuation">(</span>pPacket<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token punctuation">}</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>list<span class="token operator">&lt;</span>Packet<span class="token operator">*</span><span class="token operator">&gt;</span> sendQueue_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> sequence_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> resendTimeout_ <span class="token operator">=</span> <span class="token number">500</span><span class="token punctuation">;</span>  <span class="token comment">// 500ms</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> maxResendTimes_ <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-与-tcp-的对比" tabindex="-1"><a class="header-anchor" href="#_4-2-与-tcp-的对比"><span>4.2 与 TCP 的对比</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              KBEngine 可靠 UDP vs TCP                       │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  KBEngine 可靠 UDP 优势：                                    │</span>
<span class="line">│  ├── 无连接建立延迟                                         │</span>
<span class="line">│  ├── 更灵活的重传策略                                       │</span>
<span class="line">│  ├── 可选择性地启用可靠性                                   │</span>
<span class="line">│  └── 避免 TCP 的 HOLE 问题                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  KBEngine 默认使用 TCP：                                     │</span>
<span class="line">│  ├── 开发更简单                                             │</span>
<span class="line">│  ├── 调试更方便                                             │</span>
<span class="line">│  ├── 兼容性更好                                             │</span>
<span class="line">│  └── 对于大多数场景足够                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、高级特性" tabindex="-1"><a class="header-anchor" href="#五、高级特性"><span>五、高级特性</span></a></h2><h3 id="_5-1-选择性确认-sack" tabindex="-1"><a class="header-anchor" href="#_5-1-选择性确认-sack"><span>5.1 选择性确认 (SACK)</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  SACK (Selective ACK)                       │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景：发送方发送 1-5 号包，其中 2、4 丢失                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  传统 ACK：                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  接收方: ACK 1                                   │       │</span>
<span class="line">│  │  接收方: ACK 1 (等待 2)                          │       │</span>
<span class="line">│  │  接收方: ACK 3 (暗示 2 丢失)                     │       │</span>
<span class="line">│  │  发送方: 重传 2                                   │       │</span>
<span class="line">│  │  接收方: ACK 4 (暗示 4 丢失)                     │       │</span>
<span class="line">│  │  发送方: 重传 4                                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  SACK：                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  接收方: ACK 1, SACK {1}                         │       │</span>
<span class="line">│  │  接收方: ACK 3, SACK {1, 3} (暗示 2 丢失)        │       │</span>
<span class="line">│  │  发送方: 重传 2, 4 (一次性重传所有丢失包)         │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-fec-前向纠错" tabindex="-1"><a class="header-anchor" href="#_5-2-fec-前向纠错"><span>5.2 FEC 前向纠错</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// FEC + 可靠 UDP 组合</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">FECReliableUDP</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 编码窗口</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">FECWindow</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Packet<span class="token operator">&gt;</span> dataPackets<span class="token punctuation">;</span></span>
<span class="line">        Packet fecPacket<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> baseSeq<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送数据</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">send</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Packet<span class="token operator">&gt;</span><span class="token operator">&amp;</span> packets<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        FECWindow window<span class="token punctuation">;</span></span>
<span class="line">        window<span class="token punctuation">.</span>dataPackets <span class="token operator">=</span> packets<span class="token punctuation">;</span></span>
<span class="line">        window<span class="token punctuation">.</span>baseSeq <span class="token operator">=</span> nextSeq_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送数据包</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> packet <span class="token operator">:</span> packets<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            packet<span class="token punctuation">.</span>seq <span class="token operator">=</span> nextSeq_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">            udpSocket_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>packet<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 计算 FEC 包</span></span>
<span class="line">        window<span class="token punctuation">.</span>fecPacket <span class="token operator">=</span> <span class="token function">calculateFEC</span><span class="token punctuation">(</span>packets<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        window<span class="token punctuation">.</span>fecPacket<span class="token punctuation">.</span>seq <span class="token operator">=</span> nextSeq_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">        window<span class="token punctuation">.</span>fecPacket<span class="token punctuation">.</span>isFEC <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送 FEC 包</span></span>
<span class="line">        udpSocket_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>window<span class="token punctuation">.</span>fecPacket<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        fecWindows_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>window<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收数据</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onPacketReceived</span><span class="token punctuation">(</span><span class="token keyword">const</span> Packet<span class="token operator">&amp;</span> packet<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>packet<span class="token punctuation">.</span>isFEC<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// FEC 包，存储备用</span></span>
<span class="line">            fecPackets_<span class="token punctuation">[</span>packet<span class="token punctuation">.</span>seq<span class="token punctuation">]</span> <span class="token operator">=</span> packet<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 数据包</span></span>
<span class="line">            dataPackets_<span class="token punctuation">[</span>packet<span class="token punctuation">.</span>seq<span class="token punctuation">]</span> <span class="token operator">=</span> packet<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 检查是否有 FEC 窗口可以恢复</span></span>
<span class="line">            <span class="token function">checkFECRecovery</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 检查 FEC 恢复</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">checkFECRecovery</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> window <span class="token operator">:</span> fecWindows_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">bool</span> allReceived <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            Packet<span class="token operator">*</span> missingPacket <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> packet <span class="token operator">:</span> window<span class="token punctuation">.</span>dataPackets<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">auto</span> it <span class="token operator">=</span> dataPackets_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>packet<span class="token punctuation">.</span>seq<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> dataPackets_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    allReceived <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">                    missingPacket <span class="token operator">=</span> <span class="token operator">&amp;</span>packet<span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 如果只有一个包丢失，尝试恢复</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>allReceived <span class="token operator">&amp;&amp;</span> missingPacket<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">auto</span> fecIt <span class="token operator">=</span> fecPackets_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>window<span class="token punctuation">.</span>fecPacket<span class="token punctuation">.</span>seq<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>fecIt <span class="token operator">!=</span> fecPackets_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">recoverPacket</span><span class="token punctuation">(</span>missingPacket<span class="token punctuation">,</span> window<span class="token punctuation">,</span> fecIt<span class="token operator">-&gt;</span>second<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>FECWindow<span class="token operator">&gt;</span> fecWindows_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint16_t</span><span class="token punctuation">,</span> Packet<span class="token operator">&gt;</span> dataPackets_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint16_t</span><span class="token punctuation">,</span> Packet<span class="token operator">&gt;</span> fecPackets_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、总结" tabindex="-1"><a class="header-anchor" href="#六、总结"><span>六、总结</span></a></h2><h3 id="实现对比" tabindex="-1"><a class="header-anchor" href="#实现对比"><span>实现对比</span></a></h3><table><thead><tr><th>特性</th><th>TCP</th><th>KCP</th><th>自研可靠 UDP</th></tr></thead><tbody><tr><td><strong>实现复杂度</strong></td><td>低</td><td>中</td><td>高</td></tr><tr><td><strong>延迟</strong></td><td>中</td><td>低</td><td>可定制</td></tr><tr><td><strong>可靠性</strong></td><td>高</td><td>高</td><td>可控</td></tr><tr><td><strong>灵活性</strong></td><td>低</td><td>中</td><td>高</td></tr><tr><td><strong>维护成本</strong></td><td>低</td><td>中</td><td>高</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 大多数情况使用 TCP</span>
<span class="line">   - KBEngine 默认选择</span>
<span class="line">   - 开发简单、调试方便</span>
<span class="line"></span>
<span class="line">2. 延迟敏感场景使用 KCP</span>
<span class="line">   - 实时战斗</span>
<span class="line">   - 状态同步</span>
<span class="line"></span>
<span class="line">3. 特殊需求自研可靠 UDP</span>
<span class="line">   - 有专门的协议团队</span>
<span class="line">   - 需要深度定制</span>
<span class="line">   - 长期投入维护</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/server/network/reliable_udp.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - reliable_udp.h</a></li><li><a href="https://github.com/skywind3000/kcp" target="_blank" rel="noopener noreferrer">KCP 协议</a></li><li><a href="https://en.wikipedia.org/wiki/TCP_congestion_control" target="_blank" rel="noopener noreferrer">TCP 拥塞控制</a></li><li><a href="https://tools.ietf.org/html/rfc2018" target="_blank" rel="noopener noreferrer">RFC 2018 - TCP Selective Acknowledgment Options</a></li></ul>`,34)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};