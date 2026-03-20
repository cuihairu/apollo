import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q25-c10k-problem.html","title":"Q25: 连接数上限由什么决定？如何突破 C10K 问题？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q25-c10k-problem.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q25-c10k-problem.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q25-连接数上限由什么决定-如何突破-c10k-问题" tabindex="-1"><a class="header-anchor" href="#q25-连接数上限由什么决定-如何突破-c10k-问题"><span>Q25: 连接数上限由什么决定？如何突破 C10K 问题？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对服务器并发连接能力的理解：</p><ul><li>C10K 问题的本质</li><li>操作系统的连接数限制</li><li>IO 多路复用技术</li><li>KBEngine 如何处理高并发</li></ul><hr><h2 id="一、c10k-问题" tabindex="-1"><a class="header-anchor" href="#一、c10k-问题"><span>一、C10K 问题</span></a></h2><h3 id="_1-1-什么是-c10k" tabindex="-1"><a class="header-anchor" href="#_1-1-什么是-c10k"><span>1.1 什么是 C10K</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      C10K 问题                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  C10K = Concurrent 10,000 connections                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题：单机同时处理 10,000 个并发连接                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  历史背景：                                                 │</span>
<span class="line">│  ├── 1999年：Dan Kegel 提出 C10K 问题                      │</span>
<span class="line">│  ├── 当时：每连接一个线程/进程                              │</span>
<span class="line">│  ├── 问题：10,000 连接 = 10,000 线程 = 资源耗尽            │</span>
<span class="line">│  └── 挑战：如何高效处理大量并发连接？                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  现在的标准：                                               │</span>
<span class="line">│  ├── C10K ✓ 已解决                                        │</span>
<span class="line">│  ├── C100K ✓ 可实现                                       │</span>
<span class="line">│  └── C1M ✓ 某些场景可达                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-传统模型的局限" tabindex="-1"><a class="header-anchor" href="#_1-2-传统模型的局限"><span>1.2 传统模型的局限</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              传统模型 vs 高并发模型                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">                                                             │</span>
<span class="line">│  传统模型（每连接一线程）：                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  Client 1 ──► Thread 1                          │       │</span>
<span class="line">│  │  Client 2 ──► Thread 2                          │       │</span>
<span class="line">│  │  ...                                              │       │</span>
<span class="line">│  │  Client 10000 ──► Thread 10000                  │       │</span>
<span class="line">│  │                                                    │       │</span>
<span class="line">│  │  问题：                                            │       │</span>
<span class="line">│  │  ├── 内存消耗：10,000 线程 × 8MB = 80GB           │       │</span>
<span class="line">│  │  ├── 上下文切换：10,000 线程频繁切换              │       │</span>
<span class="line">│  │  └── CPU 浪费：大部分线程在等待                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  高并发模型（单线程 + IO 多路复用）：                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  所有客户端                                       │       │</span>
<span class="line">│  │      │                                            │       │</span>
<span class="line">│  │      ▼                                            │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────┐         │       │</span>
<span class="line">│  │  │  IO 多路复用 (epoll/kqueue/IOCP)     │         │       │</span>
<span class="line">│  │  │  ┌───────────────────────────────┐   │         │       │</span>
<span class="line">│  │  │  │ 事件循环                     │   │         │       │</span>
<span class="line">│  │  │  │ while(true) {               │   │         │       │</span>
<span class="line">│  │  │  │   events = epoll_wait();   │   │         │       │</span>
<span class="line">│  │  │  │   for (e : events) {       │   │         │       │</span>
<span class="line">│  │  │  │     handle(e);             │   │         │       │</span>
<span class="line">│  │  │  │   }                        │   │         │       │</span>
<span class="line">│  │  │  │ }                          │   │         │       │</span>
<span class="line">│  │  │  └───────────────────────────────┘   │         │       │</span>
<span class="line">│  │  └─────────────────────────────────────┘         │       │</span>
<span class="line">│  │                                                    │       │</span>
<span class="line">│  │  优势：                                            │       │</span>
<span class="line">│  │  ├── 单线程处理所有连接                            │       │</span>
<span class="line">│  │  ├── 内存占用小                                    │       │</span>
<span class="line">│  │  └── 无上下文切换                                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、系统限制因素" tabindex="-1"><a class="header-anchor" href="#二、系统限制因素"><span>二、系统限制因素</span></a></h2><h3 id="_2-1-限制因素分析" tabindex="-1"><a class="header-anchor" href="#_2-1-限制因素分析"><span>2.1 限制因素分析</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    连接数限制因素                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 文件描述符限制                                           │</span>
<span class="line">│     ├── 单进程打开文件数限制                                 │</span>
<span class="line">│     ├── 默认：1024 (Linux)                                  │</span>
<span class="line">│     └── 调整：ulimit -n 65536                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 端口范围限制                                             │</span>
<span class="line">│     ├── 客户端端口范围：1024-65535                          │</span>
<span class="line">│     ├── 可用端口：~64,000                                   │</span>
<span class="line">│     └── TIME_WAIT 占用端口                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 内存限制                                                 │</span>
<span class="line">│     ├── 每连接内存占用                                      │</span>
<span class="line">│     ├── TCP 读写缓冲区                                      │</span>
<span class="line">│     └── 连接状态结构                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. CPU 限制                                                │</span>
<span class="line">│     ├── 上下文切换开销                                      │</span>
<span class="line">│     ├── 中断处理                                            │</span>
<span class="line">│     └── 数据拷贝                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 网络带宽限制                                             │</span>
<span class="line">│     ├── 出口带宽                                            │</span>
<span class="line">│     ├── 入口带宽                                            │</span>
<span class="line">│     └── PPS (包每秒) 限制                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-系统参数调优" tabindex="-1"><a class="header-anchor" href="#_2-2-系统参数调优"><span>2.2 系统参数调优</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># Linux 系统参数调优</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 1. 文件描述符限制</span></span>
<span class="line"><span class="token comment"># /etc/security/limits.conf</span></span>
<span class="line">* soft nofile <span class="token number">65536</span></span>
<span class="line">* hard nofile <span class="token number">65536</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. 内核参数</span></span>
<span class="line"><span class="token comment"># /etc/sysctl.conf</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># TCP 读写缓冲区</span></span>
<span class="line">net.core.rmem_max <span class="token operator">=</span> <span class="token number">16777216</span>    <span class="token comment"># 接收缓冲区最大值 16MB</span></span>
<span class="line">net.core.wmem_max <span class="token operator">=</span> <span class="token number">16777216</span>    <span class="token comment"># 发送缓冲区最大值 16MB</span></span>
<span class="line">net.ipv4.tcp_rmem <span class="token operator">=</span> <span class="token number">4096</span> <span class="token number">87380</span> <span class="token number">16777216</span></span>
<span class="line">net.ipv4.tcp_wmem <span class="token operator">=</span> <span class="token number">4096</span> <span class="token number">65536</span> <span class="token number">16777216</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># TIME_WAIT 优化</span></span>
<span class="line">net.ipv4.tcp_tw_reuse <span class="token operator">=</span> <span class="token number">1</span>       <span class="token comment"># 重用 TIME_WAIT socket</span></span>
<span class="line">net.ipv4.tcp_tw_recycle <span class="token operator">=</span> <span class="token number">0</span>     <span class="token comment"># 禁用快速回收（有问题）</span></span>
<span class="line">net.ipv4.tcp_fin_timeout <span class="token operator">=</span> <span class="token number">30</span>   <span class="token comment"># TIME_WAIT 超时 30s</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 连接队列</span></span>
<span class="line">net.core.somaxconn <span class="token operator">=</span> <span class="token number">32768</span>       <span class="token comment"># 连接队列长度</span></span>
<span class="line">net.ipv4.tcp_max_syn_backlog <span class="token operator">=</span> <span class="token number">8192</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 端口范围</span></span>
<span class="line">net.ipv4.ip_local_port_range <span class="token operator">=</span> <span class="token number">10000</span> <span class="token number">65535</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 应用配置</span></span>
<span class="line"><span class="token function">sysctl</span> <span class="token parameter variable">-p</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、io-多路复用技术" tabindex="-1"><a class="header-anchor" href="#三、io-多路复用技术"><span>三、IO 多路复用技术</span></a></h2><h3 id="_3-1-技术对比" tabindex="-1"><a class="header-anchor" href="#_3-1-技术对比"><span>3.1 技术对比</span></a></h3><table><thead><tr><th>技术</th><th>平台</th><th>复杂度</th><th>性能</th><th>连接数上限</th></tr></thead><tbody><tr><td><strong>select</strong></td><td>跨平台</td><td>低</td><td>低</td><td>~1024</td></tr><tr><td><strong>poll</strong></td><td>跨平台</td><td>低</td><td>中</td><td>~10,000</td></tr><tr><td><strong>epoll</strong></td><td>Linux</td><td>中</td><td>高</td><td>~100,000+</td></tr><tr><td><strong>kqueue</strong></td><td>BSD/macOS</td><td>中</td><td>高</td><td>~100,000+</td></tr><tr><td><strong>IOCP</strong></td><td>Windows</td><td>高</td><td>高</td><td>~100,000+</td></tr></tbody></table><h3 id="_3-2-epoll-实现示例" tabindex="-1"><a class="header-anchor" href="#_3-2-epoll-实现示例"><span>3.2 epoll 实现示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// epoll 高并发服务器实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EpollServer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_EVENTS <span class="token operator">=</span> <span class="token number">1024</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_CONNECTIONS <span class="token operator">=</span> <span class="token number">100000</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// epoll 文件描述符</span></span>
<span class="line">    <span class="token keyword">int</span> epfd_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 事件数组</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">epoll_event</span> events_<span class="token punctuation">[</span>MAX_EVENTS<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 连接管理</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">int</span><span class="token punctuation">,</span> Connection<span class="token operator">*</span><span class="token operator">&gt;</span> connections_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">start</span><span class="token punctuation">(</span><span class="token keyword">int</span> port<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 创建 epoll</span></span>
<span class="line">        epfd_ <span class="token operator">=</span> <span class="token function">epoll_create1</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>epfd_ <span class="token operator">==</span> <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">perror</span><span class="token punctuation">(</span><span class="token string">&quot;epoll_create1&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 创建监听 socket</span></span>
<span class="line">        <span class="token keyword">int</span> listenfd <span class="token operator">=</span> <span class="token function">createListenSocket</span><span class="token punctuation">(</span>port<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 添加到 epoll</span></span>
<span class="line">        <span class="token keyword">struct</span> <span class="token class-name">epoll_event</span> ev<span class="token punctuation">;</span></span>
<span class="line">        ev<span class="token punctuation">.</span>events <span class="token operator">=</span> EPOLLIN <span class="token operator">|</span> EPOLLET<span class="token punctuation">;</span>  <span class="token comment">// 边缘触发</span></span>
<span class="line">        ev<span class="token punctuation">.</span>data<span class="token punctuation">.</span>fd <span class="token operator">=</span> listenfd<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">epoll_ctl</span><span class="token punctuation">(</span>epfd_<span class="token punctuation">,</span> EPOLL_CTL_ADD<span class="token punctuation">,</span> listenfd<span class="token punctuation">,</span> <span class="token operator">&amp;</span>ev<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 事件循环</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">int</span> nfds <span class="token operator">=</span> <span class="token function">epoll_wait</span><span class="token punctuation">(</span>epfd_<span class="token punctuation">,</span> events_<span class="token punctuation">,</span> MAX_EVENTS<span class="token punctuation">,</span> <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> nfds<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>events_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>data<span class="token punctuation">.</span>fd <span class="token operator">==</span> listenfd<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token comment">// 新连接</span></span>
<span class="line">                    <span class="token function">acceptConnection</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token comment">// 数据到达</span></span>
<span class="line">                    <span class="token function">handleData</span><span class="token punctuation">(</span>events_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>data<span class="token punctuation">.</span>fd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">acceptConnection</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">struct</span> <span class="token class-name">sockaddr_in</span> clientAddr<span class="token punctuation">;</span></span>
<span class="line">            socklen_t addrLen <span class="token operator">=</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>clientAddr<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">int</span> clientfd <span class="token operator">=</span> <span class="token function">accept</span><span class="token punctuation">(</span>listenfd_<span class="token punctuation">,</span></span>
<span class="line">                                  <span class="token punctuation">(</span><span class="token keyword">struct</span> <span class="token class-name">sockaddr</span><span class="token operator">*</span><span class="token punctuation">)</span><span class="token operator">&amp;</span>clientAddr<span class="token punctuation">,</span></span>
<span class="line">                                  <span class="token operator">&amp;</span>addrLen<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>clientfd <span class="token operator">==</span> <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>errno <span class="token operator">==</span> EAGAIN <span class="token operator">||</span> errno <span class="token operator">==</span> EWOULDBLOCK<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span>  <span class="token comment">// 没有更多连接</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">                <span class="token keyword">continue</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 设置非阻塞</span></span>
<span class="line">            <span class="token function">setNonBlocking</span><span class="token punctuation">(</span>clientfd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 创建连接对象</span></span>
<span class="line">            Connection<span class="token operator">*</span> conn <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">Connection</span><span class="token punctuation">(</span>clientfd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            connections_<span class="token punctuation">[</span>clientfd<span class="token punctuation">]</span> <span class="token operator">=</span> conn<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 添加到 epoll</span></span>
<span class="line">            <span class="token keyword">struct</span> <span class="token class-name">epoll_event</span> ev<span class="token punctuation">;</span></span>
<span class="line">            ev<span class="token punctuation">.</span>events <span class="token operator">=</span> EPOLLIN <span class="token operator">|</span> EPOLLET <span class="token operator">|</span> EPOLLRDHUP<span class="token punctuation">;</span></span>
<span class="line">            ev<span class="token punctuation">.</span>data<span class="token punctuation">.</span>ptr <span class="token operator">=</span> conn<span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">epoll_ctl</span><span class="token punctuation">(</span>epfd_<span class="token punctuation">,</span> EPOLL_CTL_ADD<span class="token punctuation">,</span> clientfd<span class="token punctuation">,</span> <span class="token operator">&amp;</span>ev<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleData</span><span class="token punctuation">(</span><span class="token keyword">int</span> fd<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Connection<span class="token operator">*</span> conn <span class="token operator">=</span> connections_<span class="token punctuation">[</span>fd<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>conn<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 读取数据</span></span>
<span class="line">        <span class="token keyword">char</span> buffer<span class="token punctuation">[</span><span class="token number">4096</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            ssize_t n <span class="token operator">=</span> <span class="token function">read</span><span class="token punctuation">(</span>fd<span class="token punctuation">,</span> buffer<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>buffer<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>n <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 处理数据</span></span>
<span class="line">                conn<span class="token operator">-&gt;</span><span class="token function">onData</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> n<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>n <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 连接关闭</span></span>
<span class="line">                <span class="token function">closeConnection</span><span class="token punctuation">(</span>fd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>errno <span class="token operator">==</span> EAGAIN <span class="token operator">||</span> errno <span class="token operator">==</span> EWOULDBLOCK<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span>  <span class="token comment">// 没有更多数据</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">                <span class="token comment">// 错误</span></span>
<span class="line">                <span class="token function">closeConnection</span><span class="token punctuation">(</span>fd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setNonBlocking</span><span class="token punctuation">(</span><span class="token keyword">int</span> fd<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> flags <span class="token operator">=</span> <span class="token function">fcntl</span><span class="token punctuation">(</span>fd<span class="token punctuation">,</span> F_GETFL<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">fcntl</span><span class="token punctuation">(</span>fd<span class="token punctuation">,</span> F_SETFL<span class="token punctuation">,</span> flags <span class="token operator">|</span> O_NONBLOCK<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-边缘触发-vs-水平触发" tabindex="-1"><a class="header-anchor" href="#_3-3-边缘触发-vs-水平触发"><span>3.3 边缘触发 vs 水平触发</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              EPOLLLT vs EPOLLET                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  水平触发 (Level Triggered, EPOLLLT)：                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  特点：只要缓冲区有数据，就会触发事件           │       │</span>
<span class="line">│  │                                                    │       │</span>
<span class="line">│  │  优点：                                           │       │</span>
<span class="line">│  │  ├── 编程简单                                    │       │</span>
<span class="line">│  │  ├── 不容易遗漏事件                              │       │</span>
<span class="line">│  │                                                    │       │</span>
<span class="line">│  │  缺点：                                           │       │</span>
<span class="line">│  │  ├── 可能重复触发                                │       │</span>
<span class="line">│  │  ├── 需要处理 EAGAIN                            │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  边缘触发 (Edge Triggered, EPOLLET)：                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  特点：只在状态变化时触发一次                   │       │</span>
<span class="line">│  │                                                    │       │</span>
<span class="line">│  │  优点：                                           │       │</span>
<span class="line">│  │  ├── 减少触发次数                                │       │</span>
<span class="line">│  │  ├── 更高性能                                    │       │</span>
<span class="line">│  │                                                    │       │</span>
<span class="line">│  │  缺点：                                           │       │</span>
<span class="line">│  │  ├── 编程复杂                                    │       │</span>
<span class="line">│  │  ├── 必须一次性读完所有数据                      │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-的高并发实现" tabindex="-1"><a class="header-anchor" href="#四、kbengine-的高并发实现"><span>四、KBEngine 的高并发实现</span></a></h2><h3 id="_4-1-poller-机制" tabindex="-1"><a class="header-anchor" href="#_4-1-poller-机制"><span>4.1 Poller 机制</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine 源码</a>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Poller 实现</span></span>
<span class="line"><span class="token comment">// src/lib/network/poller.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Poller</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 事件处理器接口</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">PollerDescriptor</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">int</span> <span class="token function">readFD</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">int</span> <span class="token function">writeFD</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">handleInputNotification</span><span class="token punctuation">(</span><span class="token keyword">int</span> fd<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">handleOutputNotification</span><span class="token punctuation">(</span><span class="token keyword">int</span> fd<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加到 poller</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">addToPoller</span><span class="token punctuation">(</span>PollerDescriptor<span class="token operator">*</span> pDescriptor<span class="token punctuation">,</span> <span class="token keyword">bool</span> isRead <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> fd <span class="token operator">=</span> isRead <span class="token operator">?</span> pDescriptor<span class="token operator">-&gt;</span><span class="token function">readFD</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">:</span> pDescriptor<span class="token operator">-&gt;</span><span class="token function">writeFD</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">ifdef</span> <span class="token expression">USE_EPOLL</span></span></span>
<span class="line">        <span class="token keyword">struct</span> <span class="token class-name">epoll_event</span> ev<span class="token punctuation">;</span></span>
<span class="line">        ev<span class="token punctuation">.</span>events <span class="token operator">=</span> isRead <span class="token operator">?</span> EPOLLIN <span class="token operator">:</span> EPOLLOUT<span class="token punctuation">;</span></span>
<span class="line">        ev<span class="token punctuation">.</span>events <span class="token operator">|=</span> EPOLLET<span class="token punctuation">;</span>  <span class="token comment">// 边缘触发</span></span>
<span class="line">        ev<span class="token punctuation">.</span>data<span class="token punctuation">.</span>ptr <span class="token operator">=</span> pDescriptor<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">epoll_ctl</span><span class="token punctuation">(</span>epfd_<span class="token punctuation">,</span> EPOLL_CTL_ADD<span class="token punctuation">,</span> fd<span class="token punctuation">,</span> <span class="token operator">&amp;</span>ev<span class="token punctuation">)</span> <span class="token operator">&lt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">endif</span></span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 事件循环</span></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">processUntilBreak</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">ifdef</span> <span class="token expression">USE_EPOLL</span></span></span>
<span class="line">        <span class="token keyword">struct</span> <span class="token class-name">epoll_event</span> events<span class="token punctuation">[</span>MAX_EVENTS<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> nfds <span class="token operator">=</span> <span class="token function">epoll_wait</span><span class="token punctuation">(</span>epfd_<span class="token punctuation">,</span> events<span class="token punctuation">,</span> MAX_EVENTS<span class="token punctuation">,</span> timeout_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> nfds<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            PollerDescriptor<span class="token operator">*</span> pDescriptor <span class="token operator">=</span></span>
<span class="line">                <span class="token punctuation">(</span>PollerDescriptor<span class="token operator">*</span><span class="token punctuation">)</span>events<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>data<span class="token punctuation">.</span>ptr<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>events<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>events <span class="token operator">&amp;</span> EPOLLIN<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                pDescriptor<span class="token operator">-&gt;</span><span class="token function">handleInputNotification</span><span class="token punctuation">(</span>pDescriptor<span class="token operator">-&gt;</span><span class="token function">readFD</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>events<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>events <span class="token operator">&amp;</span> EPOLLOUT<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                pDescriptor<span class="token operator">-&gt;</span><span class="token function">handleOutputNotification</span><span class="token punctuation">(</span>pDescriptor<span class="token operator">-&gt;</span><span class="token function">writeFD</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">endif</span></span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> nfds<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">int</span> epfd_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_EVENTS <span class="token operator">=</span> <span class="token number">256</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> timeout_ <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span>  <span class="token comment">// 100ms</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-channel-管理" tabindex="-1"><a class="header-anchor" href="#_4-2-channel-管理"><span>4.2 Channel 管理</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Channel 管理</span></span>
<span class="line"><span class="token comment">// src/lib/network/channel.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> Poller<span class="token double-colon punctuation">::</span><span class="token class-name">PollerDescriptor</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 接收数据</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">handleInputNotification</span><span class="token punctuation">(</span><span class="token keyword">int</span> fd<span class="token punctuation">)</span> <span class="token keyword">override</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 接收数据包</span></span>
<span class="line">            Packet<span class="token operator">*</span> pPacket <span class="token operator">=</span> <span class="token keyword">this</span><span class="token operator">-&gt;</span>pNetworkInterface_<span class="token operator">-&gt;</span><span class="token function">receivePacket</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>pPacket<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>errno <span class="token operator">==</span> EAGAIN <span class="token operator">||</span> errno <span class="token operator">==</span> EWOULDBLOCK<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span>  <span class="token comment">// 没有更多数据</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span>  <span class="token comment">// 错误</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 处理数据包</span></span>
<span class="line">            <span class="token keyword">this</span><span class="token operator">-&gt;</span><span class="token function">processPacket</span><span class="token punctuation">(</span>pPacket<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送数据</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">handleOutputNotification</span><span class="token punctuation">(</span><span class="token keyword">int</span> fd<span class="token punctuation">)</span> <span class="token keyword">override</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 发送缓冲区中的数据</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>sendQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Packet<span class="token operator">*</span> pPacket <span class="token operator">=</span> sendQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            ssize_t sent <span class="token operator">=</span> <span class="token function">send</span><span class="token punctuation">(</span>fd_<span class="token punctuation">,</span> pPacket<span class="token operator">-&gt;</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                              pPacket<span class="token operator">-&gt;</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>sent <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                sendQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">delete</span> pPacket<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>sent <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>errno <span class="token operator">==</span> EAGAIN <span class="token operator">||</span> errno <span class="token operator">==</span> EWOULDBLOCK<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span>  <span class="token comment">// 发送缓冲区满</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span>  <span class="token comment">// 错误</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、性能优化技巧" tabindex="-1"><a class="header-anchor" href="#五、性能优化技巧"><span>五、性能优化技巧</span></a></h2><h3 id="_5-1-连接复用" tabindex="-1"><a class="header-anchor" href="#_5-1-连接复用"><span>5.1 连接复用</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 连接池管理</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ConnectionPool</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 连接池</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Connection<span class="token operator">*</span><span class="token operator">&gt;</span> pool_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>Connection<span class="token operator">*</span><span class="token operator">&gt;</span> freeList_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取连接</span></span>
<span class="line">    Connection<span class="token operator">*</span> <span class="token function">acquire</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>freeList_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Connection<span class="token operator">*</span> conn <span class="token operator">=</span> freeList_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> conn<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 创建新连接</span></span>
<span class="line">        Connection<span class="token operator">*</span> conn <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">Connection</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        pool_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> conn<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 释放连接</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">release</span><span class="token punctuation">(</span>Connection<span class="token operator">*</span> conn<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        conn<span class="token operator">-&gt;</span><span class="token function">reset</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        freeList_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-零拷贝技术" tabindex="-1"><a class="header-anchor" href="#_5-2-零拷贝技术"><span>5.2 零拷贝技术</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 零拷贝发送 (sendfile)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ZeroCopySender</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 使用 sendfile 零拷贝发送文件</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">sendFile</span><span class="token punctuation">(</span><span class="token keyword">int</span> outfd<span class="token punctuation">,</span> <span class="token keyword">int</span> infd<span class="token punctuation">,</span> off_t offset<span class="token punctuation">,</span> size_t count<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">ifdef</span> <span class="token expression">USE_SENDFILE</span></span></span>
<span class="line">        off_t sent <span class="token operator">=</span> offset<span class="token punctuation">;</span></span>
<span class="line">        ssize_t n <span class="token operator">=</span> <span class="token function">sendfile</span><span class="token punctuation">(</span>outfd<span class="token punctuation">,</span> infd<span class="token punctuation">,</span> <span class="token operator">&amp;</span>sent<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>n <span class="token operator">==</span> count<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">endif</span></span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 使用 splice 零拷贝管道传输</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">spliceData</span><span class="token punctuation">(</span><span class="token keyword">int</span> pipefd<span class="token punctuation">,</span> <span class="token keyword">int</span> sockfd<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">ifdef</span> <span class="token expression">USE_SPLICE</span></span></span>
<span class="line">        ssize_t n <span class="token operator">=</span> <span class="token function">splice</span><span class="token punctuation">(</span>pipefd<span class="token punctuation">,</span> <span class="token constant">NULL</span><span class="token punctuation">,</span> sockfd<span class="token punctuation">,</span> <span class="token constant">NULL</span><span class="token punctuation">,</span> len<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> n <span class="token operator">==</span> len<span class="token punctuation">;</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">endif</span></span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-3-内存池优化" tabindex="-1"><a class="header-anchor" href="#_5-3-内存池优化"><span>5.3 内存池优化</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 连接对象内存池</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ConnectionPool</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> POOL_SIZE <span class="token operator">=</span> <span class="token number">10000</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token function">ConnectionPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 预分配对象池</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> POOL_SIZE<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span><span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    T<span class="token operator">*</span> <span class="token function">acquire</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>freeList_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 池空，分配新的</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        T<span class="token operator">*</span> obj <span class="token operator">=</span> freeList_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        freeList_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> obj<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">release</span><span class="token punctuation">(</span>T<span class="token operator">*</span> obj<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        obj<span class="token operator">-&gt;</span><span class="token function">reset</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        freeList_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>obj<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>T<span class="token operator">*</span><span class="token operator">&gt;</span> freeList_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、实战配置" tabindex="-1"><a class="header-anchor" href="#六、实战配置"><span>六、实战配置</span></a></h2><h3 id="_6-1-生产环境配置" tabindex="-1"><a class="header-anchor" href="#_6-1-生产环境配置"><span>6.1 生产环境配置</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># KBEngine 生产环境配置脚本</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">#!/bin/bash</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 1. 修改文件描述符限制</span></span>
<span class="line"><span class="token builtin class-name">echo</span> <span class="token string">&quot;* soft nofile 100000&quot;</span> <span class="token operator">&gt;&gt;</span> /etc/security/limits.conf</span>
<span class="line"><span class="token builtin class-name">echo</span> <span class="token string">&quot;* hard nofile 100000&quot;</span> <span class="token operator">&gt;&gt;</span> /etc/security/limits.conf</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. 优化内核参数</span></span>
<span class="line"><span class="token function">cat</span> <span class="token operator">&gt;&gt;</span> /etc/sysctl.conf <span class="token operator">&lt;&lt;</span> <span class="token string">EOF</span>
<span class="line"># TCP 连接优化</span>
<span class="line">net.ipv4.tcp_max_syn_backlog = 8192</span>
<span class="line">net.core.somaxconn = 8192</span>
<span class="line"></span>
<span class="line"># TCP 缓冲区优化</span>
<span class="line">net.core.rmem_max = 16777216</span>
<span class="line">net.core.wmem_max = 16777216</span>
<span class="line">net.ipv4.tcp_rmem = 4096 87380 16777216</span>
<span class="line">net.ipv4.tcp_wmem = 4096 65536 16777216</span>
<span class="line"></span>
<span class="line"># TIME_WAIT 优化</span>
<span class="line">net.ipv4.tcp_tw_reuse = 1</span>
<span class="line">net.ipv4.tcp_fin_timeout = 15</span>
<span class="line"></span>
<span class="line"># 端口范围</span>
<span class="line">net.ipv4.ip_local_port_range = 1024 65535</span>
<span class="line">EOF</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 3. 应用配置</span></span>
<span class="line"><span class="token function">sysctl</span> <span class="token parameter variable">-p</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 4. 验证</span></span>
<span class="line"><span class="token builtin class-name">ulimit</span> <span class="token parameter variable">-n</span></span>
<span class="line"><span class="token function">cat</span> /proc/sys/net/core/somaxconn</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-监控连接数" tabindex="-1"><a class="header-anchor" href="#_6-2-监控连接数"><span>6.2 监控连接数</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 监控脚本</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">#!/bin/bash</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">while</span> <span class="token boolean">true</span><span class="token punctuation">;</span> <span class="token keyword">do</span></span>
<span class="line">    <span class="token comment"># 当前连接数</span></span>
<span class="line">    <span class="token assign-left variable">ESTABLISHED</span><span class="token operator">=</span><span class="token variable"><span class="token variable">$(</span><span class="token function">netstat</span> <span class="token parameter variable">-an</span> <span class="token operator">|</span> <span class="token function">grep</span> ESTABLISHED <span class="token operator">|</span> <span class="token function">wc</span> <span class="token parameter variable">-l</span><span class="token variable">)</span></span></span>
<span class="line">    <span class="token assign-left variable">TIME_WAIT</span><span class="token operator">=</span><span class="token variable"><span class="token variable">$(</span><span class="token function">netstat</span> <span class="token parameter variable">-an</span> <span class="token operator">|</span> <span class="token function">grep</span> TIME_WAIT <span class="token operator">|</span> <span class="token function">wc</span> <span class="token parameter variable">-l</span><span class="token variable">)</span></span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 系统资源</span></span>
<span class="line">    <span class="token assign-left variable">CPU</span><span class="token operator">=</span><span class="token variable"><span class="token variable">$(</span><span class="token function">top</span> <span class="token parameter variable">-bn1</span> <span class="token operator">|</span> <span class="token function">grep</span> <span class="token string">&quot;Cpu(s)&quot;</span> <span class="token operator">|</span> <span class="token function">awk</span> <span class="token string">&#39;{print $2}&#39;</span> <span class="token operator">|</span> <span class="token function">cut</span> -d<span class="token string">&#39;%&#39;</span> <span class="token parameter variable">-f1</span><span class="token variable">)</span></span></span>
<span class="line">    <span class="token assign-left variable">MEM</span><span class="token operator">=</span><span class="token variable"><span class="token variable">$(</span><span class="token function">free</span> <span class="token parameter variable">-m</span> <span class="token operator">|</span> <span class="token function">grep</span> Mem <span class="token operator">|</span> <span class="token function">awk</span> <span class="token string">&#39;{printf &quot;%.1f&quot;, $3/$2 * 100.0}&#39;</span><span class="token variable">)</span></span></span>
<span class="line"></span>
<span class="line">    <span class="token builtin class-name">echo</span> <span class="token string">&quot;[<span class="token variable"><span class="token variable">$(</span><span class="token function">date</span><span class="token variable">)</span></span>] ESTABLISHED: <span class="token variable">$ESTABLISHED</span>, TIME_WAIT: <span class="token variable">$TIME_WAIT</span>, CPU: <span class="token variable">\${CPU}</span>%, MEM: <span class="token variable">\${MEM}</span>%&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token function">sleep</span> <span class="token number">5</span></span>
<span class="line"><span class="token keyword">done</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="连接数限制总结" tabindex="-1"><a class="header-anchor" href="#连接数限制总结"><span>连接数限制总结</span></a></h3><table><thead><tr><th>限制因素</th><th>默认值</th><th>调优后</th><th>影响</th></tr></thead><tbody><tr><td><strong>文件描述符</strong></td><td>1024</td><td>100,000</td><td>连接数上限</td></tr><tr><td><strong>端口范围</strong></td><td>~28,000</td><td>~65,000</td><td>客户端连接</td></tr><tr><td><strong>内存</strong></td><td>取决于配置</td><td>优化后</td><td>每连接内存</td></tr><tr><td><strong>CPU</strong></td><td>100% 核心</td><td>多核</td><td>处理能力</td></tr></tbody></table><h3 id="c10k-解决方案" tabindex="-1"><a class="header-anchor" href="#c10k-解决方案"><span>C10K 解决方案</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 使用 IO 多路复用</span>
<span class="line">   - Linux: epoll</span>
<span class="line">   - Windows: IOCP</span>
<span class="line">   - BSD/macOS: kqueue</span>
<span class="line"></span>
<span class="line">2. 调整系统参数</span>
<span class="line">   - 文件描述符限制</span>
<span class="line">   - TCP 缓冲区大小</span>
<span class="line">   - TIME_WAIT 优化</span>
<span class="line"></span>
<span class="line">3. 采用事件驱动架构</span>
<span class="line">   - 单线程事件循环</span>
<span class="line">   - 非阻塞 IO</span>
<span class="line">   - 异步处理</span>
<span class="line"></span>
<span class="line">4. 优化内存使用</span>
<span class="line">   - 对象池</span>
<span class="line">   - 零拷贝</span>
<span class="line">   - 内存复用</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/tree/master/kbe/src/lib/network" target="_blank" rel="noopener noreferrer">KBEngine GitHub - Poller</a></li><li><a href="https://www.kegel.com/c10k.html" target="_blank" rel="noopener noreferrer">C10K Problem</a></li><li><a href="https://man7.org/linux/man-pages/man7/epoll.7.html" target="_blank" rel="noopener noreferrer">epoll 官方文档</a></li></ul>`,54)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};