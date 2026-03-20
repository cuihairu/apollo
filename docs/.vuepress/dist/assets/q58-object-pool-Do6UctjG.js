import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q58-object-pool.html","title":"Q58: 对象池是什么？如何设计？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q58-object-pool.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q58-object-pool.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q58-对象池是什么-如何设计" tabindex="-1"><a class="header-anchor" href="#q58-对象池是什么-如何设计"><span>Q58: 对象池是什么？如何设计？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对对象池（Object Pool）设计模式的理解：</p><ul><li>对象池的原理和作用</li><li>游戏服务器中的应用</li><li>KBEngine 的对象池实现</li><li>高性能对象池设计</li></ul><hr><h2 id="一、对象池基础" tabindex="-1"><a class="header-anchor" href="#一、对象池基础"><span>一、对象池基础</span></a></h2><h3 id="_1-1-什么是对象池" tabindex="-1"><a class="header-anchor" href="#_1-1-什么是对象池"><span>1.1 什么是对象池</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    对象池概念                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  对象池：预先创建一组对象，按需分配和回收                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  不使用对象池：                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  每次创建新对象                                   │       │</span>
<span class="line">│  │  │                                                   │       │</span>
<span class="line">│  │  ▼                                                   │       │</span>
<span class="line">│  │  new Packet()    new Packet()    new Packet()        │       │</span>
<span class="line">│  │     分配           分配           分配                │       │</span>
<span class="line">│  │        │           │            │                    │       │</span>
<span class="line">│  │        ▼           ▼            ▼                    │       │</span>
<span class="line">│  │     delete        delete        delete              │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  问题：                                            │       │</span>
<span class="line">│  │  ├── 内存碎片                                       │       │</span>
<span class="line">│  │  ├── 分配/释放开销                                  │       │</span>
<span class="line">│  │  └── 不确定性的延迟                                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  使用对象池：                                                │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  对象池预先分配 100 个对象                         │       │</span>
<span class="line">│  │  │                                                   │       │</span>
<span class="line">│  │  [空闲: obj1, obj2, ...] [使用: obj3, obj4]      │       │</span>
<span class="line">│  │         ▲                                     │       │</span>
<span class="line">│  │  │ acquire() - 快速分配                            │       │</span>
<span class="line">│  │  │                                                   │       │</span>
<span class="line">│  │  obj1 ─────► 使用                                 │       │</span>
<span class="line">│  │         │                                           │       │</span>
<span class="line">│  │         ▲ release() - 回收                            │       │</span>
<span class="line">│  │         │                                           │       │</span>
<span class="line">│  │  └── [空闲: obj1] [使用: obj3, obj4]              │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优势：                                            │       │</span>
<span class="line">│  │  ├── 减少内存分配次数                               │       │</span>
<span class="line">│  │  ├── 避免内存碎片                                   │       │</span>
<span class="line">│  │  ├── 降低内存开销                                   │       │</span>
<span class="line">│  │  └── 分配速度快                                    │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-对象池适用场景" tabindex="-1"><a class="header-anchor" href="#_1-2-对象池适用场景"><span>1.2 对象池适用场景</span></a></h3><table><thead><tr><th>场景</th><th>适用对象</th><th>原因</th></tr></thead><tbody><tr><td><strong>高频分配</strong></td><td>Packet, Message</td><td>减少分配开销</td></tr><tr><td><strong>大小固定</strong></td><td>Entity, Bullet</td><td>避免碎片</td></tr><tr><td><strong>创建昂贵</strong></td><td>Connection, Thread</td><td>降低创建成本</td></tr><tr><td><strong>周期使用</strong></td><td>SkillEffect, Buff</td><td>复用对象</td></tr></tbody></table><hr><h2 id="二、对象池设计" tabindex="-1"><a class="header-anchor" href="#二、对象池设计"><span>二、对象池设计</span></a></h2><h3 id="_2-1-基础对象池" tabindex="-1"><a class="header-anchor" href="#_2-1-基础对象池"><span>2.1 基础对象池</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 基础对象池实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ObjectPool</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 构造：预分配对象</span></span>
<span class="line">    <span class="token function">ObjectPool</span><span class="token punctuation">(</span>size_t initialSize <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> initialSize<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 析构：回收所有对象</span></span>
<span class="line">    <span class="token operator">~</span><span class="token function">ObjectPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取对象</span></span>
<span class="line">    T<span class="token operator">*</span> <span class="token function">acquire</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        T<span class="token operator">*</span> obj <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>freeList_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            obj <span class="token operator">=</span> freeList_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">pop_back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 池空，分配新对象</span></span>
<span class="line">            obj <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> obj<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 回收对象</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">release</span><span class="token punctuation">(</span>T<span class="token operator">*</span> obj<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>obj<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            obj<span class="token operator">-&gt;</span><span class="token function">reset</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 重置对象状态</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>obj<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 清空对象池</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>T<span class="token operator">*</span> obj <span class="token operator">:</span> freeList_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">delete</span> obj<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        freeList_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取统计信息</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Stats</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t totalAllocated<span class="token punctuation">;</span>    <span class="token comment">// 总分配数</span></span>
<span class="line">        size_t currentlyInUse<span class="token punctuation">;</span>     <span class="token comment">// 当前使用数</span></span>
<span class="line">        size_t peakUsage<span class="token punctuation">;</span>         <span class="token comment">// 峰值使用</span></span>
<span class="line">        size_t poolSize<span class="token punctuation">;</span>          <span class="token comment">// 池大小</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    Stats <span class="token function">getStats</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        Stats s<span class="token punctuation">;</span></span>
<span class="line">        s<span class="token punctuation">.</span>totalAllocated <span class="token operator">=</span> totalAllocated_<span class="token punctuation">;</span></span>
<span class="line">        s<span class="token punctuation">.</span>currentlyInUse <span class="token operator">=</span> allocatedCount_<span class="token punctuation">;</span></span>
<span class="line">        s<span class="token punctuation">.</span>peakUsage <span class="token operator">=</span> peakUsage_<span class="token punctuation">;</span></span>
<span class="line">        s<span class="token punctuation">.</span>poolSize <span class="token operator">=</span> freeList_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">+</span> allocatedCount_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> s<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>T<span class="token operator">*</span><span class="token operator">&gt;</span> freeList_<span class="token punctuation">;</span></span>
<span class="line">    size_t allocatedCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    size_t peakUsage_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    size_t totalAllocated_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-线程安全对象池" tabindex="-1"><a class="header-anchor" href="#_2-2-线程安全对象池"><span>2.2 线程安全对象池</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 线程安全对象池</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ThreadSafeObjectPool</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">ThreadSafeObjectPool</span><span class="token punctuation">(</span>size_t initialSize <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> initialSize<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">~</span><span class="token function">ThreadSafeObjectPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取对象</span></span>
<span class="line">    T<span class="token operator">*</span> <span class="token function">acquire</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        T<span class="token operator">*</span> obj <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>freeList_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            obj <span class="token operator">=</span> freeList_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">pop_back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            obj <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        allocatedCount_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">        peakUsage_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">max</span><span class="token punctuation">(</span>peakUsage_<span class="token punctuation">,</span> allocatedCount_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> obj<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 回收对象</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">release</span><span class="token punctuation">(</span>T<span class="token operator">*</span> obj<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>obj<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        obj<span class="token operator">-&gt;</span><span class="token function">reset</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>obj<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            allocatedCount_<span class="token operator">--</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>T<span class="token operator">*</span><span class="token operator">&gt;</span> freeList_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex mutex_<span class="token punctuation">;</span></span>
<span class="line">    size_t allocatedCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    size_t peakUsage_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、kbengine-对象池" tabindex="-1"><a class="header-anchor" href="#三、kbengine-对象池"><span>三、KBEngine 对象池</span></a></h2><h3 id="_3-1-kbengine-对象池实现" tabindex="-1"><a class="header-anchor" href="#_3-1-kbengine-对象池实现"><span>3.1 KBEngine 对象池实现</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine 源码</a>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 对象池实现</span></span>
<span class="line"><span class="token comment">// src/lib/helpers/objectpool.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">ObjectPool</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">        <span class="token keyword">static</span> T<span class="token operator">*</span> <span class="token function">create</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token function">objPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">create</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">static</span> <span class="token keyword">void</span> <span class="token function">reclaim</span><span class="token punctuation">(</span>T<span class="token operator">*</span> obj<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">objPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">reclaim</span><span class="token punctuation">(</span>obj<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">static</span> <span class="token keyword">void</span> <span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">objPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">static</span> size_t <span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token function">objPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">static</span> size_t <span class="token function">totalCount</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token function">objPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">totalCount</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">        <span class="token keyword">struct</span> <span class="token class-name">Pool</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token operator">~</span><span class="token function">Pool</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            T<span class="token operator">*</span> <span class="token function">create</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                T<span class="token operator">*</span> obj <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>objects_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    obj <span class="token operator">=</span> objects_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    objects_<span class="token punctuation">.</span><span class="token function">pop_back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                    obj <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    totalCount_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">                    peakCount_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">max</span><span class="token punctuation">(</span>peakCount_<span class="token punctuation">,</span> totalCount_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">                allocatedCount_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">return</span> obj<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">void</span> <span class="token function">reclaim</span><span class="token punctuation">(</span>T<span class="token operator">*</span> obj<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>obj<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    objects_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>obj<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    allocatedCount_<span class="token operator">--</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">void</span> <span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span>T<span class="token operator">*</span> obj <span class="token operator">:</span> objects_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">delete</span> obj<span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">                objects_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                allocatedCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            size_t <span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> objects_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            size_t <span class="token function">totalCount</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> totalCount_<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>T<span class="token operator">*</span><span class="token operator">&gt;</span> objects_<span class="token punctuation">;</span></span>
<span class="line">            size_t allocatedCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">            size_t totalCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">            size_t peakCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">static</span> Pool<span class="token operator">&amp;</span> <span class="token function">objPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">static</span> Pool pool<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> pool<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">Packet</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">reset</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 重置状态</span></span>
<span class="line">        data<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用对象池</span></span>
<span class="line">Packet<span class="token operator">*</span> packet <span class="token operator">=</span> <span class="token class-name">ObjectPool</span><span class="token operator">&lt;</span>Packet<span class="token operator">&gt;</span><span class="token double-colon punctuation">::</span><span class="token function">create</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token comment">// 使用 packet...</span></span>
<span class="line"><span class="token class-name">ObjectPool</span><span class="token operator">&lt;</span>Packet<span class="token operator">&gt;</span><span class="token double-colon punctuation">::</span><span class="token function">reclaim</span><span class="token punctuation">(</span>packet<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-kbengine-内置对象池" tabindex="-1"><a class="header-anchor" href="#_3-2-kbengine-内置对象池"><span>3.2 KBEngine 内置对象池</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 预定义对象池</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// Bundle 消息包对象池</span></span>
<span class="line">    <span class="token keyword">using</span> BundlePool <span class="token operator">=</span> ObjectPool<span class="token operator">&lt;</span>Bundle<span class="token operator">&gt;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Channel 通道对象池</span></span>
<span class="line">    <span class="token keyword">using</span> ChannelPool <span class="token operator">=</span> ObjectPool<span class="token operator">&lt;</span>Channel<span class="token operator">&gt;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Mailbox 邮箱对象池</span></span>
<span class="line">    <span class="token keyword">using</span> MailboxPool <span class="token operator">=</span> ObjectPool<span class="token operator">&lt;</span>Mailbox<span class="token operator">&gt;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// TelnetSession 会话对象池</span></span>
<span class="line">    <span class="token keyword">using</span> TelnetSessionPool <span class="token operator">=</span> ObjectPool<span class="token operator">&lt;</span>TelnetSession<span class="token operator">&gt;</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、高级对象池设计" tabindex="-1"><a class="header-anchor" href="#四、高级对象池设计"><span>四、高级对象池设计</span></a></h2><h3 id="_4-1-分层对象池" tabindex="-1"><a class="header-anchor" href="#_4-1-分层对象池"><span>4.1 分层对象池</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 分层对象池（按大小分类）</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TieredObjectPool</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">Size</span> <span class="token punctuation">{</span></span>
<span class="line">        SMALL<span class="token punctuation">,</span>    <span class="token comment">// 64 bytes</span></span>
<span class="line">        MEDIUM<span class="token punctuation">,</span>   <span class="token comment">// 256 bytes</span></span>
<span class="line">        LARGE     <span class="token comment">// 1024 bytes</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token function">TieredObjectPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        pools_<span class="token punctuation">[</span>Size<span class="token double-colon punctuation">::</span>SMALL<span class="token punctuation">]</span> <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">make_unique</span><span class="token generic class-name"><span class="token operator">&lt;</span>ObjectPool<span class="token operator">&lt;</span>T<span class="token operator">&gt;&gt;</span></span></span><span class="token punctuation">(</span><span class="token number">1000</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        pools_<span class="token punctuation">[</span>Size<span class="token double-colon punctuation">::</span>MEDIUM<span class="token punctuation">]</span> <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">make_unique</span><span class="token generic class-name"><span class="token operator">&lt;</span>ObjectPool<span class="token operator">&lt;</span>T<span class="token operator">&gt;&gt;</span></span></span><span class="token punctuation">(</span><span class="token number">100</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        pools_<span class="token punctuation">[</span>Size<span class="token double-colon punctuation">::</span>LARGE<span class="token punctuation">]</span> <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">make_unique</span><span class="token generic class-name"><span class="token operator">&lt;</span>ObjectPool<span class="token operator">&lt;</span>T<span class="token operator">&gt;&gt;</span></span></span><span class="token punctuation">(</span><span class="token number">10</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    T<span class="token operator">*</span> <span class="token function">acquire</span><span class="token punctuation">(</span>size_t size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Size tier <span class="token operator">=</span> <span class="token function">getSizeTier</span><span class="token punctuation">(</span>size<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> pools_<span class="token punctuation">[</span>tier<span class="token punctuation">]</span><span class="token operator">-&gt;</span><span class="token function">acquire</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">release</span><span class="token punctuation">(</span>T<span class="token operator">*</span> obj<span class="token punctuation">,</span> size_t size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Size tier <span class="token operator">=</span> <span class="token function">getSizeTier</span><span class="token punctuation">(</span>size<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        pools_<span class="token punctuation">[</span>tier<span class="token punctuation">]</span><span class="token operator">-&gt;</span><span class="token function">release</span><span class="token punctuation">(</span>obj<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    Size <span class="token function">getSizeTier</span><span class="token punctuation">(</span>size_t size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>size <span class="token operator">&lt;=</span> <span class="token number">64</span><span class="token punctuation">)</span> <span class="token keyword">return</span> Size<span class="token double-colon punctuation">::</span>SMALL<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>size <span class="token operator">&lt;=</span> <span class="token number">256</span><span class="token punctuation">)</span> <span class="token keyword">return</span> Size<span class="token double-colon punctuation">::</span>MEDIUM<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> Size<span class="token double-colon punctuation">::</span>LARGE<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>Size<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>ObjectPool<span class="token operator">&lt;</span>T<span class="token operator">&gt;&gt;</span><span class="token operator">&gt;</span> pools_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-自适应对象池" tabindex="-1"><a class="header-anchor" href="#_4-2-自适应对象池"><span>4.2 自适应对象池</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 自适应对象池（动态调整大小）</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AdaptiveObjectPool</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">AdaptiveObjectPool</span><span class="token punctuation">(</span>size_t initialSize <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">minSize_</span><span class="token punctuation">(</span>initialSize<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">maxSize_</span><span class="token punctuation">(</span>initialSize <span class="token operator">*</span> <span class="token number">10</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">resize</span><span class="token punctuation">(</span>initialSize<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    T<span class="token operator">*</span> <span class="token function">acquire</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>freeList_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 扩容</span></span>
<span class="line">            size_t newSize <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">min</span><span class="token punctuation">(</span>capacity_ <span class="token operator">*</span> <span class="token number">2</span><span class="token punctuation">,</span> maxSize_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">resize</span><span class="token punctuation">(</span>newSize<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        T<span class="token operator">*</span> obj <span class="token operator">=</span> freeList_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        freeList_<span class="token punctuation">.</span><span class="token function">pop_back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        allocated_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">        peakUsage_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">max</span><span class="token punctuation">(</span>peakUsage_<span class="token punctuation">,</span> allocated_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> obj<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">release</span><span class="token punctuation">(</span>T<span class="token operator">*</span> obj<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>obj<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        obj<span class="token operator">-&gt;</span><span class="token function">reset</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>obj<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            allocated_<span class="token operator">--</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 空闲时收缩</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>allocated_ <span class="token operator">&lt;</span> capacity_ <span class="token operator">/</span> <span class="token number">4</span> <span class="token operator">&amp;&amp;</span> capacity_ <span class="token operator">&gt;</span> minSize_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">shrink</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">resize</span><span class="token punctuation">(</span>size_t newSize<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>newSize <span class="token operator">&lt;</span> freeList_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 收缩</span></span>
<span class="line">            <span class="token keyword">while</span> <span class="token punctuation">(</span>freeList_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> newSize<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">delete</span> freeList_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                freeList_<span class="token punctuation">.</span><span class="token function">pop_back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 扩容</span></span>
<span class="line">            <span class="token keyword">while</span> <span class="token punctuation">(</span>freeList_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> newSize<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                freeList_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        capacity_ <span class="token operator">=</span> newSize<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">shrink</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t targetSize <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">max</span><span class="token punctuation">(</span>minSize_<span class="token punctuation">,</span> capacity_ <span class="token operator">/</span> <span class="token number">2</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">resize</span><span class="token punctuation">(</span>targetSize<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>T<span class="token operator">*</span><span class="token operator">&gt;</span> freeList_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex mutex_<span class="token punctuation">;</span></span>
<span class="line">    size_t capacity_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    size_t allocated_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    size_t peakUsage_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    size_t minSize_<span class="token punctuation">;</span></span>
<span class="line">    size_t maxSize_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、性能对比" tabindex="-1"><a class="header-anchor" href="#五、性能对比"><span>五、性能对比</span></a></h2><h3 id="_5-1-性能测试" tabindex="-1"><a class="header-anchor" href="#_5-1-性能测试"><span>5.1 性能测试</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 性能测试对比</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;chrono&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;iostream&gt;</span></span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">benchmarkPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">const</span> <span class="token keyword">int</span> N <span class="token operator">=</span> <span class="token number">100000</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    ObjectPool<span class="token operator">&lt;</span>Packet<span class="token operator">&gt;</span> <span class="token function">pool</span><span class="token punctuation">(</span><span class="token number">1000</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">auto</span> start <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>high_resolution_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Packet<span class="token operator">*</span><span class="token operator">&gt;</span> packets<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> N<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Packet<span class="token operator">*</span> p <span class="token operator">=</span> pool<span class="token punctuation">.</span><span class="token function">acquire</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        packets<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>p<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> p <span class="token operator">:</span> packets<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        pool<span class="token punctuation">.</span><span class="token function">release</span><span class="token punctuation">(</span>p<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">auto</span> end <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>high_resolution_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">auto</span> duration <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">duration_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>microseconds<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>end <span class="token operator">-</span> start<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>cout <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;Pool: &quot;</span> <span class="token operator">&lt;&lt;</span> duration<span class="token punctuation">.</span><span class="token function">count</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> <span class="token string">&quot; us\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">benchmarkNew</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">const</span> <span class="token keyword">int</span> N <span class="token operator">=</span> <span class="token number">100000</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">auto</span> start <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>high_resolution_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Packet<span class="token operator">*</span><span class="token operator">&gt;</span> packets<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> N<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Packet<span class="token operator">*</span> p <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">Packet</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        packets<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>p<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> p <span class="token operator">:</span> packets<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">delete</span> p<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">auto</span> end <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>high_resolution_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">auto</span> duration <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">duration_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>microseconds<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>end <span class="token operator">-</span> start<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>cout <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;New/Delete: &quot;</span> <span class="token operator">&lt;&lt;</span> duration<span class="token punctuation">.</span><span class="token function">count</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> <span class="token string">&quot; us\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 结果示例:</span></span>
<span class="line"><span class="token comment">// Pool: 15234 us</span></span>
<span class="line"><span class="token comment">// New/Delete: 45678 us</span></span>
<span class="line"><span class="token comment">// 性能提升: 3x</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-内存对比" tabindex="-1"><a class="header-anchor" href="#_5-2-内存对比"><span>5.2 内存对比</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              内存分配对比 (100,000 次操作)                 │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  new/delete:                                               │</span>
<span class="line">│  ├── 总分配次数: 100,000                                  │</span>
<span class="line">│  ├── 内存碎片: 高                                         │</span>
<span class="line">│  ├── 分配时间: ~45ms                                       │</span>
<span class="line">│  └── 内存峰值: ~80MB                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  对象池:                                                   │</span>
<span class="line">│  ├── 总分配次数: 0 (复用)                                │</span>
<span class="line">│  ├── 内存碎片: 低                                         │</span>
<span class="line">│  ├── 分配时间: ~15ms                                       │</span>
<span class="line">│  └── 内存峰值: ~60MB (预分配)                             │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-设计原则" tabindex="-1"><a class="header-anchor" href="#_6-1-设计原则"><span>6.1 设计原则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  对象池设计原则                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 对象必须可重置                                         │</span>
<span class="line">│     ├── 提供 reset() 方法                                  │</span>
<span class="line">│     ├── 清理所有状态                                       │</span>
<span class="line">│     ├── 避免残留数据                                       │</span>
<span class="line">│     └── 确保可安全复用                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 对象大小一致                                           │</span>
<span class="line">│     ├── 避免大小差异导致的问题                             │</span>
<span class="line">│     ├── 统一分配固定大小                                   │</span>
<span class="line">│     └── 或使用分层池                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 线程安全                                                │</span>
<span class="line">│     ├── 使用互斥锁                                         │</span>
<span class="line">│     ├── 或使用无锁队列                                     │</span>
<span class="line">│     └── 避免死锁                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 适度池大小                                               │</span>
<span class="line">│     ├── 预分配合理数量                                       │</span>
<span class="line">│     ├── 动态调整池大小                                     │</span>
<span class="line">│     └── 避免内存浪费                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-使用注意事项" tabindex="-1"><a class="header-anchor" href="#_6-2-使用注意事项"><span>6.2 使用注意事项</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  对象池使用注意                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  ✓ 推荐：                                                  │</span>
<span class="line">│     ├── 频繁创建/销毁的小对象                              │</span>
<span class="line">│     ├── 大小固定的对象                                     │</span>
<span class="line">│     ├── 生命周期短的对象                                   │</span>
<span class="line">│     └── 内存分配密集型场景                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ✗ 不推荐：                                                │</span>
<span class="line">│     ├── 大对象 (避免预分配大量内存)                         │</span>
<span class="line">│     ├── 生命周期不确定的对象                               │</span>
<span class="line">│     ├── 包含资源句柄的对象 (需要手动释放)                   │</span>
<span class="line">│     └── 多态对象 (虚函数等)                               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="对象池优势总结" tabindex="-1"><a class="header-anchor" href="#对象池优势总结"><span>对象池优势总结</span></a></h3><table><thead><tr><th>优势</th><th>说明</th><th>提升</th></tr></thead><tbody><tr><td><strong>性能</strong></td><td>减少分配/释放开销</td><td>3-10x</td></tr><tr><td><strong>内存</strong></td><td>减少碎片和峰值占用</td><td>20-30%</td></tr><tr><td><strong>稳定性</strong></td><td>避免分配失败</td><td>更稳定</td></tr><tr><td><strong>可预测性</strong></td><td>消除不确定性</td><td>延迟更稳定</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 正确使用对象池</span>
<span class="line">   - 必须提供 reset 方法</span>
<span class="line">   - 使用后必须释放</span>
<span class="line">   - 避免对象悬空</span>
<span class="line"></span>
<span class="line">2. 线程安全设计</span>
<span class="line">   - 使用互斥锁</span>
<span class="line">   - 或使用无锁队列</span>
<span class="line">   - 注意死锁</span>
<span class="line"></span>
<span class="line">3. 监控池状态</span>
<span class="line">   - 统计分配/释放次数</span>
<span class="line">   - 监控池利用率</span>
<span class="line">   - 调整池大小</span>
<span class="line"></span>
<span class="line">4. 测试验证</span>
<span class="line">   - 单元测试正确性</span>
<span class="line">   - 性能测试效果</span>
<span class="line">   - 压力测试稳定性</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/lib/helpers/objectpool.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - ObjectPool</a></li><li><a href="https://www.gamedeveloper.net/design-patterns/object-pool/" target="_blank" rel="noopener noreferrer">游戏服务器对象池设计</a></li></ul>`,50)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};