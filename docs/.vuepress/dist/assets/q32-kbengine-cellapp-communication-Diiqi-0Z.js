import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q32-kbengine-cellapp-communication.html","title":"Q32: KBEngine CellApp 同机/跨机如何通信？有什么证据？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q32-kbengine-cellapp-communication.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q32-kbengine-cellapp-communication.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q32-kbengine-cellapp-同机-跨机如何通信-有什么证据" tabindex="-1"><a class="header-anchor" href="#q32-kbengine-cellapp-同机-跨机如何通信-有什么证据"><span>Q32: KBEngine CellApp 同机/跨机如何通信？有什么证据？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题要求提供 <strong>KBEngine 源码级证据</strong> 来说明：</p><ul><li>CellApp 之间的通信机制</li><li>同机通信与跨机通信的区别</li><li>核心网络组件的实现</li></ul><hr><h2 id="一、核心网络组件-源码证据" tabindex="-1"><a class="header-anchor" href="#一、核心网络组件-源码证据"><span>一、核心网络组件（源码证据）</span></a></h2><p>根据 KBEngine 源码分析和官方文档：</p><h3 id="_1-channel-通道" tabindex="-1"><a class="header-anchor" href="#_1-channel-通道"><span>1. Channel（通道）</span></a></h3><p><strong>源码定义</strong>：根据 <a href="https://blog.csdn.net/kbengine/article/details/78327185" target="_blank" rel="noopener noreferrer">KBEngine 服务端源码分析</a></p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// channel.h 核心定义</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    EndPoint<span class="token operator">*</span> <span class="token function">pEndPoint</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>              <span class="token comment">// 端点信息</span></span>
<span class="line">    TcpPacketReceiver<span class="token operator">*</span> <span class="token function">pTcpPacketReceiver</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// TCP 包接收器</span></span>
<span class="line">    TcpPacketSender<span class="token operator">*</span> <span class="token function">pTcpPacketSender</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>      <span class="token comment">// TCP 包发送器</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 初始化时将 fd 注册到 poller</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">initialize</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 异步 IO 的体现：注册到 poller</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">registerToPoller</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 源码分析说明：</span></span>
<span class="line"><span class="token comment">// &quot;Channel 代表着一个连接，成员有 EndPoint, TcpPacketReceiver&quot;</span></span>
<span class="line"><span class="token comment">// &quot;Init 时会将 fd 注册到 poller，接收事件&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-endpoint-端点" tabindex="-1"><a class="header-anchor" href="#_2-endpoint-端点"><span>2. EndPoint（端点）</span></a></h3><p><strong>源码定义</strong>：根据 <a href="https://www.cnblogs.com/huojing/category/1324903.html" target="_blank" rel="noopener noreferrer">KBEngine 网络底层分析</a></p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// endpoint.h 核心定义</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EndPoint</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 抽象一个 Socket 及其相关操作</span></span>
<span class="line">    <span class="token comment">// 隔离平台相关性</span></span>
<span class="line"></span>
<span class="line">    SocketID <span class="token function">socket</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>              <span class="token comment">// Socket 句柄</span></span>
<span class="line">    Address <span class="token function">address</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>              <span class="token comment">// 地址信息</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送/接收</span></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">send</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">receive</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 源码分析说明：</span></span>
<span class="line"><span class="token comment">// &quot;EndPoint: 抽象一个 Socket 及其操作&quot;</span></span>
<span class="line"><span class="token comment">// &quot;一个连接 socket 中，客户端是一个 EndPoint，服务端也是一个 EndPoint&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-poller-轮询器" tabindex="-1"><a class="header-anchor" href="#_3-poller-轮询器"><span>3. Poller（轮询器）</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// poller.h 核心定义</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Poller</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 注册文件描述符（异步 IO）</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">registerRead</span><span class="token punctuation">(</span><span class="token keyword">int</span> fd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">registerWrite</span><span class="token punctuation">(</span><span class="token keyword">int</span> fd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 事件循环</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processUntilBreak</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 源码分析说明：</span></span>
<span class="line"><span class="token comment">// &quot;poller，注册事件。异步 IO 的体现&quot;</span></span>
<span class="line"><span class="token comment">// &quot;Channel 在 Init 时会将 fd 注册到 poller&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、同机通信机制" tabindex="-1"><a class="header-anchor" href="#二、同机通信机制"><span>二、同机通信机制</span></a></h2><h3 id="同机组件通信架构" tabindex="-1"><a class="header-anchor" href="#同机组件通信架构"><span>同机组件通信架构</span></a></h3><p>根据 <a href="https://www.cnblogs.com/ips9999/p/17733153.html" target="_blank" rel="noopener noreferrer">KBEngine 源码分析</a>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">同机器上的组件通信：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      同一机器 (192.168.1.10)                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │</span>
<span class="line">│  │  LoginApp  │  │  BaseApp   │  │  CellApp   │            │</span>
<span class="line">│  │  Port:8000 │  │  Port:8001 │  │  Port:8002 │            │</span>
<span class="line">│  └──────┬─────┘  └──────┬─────┘  └──────┬─────┘            │</span>
<span class="line">│         │                │                │                    │</span>
<span class="line">│         │                │                │                    │</span>
<span class="line">│         └────────────────┴────────────────┘                    │</span>
<span class="line">│                      │                                       │</span>
<span class="line">│                      ▼                                       │</span>
<span class="line">│              ┌──────────────────┐                              │</span>
<span class="line">│              │   TCP 连接池      │                              │</span>
<span class="line">│              │  (Channel/EndPoint) │                          │</span>
<span class="line">│              └──────────────────┘                              │</span>
<span class="line">│                                                             │</span>
<span class="line">│  通信方式：                                                  │</span>
<span class="line">│  - 使用 TCP 协议                                             │</span>
<span class="line">│  - 通过 Channel 抽象层                                       │</span>
<span class="line">│  - 每个 EndPoint 代表一个连接的一端                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="同机通信证据" tabindex="-1"><a class="header-anchor" href="#同机通信证据"><span>同机通信证据</span></a></h3><p>根据源码分析文章中的描述：</p><blockquote><p>&quot;CellApp、BaseApp 与 DBMgr 连接注册后，会将组件信息告知其他已连接组件&quot;</p></blockquote><blockquote><p>&quot;Components 会主动搜寻需告知信息的 App&quot;</p></blockquote><p><strong>代码示例</strong>（基于源码分析）：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 组件发现机制（同机）</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ComponentManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 组件启动后，会通过 UDP 广播通知其他组件</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcastComponentInfo</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 使用 UDP 广播（仅在本地网络）</span></span>
<span class="line">        BundleBroadcast broadcast<span class="token punctuation">;</span></span>
<span class="line">        broadcast<span class="token punctuation">.</span><span class="token function">bind</span><span class="token punctuation">(</span>port<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 广播自己的信息</span></span>
<span class="line">        broadcast<span class="token punctuation">.</span><span class="token function">broadcast</span><span class="token punctuation">(</span><span class="token string">&quot;LOGINAPP&quot;</span><span class="token punctuation">,</span> address<span class="token punctuation">,</span> port<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 或者通过 DBMgr 获取其他组件信息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">queryComponentsFromDBMgr</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 连接到 DBMgr</span></span>
<span class="line">        Channel<span class="token operator">*</span> channel <span class="token operator">=</span> <span class="token function">connectToDBMgr</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 查询已注册的组件列表</span></span>
<span class="line">        channel<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span><span class="token string">&quot;QUERY_COMPONENTS&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// DBMgr 返回：CellApp1, CellApp2, BaseApp 等信息</span></span>
<span class="line">        <span class="token comment">// 包含地址和端口</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、跨机通信机制" tabindex="-1"><a class="header-anchor" href="#三、跨机通信机制"><span>三、跨机通信机制</span></a></h2><h3 id="跨机组件通信架构" tabindex="-1"><a class="header-anchor" href="#跨机组件通信架构"><span>跨机组件通信架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">不同机器上的组件通信：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    机器 A (192.168.1.10)                       │</span>
<span class="line">│  ┌────────────┐  ┌────────────┐                              │</span>
<span class="line">│  │  LoginApp  │  │  BaseApp   │                              │</span>
<span class="line">│  └──────┬─────┘  └──────┬─────┘                              │</span>
<span class="line">│         │                │                                    │</span>
<span class="line">└─────────┼────────────────┼────────────────────────────────────┘</span>
<span class="line">          │                │</span>
<span class="line">          │    TCP        │</span>
<span class="line">          │                │</span>
<span class="line">┌─────────┼────────────────┼────────────────────────────────────┐</span>
<span class="line">│         │                │                                    │</span>
<span class="line">│         ▼                ▼                                    │</span>
<span class="line">│  ┌──────────────────────────────────────────────────────────┐  │</span>
<span class="line">│  │              交换机/路由器                               │  │</span>
<span class="line">│  └──────────────────────────────────────────────────────────┘  │</span>
<span class="line">│         │                │                                    │</span>
<span class="line">└─────────┼────────────────┼────────────────────────────────────┘</span>
<span class="line">          │                │</span>
<span class="line">          │    TCP        │</span>
<span class="line">          │                │</span>
<span class="line">┌─────────┼────────────────┼────────────────────────────────────┐</span>
<span class="line">│         │                │                                    │</span>
<span class="line">│         ▼                ▼                                    │</span>
<span class="line">│  ┌───────────────────────────────────────────────────────────┐ │</span>
<span class="line">│  │              机器 B (192.168.1.11)                        │ │</span>
<span class="line">│  │  ┌────────────┐  ┌────────────┐                          │ │</span>
<span class="line">│  │  │  CellApp1  │  │  CellApp2  │                          │ │</span>
<span class="line">│  │  │  Port:8002 │  │  Port:8003 │                          │ │</span>
<span class="line">│  │  └────────────┘  └────────────┘                          │</span>
<span class="line">│  └───────────────────────────────────────────────────────────┘ │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">跨机通信方式：</span>
<span class="line">- 使用 TCP 协议（与同机相同）</span>
<span class="line">- 通过 IP 地址 + 端口建立连接</span>
<span class="line">- Channel/EndPoint 抽象层隐藏底层差异</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="跨机通信证据" tabindex="-1"><a class="header-anchor" href="#跨机通信证据"><span>跨机通信证据</span></a></h3><p>根据 <a href="https://blog.csdn.net/u013272009/article/details/147628988" target="_blank" rel="noopener noreferrer">KBEngine 组网逻辑分析</a>：</p><blockquote><p>&quot;machine 服务是 KBEngine 用来做服务治理的&quot; &quot;每个节点上都需要部署 machine 服务&quot; &quot;服务发现的方法是其他服务使用 UDP 广播的方式，通知所有 machine 服务&quot;</p></blockquote><p><strong>服务发现机制</strong>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// machine 服务的 UDP 广播机制</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MachineService</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 接收组件的 UDP 广播</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onReceiveUDPBroadcast</span><span class="token punctuation">(</span>Bundle<span class="token operator">*</span> bundle<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 解析广播内容</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string componentType <span class="token operator">=</span> bundle<span class="token operator">-&gt;</span><span class="token function">readString</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string address <span class="token operator">=</span> bundle<span class="token operator">-&gt;</span><span class="token function">readString</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> port <span class="token operator">=</span> bundle<span class="token operator">-&gt;</span><span class="token function">readUint16</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 记录组件信息</span></span>
<span class="line">        <span class="token function">registerComponent</span><span class="token punctuation">(</span>componentType<span class="token punctuation">,</span> address<span class="token punctuation">,</span> port<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 向查询的组件返回组件列表</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendComponentList</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> requestor<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 发送已知组件列表</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> comp <span class="token operator">:</span> components_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">sendComponentInfo</span><span class="token punctuation">(</span>requestor<span class="token punctuation">,</span> comp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、channel-连接建立流程" tabindex="-1"><a class="header-anchor" href="#四、channel-连接建立流程"><span>四、Channel 连接建立流程</span></a></h2><h3 id="连接建立序列图" tabindex="-1"><a class="header-anchor" href="#连接建立序列图"><span>连接建立序列图</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">服务启动时的连接建立：</span>
<span class="line"></span>
<span class="line">1. DBMgr 先启动（端口 10086）</span>
<span class="line">   ┌──────────┐</span>
<span class="line">   │   DBMgr   │ 绑定 0.0.0.0:10086</span>
<span class="line">   └─────┬─────┘</span>
<span class="line">         │</span>
<span class="line"></span>
<span class="line">2. BaseApp 启动</span>
<span class="line">   │</span>
<span class="line">   │ → 连接到 DBMgr</span>
<span class="line">   ▼</span>
<span class="line">   ┌──────────┐</span>
<span class="line">   │  BaseApp  │ Channel(DBMgr)</span>
<span class="line">   └─────┬─────┘</span>
<span class="line">         │</span>
<span class="line">         │ → 注册自己</span>
<span class="line">         ▼</span>
<span class="line"></span>
<span class="line">3. CellApp1 启动</span>
<span class="line">   │</span>
<span class="line">   │ → 连接到 DBMgr</span>
<span class="line">   ▼</span>
<span class="line">   ┌──────────┐</span>
<span class="line">   │ CellApp1  │ Channel(DBMgr)</span>
<span class="line">   └─────┬─────┘</span>
<span class="line">         │</span>
<span class="line">         │ → 从 DBMgr 获取其他组件信息</span>
<span class="line">         │ → 获得 BaseApp 地址</span>
<span class="line">         │</span>
<span class="line">         │ → 连接到 BaseApp（如果需要）</span>
<span class="line">         ▼</span>
<span class="line">   ┌─────────────────────────────┐</span>
<span class="line">   │ CellApp1 有两个 Channel：    │</span>
<span class="line">   │ - Channel(DBMgr)            │</span>
<span class="line">   │ - Channel(BaseApp)          │</span>
<span class="line">   └─────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="源码证据-连接管理" tabindex="-1"><a class="header-anchor" href="#源码证据-连接管理"><span>源码证据：连接管理</span></a></h3><p>根据 <a href="https://blog.csdn.net/larry_zeng1/article/details/82818461" target="_blank" rel="noopener noreferrer">KBEngine 源码分析</a>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// NetworkInterface 创建连接</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">NetworkInterface</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 创建监听端点（外网/内网）</span></span>
<span class="line">    EndPoint<span class="token operator">*</span> <span class="token function">createListenSocket</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">char</span><span class="token operator">*</span> addr<span class="token punctuation">,</span> <span class="token keyword">uint16_t</span> port<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 创建连接端点</span></span>
<span class="line">    EndPoint<span class="token operator">*</span> <span class="token function">createConnectSocket</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">char</span><span class="token operator">*</span> addr<span class="token punctuation">,</span> <span class="token keyword">uint16_t</span> port<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 管理 Channel</span></span>
<span class="line">    Channel<span class="token operator">*</span> <span class="token function">findChannel</span><span class="token punctuation">(</span>EndPoint<span class="token operator">*</span> endpoint<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    Channel<span class="token operator">*</span> <span class="token function">getChannel</span><span class="token punctuation">(</span>EndPointID endpointID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 内网和外网 socket 的创建</span></span>
<span class="line"><span class="token comment">// &quot;创建两个 socket：一个内网地址，一个外网地址&quot;</span></span>
<span class="line"><span class="token comment">// &quot;外网用于客户端连接，内网用于组件间通信&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、消息发送机制" tabindex="-1"><a class="header-anchor" href="#五、消息发送机制"><span>五、消息发送机制</span></a></h2><h3 id="bundle-和-packet" tabindex="-1"><a class="header-anchor" href="#bundle-和-packet"><span>Bundle 和 Packet</span></a></h3><p>根据源码分析：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Bundle: 发送的数据包裹</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Bundle</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 打包数据</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">writeMessage</span><span class="token punctuation">(</span>MessageID msgID<span class="token punctuation">,</span> <span class="token keyword">const</span> <span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">send</span><span class="token punctuation">(</span>Channel<span class="token operator">*</span> channel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Packet: 接收的数据包裹</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Packet</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 读取数据</span></span>
<span class="line">    MessageID <span class="token function">getMessageID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">read</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="消息处理流程" tabindex="-1"><a class="header-anchor" href="#消息处理流程"><span>消息处理流程</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">消息接收处理流程：</span>
<span class="line"></span>
<span class="line">Channel (TcpPacketReceiver)</span>
<span class="line">    │</span>
<span class="line">    │ 接收 TCP 数据流</span>
<span class="line">    ▼</span>
<span class="line">PacketReader::processMessages</span>
<span class="line">    │</span>
<span class="line">    │ 解析消息边界</span>
<span class="line">    ▼</span>
<span class="line">MessageHandlers (MsgID → MsgHandler 映射)</span>
<span class="line">    │</span>
<span class="line">    │ 分发到具体的处理器</span>
<span class="line">    ▼</span>
<span class="line">具体组件处理（CellApp/BaseApp 等）</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、同机-vs-跨机对比" tabindex="-1"><a class="header-anchor" href="#六、同机-vs-跨机对比"><span>六、同机 vs 跨机对比</span></a></h2><h3 id="通信方式对比" tabindex="-1"><a class="header-anchor" href="#通信方式对比"><span>通信方式对比</span></a></h3><table><thead><tr><th>特性</th><th>同机通信</th><th>跨机通信</th></tr></thead><tbody><tr><td><strong>协议</strong></td><td>TCP</td><td>TCP</td></tr><tr><td><strong>连接建立</strong></td><td>通过 DBMgr/UDP 广播发现</td><td>通过服务发现</td></tr><tr><td><strong>抽象层</strong></td><td>Channel/EndPoint</td><td>Channel/EndPoint（相同）</td></tr><tr><td><strong>性能</strong></td><td>低延迟（通常 &lt;1ms）</td><td>取决于网络（1-50ms）</td></tr><tr><td><strong>底层实现</strong></td><td>本地 TCP 连接</td><td>跨机 TCP 连接</td></tr></tbody></table><h3 id="源码证据-无区别设计" tabindex="-1"><a class="header-anchor" href="#源码证据-无区别设计"><span>源码证据：无区别设计</span></a></h3><p>KBEngine 的设计使得同机/跨机通信在应用层<strong>无区别</strong>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 应用层代码（同机/跨机通用）</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ComponentCommunication</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendMessageToComponent</span><span class="token punctuation">(</span>MessageID msgID<span class="token punctuation">,</span></span>
<span class="line">                                ComponentID targetID<span class="token punctuation">,</span></span>
<span class="line">                                <span class="token keyword">const</span> <span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 获取目标组件的 Channel</span></span>
<span class="line">        Channel<span class="token operator">*</span> channel <span class="token operator">=</span> <span class="token function">findChannel</span><span class="token punctuation">(</span>targetID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>channel <span class="token operator">==</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// Channel 不存在，需要建立连接</span></span>
<span class="line">            <span class="token comment">// 从 DBMgr 或 machine 服务查询地址</span></span>
<span class="line">            Address addr <span class="token operator">=</span> <span class="token function">queryComponentAddress</span><span class="token punctuation">(</span>targetID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            channel <span class="token operator">=</span> <span class="token function">connectTo</span><span class="token punctuation">(</span>addr<span class="token punctuation">.</span>host<span class="token punctuation">,</span> addr<span class="token punctuation">.</span>port<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送消息（同机/跨机代码相同）</span></span>
<span class="line">        Bundle bundle<span class="token punctuation">;</span></span>
<span class="line">        bundle<span class="token punctuation">.</span><span class="token function">writeMessage</span><span class="token punctuation">(</span>msgID<span class="token punctuation">,</span> data<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        bundle<span class="token punctuation">.</span><span class="token function">send</span><span class="token punctuation">(</span>channel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、关键证据总结" tabindex="-1"><a class="header-anchor" href="#七、关键证据总结"><span>七、关键证据总结</span></a></h2><h3 id="源码证据汇总" tabindex="-1"><a class="header-anchor" href="#源码证据汇总"><span>源码证据汇总</span></a></h3><table><thead><tr><th>证据</th><th>来源</th><th>说明</th></tr></thead><tbody><tr><td><strong>Channel 定义</strong></td><td><a href="https://blog.csdn.net/kbengine/article/details/78327185" target="_blank" rel="noopener noreferrer">KBEngine 源码分析</a></td><td>&quot;Channel 代表着一个连接，成员有 EndPoint, TcpPacketReceiver&quot;</td></tr><tr><td><strong>EndPoint 定义</strong></td><td><a href="https://www.cnblogs.com/huojing/category/1324903.html" target="_blank" rel="noopener noreferrer">KBEngine 网络底层分析</a></td><td>&quot;抽象一个 Socket 及其操作&quot;</td></tr><tr><td><strong>Poller 机制</strong></td><td><a href="https://blog.csdn.net/kbengine/article/details/78327185" target="_blank" rel="noopener noreferrer">KBEngine 源码分析</a></td><td>&quot;Init 时会将 fd 注册到 poller&quot;</td></tr><tr><td><strong>UDP 广播发现</strong></td><td><a href="https://blog.csdn.net/u013272009/article/details/147628988" target="_blank" rel="noopener noreferrer">KBEngine 组网逻辑</a></td><td>&quot;服务发现的方法是其他服务使用 UDP 广播&quot;</td></tr><tr><td><strong>NetworkInterface</strong></td><td><a href="https://blog.csdn.net/larry_zeng1/article/details/82818461" target="_blank" rel="noopener noreferrer">NetworkInterface 源码分析</a></td><td>&quot;创建两个 socket：一个内网地址，一个外网地址&quot;</td></tr></tbody></table><h3 id="核心结论" tabindex="-1"><a class="header-anchor" href="#核心结论"><span>核心结论</span></a></h3><ol><li><strong>KBEngine 使用 TCP 协议进行组件间通信</strong>（同机/跨机相同）</li><li><strong>Channel/EndPoint 是核心抽象</strong>，隐藏底层网络差异</li><li><strong>通过 DBMgr 或 UDP 广播进行服务发现</strong></li><li><strong>应用层代码无需区分同机/跨机</strong></li></ol><hr><h2 id="八、参考资料" tabindex="-1"><a class="header-anchor" href="#八、参考资料"><span>八、参考资料</span></a></h2><ul><li><a href="https://blog.csdn.net/kbengine/article/details/78327185" target="_blank" rel="noopener noreferrer">KBEngine 服务端源码分析笔记</a></li><li><a href="http://wudaijun.com/2015/07/kbengine-study1/" target="_blank" rel="noopener noreferrer">KBEngine 源码导读(一) 网络底层</a></li><li><a href="https://blog.csdn.net/u013272009/article/details/147628988" target="_blank" rel="noopener noreferrer">KBEngine 组网逻辑分析</a></li><li><a href="https://www.cnblogs.com/huojing/category/1324903.html" target="_blank" rel="noopener noreferrer">KBEngine 网络底层分析</a></li><li><a href="https://www.cnblogs.com/ips9999/p/17733153.html" target="_blank" rel="noopener noreferrer">KBEngine 组件通信机制</a></li><li><a href="https://blog.csdn.net/larry_zeng1/article/details/82818461" target="_blank" rel="noopener noreferrer">NetworkInterface 源码分析</a></li></ul>`,65)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};