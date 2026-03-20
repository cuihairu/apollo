import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q7-process-architecture.html","title":"Q7: 进程内架构 vs 多进程架构，各有什么优劣？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q7-process-architecture.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q7-process-architecture.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q7-进程内架构-vs-多进程架构-各有什么优劣" tabindex="-1"><a class="header-anchor" href="#q7-进程内架构-vs-多进程架构-各有什么优劣"><span>Q7: 进程内架构 vs 多进程架构，各有什么优劣？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对进程架构设计的理解：</p><ul><li>进程内架构（单进程多线程）vs 多进程架构</li><li>KBEngine 为什么选择多进程架构</li><li>两种架构在游戏服务器中的适用场景</li><li>各自的优势和劣势</li></ul><hr><h2 id="一、架构定义" tabindex="-1"><a class="header-anchor" href="#一、架构定义"><span>一、架构定义</span></a></h2><h3 id="_1-1-进程内架构-single-process-multi-thread" tabindex="-1"><a class="header-anchor" href="#_1-1-进程内架构-single-process-multi-thread"><span>1.1 进程内架构（Single-Process Multi-Thread）</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    进程内架构                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │              单一进程                            │       │</span>
<span class="line">│   │  ┌───────────────────────────────────────────┐  │       │</span>
<span class="line">│   │  │  主线程  │ 网络线程 │ 逻辑线程 │ DB线程  │  │       │</span>
<span class="line">│   │  │   │        │         │         │        │  │       │</span>
<span class="line">│   │  │   └────────┴─────────┴─────────┘        │  │       │</span>
<span class="line">│   │  │            共享内存空间                   │  │       │</span>
<span class="line">│   │  └───────────────────────────────────────────┘  │       │</span>
<span class="line">│   └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                        │                                     │</span>
<span class="line">│                        ▼                                     │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │              共享资源                            │       │</span>
<span class="line">│   │  - 全局对象池                                      │       │</span>
<span class="line">│   │  - 统一内存管理                                    │       │</span>
<span class="line">│   │  - 锁保护的共享数据                               │       │</span>
<span class="line">│   └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">特点：</span>
<span class="line">- 所有线程共享同一内存空间</span>
<span class="line">- 线程间通信成本低（共享内存）</span>
<span class="line">- 需要处理锁竞争和线程安全</span>
<span class="line">- 一个线程崩溃可能导致整个进程崩溃</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-多进程架构-multi-process" tabindex="-1"><a class="header-anchor" href="#_1-2-多进程架构-multi-process"><span>1.2 多进程架构（Multi-Process）</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    KBEngine 多进程架构                       │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │</span>
<span class="line">│   │  Loginapp   │  │  Baseapp    │  │  Cellapp    │        │</span>
<span class="line">│   │  进程 1     │  │  进程 2     │  │  进程 3     │        │</span>
<span class="line">│   │  ┌───────┐  │  │  ┌───────┐  │  │  ┌───────┐  │        │</span>
<span class="line">│   │  │线程池 │  │  │  │线程池 │  │  │  │线程池 │  │        │</span>
<span class="line">│   │  └───────┘  │  │  └───────┘  │  │  └───────┘  │        │</span>
<span class="line">│   └──────┬──────┘  └──────┬──────┘  └──────┬──────┘        │</span>
<span class="line">│          │                │                │                │</span>
<span class="line">│          └────────────────┼────────────────┘                │</span>
<span class="line">│                          │                                 │</span>
<span class="line">│                          ▼                                 │</span>
<span class="line">│                   ┌─────────────┐                          │</span>
<span class="line">│                   │    DBMgr    │                          │</span>
<span class="line">│                   │  进程 4      │                          │</span>
<span class="line">│                   └──────┬──────┘                          │</span>
<span class="line">│                          │                                 │</span>
<span class="line">│                          ▼                                 │</span>
<span class="line">│                   ┌─────────────┐                          │</span>
<span class="line">│                   │  Database   │                          │</span>
<span class="line">│                   └─────────────┘                          │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">特点：</span>
<span class="line">- 每个进程独立内存空间</span>
<span class="line">- 进程间通过 IPC（TCP/共享内存）通信</span>
<span class="line">- 进程隔离，单个崩溃不影响其他</span>
<span class="line">- 部署和运维更复杂</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、kbengine-的多进程设计" tabindex="-1"><a class="header-anchor" href="#二、kbengine-的多进程设计"><span>二、KBEngine 的多进程设计</span></a></h2><h3 id="_2-1-进程架构图" tabindex="-1"><a class="header-anchor" href="#_2-1-进程架构图"><span>2.1 进程架构图</span></a></h3><p>根据 <a href="https://www.kbelab.com/manual/engine-overview.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 引擎概览</a>：</p>`,14),i(d,{code:`eJxLy8kvT85ILCpRCHHiUgCC4tKk9KLEggwF38TkjMy81Gil550rn09oezan92nXwqczVyjFgtWhqPXJT8/MCyjKTy4GKp+5++neqS/2z36+ovv57hYk5SDgYxgNVpxYUKCgbIgmZ4QkZ4SQS81L4cK00ymxOBVq5dP5u54vbMBlpZNvNEgt0FTf9CI0KUdDmByGa5wcjRBySK6ByBkj5IwJudQ5NScHFjgrd72cvgWXS519o0FqsbnUGehSqBzQpTZJRfp2T3bsfjq/D2IiumIjhGIjsOKX7X1Pl0zDrtgYodgYrPhp5/pnc9agK8buOZfEkkSo555N3fCsdx0uz7n4Rrs4ofgLZCCKwT6GCmrAVKCgq2sHjDWwEDCGgGLAuACTxvikbEBywHACcoEBACaNwQrRxMBmOOM23sUX4priksqcVKBlCmmZOTlWymlpqclJEAMhMs44ZVzgMhZJSSmWXAAR9QSr`}),o[1]||=e(`<h3 id="_2-2-各进程的线程模型" tabindex="-1"><a class="header-anchor" href="#_2-2-各进程的线程模型"><span>2.2 各进程的线程模型</span></a></h3><table><thead><tr><th>进程</th><th>线程模型</th><th>说明</th></tr></thead><tbody><tr><td><strong>Loginapp</strong></td><td>主线程 + 网络线程</td><td>处理登录请求，负载轻</td></tr><tr><td><strong>Baseapp</strong></td><td>主线程 + 网络线程 + DB线程</td><td>Proxy管理，数据缓存</td></tr><tr><td><strong>Cellapp</strong></td><td>主线程 + 网络线程 + 逻辑线程</td><td>游戏逻辑，AOI计算</td></tr><tr><td><strong>DBMgr</strong></td><td>主线程 + 工作线程池</td><td>数据库连接池管理</td></tr></tbody></table><h3 id="_2-3-进程间通信方式" tabindex="-1"><a class="header-anchor" href="#_2-3-进程间通信方式"><span>2.3 进程间通信方式</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 使用 TCP 进行进程间通信</span></span>
<span class="line"><span class="token comment">// src/server/network/bundle.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Bundle</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> Mercury<span class="token double-colon punctuation">::</span><span class="token class-name">Bundle</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 消息打包器</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">newMessage</span><span class="token punctuation">(</span>MessageID msgID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送到指定进程</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">send</span><span class="token punctuation">(</span>Channel<span class="token operator">*</span> pChannel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 内部使用 TCP 长连接</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// src/server/network/channel.h</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 通道类型</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">ChannelType</span> <span class="token punctuation">{</span></span>
<span class="line">        CHANNEL_TCP <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span>      <span class="token comment">// TCP 连接</span></span>
<span class="line">        CHANNEL_UDP <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span>      <span class="token comment">// UDP 连接（内部未使用）</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 唯一标识</span></span>
<span class="line">    Components<span class="token double-colon punctuation">::</span>COMPONENT_TYPE <span class="token function">type</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span><span class="token punctuation">;</span></span>
<span class="line">    COMPONENT_ID <span class="token function">componentID</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、对比分析" tabindex="-1"><a class="header-anchor" href="#三、对比分析"><span>三、对比分析</span></a></h2><h3 id="_3-1-详细对比表" tabindex="-1"><a class="header-anchor" href="#_3-1-详细对比表"><span>3.1 详细对比表</span></a></h3><table><thead><tr><th>维度</th><th>进程内架构</th><th>多进程架构 (KBEngine)</th></tr></thead><tbody><tr><td><strong>内存隔离</strong></td><td>❌ 共享内存，风险高</td><td>✅ 独立内存，隔离好</td></tr><tr><td><strong>故障隔离</strong></td><td>❌ 线程崩溃可能影响进程</td><td>✅ 进程崩溃不影响其他</td></tr><tr><td><strong>通信成本</strong></td><td>✅ 低（共享内存+锁）</td><td>⚠️ 中（序列化+TCP）</td></tr><tr><td><strong>并发能力</strong></td><td>⚠️ 受 GIL/锁限制</td><td>✅ 真正并行</td></tr><tr><td><strong>部署复杂度</strong></td><td>✅ 低（单进程）</td><td>⚠️ 中高（多进程协调）</td></tr><tr><td><strong>调试难度</strong></td><td>⚠️ 中（线程调试）</td><td>⚠️ 中高（跨进程调试）</td></tr><tr><td><strong>资源利用</strong></td><td>⚠️ 单机资源上限</td><td>✅ 可跨机器分布</td></tr><tr><td><strong>扩展性</strong></td><td>❌ 垂直扩展</td><td>✅ 水平扩展</td></tr><tr><td><strong>热更新</strong></td><td>❌ 需重启整个服务</td><td>✅ 可逐进程重启</td></tr><tr><td><strong>监控运维</strong></td><td>✅ 简单</td><td>⚠️ 复杂</td></tr></tbody></table><h3 id="_3-2-性能对比" tabindex="-1"><a class="header-anchor" href="#_3-2-性能对比"><span>3.2 性能对比</span></a></h3><h4 id="内存访问" tabindex="-1"><a class="header-anchor" href="#内存访问"><span>内存访问</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">进程内（共享内存）：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  Thread A                                    Thread B        │</span>
<span class="line">│      │                                           │           │</span>
<span class="line">│      │  ┌─────────────────────────────────┐        │           │</span>
<span class="line">│      ├─►│         Shared Memory           │◄───────┤           │</span>
<span class="line">│      │  │  ┌─────────────────────────┐    │        │           │</span>
<span class="line">│      │  │  │    Entity Data          │    │        │           │</span>
<span class="line">│      │  │  └─────────────────────────┘    │        │           │</span>
<span class="line">│      │  └─────────────────────────────────┘        │           │</span>
<span class="line">│      │                                           │           │</span>
<span class="line">│  读/写 ~10ns                                     读/写 ~10ns   │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">多进程（IPC）：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  Process A                              Process B            │</span>
<span class="line">│      │                                        │             │</span>
<span class="line">│      │  序列化                    序列化      │             │</span>
<span class="line">│      ├──────► TCP ──────────────────────────►┤             │</span>
<span class="line">│      │                                        │             │</span>
<span class="line">│   ~100μs                                   ~100μs            │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="并行计算" tabindex="-1"><a class="header-anchor" href="#并行计算"><span>并行计算</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">进程内（多线程）：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    CPU 利用率                                │</span>
<span class="line">│  ████████████████████████████████ 95%                       │</span>
<span class="line">│  问题：GIL（Python）或锁竞争限制并行度                      │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">多进程：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    CPU 利用率（多核）                        │</span>
<span class="line">│  Process 1: ████████████████████████░░ 80%  (Core 1)       │</span>
<span class="line">│  Process 2: ██████████████████████████ 85%  (Core 2)       │</span>
<span class="line">│  Process 3: ████████████████████░░░░░░ 70%  (Core 3)       │</span>
<span class="line">│  Process 4: ██████████████████████░░░░ 75%  (Core 4)       │</span>
<span class="line">│  优势：真正并行，充分利用多核                               │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-故障场景对比" tabindex="-1"><a class="header-anchor" href="#_3-3-故障场景对比"><span>3.3 故障场景对比</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">场景：游戏逻辑出现 Bug 导致崩溃</span>
<span class="line"></span>
<span class="line">进程内架构：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │              单一进程                            │       │</span>
<span class="line">│   │  ┌───────────────────────────────────────────┐  │       │</span>
<span class="line">│   │  │  登录 │ 聊天 │ 战斗 │ 数据 │ AI │ ...  │  │       │</span>
<span class="line">│   │  └───────────────────────────────────────────┘  │       │</span>
<span class="line">│   │                        │                        │       │</span>
<span class="line">│   │                        ▼                        │       │</span>
<span class="line">│   │                   战斗逻辑崩溃 ❌                │       │</span>
<span class="line">│   │                        │                        │       │</span>
<span class="line">│   │                        ▼                        │       │</span>
<span class="line">│   │              整个进程崩溃 ❌❌❌                  │       │</span>
<span class="line">│   │              所有玩家掉线！                      │       │</span>
<span class="line">│   └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">多进程架构 (KBEngine)：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌──────────┐  ┌──────────┐  ┌──────────┐                │</span>
<span class="line">│   │Loginapp  │  │ Baseapp  │  │ Cellapp  │                │</span>
<span class="line">│   │   ✓      │  │    ✓     │  │    ✗     │                │</span>
<span class="line">│   └──────────┘  └──────────┘  └──────────┘                │</span>
<span class="line">│       │              │              │                      │</span>
<span class="line">│       │              │              ▼                      │</span>
<span class="line">│       │              │         Cellapp 崩溃                  │</span>
<span class="line">│       │              │              │                      │</span>
<span class="line">│       │              │         其他组件不受影响               │</span>
<span class="line">│       │              │         CellappMgr 自动重启          │</span>
<span class="line">│       │              │              │                      │</span>
<span class="line">│       ▼              ▼              ▼                      │</span>
<span class="line">│   玩家可以        玩家数据        受影响的                   │</span>
<span class="line">│   正常登录        正常保存         空间玩家                   │</span>
<span class="line">│                  掉线重连                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-为什么选择多进程" tabindex="-1"><a class="header-anchor" href="#四、kbengine-为什么选择多进程"><span>四、KBEngine 为什么选择多进程？</span></a></h2><h3 id="_4-1-设计理念" tabindex="-1"><a class="header-anchor" href="#_4-1-设计理念"><span>4.1 设计理念</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine 源码分析</a>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 的核心设计思想：</span></span>
<span class="line"><span class="token comment">// 1. 进程隔离 - 单个组件崩溃不影响整体</span></span>
<span class="line"><span class="token comment">// 2. 动态负载均衡 - 可根据负载动态调整进程数</span></span>
<span class="line"><span class="token comment">// 3. 分布式部署 - 可跨多台机器部署</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// src/server/components.h</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">COMPONENT_TYPE</span> <span class="token punctuation">{</span></span>
<span class="line">    UNKNOWN_COMPONENT_TYPE <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">    LOGINAPP_TYPE<span class="token punctuation">,</span>          <span class="token comment">// 登录服务</span></span>
<span class="line">    BASEAPP_TYPE<span class="token punctuation">,</span>           <span class="token comment">// 基础应用</span></span>
<span class="line">    CELLAPP_TYPE<span class="token punctuation">,</span>           <span class="token comment">// 空间应用</span></span>
<span class="line">    BASEAPPMGR_TYPE<span class="token punctuation">,</span>        <span class="token comment">// 基础应用管理器</span></span>
<span class="line">    CELLAPPMGR_TYPE<span class="token punctuation">,</span>        <span class="token comment">// 空间应用管理器</span></span>
<span class="line">    DBMGR_TYPE<span class="token punctuation">,</span>             <span class="token comment">// 数据库管理器</span></span>
<span class="line">    <span class="token comment">// ...</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-多进程的优势" tabindex="-1"><a class="header-anchor" href="#_4-2-多进程的优势"><span>4.2 多进程的优势</span></a></h3><h4 id="_1-故障隔离" tabindex="-1"><a class="header-anchor" href="#_1-故障隔离"><span>1. 故障隔离</span></a></h4><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 中的自动恢复机制</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># scripts/kbe_scripts/base/baseapp.py</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Baseapp</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Base<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onMgrRecoverBaseapp</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> deadBaseappID<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        当某个 Baseapp 崩溃时，其他 Baseapp 接管其数据</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>info<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Baseapp </span><span class="token interpolation"><span class="token punctuation">{</span>deadBaseappID<span class="token punctuation">}</span></span><span class="token string"> 崩溃，开始恢复数据&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 加载崩溃 Baseapp 的备份数据</span></span>
<span class="line">        self<span class="token punctuation">.</span>loadBackupData<span class="token punctuation">(</span>deadBaseappID<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 重新分配受影响的玩家</span></span>
<span class="line">        self<span class="token punctuation">.</span>reassignPlayers<span class="token punctuation">(</span>deadBaseappID<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="_2-动态扩展" tabindex="-1"><a class="header-anchor" href="#_2-动态扩展"><span>2. 动态扩展</span></a></h4><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 支持动态添加进程</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 1. 启动新的 Cellapp</span></span>
<span class="line">$ cellapp</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. Cellapp 自动注册到 CellappMgr</span></span>
<span class="line"><span class="token comment"># 3. CellappMgr 开始将新实体分配到新的 Cellapp</span></span>
<span class="line"><span class="token comment"># 4. 无需停机，无缝扩展</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="_3-资源隔离" tabindex="-1"><a class="header-anchor" href="#_3-资源隔离"><span>3. 资源隔离</span></a></h4><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 每个 Cellapp 可以独立设置资源限制</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// cellapp.xml</span></span>
<span class="line"><span class="token operator">&lt;</span>Cellapp<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span><span class="token operator">!</span><span class="token operator">--</span> 单个 Cellapp 最大实体数 <span class="token operator">--</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>maxEntities<span class="token operator">&gt;</span><span class="token number">5000</span><span class="token operator">&lt;</span><span class="token operator">/</span>maxEntities<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span><span class="token operator">!</span><span class="token operator">--</span> 内存使用限制 <span class="token operator">--</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>maxMemoryUsage<span class="token operator">&gt;</span><span class="token number">2</span>GB<span class="token operator">&lt;</span><span class="token operator">/</span>maxMemoryUsage<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span><span class="token operator">!</span><span class="token operator">--</span> CPU 使用限制 <span class="token operator">--</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>cpuAffinity<span class="token operator">&gt;</span><span class="token number">1</span><span class="token operator">&lt;</span><span class="token operator">/</span>cpuAffinity<span class="token operator">&gt;</span>  <span class="token operator">&lt;</span><span class="token operator">!</span><span class="token operator">--</span> 绑定到 CPU 核心 <span class="token number">1</span> <span class="token operator">--</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>Cellapp<span class="token operator">&gt;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-多进程的代价" tabindex="-1"><a class="header-anchor" href="#_4-3-多进程的代价"><span>4.3 多进程的代价</span></a></h3><h4 id="_1-通信开销" tabindex="-1"><a class="header-anchor" href="#_1-通信开销"><span>1. 通信开销</span></a></h4><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 进程内：直接访问</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">updateHP</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> <span class="token keyword">int</span> hp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    entity<span class="token operator">-&gt;</span>hp <span class="token operator">=</span> hp<span class="token punctuation">;</span>  <span class="token comment">// ~10ns</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 多进程：网络传输</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">updateHP</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">,</span> <span class="token keyword">int</span> hp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line">    stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>hp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 序列化 + TCP 传输 + 反序列化</span></span>
<span class="line">    channel<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>stream<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> stream<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// ~100μs</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 开销对比：100μs / 10ns = 10,000 倍</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="_2-部署复杂度" tabindex="-1"><a class="header-anchor" href="#_2-部署复杂度"><span>2. 部署复杂度</span></a></h4><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 进程内：单个可执行文件</span></span>
<span class="line">$ ./game_server</span>
<span class="line"></span>
<span class="line"><span class="token comment"># KBEngine：多个进程</span></span>
<span class="line">$ machine</span>
<span class="line">$ loginapp</span>
<span class="line">$ baseappmgr</span>
<span class="line">$ baseapp</span>
<span class="line">$ baseapp</span>
<span class="line">$ cellappmgr</span>
<span class="line">$ cellapp</span>
<span class="line">$ cellapp</span>
<span class="line">$ cellapp</span>
<span class="line">$ dbmgr</span>
<span class="line"><span class="token comment"># ... 需要按顺序启动，监控各自状态</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、选择建议" tabindex="-1"><a class="header-anchor" href="#五、选择建议"><span>五、选择建议</span></a></h2><h3 id="_5-1-决策流程图" tabindex="-1"><a class="header-anchor" href="#_5-1-决策流程图"><span>5.1 决策流程图</span></a></h3>`,35),i(d,{code:`eJxtj09LAkEYxu9+ioGOIYsSkR4KRCsISbPb4iEn1xaWDWwjxA08VGjjph3sn7JlCdqfSxeVDPwyzYx+i8YZXVZwTs/wPr/3eV5FOz6DRwdZA+yHPYC9hME+Mv4t4DYaF0oEvSeB17sO4r486fdJsUK/B9hGG+cebo/7JlMzGt3di21Jm7GEhFvtv/4drZVNED3VDFXGrafRsE47iNhdYl8k3Ry2mrRUlsgbYmslXLdxtYiLXRPE/Xlcfx0/PNNBZfxRduL8HCONEmlY+LqJHzuTqpVbYTPBdi6VVQ9l0uuxTXOBU/L+ZWZNqHpGS8uiG766nLm5nVfnh3MV0dnOm87IqgbBTiiiZ1Q9DRYeJgpwUkg3iq2aIMAyoOiLfiIQDglOtOGckAs5Fkl/hkxMa54YOQbNKgJF1bTgElxLr8KAa+4UmRoURQnAFZfBSXQMEKag5x/JQ/I+`}),o[2]||=e(`<h3 id="_5-2-场景建议表" tabindex="-1"><a class="header-anchor" href="#_5-2-场景建议表"><span>5.2 场景建议表</span></a></h3><table><thead><tr><th>场景</th><th>推荐架构</th><th>原因</th></tr></thead><tbody><tr><td><strong>卡牌游戏</strong></td><td>进程内</td><td>逻辑简单，无需复杂架构</td></tr><tr><td><strong>棋类游戏</strong></td><td>进程内</td><td>状态少，计算轻</td></tr><tr><td><strong>回合制 RPG</strong></td><td>混合</td><td>单进程 + 独立 DB</td></tr><tr><td><strong>MMORPG</strong></td><td>多进程(KBEngine)</td><td>空间大，逻辑复杂</td></tr><tr><td><strong>FPS 游戏</strong></td><td>多进程</td><td>低延迟，高并发</td></tr><tr><td><strong>MOBA 游戏</strong></td><td>多进程</td><td>5v5，但多房间</td></tr><tr><td><strong>休闲游戏</strong></td><td>进程内</td><td>快速开发</td></tr></tbody></table><h3 id="_5-3-团队能力匹配" tabindex="-1"><a class="header-anchor" href="#_5-3-团队能力匹配"><span>5.3 团队能力匹配</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">团队规模与架构选择：</span>
<span class="line"></span>
<span class="line">1-2 人：进程内架构</span>
<span class="line">   - 开发速度快</span>
<span class="line">   - 运维成本低</span>
<span class="line">   - 适合小规模游戏</span>
<span class="line"></span>
<span class="line">3-5 人：混合架构</span>
<span class="line">   - 单进程 + 独立 DB</span>
<span class="line">   - 可以使用现成框架</span>
<span class="line">   - 有一定扩展能力</span>
<span class="line"></span>
<span class="line">5-10 人：多进程架构</span>
<span class="line">   - 可以使用 KBEngine</span>
<span class="line">   - 需要专门的运维人员</span>
<span class="line">   - 适合商业化运营</span>
<span class="line"></span>
<span class="line">10+ 人：自研分布式架构</span>
<span class="line">   - 可以考虑自研</span>
<span class="line">   - 需要架构师角色</span>
<span class="line">   - 长期投入</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、混合架构方案" tabindex="-1"><a class="header-anchor" href="#六、混合架构方案"><span>六、混合架构方案</span></a></h2><h3 id="_6-1-渐进式架构演进" tabindex="-1"><a class="header-anchor" href="#_6-1-渐进式架构演进"><span>6.1 渐进式架构演进</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">阶段 1：单进程起步</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │           单一进程 (多线程)                      │       │</span>
<span class="line">│  │  ┌───────────────────────────────────────────┐  │       │</span>
<span class="line">│  │  │  网络 │ 逻辑 │ 数据 │ AI │ 其他           │  │       │</span>
<span class="line">│  │  └───────────────────────────────────────────┘  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line">         │ 玩家增长，性能瓶颈</span>
<span class="line">         ▼</span>
<span class="line">阶段 2：分离数据库</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  ┌─────────────────────────┐      ┌──────────────────┐      │</span>
<span class="line">│  │    游戏服务器 (单进程)   │──────│    独立数据库    │      │</span>
<span class="line">│  │  网络/逻辑/AI/缓存       │      │  MySQL/Redis    │      │</span>
<span class="line">│  └─────────────────────────┘      └──────────────────┘      │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line">         │ 继续增长</span>
<span class="line">         ▼</span>
<span class="line">阶段 3：逻辑分离</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │</span>
<span class="line">│  │ 网关进程 │  │ 逻辑进程 │  │ 空间进程 │  │ 数据进程 │   │</span>
<span class="line">│  │  Gateway │  │  BaseApp │  │ CellApp  │  │  DBMgr   │   │</span>
<span class="line">│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line">         │ 完全分布式</span>
<span class="line">         ▼</span>
<span class="line">阶段 4：完全分布式 (KBEngine)</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  多机器部署，动态负载均衡，组件化设计                        │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-功能模块分离建议" tabindex="-1"><a class="header-anchor" href="#_6-2-功能模块分离建议"><span>6.2 功能模块分离建议</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">哪些模块适合分离？</span>
<span class="line"></span>
<span class="line">适合独立进程的模块：</span>
<span class="line">├── 登录服务      → 独立的 Loginapp</span>
<span class="line">├── 聊天服务      → 独立的 Chatapp (高并发，频繁读写)</span>
<span class="line">├── 排行榜        → 独立服务或 Redis</span>
<span class="line">├── 匹配系统      → 独立的 Matchapp</span>
<span class="line">├── 统计分析      → 独立的 Statsapp (不影响游戏)</span>
<span class="line">└── GM 系统       → 独立的 GMapp</span>
<span class="line"></span>
<span class="line">必须在一起的模块：</span>
<span class="line">├── 战斗系统      ─┐</span>
<span class="line">├── AOI 系统      ─┼─ 在 Cellapp 中</span>
<span class="line">├── 空间管理      ─┘</span>
<span class="line">├── 玩家数据      ─┐</span>
<span class="line">└── 社交系统      ─┘ 在 Baseapp 中</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、实战建议" tabindex="-1"><a class="header-anchor" href="#七、实战建议"><span>七、实战建议</span></a></h2><h3 id="_7-1-小团队建议" tabindex="-1"><a class="header-anchor" href="#_7-1-小团队建议"><span>7.1 小团队建议</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">如果你是 &lt; 5 人团队：</span>
<span class="line"></span>
<span class="line">1. 第一款游戏：进程内架构</span>
<span class="line">   ├── 使用成熟框架（如 Flask/Django + SQLAlchemy）</span>
<span class="line">   ├── 单进程 + 多线程</span>
<span class="line">   ├── Redis 做缓存</span>
<span class="line">   └── MySQL 做存储</span>
<span class="line"></span>
<span class="line">2. 验证成功后：考虑迁移</span>
<span class="line">   ├── 使用 KBEngine 等现成 MMO 框架</span>
<span class="line">   ├── 或自研分布式架构</span>
<span class="line">   └── 需要投入学习成本</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-2-中等团队建议" tabindex="-1"><a class="header-anchor" href="#_7-2-中等团队建议"><span>7.2 中等团队建议</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">如果你是 5-10 人团队：</span>
<span class="line"></span>
<span class="line">1. 直接使用 KBEngine</span>
<span class="line">   ├── 多进程架构已经设计好</span>
<span class="line">   ├── 只需关注游戏逻辑</span>
<span class="line">   ├── 可参考官方 Demo</span>
<span class="line">   └── 社区有经验分享</span>
<span class="line"></span>
<span class="line">2. 或混合方案</span>
<span class="line">   ├── 单进程游戏服务器</span>
<span class="line">   ├── Redis 做缓存和排行</span>
<span class="line">   ├── Kafka 做消息队列</span>
<span class="line">   └── 分离非关键模块</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-3-大团队建议" tabindex="-1"><a class="header-anchor" href="#_7-3-大团队建议"><span>7.3 大团队建议</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">如果你是 10+ 人团队：</span>
<span class="line"></span>
<span class="line">1. 自研分布式架构</span>
<span class="line">   ├── 参考 KBEngine 设计</span>
<span class="line">   ├── 根据团队技术栈选择</span>
<span class="line">   ├── 可以使用 RPC 框架（gRPC/Thrift）</span>
<span class="line">   └── 需要架构师和技术专家</span>
<span class="line"></span>
<span class="line">2. 或深度定制 KBEngine</span>
<span class="line">   ├── 修改底层 C++ 代码</span>
<span class="line">   ├── 扩展 Python 脚本接口</span>
<span class="line">   └── 优化性能瓶颈</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="核心观点" tabindex="-1"><a class="header-anchor" href="#核心观点"><span>核心观点</span></a></h3><table><thead><tr><th>架构</th><th>优势</th><th>劣势</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>进程内</strong></td><td>开发快、部署简单、通信高效</td><td>扩展性差、故障风险高</td><td>小规模游戏、原型验证</td></tr><tr><td><strong>多进程</strong></td><td>扩展性强、故障隔离、真正并行</td><td>开发复杂、通信开销</td><td>大规模游戏、商业运营</td></tr><tr><td><strong>混合</strong></td><td>平衡开发效率与扩展性</td><td>需要设计边界</td><td>中等规模游戏</td></tr></tbody></table><h3 id="决策清单" tabindex="-1"><a class="header-anchor" href="#决策清单"><span>决策清单</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">架构选择检查清单：</span>
<span class="line"></span>
<span class="line">□ 预期在线人数规模？</span>
<span class="line">□ 游戏类型（空间型/非空间型）？</span>
<span class="line">□ 团队规模和经验？</span>
<span class="line">□ 开发周期要求？</span>
<span class="line">□ 运维能力如何？</span>
<span class="line">□ 是否需要动态扩容？</span>
<span class="line">□ 对故障恢复的要求？</span>
<span class="line">□ 长期维护考虑？</span>
<span class="line"></span>
<span class="line">建议：</span>
<span class="line">- 6+ 个答案指向同一方向，就选择该架构</span>
<span class="line">- 不确定时，先选简单的，后续可以重构</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine GitHub - 源码分析</a></li><li><a href="https://www.kbelab.com/manual/engine-overview.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 引擎概览</a></li><li><a href="https://www.gamasutra.com/blogs/MichaelKerr/20190314/339300/" target="_blank" rel="noopener noreferrer">多进程 vs 多线程游戏服务器</a></li><li><a href="https://www.bigworldtech.com/technology" target="_blank" rel="noopener noreferrer">BigWorld 架构设计白皮书</a></li></ul>`,27)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};