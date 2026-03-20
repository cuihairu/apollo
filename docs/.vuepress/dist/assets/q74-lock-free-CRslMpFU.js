import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q74-lock-free.html","title":"Q74: 如何设计无锁数据结构？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q74-lock-free.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q74-lock-free.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q74-如何设计无锁数据结构" tabindex="-1"><a class="header-anchor" href="#q74-如何设计无锁数据结构"><span>Q74: 如何设计无锁数据结构？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对无锁编程的理解：</p><ul><li>原子操作</li><li>CAS (Compare-And-Swap)</li><li>ABA 问题</li><li>常见无锁结构</li></ul><hr><h2 id="一、无锁基础" tabindex="-1"><a class="header-anchor" href="#一、无锁基础"><span>一、无锁基础</span></a></h2><h3 id="_1-1-原子操作" tabindex="-1"><a class="header-anchor" href="#_1-1-原子操作"><span>1.1 原子操作</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    原子操作类型                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  内存序 (Memory Order):                                     │</span>
<span class="line">│  ├── relaxed: 无序                                          │</span>
<span class="line">│  ├── acquire: 获取语义 (读屏障)                              │</span>
<span class="line">│  ├── release: 释放语义 (写屏障)                              │</span>
<span class="line">│  ├── acq_rel: 两者兼有                                      │</span>
<span class="line">│  └── seq_cst: 顺序一致性                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  原子操作:                                                   │</span>
<span class="line">│  ├── load/store: 读写                                        │</span>
<span class="line">│  ├── exchange: 交换                                         │</span>
<span class="line">│  ├── compare_exchange: CAS                                  │</span>
<span class="line">│  ├── fetch_add: 加法并返回旧值                               │</span>
<span class="line">│  └── fetch_sub: 减法并返回旧值                               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-cas-原理" tabindex="-1"><a class="header-anchor" href="#_1-2-cas-原理"><span>1.2 CAS 原理</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Compare-And-Swap (CAS)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">bool</span> <span class="token function">compareAndSwap</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>T<span class="token operator">&gt;</span><span class="token operator">*</span> ptr<span class="token punctuation">,</span> T expected<span class="token punctuation">,</span> T desired<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 原子地: 如果 *ptr == expected，则 *ptr = desired</span></span>
<span class="line">    <span class="token comment">// 返回是否成功</span></span>
<span class="line">    <span class="token keyword">return</span> ptr<span class="token operator">-&gt;</span><span class="token function">compare_exchange_weak</span><span class="token punctuation">(</span>expected<span class="token punctuation">,</span> desired<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例: 无锁计数器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LockFreeCounter</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">increment</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> oldValue <span class="token operator">=</span> value_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> newValue<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">do</span> <span class="token punctuation">{</span></span>
<span class="line">            newValue <span class="token operator">=</span> oldValue <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token comment">// 如果 value_ 仍是 oldValue，则更新为 newValue</span></span>
<span class="line">            <span class="token comment">// 否则重试</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>value_<span class="token punctuation">.</span><span class="token function">compare_exchange_weak</span><span class="token punctuation">(</span></span>
<span class="line">            oldValue<span class="token punctuation">,</span> newValue<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>memory_order_release<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>memory_order_relaxed</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">get</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> value_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_acquire<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span><span class="token keyword">int</span><span class="token operator">&gt;</span> value_<span class="token punctuation">{</span><span class="token number">0</span><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、aba-问题" tabindex="-1"><a class="header-anchor" href="#二、aba-问题"><span>二、ABA 问题</span></a></h2><h3 id="_2-1-aba-问题说明" tabindex="-1"><a class="header-anchor" href="#_2-1-aba-问题说明"><span>2.1 ABA 问题说明</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    ABA 问题                                   │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  初始状态:                                                   │</span>
<span class="line">│  ┌─────────┐                                               │</span>
<span class="line">│  │ Stack   │  Top → A → B → C                              │</span>
<span class="line">│  └─────────┘                                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  线程 1: pop A                                              │</span>
<span class="line">│  - 读取 Top = A                                             │</span>
<span class="line">│  - (被挂起)                                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  线程 2:                                                    │</span>
<span class="line">│  - pop A, pop B                                            │</span>
<span class="line">│  - push D, push A                                          │</span>
<span class="line">│  - 现在 Top → A → D (A 回来了！)                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  线程 1 恢复:                                               │</span>
<span class="line">│  - CAS(Top, A, B)                                          │</span>
<span class="line">│  - 成功！(因为 Top 还是 A)                                 │</span>
<span class="line">│  - 但 B 已经不在栈中了！                                     │</span>
<span class="line">│  - 结果: B 丢失                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-解决方案-版本号" tabindex="-1"><a class="header-anchor" href="#_2-2-解决方案-版本号"><span>2.2 解决方案: 版本号</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 带版本号的指针解决 ABA</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">VersionedPointer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Pointer</span> <span class="token punctuation">{</span></span>
<span class="line">        T<span class="token operator">*</span> ptr<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> version<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">compareAndSwap</span><span class="token punctuation">(</span>Pointer<span class="token operator">&amp;</span> expected<span class="token punctuation">,</span> <span class="token keyword">const</span> Pointer<span class="token operator">&amp;</span> desired<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// CAS 时同时比较指针和版本号</span></span>
<span class="line">        <span class="token keyword">return</span> value_<span class="token punctuation">.</span><span class="token function">compare_exchange_weak</span><span class="token punctuation">(</span></span>
<span class="line">            expected<span class="token punctuation">,</span> desired<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>memory_order_release<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>memory_order_acquire</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>Pointer<span class="token operator">&gt;</span> value_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用: 无锁栈</span></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LockFreeStack</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">push</span><span class="token punctuation">(</span>T<span class="token operator">*</span> item<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Node<span class="token operator">*</span> node <span class="token operator">=</span> <span class="token keyword">new</span> Node<span class="token punctuation">{</span>item<span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        Node<span class="token operator">*</span> oldTop <span class="token operator">=</span> top_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_acquire<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">do</span> <span class="token punctuation">{</span></span>
<span class="line">            node<span class="token operator">-&gt;</span>next <span class="token operator">=</span> oldTop<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>top_<span class="token punctuation">.</span><span class="token function">compare_exchange_weak</span><span class="token punctuation">(</span></span>
<span class="line">            oldTop<span class="token punctuation">,</span> node<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>memory_order_release<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>memory_order_acquire</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    T<span class="token operator">*</span> <span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Node<span class="token operator">*</span> oldTop <span class="token operator">=</span> top_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_acquire<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        Node<span class="token operator">*</span> newTop<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">do</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>oldTop <span class="token operator">==</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            newTop <span class="token operator">=</span> oldTop<span class="token operator">-&gt;</span>next<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>top_<span class="token punctuation">.</span><span class="token function">compare_exchange_weak</span><span class="token punctuation">(</span></span>
<span class="line">            oldTop<span class="token punctuation">,</span> newTop<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>memory_order_release<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>memory_order_acquire</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        T<span class="token operator">*</span> item <span class="token operator">=</span> oldTop<span class="token operator">-&gt;</span>data<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 延迟删除 (避免 ABA)</span></span>
<span class="line">        <span class="token function">reclaimLater</span><span class="token punctuation">(</span>oldTop<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> item<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Node</span> <span class="token punctuation">{</span></span>
<span class="line">        T<span class="token operator">*</span> data<span class="token punctuation">;</span></span>
<span class="line">        Node<span class="token operator">*</span> next<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">reclaimLater</span><span class="token punctuation">(</span>Node<span class="token operator">*</span> node<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 添加到待删除列表</span></span>
<span class="line">        <span class="token comment">// 使用 hazard pointer 或 epoch-based reclamation</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>Node<span class="token operator">*</span><span class="token operator">&gt;</span> top_<span class="token punctuation">{</span><span class="token keyword">nullptr</span><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、无锁队列" tabindex="-1"><a class="header-anchor" href="#三、无锁队列"><span>三、无锁队列</span></a></h2><h3 id="_3-1-mpmc-无锁队列" tabindex="-1"><a class="header-anchor" href="#_3-1-mpmc-无锁队列"><span>3.1 MPMC 无锁队列</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 多生产者多消费者无锁队列</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LockFreeQueue</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">explicit</span> <span class="token function">LockFreeQueue</span><span class="token punctuation">(</span>size_t capacity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 容量必须是 2 的幂</span></span>
<span class="line">        <span class="token function">assert</span><span class="token punctuation">(</span>capacity <span class="token operator">&amp;&amp;</span> <span class="token operator">!</span><span class="token punctuation">(</span>capacity <span class="token operator">&amp;</span> <span class="token punctuation">(</span>capacity <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        capacity_ <span class="token operator">=</span> capacity<span class="token punctuation">;</span></span>
<span class="line">        mask_ <span class="token operator">=</span> capacity <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 分配对齐的节点</span></span>
<span class="line">        nodes_ <span class="token operator">=</span> <span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span>Node<span class="token operator">*</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token function">aligned_alloc</span><span class="token punctuation">(</span><span class="token keyword">alignof</span><span class="token punctuation">(</span>Node<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>Node<span class="token punctuation">)</span> <span class="token operator">*</span> capacity<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> capacity<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            nodes_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>sequence<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span>i<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        enqueuePos_<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        dequeuePos_<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">~</span><span class="token function">LockFreeQueue</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> capacity_<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            nodes_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token operator">~</span><span class="token function">Node</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token function">free</span><span class="token punctuation">(</span>nodes_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">push</span><span class="token punctuation">(</span>T<span class="token operator">&amp;&amp;</span> item<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Node<span class="token operator">*</span> node<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        size_t pos <span class="token operator">=</span> enqueuePos_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            node <span class="token operator">=</span> <span class="token operator">&amp;</span>nodes_<span class="token punctuation">[</span>pos <span class="token operator">&amp;</span> mask_<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            size_t seq <span class="token operator">=</span> node<span class="token operator">-&gt;</span>sequence<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_acquire<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            intptr_t diff <span class="token operator">=</span> <span class="token punctuation">(</span>intptr_t<span class="token punctuation">)</span>seq <span class="token operator">-</span> <span class="token punctuation">(</span>intptr_t<span class="token punctuation">)</span>pos<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>diff <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 尝试获取这个位置</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>enqueuePos_<span class="token punctuation">.</span><span class="token function">compare_exchange_weak</span><span class="token punctuation">(</span></span>
<span class="line">                    pos<span class="token punctuation">,</span> pos <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">                    std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">,</span></span>
<span class="line">                    std<span class="token double-colon punctuation">::</span>memory_order_relaxed</span>
<span class="line">                <span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>diff <span class="token operator">&lt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 队列满</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                pos <span class="token operator">=</span> enqueuePos_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 写入数据</span></span>
<span class="line">        node<span class="token operator">-&gt;</span>data <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>item<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新序列号</span></span>
<span class="line">        node<span class="token operator">-&gt;</span>sequence<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span>pos <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_release<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">pop</span><span class="token punctuation">(</span>T<span class="token operator">&amp;</span> item<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Node<span class="token operator">*</span> node<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        size_t pos <span class="token operator">=</span> dequeuePos_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            node <span class="token operator">=</span> <span class="token operator">&amp;</span>nodes_<span class="token punctuation">[</span>pos <span class="token operator">&amp;</span> mask_<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            size_t seq <span class="token operator">=</span> node<span class="token operator">-&gt;</span>sequence<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_acquire<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            intptr_t diff <span class="token operator">=</span> <span class="token punctuation">(</span>intptr_t<span class="token punctuation">)</span>seq <span class="token operator">-</span> <span class="token punctuation">(</span>intptr_t<span class="token punctuation">)</span><span class="token punctuation">(</span>pos <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>diff <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>dequeuePos_<span class="token punctuation">.</span><span class="token function">compare_exchange_weak</span><span class="token punctuation">(</span></span>
<span class="line">                    pos<span class="token punctuation">,</span> pos <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">                    std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">,</span></span>
<span class="line">                    std<span class="token double-colon punctuation">::</span>memory_order_relaxed</span>
<span class="line">                <span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>diff <span class="token operator">&lt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 队列空</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                pos <span class="token operator">=</span> dequeuePos_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 读取数据</span></span>
<span class="line">        item <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>node<span class="token operator">-&gt;</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新序列号</span></span>
<span class="line">        node<span class="token operator">-&gt;</span>sequence<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span>pos <span class="token operator">+</span> mask_ <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_release<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Node</span> <span class="token punctuation">{</span></span>
<span class="line">        T data<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>size_t<span class="token operator">&gt;</span> sequence<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    Node<span class="token operator">*</span> nodes_<span class="token punctuation">;</span></span>
<span class="line">    size_t capacity_<span class="token punctuation">;</span></span>
<span class="line">    size_t mask_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>size_t<span class="token operator">&gt;</span> enqueuePos_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>size_t<span class="token operator">&gt;</span> dequeuePos_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、hazard-pointer" tabindex="-1"><a class="header-anchor" href="#四、hazard-pointer"><span>四、Hazard Pointer</span></a></h2><h3 id="_4-1-内存回收方案" tabindex="-1"><a class="header-anchor" href="#_4-1-内存回收方案"><span>4.1 内存回收方案</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Hazard Pointer - 解决无锁结构内存回收</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">HazardPointer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">Holder</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">        <span class="token function">Holder</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token operator">*</span><span class="token operator">*</span> ptr<span class="token punctuation">)</span> <span class="token operator">:</span> <span class="token function">ptr_</span><span class="token punctuation">(</span>ptr<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token operator">*</span>ptr_ <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span>  <span class="token comment">// 初始化</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">~</span><span class="token function">Holder</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token operator">*</span>ptr_ <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span>  <span class="token comment">// 清除</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">protect</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token operator">*</span> ptr<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token operator">*</span>ptr_ <span class="token operator">=</span> ptr<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">        <span class="token keyword">void</span><span class="token operator">*</span><span class="token operator">*</span> ptr_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 尝试回收节点</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">reclaim</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token operator">*</span> node<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 检查是否有 hazard pointer 指向</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> MAX_THREADS<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">void</span><span class="token operator">*</span> hazard <span class="token operator">=</span> hazardPointers_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_acquire<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>hazard <span class="token operator">==</span> node<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 有线程在使用，延迟回收</span></span>
<span class="line">                toRetract_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>node<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 安全删除</span></span>
<span class="line">        <span class="token function">deleteNode</span><span class="token punctuation">(</span>node<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 尝试回收延迟的节点</span></span>
<span class="line">        <span class="token function">retract</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span><span class="token operator">*</span> <span class="token function">getHazardPointer</span><span class="token punctuation">(</span><span class="token keyword">int</span> threadId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token operator">&amp;</span>hazardPointers_<span class="token punctuation">[</span>threadId <span class="token operator">%</span> MAX_THREADS<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">retract</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 定期尝试回收延迟节点</span></span>
<span class="line">        <span class="token comment">// ...</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">deleteNode</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token operator">*</span> node<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 根据类型删除</span></span>
<span class="line">        <span class="token comment">// ...</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_THREADS <span class="token operator">=</span> <span class="token number">64</span><span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>array<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token operator">*</span><span class="token operator">&gt;</span><span class="token punctuation">,</span> MAX_THREADS<span class="token operator">&gt;</span> hazardPointers_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token operator">*</span><span class="token operator">&gt;</span> toRetract_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、无锁数据结构应用" tabindex="-1"><a class="header-anchor" href="#五、无锁数据结构应用"><span>五、无锁数据结构应用</span></a></h2><h3 id="_5-1-游戏服务器应用" tabindex="-1"><a class="header-anchor" href="#_5-1-游戏服务器应用"><span>5.1 游戏服务器应用</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 无锁结构在游戏中的应用</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LockFreeGameServer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 1. 无锁玩家列表</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addPlayer</span><span class="token punctuation">(</span>Player<span class="token operator">*</span> player<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        playerList_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>player<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    Player<span class="token operator">*</span> <span class="token function">getNextPlayer</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Player<span class="token operator">*</span> player<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>playerList_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span>player<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> player<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 2. 无锁事件队列</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">postEvent</span><span class="token punctuation">(</span><span class="token keyword">const</span> Event<span class="token operator">&amp;</span> event<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        eventQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>event<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processEvents</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Event event<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>eventQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span>event<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">handleEvent</span><span class="token punctuation">(</span>event<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 3. 无锁引用计数</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addRef</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span>refCount_<span class="token punctuation">.</span><span class="token function">fetch_add</span><span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">releaseRef</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span>refCount_<span class="token punctuation">.</span><span class="token function">fetch_sub</span><span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_acq_rel<span class="token punctuation">)</span> <span class="token operator">==</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">reclaimEntity</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    LockFreeStack<span class="token operator">&lt;</span>Player<span class="token operator">&gt;</span> playerList_<span class="token punctuation">;</span></span>
<span class="line">    LockFreeQueue<span class="token operator">&lt;</span>Event<span class="token operator">&gt;</span> eventQueue_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、kbengine-无锁实践" tabindex="-1"><a class="header-anchor" href="#六、kbengine-无锁实践"><span>六、KBEngine 无锁实践</span></a></h2><h3 id="_6-1-kbengine-中的无锁使用" tabindex="-1"><a class="header-anchor" href="#_6-1-kbengine-中的无锁使用"><span>6.1 KBEngine 中的无锁使用</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 使用单线程事件循环避免锁</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 的无锁策略:</span>
<span class="line"></span>
<span class="line">1. 每个进程内单线程事件循环:</span>
<span class="line">   - 避免了进程内锁竞争</span>
<span class="line">   - 消息串行处理</span>
<span class="line"></span>
<span class="line">2. 进程间异步消息:</span>
<span class="line">   - 无需等待响应</span>
<span class="line">   - 避免死锁</span>
<span class="line"></span>
<span class="line">3. 实体邮件箱:</span>
<span class="line">   - 每个实体独立队列</span>
<span class="line">   - 自然并发</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># KBEngine 风格实现</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EntityMailbox</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;实体邮件箱 - 无锁队列&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entityId<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>entityId <span class="token operator">=</span> entityId</span>
<span class="line">        self<span class="token punctuation">.</span>messages <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span>  <span class="token comment"># 单线程访问，无需锁</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">post</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;发送消息 (单线程)&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>messages<span class="token punctuation">.</span>append<span class="token punctuation">(</span>message<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">process</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;处理消息 (单线程)&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">while</span> self<span class="token punctuation">.</span>messages<span class="token punctuation">:</span></span>
<span class="line">            msg <span class="token operator">=</span> self<span class="token punctuation">.</span>messages<span class="token punctuation">.</span>pop<span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>handleMessage<span class="token punctuation">(</span>msg<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="无锁编程要点" tabindex="-1"><a class="header-anchor" href="#无锁编程要点"><span>无锁编程要点</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">无锁数据结构 = 原子操作 + CAS + 内存序 + ABA 解决</span>
<span class="line">- 理解内存序</span>
<span class="line">- 注意 ABA 问题</span>
<span class="line">- 使用 Hazard Pointer</span>
<span class="line">- 调试验证复杂度高</span>
<span class="line">- KBEngine 选择 Actor 模型替代</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.manning.com/books/c-plus-plus-concurrency-in-action" target="_blank" rel="noopener noreferrer">C++ Concurrency in Action - Lock-Free Data Structures</a></li><li><a href="https://www.boost.org/doc/libs/release/libs/smart_ptr/doc/hazard_pointer_explained.html" target="_blank" rel="noopener noreferrer">Hazard Pointers</a></li><li><a href="https://preshing.com/20120612/an-introduction-to-lock-free-programming/" target="_blank" rel="noopener noreferrer">Lock-Free Programming</a></li></ul>`,39)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};