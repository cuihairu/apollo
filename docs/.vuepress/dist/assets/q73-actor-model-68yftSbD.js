import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q73-actor-model.html","title":"Q73: Actor 模型是什么？有什么优势？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q73-actor-model.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q73-actor-model.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q73-actor-模型是什么-有什么优势" tabindex="-1"><a class="header-anchor" href="#q73-actor-模型是什么-有什么优势"><span>Q73: Actor 模型是什么？有什么优势？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对 Actor 并发模型的理解：</p><ul><li>Actor 模型的核心概念</li><li>消息传递机制</li><li>在游戏服务器中的应用</li><li>与传统多线程模型的对比</li></ul><hr><h2 id="一、actor-模型基础" tabindex="-1"><a class="header-anchor" href="#一、actor-模型基础"><span>一、Actor 模型基础</span></a></h2><h3 id="_1-1-核心概念" tabindex="-1"><a class="header-anchor" href="#_1-1-核心概念"><span>1.1 核心概念</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    Actor 模型核心                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  Actor = 实体 + 邮箱 (Mailbox)                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  Actor                                            │       │</span>
<span class="line">│  │  │  ┌───────────────────────────────────┐  │       │</span>
<span class="line">│  │  │  │  Mailbox (消息队列)              │  │       │</span>
<span class="line">│  │  │  └───────────────────────────────────┘  │       │</span>
<span class="line">│  │  │                                              │       │</span>
<span class="line">│  │  │  ┌───────────────────────────────────┐  │       │</span>
<span class="line">│  │  │  │  Behavior (状态+行为)           │  │       │</span>
<span class="line">│  │  │  │  - processMessages()              │  │       │</span>
<span class="line">│  │  │  │  - handleMessage()             │  │       │</span>
<span class="line">│  │  │  └───────────────────────────────────┘  │       │</span>
<span class="line">│  │  └─────────────────────────────────────────┘       │</span>
<span class="line">│  │                                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  特点：                                                    │</span>
<span class="line">│  ├── 每个 Actor 串行处理自己的消息                        │</span>
<span class="line">│  ├── Actor 之间通过消息通信                              │</span>
<span class="line">│  ├── 无锁竞争（每个 Actor 独立队列）                     │</span>
<span class="line">│  └── 易于并发和分布式                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-actor-消息" tabindex="-1"><a class="header-anchor" href="#_1-2-actor-消息"><span>1.2 Actor 消息</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    Actor 消息类型                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 普通消息                                                │</span>
<span class="line">│     ├── 异步发送                                            │</span>
<span class="line">│     ├── 可能乱序到达                                        │</span>
<span class="line">│     └── 需要保证顺序时使用序列号                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 请求/响应消息                                            │</span>
<span class="line">│     ├── 期待回复                                            │</span>
<span class="line">│     ├── 使用 Future/Promise 模式                              │</span>
<span class="line">│     └── 支持超时                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 系统消息                                                │</span>
<span class="line">│     ├── 系统控制                                            │</span>
<span class="line">│     ├── 启动/停止                                            │</span>
<span class="line">│     └── 监控                                                │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、actor-实现" tabindex="-1"><a class="header-anchor" href="#二、actor-实现"><span>二、Actor 实现</span></a></h2><h3 id="_2-1-actor-基础类" tabindex="-1"><a class="header-anchor" href="#_2-1-actor-基础类"><span>2.1 Actor 基础类</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Actor 基础实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Actor</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// Actor ID</span></span>
<span class="line">    <span class="token keyword">using</span> ID <span class="token operator">=</span> <span class="token keyword">uint64_t</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 构造函数</span></span>
<span class="line">    <span class="token function">Actor</span><span class="token punctuation">(</span>ID id<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>string name<span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">id_</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">name_</span><span class="token punctuation">(</span>name<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">running_</span><span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 创建消息队列线程</span></span>
<span class="line">        thread_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>Actor<span class="token double-colon punctuation">::</span>messageLoop<span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 析构函数</span></span>
<span class="line">    <span class="token operator">~</span><span class="token function">Actor</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            running_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            queueCV_<span class="token punctuation">.</span><span class="token function">notify_one</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>thread_<span class="token punctuation">.</span><span class="token function">joinable</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            thread_<span class="token punctuation">.</span><span class="token function">join</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送消息</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">send</span><span class="token punctuation">(</span>ID targetId<span class="token punctuation">,</span> <span class="token keyword">const</span> T<span class="token operator">&amp;</span> message<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 查找目标 Actor</span></span>
<span class="line">        Actor<span class="token operator">*</span> target <span class="token operator">=</span> <span class="token class-name">ActorSystem</span><span class="token double-colon punctuation">::</span><span class="token function">getActor</span><span class="token punctuation">(</span>targetId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            target<span class="token operator">-&gt;</span><span class="token function">receive</span><span class="token punctuation">(</span>message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收消息</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">receive</span><span class="token punctuation">(</span><span class="token keyword">const</span> T<span class="token operator">&amp;</span> message<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 创建消息包装</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> msg <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token class-name">Message</span><span class="token double-colon punctuation">::</span><span class="token function">type</span><span class="token punctuation">(</span>message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token operator">-&gt;</span>sender <span class="token operator">=</span> <span class="token keyword">this</span><span class="token operator">-&gt;</span>id_<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token operator">-&gt;</span>data <span class="token operator">=</span> message<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 加入队列</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            messageQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            queueCV_<span class="token punctuation">.</span><span class="token function">notify_one</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 消息循环（在独立线程中运行）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">messageLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Message<span class="token operator">*</span> msg <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token punctuation">{</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                queueCV_<span class="token punctuation">.</span><span class="token function">wait</span><span class="token punctuation">(</span>lock<span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token operator">!</span>messageQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> <span class="token operator">!</span>running_<span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>running_<span class="token punctuation">)</span> <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>messageQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    msg <span class="token operator">=</span> messageQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    messageQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">handleMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">delete</span> msg<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理消息（子类实现）</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">handleMessage</span><span class="token punctuation">(</span>Message<span class="token operator">*</span> msg<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Message</span> <span class="token punctuation">{</span></span>
<span class="line">        ID sender<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>any data<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">process</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>thread thread_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex queueMutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>condition_variable queueCV_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>Message<span class="token operator">*</span><span class="token operator">&gt;</span> messageQueue_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> running_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string name_<span class="token punctuation">;</span></span>
<span class="line">    ID id_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-actor-系统" tabindex="-1"><a class="header-anchor" href="#_2-2-actor-系统"><span>2.2 Actor 系统</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Actor 管理器</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ActorSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> ActorSystem<span class="token operator">&amp;</span> <span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">static</span> ActorSystem inst<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> inst<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 注册 Actor</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token keyword">register</span><span class="token punctuation">(</span>Actor<span class="token operator">*</span> actor<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>actorsMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        actors_<span class="token punctuation">[</span>actor<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">]</span> <span class="token operator">=</span> actor<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取 Actor</span></span>
<span class="line">    Actor<span class="token operator">*</span> <span class="token function">getActor</span><span class="token punctuation">(</span>Actor<span class="token double-colon punctuation">::</span>ID id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>actorsMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> actors_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> it <span class="token operator">!=</span> actors_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">?</span> it<span class="token operator">-&gt;</span>second <span class="token operator">:</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播消息到所有 Actor</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcast</span><span class="token punctuation">(</span><span class="token keyword">const</span> T<span class="token operator">&amp;</span> message<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>actorsMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>id<span class="token punctuation">,</span> actor<span class="token punctuation">]</span> <span class="token operator">:</span> actors_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            actor<span class="token operator">-&gt;</span><span class="token function">receive</span><span class="token punctuation">(</span>message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 创建 Actor</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token punctuation">,</span> <span class="token keyword">typename</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">.</span> Args<span class="token operator">&gt;</span></span>
<span class="line">    T<span class="token operator">*</span> <span class="token function">create</span><span class="token punctuation">(</span>Args<span class="token operator">&amp;&amp;</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">.</span> args<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> actor <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">forward</span><span class="token generic class-name"><span class="token operator">&lt;</span>Args<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>args<span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">register</span><span class="token punctuation">(</span>actor<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> actor<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>Actor<span class="token double-colon punctuation">::</span>ID<span class="token punctuation">,</span> Actor<span class="token operator">*</span><span class="token operator">&gt;</span> actors_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex actorsMutex_<span class="token punctuation">;</span></span>
<span class="line">    Actor<span class="token double-colon punctuation">::</span>ID nextId_ <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 全局访问函数</span></span>
<span class="line">Actor<span class="token double-colon punctuation">::</span>ID <span class="token function">createActorID</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">return</span> <span class="token class-name">ActorSystem</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>nextId<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、actor-模式-vs-多线程" tabindex="-1"><a class="header-anchor" href="#三、actor-模式-vs-多线程"><span>三、Actor 模式 vs 多线程</span></a></h2><h3 id="_3-1-对比分析" tabindex="-1"><a class="header-anchor" href="#_3-1-对比分析"><span>3.1 对比分析</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              Actor 模式 vs 传统多线程                         │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  传统多线程：                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  共享数据结构                                       │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────────┐   │       │</span>
<span class="line">│  │  │  std::map&lt;EntityID, Entity*&gt;      │   │       │</span>
<span class="line">│  │  │  ┌──┬──┬──┬──┬──┬┐        │   │       │</span>
<span class="line">│  │  │  │E1 │E2 │E3 │E4 │... │       │       │</span>
<span class="line">│  │  │  └──┴──┴──┴──┴──┘        │   │       │</span>
<span class="line">│  │  └─────────────────────────────────────────┘   │       │</span>
<span class="line">│  │  │        ▲                    │   │       │</span>
<span class="line">│  │  │   ┌────┴────┴─────┐        │   │       │</span>
<span class="line">│  │  │   │  锁竞争           │        │   │       │</span>
<span class="line">│  │  │   └────┬────┴─────┘        │   │       │</span>
<span class="line">│  │  └─────────────────────────────────────┘       │       │</span>
<span class="line">│  │                                                     │       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  线程1  │  线程2  │  线程3  │        │       │</span>
<span class="line">│  │   │      │   │      │   │     │       │</span>
<span class="line">│  │   └──────┴──────┴─────────────┘        │       │</span>
<span class="line">│  │        │  │  │  │                         │       │</span>
<span class="line">│  │        └──┴──────────────┘          │       │</span>
<span class="line">│  │                                                     │       │</span>
<span class="line">│  问题：                                                │       │</span>
<span class="line">│  ├── 锁竞争                                             │       │</span>
<span class="line">│  ├── 上下文切换                                         │       │</span>
<span class="line">│  ├── 数据竞争                                           │       │</span>
<span class="line">│  └── 难以调试                                           │       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Actor 模式：                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  Actor1  │  Actor2  │  Actor3  │  Actor4   │       │</span>
<span class="line">│  │  │  ┌──┴──┐  │  ┌──┴──┐  │  │  ┌──┴┐     │       │</span>
<span class="line">│  │  │  │Queue1 │  │  │Queue2 │  │  │Queue4   │       │</span>
<span class="line">│  │  │  └──────┘  │  └──────┘  │  │ └──────┘   │       │</span>
<span class="line">│  │  │         │  │        │  │  │           │       │</span>
<span class="line">│  │  │  ▼       │  │        │  │  │           │       │</span>
<span class="line">│  │  │ 串行处理  │  │  │        │  │  │           │       │</span>
<span class="line">│  │  │  └────────┴─────────────────────┘  │ │           │       │</span>
<span class="line">│  │  │            无锁竞争            │  │           │       │</span>
<span class="line">│  │  │            高并发               │  │           │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘       │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-性能对比" tabindex="-1"><a class="header-anchor" href="#_3-2-性能对比"><span>3.2 性能对比</span></a></h3><table><thead><tr><th>维度</th><th>传统多线程</th><th>Actor 模型</th></tr></thead><tbody><tr><td><strong>并发模型</strong></td><td>共享内存</td><td>消息传递</td></tr><tr><td><strong>锁竞争</strong></td><td>严重</td><td>无锁</td></tr><tr><td><strong>扩展性</strong></td><td>受限于单机</td><td>易于分布式</td></tr><tr><td><strong>调试难度</strong></td><td>高</td><td>中</td></tr><tr><td><strong>容错性</strong></td><td>难</td><td>好</td></tr></tbody></table><hr><h2 id="四、游戏服务器中的-actor-应用" tabindex="-1"><a class="header-anchor" href="#四、游戏服务器中的-actor-应用"><span>四、游戏服务器中的 Actor 应用</span></a></h2><h3 id="_4-1-实体作为-actor" tabindex="-1"><a class="header-anchor" href="#_4-1-实体作为-actor"><span>4.1 实体作为 Actor</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 游戏实体作为 Actor</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EntityActor</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Actor</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">EntityActor</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">Actor</span><span class="token punctuation">(</span>id<span class="token punctuation">,</span> <span class="token string">&quot;Entity&quot;</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理移动消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onMoveMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> MoveMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 更新位置</span></span>
<span class="line">        position_ <span class="token operator">=</span> msg<span class="token punctuation">.</span>newPosition<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知 AOI 系统</span></span>
<span class="line">        <span class="token function">notifyAOIUpdate</span><span class="token punctuation">(</span><span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 广播给附近的玩家</span></span>
<span class="line">        <span class="token function">broadcastToAOI</span><span class="token punctuation">(</span>MoveBroadcastMessage<span class="token punctuation">{</span></span>
<span class="line">            entityId <span class="token operator">=</span> id_<span class="token punctuation">,</span></span>
<span class="line">            oldPosition <span class="token operator">=</span> oldPosition<span class="token punctuation">,</span></span>
<span class="line">            newPosition <span class="token operator">=</span> msg<span class="token punctuation">.</span>newPosition</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理攻击消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onAttackMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> AttackMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        EntityID targetId <span class="token operator">=</span> msg<span class="token punctuation">.</span>targetId<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送攻击请求到 CellApp</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> cellApp <span class="token operator">=</span> <span class="token function">getCellAppForEntity</span><span class="token punctuation">(</span>id_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        cellApp<span class="token operator">-&gt;</span><span class="token function">sendAttackRequest</span><span class="token punctuation">(</span>id<span class="token punctuation">,</span> targetId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 等待结果</span></span>
<span class="line">        pendingAttacks_<span class="token punctuation">[</span>msg<span class="token punctuation">.</span>sequence<span class="token punctuation">]</span> <span class="token operator">=</span> msg<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理伤害结果</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onDamageResult</span><span class="token punctuation">(</span><span class="token keyword">const</span> DamageResult<span class="token operator">&amp;</span> result<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 更新血量</span></span>
<span class="line">        hp_ <span class="token operator">-=</span> result<span class="token punctuation">.</span>damage<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>hp_ <span class="token operator">&lt;=</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">onDeath</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知客户端</span></span>
<span class="line">        <span class="token function">sendToClient</span><span class="token punctuation">(</span>DamageNotification<span class="token punctuation">{</span></span>
<span class="line">            entityId <span class="token operator">=</span> id_<span class="token punctuation">,</span></span>
<span class="line">            damage <span class="token operator">=</span> result<span class="token punctuation">.</span>damage<span class="token punctuation">,</span></span>
<span class="line">            newHp <span class="token operator">=</span> hp_</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    Position position_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> hp_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> AttackMessage<span class="token operator">&gt;</span> pendingAttacks_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-系统作为-actor" tabindex="-1"><a class="header-anchor" href="#_4-2-系统作为-actor"><span>4.2 系统作为 Actor</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 系统服务 Actor</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CombatSystemActor</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Actor</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">CombatSystemActor</span><span class="token punctuation">(</span>ActorID id<span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">Actor</span><span class="token punctuation">(</span>id<span class="token punctuation">,</span> <span class="token string">&quot;CombatSystem&quot;</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理战斗匹配</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onMatchRequest</span><span class="token punctuation">(</span><span class="token keyword">const</span> MatchRequest<span class="token operator">&amp;</span> request<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 查找合适的战斗服务器</span></span>
<span class="line">        CellApp<span class="token operator">*</span> target <span class="token operator">=</span> <span class="token function">findBestCellApp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 创建战斗</span></span>
<span class="line">            BattleInstance battle <span class="token operator">=</span> <span class="token function">createBattle</span><span class="token punctuation">(</span>request<span class="token punctuation">.</span>players<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 通知所有玩家</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span> player <span class="token operator">:</span> request<span class="token punctuation">.</span>players<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">send</span><span class="token punctuation">(</span>player<span class="token punctuation">.</span>actorId<span class="token punctuation">,</span> BattleReadyMessage<span class="token punctuation">{</span></span>
<span class="line">                    battleId <span class="token operator">=</span> battle<span class="token punctuation">.</span>id<span class="token punctuation">,</span></span>
<span class="line">                    serverAddr <span class="token operator">=</span> target<span class="token operator">-&gt;</span><span class="token function">getAddress</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 战斗循环</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onBattleLoop</span><span class="token punctuation">(</span><span class="token keyword">const</span> BattleTick<span class="token operator">&amp;</span> tick<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 更新战斗状态</span></span>
<span class="line">        Battle<span class="token operator">*</span> battle <span class="token operator">=</span> <span class="token function">getBattle</span><span class="token punctuation">(</span>tick<span class="token punctuation">.</span>battleId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 处理逻辑</span></span>
<span class="line">        battle<span class="token operator">-&gt;</span><span class="token function">update</span><span class="token punctuation">(</span>tick<span class="token punctuation">.</span>deltaTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查战斗是否结束</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>battle<span class="token operator">-&gt;</span><span class="token function">isFinished</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">endBattle</span><span class="token punctuation">(</span>battle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、分布式-actor" tabindex="-1"><a class="header-anchor" href="#五、分布式-actor"><span>五、分布式 Actor</span></a></h2><h3 id="_5-1-跨节点-actor" tabindex="-1"><a class="header-anchor" href="#_5-1-跨节点-actor"><span>5.1 跨节点 Actor</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│               分布式 Actor 系统                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│   Server 1                   Server 2                   │</span>
<span class="line">│  ┌───────────┐  ┌─────────────┐                    │</span>
<span class="line">│  │ Player A  │  │  │ NPC X     │                    │</span>
<span class="line">│  │  Actor   │  │  │ Actor     │                    │</span>
<span class="line">│  │         │  └─────────────┘                    │</span>
<span class="line">│  └───────────┘  └─────────────┘                    │</span>
<span class="line">│       │                │                                │</span>
<span class="line">│       │                │  NNG (REQ/REP)             │</span>
<span class="line">│       │                │                                │</span>
<span class="line">│       ▼                ▼                                │</span>
<span class="line">│   ┌─────────────────────────────────────┐                     │</span>
<span class="line">│   │       Actor Registry (Redis)          │                     │</span>
<span class="line">│   │  - Actor 注册                   │                     │</span>
<span class="line">│   │  - 服务发现                   │                     │</span>
<span class="line">│   │  - 负载均衡                   │                     │</span>
<span class="line">│   └─────────────────────────────────────┘                     │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-actor-注册" tabindex="-1"><a class="header-anchor" href="#_5-2-actor-注册"><span>5.2 Actor 注册</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Actor 注册表 (Redis)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ActorRegistry</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 注册 Actor</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token keyword">register</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> name<span class="token punctuation">,</span></span>
<span class="line">                     <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> address<span class="token punctuation">,</span></span>
<span class="line">                     <span class="token keyword">uint16_t</span> port<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string key <span class="token operator">=</span> <span class="token string">&quot;actor:&quot;</span> <span class="token operator">+</span> name<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 使用 Redis Hash 存储</span></span>
<span class="line">        redis_<span class="token operator">-&gt;</span><span class="token function">hset</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> <span class="token string">&quot;name&quot;</span><span class="token punctuation">,</span> name<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        redis_<span class="token operator">-&gt;</span><span class="token function">hset</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> <span class="token string">&quot;address&quot;</span><span class="token punctuation">,</span> address<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        redis_<span class="token operator">-&gt;</span><span class="token function">hset</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> <span class="token string">&quot;port&quot;</span><span class="token punctuation">,</span> port<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 设置过期时间</span></span>
<span class="line">        redis_<span class="token operator">-&gt;</span><span class="token function">expire</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> <span class="token number">60</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 60秒</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发现 Actor</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">find</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> name<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string key <span class="token operator">=</span> <span class="token string">&quot;actor:&quot;</span> <span class="token operator">+</span> name<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> addr <span class="token operator">=</span> redis_<span class="token operator">-&gt;</span><span class="token function">hget</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> <span class="token string">&quot;address&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>addr<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>string port <span class="token operator">=</span> redis_<span class="token operator">-&gt;</span><span class="token function">hget</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> <span class="token string">&quot;port&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> addr <span class="token operator">+</span> <span class="token string">&quot;:&quot;</span> <span class="token operator">+</span> port<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token string">&quot;&quot;</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 心跳保持</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">heartbeat</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> name<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string key <span class="token operator">=</span> <span class="token string">&quot;actor:&quot;</span> <span class="token operator">+</span> name<span class="token punctuation">;</span></span>
<span class="line">        redis_<span class="token operator">-&gt;</span><span class="token function">expire</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> <span class="token number">60</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 刷新过期时间</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-actor-设计原则" tabindex="-1"><a class="header-anchor" href="#_6-1-actor-设计原则"><span>6.1 Actor 设计原则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  Actor 设计原则                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 单一职责                                              │</span>
<span class="line">│     ├── 每个 Actor 只做一件事                               │</span>
<span class="line">│     ├── 避免上帝对象                                       │</span>
<span class="line">│     └── 保持简单，专注                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 消息不可变                                              │</span>
<span class="line">│     ├── 消息创建后不可修改                                 │</span>
<span class="line">│     ├── 避免共享状态                                       │</span>
<span class="line">│     └── 保持幂等                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 错误处理                                                │</span>
<span class="line">│     ├── 监督策略 (Let It Crash)                             │</span>
<span class="line">│     ├── 隔离机制 (Bulkhead)                                │</span>
<span class="line">│     └── 重试机制                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 有限状态机                                              │</span>
<span class="line">│     ├── 状态转换明确                                       │</span>
<span class="line">│     ├── 避免过多状态                                       │</span>
<span class="line">│     └── 状态转换可观测                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘       │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-常见陷阱" tabindex="-1"><a class="header-anchor" href="#_6-2-常见陷阱"><span>6.2 常见陷阱</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  Actor 常见陷阱                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  ❌ 避免的做法：                                             │</span>
<span class="line">│     ├── Actor 之间直接调用方法                                 │</span>
<span class="line">│     │   └─ 不通过消息，破坏模型                       │</span>
<span class="line">│     ├── 共享可变状态                                       │</span>
<span class="line">│     │   └─ 导致数据竞争                               │</span>
<span class="line">│     ├── 阻塞消息处理                                         │</span>
<span class="line">│     │   └─ 阻塞整个 Actor                           │</span>
<span class="line">│     └── 包含大量数据                                         │</span>
<span class="line">│         └─ └─ 导致消息传递慢                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ✅ 推荐的做法：                                           │</span>
<span class="line">│     ├── 所有交互通过消息                                   │</span>
<span class="line">│     ├── 消息使用不可变类型                                 │</span>
<span class="line">│     ├── 快速处理消息                                         │</span>
<span class="line">│     ├── 使用异步消息                                         │</span>
<span class="line">│     └── 数据存储在外部                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘       │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="actor-模型总结" tabindex="-1"><a class="header-anchor" href="#actor-模型总结"><span>Actor 模型总结</span></a></h3><table><thead><tr><th>优势</th><th>说明</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>无锁竞争</strong></td><td>每个 Actor 独立队列</td><td>高并发</td></tr><tr><td><strong>易扩展</strong></td><td>Actor 可分布式</td><td>分布式系统</td></tr><tr><td><strong>容错好</strong></td><td>Actor 隔离故障</td><td>高可用</td></tr><tr><td><strong>测试简单</strong></td><td>每个 Actor 独立测试</td><td>单元测试</td></tr><tr><td><strong>延迟高</strong></td><td>消息传递开销</td><td>实时性要求低的场景</td></tr></tbody></table><h3 id="何时使用-actor" tabindex="-1"><a class="header-anchor" href="#何时使用-actor"><span>何时使用 Actor</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">✅ 推荐使用 Actor 的场景：</span>
<span class="line">- 大量独立实体（玩家、NPC、怪物）</span>
<span class="line">- 需要高并发处理</span>
<span class="line">- 分布式部署</span>
<span class="line">- 容错性要求高</span>
<span class="line"></span>
<span class="line">❌ 不推荐使用 Actor 的场景：</span>
<span class="line">- 计算密集型任务（单线程更快）</span>
<span class="line">- 需要大量共享状态</span>
<span class="line">- 简单的 CRUD 操作</span>
<span class="line">- 实时性要求极高的场景</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.semanticscholar.org/paper/7210807/" target="_blank" rel="noopener noreferrer">Actor Model 论文</a></li><li><a href="https://www.erlang.org/doc/getting_started/concurrency.html" target="_blank" rel="noopener noreferrer">Erlang/OTP 实现参考</a></li><li><a href="https://getakka.net/" target="_blank" rel="noopener noreferrer">Akka.NET Actor 模型</a></li></ul>`,49)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};