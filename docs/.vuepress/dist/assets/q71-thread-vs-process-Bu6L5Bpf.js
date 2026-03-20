import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q71-thread-vs-process.html","title":"Q71: 多线程 vs 多进程，如何选择？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q71-thread-vs-process.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q71-thread-vs-process.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q71-多线程-vs-多进程-如何选择" tabindex="-1"><a class="header-anchor" href="#q71-多线程-vs-多进程-如何选择"><span>Q71: 多线程 vs 多进程，如何选择？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对并发模型选择的理解：</p><ul><li>多线程与多进程的区别</li><li>各自的优缺点</li><li>适用场景</li><li>KBEngine 的选择</li></ul><hr><h2 id="一、基本概念" tabindex="-1"><a class="header-anchor" href="#一、基本概念"><span>一、基本概念</span></a></h2><h3 id="_1-1-对比表" tabindex="-1"><a class="header-anchor" href="#_1-1-对比表"><span>1.1 对比表</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    多线程 vs 多进程                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  多线程 (Thread):                                           │</span>
<span class="line">│  ├── 共享进程内存空间                                        │</span>
<span class="line">│  ├── 创建开销小                                             │</span>
<span class="line">│  ├── 上下文切换快                                           │</span>
<span class="line">│  ├── 需要处理同步问题                                        │</span>
<span class="line">│  └── 一个线程崩溃可能影响整个进程                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  多进程 (Process):                                          │</span>
<span class="line">│  ├── 独立内存空间                                            │</span>
<span class="line">│  ├── 创建开销大                                             │</span>
<span class="line">│  ├── 进程间通信复杂                                          │</span>
<span class="line">│  ├── 隔离性好，一个崩溃不影响其他                             │</span>
<span class="line">│  └── 可利用多核                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-详细对比" tabindex="-1"><a class="header-anchor" href="#_1-2-详细对比"><span>1.2 详细对比</span></a></h3><table><thead><tr><th>特性</th><th>多线程</th><th>多进程</th></tr></thead><tbody><tr><td><strong>内存共享</strong></td><td>共享地址空间</td><td>独立地址空间</td></tr><tr><td><strong>创建开销</strong></td><td>低 (~MB 级)</td><td>高 (~GB 级)</td></tr><tr><td><strong>上下文切换</strong></td><td>快 (~微秒)</td><td>慢 (~毫秒)</td></tr><tr><td><strong>数据通信</strong></td><td>直接读写共享内存</td><td>IPC (管道/共享内存/消息)</td></tr><tr><td><strong>同步机制</strong></td><td>锁、条件变量</td><td>IPC 信号量、消息队列</td></tr><tr><td><strong>隔离性</strong></td><td>差，一个线程崩溃可能影响进程</td><td>好，进程间完全隔离</td></tr><tr><td><strong>CPU 利用</strong></td><td>多核多线程并行</td><td>多核多进程并行</td></tr><tr><td><strong>适用场景</strong></td><td>共享数据密集型</td><td>隔离要求高、任务独立</td></tr></tbody></table><hr><h2 id="二、多线程架构" tabindex="-1"><a class="header-anchor" href="#二、多线程架构"><span>二、多线程架构</span></a></h2><h3 id="_2-1-典型多线程服务器" tabindex="-1"><a class="header-anchor" href="#_2-1-典型多线程服务器"><span>2.1 典型多线程服务器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 多线程游戏服务器架构</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ThreadedGameServer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">start</span><span class="token punctuation">(</span><span class="token keyword">int</span> threadCount <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 主线程处理网络 I/O</span></span>
<span class="line">        acceptThread_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>ThreadedGameServer<span class="token double-colon punctuation">::</span>acceptLoop<span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 工作线程处理游戏逻辑</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> threadCount<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            workers_<span class="token punctuation">.</span><span class="token function">emplace_back</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>ThreadedGameServer<span class="token double-colon punctuation">::</span>workerLoop<span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">,</span> i<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 数据库线程</span></span>
<span class="line">        dbThread_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>ThreadedGameServer<span class="token double-colon punctuation">::</span>dbLoop<span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">acceptLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 接受新连接</span></span>
<span class="line">            <span class="token keyword">int</span> fd <span class="token operator">=</span> <span class="token function">accept</span><span class="token punctuation">(</span>listenFd_<span class="token punctuation">,</span> <span class="token keyword">nullptr</span><span class="token punctuation">,</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 分配给玩家</span></span>
<span class="line">            <span class="token function">assignToPlayer</span><span class="token punctuation">(</span>fd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">workerLoop</span><span class="token punctuation">(</span><span class="token keyword">int</span> workerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 每个工作线程有自己的消息队列</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> queue <span class="token operator">=</span> workerQueues_<span class="token punctuation">[</span>workerId<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Message msg<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>queue<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">handleMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">dbLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 专门处理数据库操作</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            DBRequest req<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>dbQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span>req<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">executeDBRequest</span><span class="token punctuation">(</span>req<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>thread acceptThread_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>thread<span class="token operator">&gt;</span> workers_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>thread dbThread_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>array<span class="token operator">&lt;</span>ThreadSafeQueue<span class="token operator">&lt;</span>Message<span class="token operator">&gt;</span><span class="token punctuation">,</span> <span class="token number">16</span><span class="token operator">&gt;</span> workerQueues_<span class="token punctuation">;</span></span>
<span class="line">    ThreadSafeQueue<span class="token operator">&lt;</span>DBRequest<span class="token operator">&gt;</span> dbQueue_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-kbengine-多线程设计" tabindex="-1"><a class="header-anchor" href="#_2-2-kbengine-多线程设计"><span>2.2 KBEngine 多线程设计</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 多线程架构</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 进程模型:</span>
<span class="line"></span>
<span class="line">LoginApp:</span>
<span class="line">├── 主线程: 网络事件循环</span>
<span class="line">├── DB线程: 数据库操作</span>
<span class="line">└── Timer线程: 定时器处理</span>
<span class="line"></span>
<span class="line">BaseApp:</span>
<span class="line">├── 主线程: 网络事件 + 游戏逻辑</span>
<span class="line">├── DB线程: 数据库异步操作</span>
<span class="line">└── 备份线程: 数据快照</span>
<span class="line"></span>
<span class="line">CellApp:</span>
<span class="line">├── 主线程: 游戏逻辑 + 物理模拟</span>
<span class="line">└── 网络由内部网络层处理</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># KBEngine 使用混合模型: 多进程 + 进程内多线程</span></span>
<span class="line"><span class="token comment"># 优点: 结合了隔离性和共享内存的高效</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、多进程架构" tabindex="-1"><a class="header-anchor" href="#三、多进程架构"><span>三、多进程架构</span></a></h2><h3 id="_3-1-经典多进程服务器" tabindex="-1"><a class="header-anchor" href="#_3-1-经典多进程服务器"><span>3.1 经典多进程服务器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 多进程服务器 (Pre-fork 模型)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PreforkServer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">start</span><span class="token punctuation">(</span><span class="token keyword">int</span> workerCount <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 创建多个工作进程</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> workerCount<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            pid_t pid <span class="token operator">=</span> <span class="token function">fork</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>pid <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 子进程</span></span>
<span class="line">                <span class="token function">workerLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">exit</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>pid <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 父进程</span></span>
<span class="line">                workerPids_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>pid<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 父进程监控子进程</span></span>
<span class="line">        <span class="token function">monitorLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">workerLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 每个工作进程独立运行</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 处理连接和逻辑</span></span>
<span class="line">            <span class="token function">handleEvents</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">monitorLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> status<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            pid_t pid <span class="token operator">=</span> <span class="token function">wait</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>status<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">WIFEXITED</span><span class="token punctuation">(</span>status<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 子进程正常退出，重启</span></span>
<span class="line">                <span class="token function">INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Worker {} exited, restarting...&quot;</span><span class="token punctuation">,</span> pid<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">restartWorker</span><span class="token punctuation">(</span>pid<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">WIFSIGNALED</span><span class="token punctuation">(</span>status<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 子进程崩溃，重启</span></span>
<span class="line">                <span class="token function">ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Worker {} crashed, restarting...&quot;</span><span class="token punctuation">,</span> pid<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">restartWorker</span><span class="token punctuation">(</span>pid<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>pid_t<span class="token operator">&gt;</span> workerPids_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-进程间通信" tabindex="-1"><a class="header-anchor" href="#_3-2-进程间通信"><span>3.2 进程间通信</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 共享内存 + 信号量</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SharedMemoryQueue</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">create</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> name<span class="token punctuation">,</span> size_t size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 创建共享内存</span></span>
<span class="line">        fd_ <span class="token operator">=</span> <span class="token function">shm_open</span><span class="token punctuation">(</span>name<span class="token punctuation">.</span><span class="token function">c_str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> O_CREAT <span class="token operator">|</span> O_RDWR<span class="token punctuation">,</span> <span class="token number">0666</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>fd_ <span class="token operator">&lt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">ftruncate</span><span class="token punctuation">(</span>fd_<span class="token punctuation">,</span> size<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 映射到进程地址空间</span></span>
<span class="line">        data_ <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">char</span><span class="token operator">*</span><span class="token punctuation">)</span><span class="token function">mmap</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">,</span> size<span class="token punctuation">,</span></span>
<span class="line">                           PROT_READ <span class="token operator">|</span> PROT_WRITE<span class="token punctuation">,</span></span>
<span class="line">                           MAP_SHARED<span class="token punctuation">,</span> fd_<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 初始化信号量</span></span>
<span class="line">        <span class="token function">sem_init</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semEmpty_<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">,</span> size<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 空槽位</span></span>
<span class="line">        <span class="token function">sem_init</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semFull_<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span>      <span class="token comment">// 数据项</span></span>
<span class="line">        <span class="token function">sem_init</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semMutex_<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span>     <span class="token comment">// 互斥</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">send</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">sem_wait</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semEmpty_<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 等待空槽位</span></span>
<span class="line">        <span class="token function">sem_wait</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 获取锁</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 写入数据</span></span>
<span class="line">        <span class="token function">memcpy</span><span class="token punctuation">(</span>data_ <span class="token operator">+</span> writePos_<span class="token punctuation">,</span> data<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        writePos_ <span class="token operator">=</span> <span class="token punctuation">(</span>writePos_ <span class="token operator">+</span> len<span class="token punctuation">)</span> <span class="token operator">%</span> size_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">sem_post</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 释放锁</span></span>
<span class="line">        <span class="token function">sem_post</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semFull_<span class="token punctuation">)</span><span class="token punctuation">;</span>   <span class="token comment">// 增加数据项</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">receive</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">sem_wait</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semFull_<span class="token punctuation">)</span><span class="token punctuation">;</span>   <span class="token comment">// 等待数据项</span></span>
<span class="line">        <span class="token function">sem_wait</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 获取锁</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 读取数据</span></span>
<span class="line">        <span class="token function">memcpy</span><span class="token punctuation">(</span>data<span class="token punctuation">,</span> data_ <span class="token operator">+</span> readPos_<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        readPos_ <span class="token operator">=</span> <span class="token punctuation">(</span>readPos_ <span class="token operator">+</span> len<span class="token punctuation">)</span> <span class="token operator">%</span> size_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">sem_post</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 释放锁</span></span>
<span class="line">        <span class="token function">sem_post</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>semEmpty_<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 增加空槽位</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">int</span> fd_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">char</span><span class="token operator">*</span> data_<span class="token punctuation">;</span></span>
<span class="line">    size_t size_<span class="token punctuation">;</span></span>
<span class="line">    size_t readPos_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    size_t writePos_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    sem_t semEmpty_<span class="token punctuation">,</span> semFull_<span class="token punctuation">,</span> semMutex_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-架构选择" tabindex="-1"><a class="header-anchor" href="#四、kbengine-架构选择"><span>四、KBEngine 架构选择</span></a></h2><h3 id="_4-1-kbengine-的多进程架构" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-的多进程架构"><span>4.1 KBEngine 的多进程架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    KBEngine 进程模型                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  [Machine]                                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ├── [LoginApp] ─────┐                                      │</span>
<span class="line">│  │   ├── 主线程       │                                      │</span>
<span class="line">│  │   ├── DB线程       ├──&gt; [Database]                       │</span>
<span class="line">│  │   └── Timer线程    │                                      │</span>
<span class="line">│  │                    │                                      │</span>
<span class="line">│  ├── [BaseAppMgr] ────┤                                      │</span>
<span class="line">│  │   └── 主线程        │                                      │</span>
<span class="line">│  │                    │                                      │</span>
<span class="line">│  ├── [BaseApp #1] ────┤                                      │</span>
<span class="line">│  │   ├── 主线程        │      ┌─────────────┐               │</span>
<span class="line">│  │   ├── DB线程        │      │   [CellApp]  │               │</span>
<span class="line">│  │   └── 备份线程       │      │   #1         │               │</span>
<span class="line">│  │                    │      │   ├── 主线程  │               │</span>
<span class="line">│  ├── [BaseApp #2] ────┼──────┼──&gt; ├── 网络层  │               │</span>
<span class="line">│  │   └── ...          │      │   └── 物理层  │               │</span>
<span class="line">│  │                    │      └─────────────┘               │</span>
<span class="line">│  ├── [CellAppMgr] ────┤                 │                    │</span>
<span class="line">│  │   └── 主线程        │                 ▼                    │</span>
<span class="line">│  │                    │          ┌─────────────┐             │</span>
<span class="line">│  └── [DBMgr] ─────────┴──────┬───│   [CellApp]  │             │</span>
<span class="line">│      └── 多线程处理           │   │   #2         │             │</span>
<span class="line">│                             │   └─────────────┘             │</span>
<span class="line">│                             │                                │</span>
<span class="line">│                             ▼                                │</span>
<span class="line">│                      ┌─────────────┐                        │</span>
<span class="line">│                      │  Database   │                        │</span>
<span class="line">│                      └─────────────┘                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-kbengine-选择原因" tabindex="-1"><a class="header-anchor" href="#_4-2-kbengine-选择原因"><span>4.2 KBEngine 选择原因</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 选择多进程架构的原因</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">/**</span>
<span class="line"> * 1. 隔离性:</span>
<span class="line"> *    - BaseApp 崩溃不影响 CellApp</span>
<span class="line"> *    - 不同玩家分布在不同 BaseApp</span>
<span class="line"> *    - 一个进程崩溃只影响部分玩家</span>
<span class="line"> *</span>
<span class="line"> * 2. 扩展性:</span>
<span class="line"> *    - 可以动态增减 BaseApp/CellApp</span>
<span class="line"> *    - 不同进程可部署在不同机器</span>
<span class="line"> *    - 负载均衡灵活</span>
<span class="line"> *</span>
<span class="line"> * 3. 简化同步:</span>
<span class="line"> *    - 进程间通过消息通信</span>
<span class="line"> *    - 避免复杂的锁机制</span>
<span class="line"> *    - Actor 模型天然支持</span>
<span class="line"> *</span>
<span class="line"> * 4. 容错性:</span>
<span class="line"> *    - 进程崩溃易检测</span>
<span class="line"> *    - 可自动重启</span>
<span class="line"> *    - 影响范围可控</span>
<span class="line"> */</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、选择决策" tabindex="-1"><a class="header-anchor" href="#五、选择决策"><span>五、选择决策</span></a></h2><h3 id="_5-1-决策树" tabindex="-1"><a class="header-anchor" href="#_5-1-决策树"><span>5.1 决策树</span></a></h3>`,31),i(d,{code:`eJxLy8kvT85ILCpRCHHhUgACx+inexqeLu+OVdDVtVNwqn45p+HFssaXq2e8nDXl+bLd9rVgVU4g2ZpnM9bXKDhHP10y68X+2c9XdMciyT2dsKxGwaX6aevGJ7tWP12y/GV7/7OpG571roOa4IIwwRVkwvNd++EmQOTAJrhVP9m9+2nXwufda56v7n7WsByq3Q2sBOgsoAOQBJ7s7QOaxwUWcQb7wD0a4raX07coeAY4QyxwBUt5REMsfTqh59napbEQXe5gKc9obyfXvPTMvFQrhWfbtz+d0AHR6AGR5QIAcWh/xQ==`}),o[1]||=e(`<h3 id="_5-2-场景推荐" tabindex="-1"><a class="header-anchor" href="#_5-2-场景推荐"><span>5.2 场景推荐</span></a></h3><table><thead><tr><th>场景</th><th>推荐</th><th>原因</th></tr></thead><tbody><tr><td><strong>Web 服务器</strong></td><td>多进程/混合</td><td>Nginx 风格，隔离 + 响应速度</td></tr><tr><td><strong>游戏逻辑</strong></td><td>多线程/Actor</td><td>共享状态多，需要高效同步</td></tr><tr><td><strong>数据处理</strong></td><td>多进程</td><td>任务独立，崩溃隔离</td></tr><tr><td><strong>IO 密集</strong></td><td>多线程/协程</td><td>共享资源，高效等待</td></tr><tr><td><strong>MMO 服务器</strong></td><td>多进程 + Actor</td><td>KBEngine 方案，隔离 + 扩展</td></tr></tbody></table><hr><h2 id="六、混合架构" tabindex="-1"><a class="header-anchor" href="#六、混合架构"><span>六、混合架构</span></a></h2><h3 id="_6-1-kbengine-风格混合" tabindex="-1"><a class="header-anchor" href="#_6-1-kbengine-风格混合"><span>6.1 KBEngine 风格混合</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 混合架构: 多进程 + 进程内多线程</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">HybridServer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">start</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 启动多个服务进程</span></span>
<span class="line">        <span class="token function">startLoginApp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">startBaseApps</span><span class="token punctuation">(</span><span class="token number">4</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">startCellApps</span><span class="token punctuation">(</span><span class="token number">8</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">startDBMgr</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">startBaseApps</span><span class="token punctuation">(</span><span class="token keyword">int</span> count<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> count<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            pid_t pid <span class="token operator">=</span> <span class="token function">fork</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>pid <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 子进程: BaseApp</span></span>
<span class="line">                BaseApp app<span class="token punctuation">;</span></span>
<span class="line">                <span class="token comment">// 进程内多线程</span></span>
<span class="line">                app<span class="token punctuation">.</span><span class="token function">start</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 内部有主线程、DB线程、备份线程</span></span>
<span class="line">                <span class="token function">exit</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">startCellApps</span><span class="token punctuation">(</span><span class="token keyword">int</span> count<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 类似 BaseApp</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="选择准则" tabindex="-1"><a class="header-anchor" href="#选择准则"><span>选择准则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">多线程 vs 多进程选择:</span>
<span class="line"></span>
<span class="line">选择多线程:</span>
<span class="line">- 需要共享大量数据</span>
<span class="line">- 通信频繁</span>
<span class="line">- 创建/销毁频繁</span>
<span class="line">- 内存紧张</span>
<span class="line"></span>
<span class="line">选择多进程:</span>
<span class="line">- 需要高隔离性</span>
<span class="line">- 任务相对独立</span>
<span class="line">- 追求稳定性</span>
<span class="line">- 可接受通信开销</span>
<span class="line"></span>
<span class="line">KBEngine 方案:</span>
<span class="line">- 多进程实现服务隔离</span>
<span class="line">- 进程内多线程处理不同任务</span>
<span class="line">- 进程间消息通信 (Actor)</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://kbengine.github.io/docs/" target="_blank" rel="noopener noreferrer">KBEngine Architecture</a></li><li><a href="https://www.geeksforgeeks.org/difference-between-multi-threading-and-multi-programming/" target="_blank" rel="noopener noreferrer">Multi-threading vs Multi-processing</a></li><li><a href="https://www.afternerd.com/blog/difference-between-process-and-thread/" target="_blank" rel="noopener noreferrer">Process vs Thread</a></li></ul>`,13)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};