import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q59-reduce-lock-contention.html","title":"Q59: 如何减少锁竞争？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q59-reduce-lock-contention.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q59-reduce-lock-contention.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q59-如何减少锁竞争" tabindex="-1"><a class="header-anchor" href="#q59-如何减少锁竞争"><span>Q59: 如何减少锁竞争？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对并发控制和锁优化的理解：</p><ul><li>锁竞争的原因和影响</li><li>无锁编程技术</li><li>Actor 模型在游戏服务器中的应用</li><li>KBEngine 的并发设计</li></ul><hr><h2 id="一、锁竞争问题" tabindex="-1"><a class="header-anchor" href="#一、锁竞争问题"><span>一、锁竞争问题</span></a></h2><h3 id="_1-1-锁竞争的危害" tabindex="-1"><a class="header-anchor" href="#_1-1-锁竞争的危害"><span>1.1 锁竞争的危害</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    锁竞争的危害                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景：多线程竞争同一把锁                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  线程 1 ──┐                                              │</span>
<span class="line">│  线程 2 ──┤                                              │</span>
<span class="line">│  线程 3 ──┤  争夺锁                                       │</span>
<span class="line">│  线程 4 ──┤                                              │</span>
<span class="line">│  线程 5 ──┘                                              │</span>
<span class="line">│         │                                                │</span>
<span class="line">│         ▼                                                │</span>
<span class="line">│  ┌─────────┐                                              │</span>
<span class="line">│  │  锁     │ 只有 1 个线程能获得锁                         │</span>
<span class="line">│  └─────────┘ 其他线程等待                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题：                                                    │</span>
<span class="line">│  ├── CPU 浪费 - 等待的线程空转                            │</span>
<span class="line">│  ├── 串行执行 - 并发能力下降                              │</span>
<span class="line">│  ├── 上下文切换 - 频繁切换开销                             │</span>
<span class="line">│  └── 死锁风险 - 设计不当会死锁                            │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-锁竞争检测" tabindex="-1"><a class="header-anchor" href="#_1-2-锁竞争检测"><span>1.2 锁竞争检测</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ThreadSanitizer 检测锁竞争</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 编译选项</span></span>
<span class="line">g<span class="token operator">++</span> <span class="token operator">-</span>fsanitize<span class="token operator">=</span>thread <span class="token operator">-</span>g <span class="token operator">-</span>O2 game<span class="token operator">-</span>server<span class="token punctuation">.</span>cpp <span class="token operator">-</span>o game<span class="token operator">-</span>server</span>
<span class="line"></span>
<span class="line"><span class="token comment">// 运行时会检测数据竞争</span></span>
<span class="line"><span class="token punctuation">.</span><span class="token operator">/</span>game<span class="token operator">-</span>server</span>
<span class="line"></span>
<span class="line"><span class="token comment">// 输出示例:</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span> <span class="token expression">WARNING<span class="token operator">:</span> ThreadSanitizer<span class="token operator">:</span> data race on addr</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span>   <span class="token expression">Read of size <span class="token number">4</span> at <span class="token number">0x7f1234567890</span> by thread T1<span class="token operator">:</span></span></span></span>
<span class="line">#     #<span class="token number">0</span> <span class="token number">0x7f1234567890</span> in <span class="token keyword">operator</span> <span class="token keyword">int</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span>   <span class="token expression">Previous write of size <span class="token number">4</span> at <span class="token number">0x7f1234567890</span> by thread T2<span class="token operator">:</span></span></span></span>
<span class="line">#     #<span class="token number">1</span> <span class="token number">0x7f1234567890</span> in <span class="token keyword">operator</span> <span class="token keyword">int</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、减少锁竞争的方法" tabindex="-1"><a class="header-anchor" href="#二、减少锁竞争的方法"><span>二、减少锁竞争的方法</span></a></h2><h3 id="_2-1-减少锁的范围" tabindex="-1"><a class="header-anchor" href="#_2-1-减少锁的范围"><span>2.1 减少锁的范围</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 不好的做法：大粒度锁</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EntityManager</span> <span class="token punctuation">{</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex mutex_<span class="token punctuation">;</span>  <span class="token comment">// 整个实体管理器一把锁</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>EntityID<span class="token punctuation">,</span> Entity<span class="token operator">*</span><span class="token operator">&gt;</span> entities_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    Entity<span class="token operator">*</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> entities_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> it <span class="token operator">!=</span> entities_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">?</span> it<span class="token operator">-&gt;</span>second <span class="token operator">:</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addEntity</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        entities_<span class="token punctuation">[</span>entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">]</span> <span class="token operator">=</span> entity<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 问题：每次操作都要锁住整个 map</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 好的做法：细粒度锁</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EntityManager</span> <span class="token punctuation">{</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>array<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token punctuation">,</span> <span class="token number">256</span><span class="token operator">&gt;</span> mutexes_<span class="token punctuation">;</span>  <span class="token comment">// 分段锁</span></span>
<span class="line"></span>
<span class="line">    Entity<span class="token operator">*</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t bucket <span class="token operator">=</span> id <span class="token operator">%</span> mutexes_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>shared_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutexes_<span class="token punctuation">[</span>bucket<span class="token punctuation">]</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> entities_<span class="token punctuation">[</span>bucket<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> it <span class="token operator">!=</span> entities_<span class="token punctuation">[</span>bucket<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">?</span> it<span class="token operator">-&gt;</span>second <span class="token operator">:</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addEntity</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t bucket <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">%</span> mutexes_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutexes_<span class="token punctuation">[</span>bucket<span class="token punctuation">]</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        entities_<span class="token punctuation">[</span>bucket<span class="token punctuation">]</span><span class="token punctuation">[</span>entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">]</span> <span class="token operator">=</span> entity<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>array<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>EntityID<span class="token punctuation">,</span> Entity<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token punctuation">,</span> <span class="token number">256</span><span class="token operator">&gt;</span> entities_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-读写锁" tabindex="-1"><a class="header-anchor" href="#_2-2-读写锁"><span>2.2 读写锁</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 读写锁使用</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ConfigManager</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">mutable</span> std<span class="token double-colon punctuation">::</span>shared_mutex mutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string config_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 读取操作 - 可以并发</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">getConfig</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>shared_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> config_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 写入操作 - 独占</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setConfig</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> config<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        config_ <span class="token operator">=</span> config<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 多个读线程可以同时访问</span></span>
<span class="line">    <span class="token comment">// 但写线程会阻塞所有读写</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、无锁编程" tabindex="-1"><a class="header-anchor" href="#三、无锁编程"><span>三、无锁编程</span></a></h2><h3 id="_3-1-原子操作" tabindex="-1"><a class="header-anchor" href="#_3-1-原子操作"><span>3.1 原子操作</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 原子操作示例</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;atomic&gt;</span></span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LockFreeCounter</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">LockFreeCounter</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">:</span> <span class="token function">value_</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 原子递增</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> <span class="token function">increment</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> value_<span class="token punctuation">.</span><span class="token function">fetch_add</span><span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 原子读取</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> <span class="token function">get</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> value_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// CAS (Compare-And-Swap)</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">compareExchange</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span><span class="token operator">&amp;</span> expected<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> desired<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> value_<span class="token punctuation">.</span><span class="token function">compare_exchange_weak</span><span class="token punctuation">(</span></span>
<span class="line">            expected<span class="token punctuation">,</span></span>
<span class="line">            desired<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>memory_order_acq_rel</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span><span class="token keyword">uint64_t</span><span class="token operator">&gt;</span> value_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-无锁队列" tabindex="-1"><a class="header-anchor" href="#_3-2-无锁队列"><span>3.2 无锁队列</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 无锁队列 (单生产者单消费者)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LockFreeQueue</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">LockFreeQueue</span><span class="token punctuation">(</span>size_t capacity<span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">buffer_</span><span class="token punctuation">(</span><span class="token keyword">new</span> Node<span class="token punctuation">[</span>capacity<span class="token punctuation">]</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">capacity_</span><span class="token punctuation">(</span>capacity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 初始化哨兵节点</span></span>
<span class="line">        Node<span class="token operator">*</span> sentinel <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">Node</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        head_<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span>sentinel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        tail_<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span>sentinel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">enqueue</span><span class="token punctuation">(</span>T value<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Node<span class="token operator">*</span> node <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">Node</span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 将新节点链接到尾部</span></span>
<span class="line">        Node<span class="token operator">*</span> prev <span class="token operator">=</span> tail_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        prev<span class="token operator">-&gt;</span>next_<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span>node<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_release<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新尾部</span></span>
<span class="line">        tail_<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span>node<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_release<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">dequeue</span><span class="token punctuation">(</span>T<span class="token operator">&amp;</span> outValue<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Node<span class="token operator">*</span> head <span class="token operator">=</span> head_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_acquire<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        Node<span class="token operator">*</span> next <span class="token operator">=</span> head<span class="token operator">-&gt;</span>next_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_acquire<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>next <span class="token operator">==</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span>  <span class="token comment">// 队列空</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新头部</span></span>
<span class="line">        head_<span class="token punctuation">.</span><span class="token function">store</span><span class="token punctuation">(</span>next<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_release<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        outValue <span class="token operator">=</span> next<span class="token operator">-&gt;</span>value<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">delete</span> head<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Node</span> <span class="token punctuation">{</span></span>
<span class="line">        T value<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>Node<span class="token operator">*</span><span class="token operator">&gt;</span> next<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>Node<span class="token operator">*</span><span class="token operator">&gt;</span> head_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>Node<span class="token operator">*</span><span class="token operator">&gt;</span> tail_<span class="token punctuation">;</span></span>
<span class="line">    Node<span class="token operator">*</span> buffer_<span class="token punctuation">;</span>  <span class="token comment">// 用于内存管理</span></span>
<span class="line">    size_t capacity_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-rcu-read-copy-update" tabindex="-1"><a class="header-anchor" href="#_3-3-rcu-read-copy-update"><span>3.3 RCU (Read-Copy-Update)</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// RCU 模式：读无锁，写复制的无锁数据结构</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RCUList</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 读取 - 无锁</span></span>
<span class="line">    T<span class="token operator">*</span> <span class="token function">find</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">bool</span><span class="token punctuation">(</span><span class="token keyword">const</span> T<span class="token operator">&amp;</span><span class="token punctuation">)</span><span class="token operator">&gt;</span> predicate<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        VersionedList<span class="token operator">*</span> list <span class="token operator">=</span> <span class="token function">getReadableList</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> item <span class="token operator">:</span> list<span class="token operator">-&gt;</span>items<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">predicate</span><span class="token punctuation">(</span>item<span class="token punctuation">.</span>data<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> item<span class="token punctuation">.</span>data<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 写入 - 复制修改</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">add</span><span class="token punctuation">(</span><span class="token keyword">const</span> T<span class="token operator">&amp;</span> item<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> newList <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">VersionedList</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 复制旧数据</span></span>
<span class="line">        VersionedList<span class="token operator">*</span> oldList <span class="token operator">=</span> <span class="token function">getReadableList</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> i <span class="token operator">:</span> oldList<span class="token operator">-&gt;</span>items<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            newList<span class="token operator">-&gt;</span>items<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">                i<span class="token punctuation">.</span>data<span class="token punctuation">,</span> i<span class="token punctuation">.</span>data  <span class="token comment">// 深拷贝</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 添加新项</span></span>
<span class="line">        newList<span class="token operator">-&gt;</span>items<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">{</span>item<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发布新版本</span></span>
<span class="line">        <span class="token function">publishList</span><span class="token punctuation">(</span>newList<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 旧版本会在后续被回收</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">VersionedList</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>RCUItem<span class="token operator">&gt;</span> items<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> version<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span>VersionedList<span class="token operator">*</span><span class="token operator">&gt;</span> currentList_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>VersionedList<span class="token operator">*</span><span class="token operator">&gt;</span> oldLists_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、actor-模型" tabindex="-1"><a class="header-anchor" href="#四、actor-模型"><span>四、Actor 模型</span></a></h2><h3 id="_4-1-actor-模型原理" tabindex="-1"><a class="header-anchor" href="#_4-1-actor-模型原理"><span>4.1 Actor 模型原理</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    Actor 模型                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  传统共享内存并发：                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  线程1 ──┐                                        │       │</span>
<span class="line">│  │  线程2 ──┼── 共享内存 ─ 锁竞争                     │       │</span>
<span class="line">│  │  线程3 ──┘                                        │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Actor 模型 (消息传递)：                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  Actor1      Actor2      Actor3                    │       │</span>
<span class="line">│  │    │            │           │                    │       │</span>
<span class="line">│  │    └────────────┴───────────┘                    │       │</span>
<span class="line">│  │           消息队列 (Mailbox)                      │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  特点：                                            │       │</span>
<span class="line">│  │  ├── 每个 Actor 有独立队列                         │       │</span>
<span class="line">│  │  ├── 串行处理消息                                  │       │</span>
<span class="line">│  │  ├── 无锁竞争                                      │       │</span>
<span class="line">│  │  └── 易于扩展                                      │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-actor-实现" tabindex="-1"><a class="header-anchor" href="#_4-2-actor-实现"><span>4.2 Actor 实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Actor 模型实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Actor</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 发送消息到 Actor</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">send</span><span class="token punctuation">(</span>EntityID targetId<span class="token punctuation">,</span> <span class="token keyword">const</span> T<span class="token operator">&amp;</span> message<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Actor<span class="token operator">*</span> target <span class="token operator">=</span> <span class="token function">getActor</span><span class="token punctuation">(</span>targetId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            target<span class="token operator">-&gt;</span><span class="token function">receive</span><span class="token punctuation">(</span>message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收消息</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">receive</span><span class="token punctuation">(</span><span class="token keyword">const</span> T<span class="token operator">&amp;</span> message<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 将消息加入队列</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        messageQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span><span class="token class-name">Message</span><span class="token double-colon punctuation">::</span><span class="token function">create</span><span class="token punctuation">(</span>message<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理消息（在 Actor 线程中调用）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processMessages</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Message<span class="token operator">*</span> msg <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token punctuation">{</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>messageQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    msg <span class="token operator">=</span> messageQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    messageQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">handleMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">delete</span> msg<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 队列空，等待</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                queueCV_<span class="token punctuation">.</span><span class="token function">wait</span><span class="token punctuation">(</span>lock<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 子类实现消息处理</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">handleMessage</span><span class="token punctuation">(</span>Message<span class="token operator">*</span> msg<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex queueMutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>condition_variable queueCV_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>Message<span class="token operator">*</span><span class="token operator">&gt;</span> messageQueue_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> running_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-actor-线程池" tabindex="-1"><a class="header-anchor" href="#_4-3-actor-线程池"><span>4.3 Actor 线程池</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Actor 线程池</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ActorSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">ActorSystem</span><span class="token punctuation">(</span>size_t numThreads <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>thread<span class="token double-colon punctuation">::</span><span class="token function">hardware_concurrency</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">running_</span><span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 创建工作线程</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> numThreads<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            workers_<span class="token punctuation">.</span><span class="token function">emplace_back</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">workerThread</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">~</span><span class="token function">ActorSystem</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        running_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> thread <span class="token operator">:</span> workers_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            thread<span class="token punctuation">.</span><span class="token function">join</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 注册 Actor</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">registerActor</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">,</span> Actor<span class="token operator">*</span> actor<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>actorsMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        actors_<span class="token punctuation">[</span>id<span class="token punctuation">]</span> <span class="token operator">=</span> actor<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 分配消息到 Actor</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">dispatch</span><span class="token punctuation">(</span>EntityID targetId<span class="token punctuation">,</span> Message<span class="token operator">*</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Actor<span class="token operator">*</span> actor <span class="token operator">=</span> <span class="token function">getActor</span><span class="token punctuation">(</span>targetId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>actor<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            actor<span class="token operator">-&gt;</span><span class="token function">receive</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">notifyActor</span><span class="token punctuation">(</span>targetId<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 唤醒 Actor 线程</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">workerThread</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 等待有消息的 Actor</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>readyMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            readyCV_<span class="token punctuation">.</span><span class="token function">wait</span><span class="token punctuation">(</span>lock<span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token operator">!</span>readyActors_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> <span class="token operator">!</span>running_<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>running_<span class="token punctuation">)</span> <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 处理所有就绪的 Actor</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID actorId <span class="token operator">:</span> readyActors_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                Actor<span class="token operator">*</span> actor <span class="token operator">=</span> <span class="token function">getActor</span><span class="token punctuation">(</span>actorId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>actor<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    actor<span class="token operator">-&gt;</span><span class="token function">processMessages</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            readyActors_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">notifyActor</span><span class="token punctuation">(</span>EntityID actorId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>readyMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        readyActors_<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>actorId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        readyCV_<span class="token punctuation">.</span><span class="token function">notify_one</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    Actor<span class="token operator">*</span> <span class="token function">getActor</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>actorsMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> actors_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> it <span class="token operator">!=</span> actors_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">?</span> it<span class="token operator">-&gt;</span>second <span class="token operator">:</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>EntityID<span class="token punctuation">,</span> Actor<span class="token operator">*</span><span class="token operator">&gt;</span> actors_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex actorsMutex_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_set<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span> readyActors_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex readyMutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>condition_variable readyCV_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>thread<span class="token operator">&gt;</span> workers_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> running_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、kbengine-并发设计" tabindex="-1"><a class="header-anchor" href="#五、kbengine-并发设计"><span>五、KBEngine 并发设计</span></a></h2><h3 id="_5-1-kbengine-单线程模型" tabindex="-1"><a class="header-anchor" href="#_5-1-kbengine-单线程模型"><span>5.1 KBEngine 单线程模型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  KBEngine 单线程设计                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  设计理念：                                                │</span>
<span class="line">│  ├── 每个 App 单线程处理消息                              │</span>
<span class="line">│  ├── 通过事件驱动 (poller)                               │</span>
<span class="line">│  ├── 避免锁竞争                                          │</span>
<span class="line">│  └── 简化开发                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  架构：                                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  BaseApp/CellApp (单线程)                         │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────┐         │       │</span>
<span class="line">│  │  │  Poller (epoll/kqueue)             │         │       │</span>
<span class="line">│  │  │  ┌───────────────────────────────────┐ │       │       │</span>
<span class="line">│  │  │  │  事件循环 (processLoop)        │ │       │       │</span>
<span class="line">│  │  │  │  ├─► 处理网络事件            │ │       │       │</span>
<span class="line">│  │  │  │  ├─► 处理定时器              │ │       │       │</span>
<span class="line">│  │  │  │  ├─► 处理实体逻辑            │ │       │       │</span>
<span class="line">│  │  │  │  └───────────────────────────────┘ │       │       │</span>
<span class="line">│  │  │  └─────────────────────────────────────┘         │       │</span>
<span class="line">│  │  └─────────────────────────────────────────┘         │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优势：                                                  │</span>
<span class="line">│  ├── 无锁竞争                                             │</span>
<span class="line">│  ├── 缓存友好                                             │</span>
<span class="line">│  ├── 调试简单                                             │</span>
<span class="line">│  └── 开发效率高                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  限制：                                                  │</span>
<span class="line">│  ├── 单核 CPU 利用率低                                    │</span>
<span class="line">│  ├── 需要多进程扩展                                        │</span>
<span class="line">│  └── 大量计算会阻塞                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-kbengine-多线程扩展" tabindex="-1"><a class="header-anchor" href="#_5-2-kbengine-多线程扩展"><span>5.2 KBEngine 多线程扩展</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 多线程支持</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// src/lib/thread/threadpool.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ThreadPool</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 创建线程池</span></span>
<span class="line">    <span class="token function">ThreadPool</span><span class="token punctuation">(</span>size_t numThreads<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> numThreads<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            threads_<span class="token punctuation">.</span><span class="token function">emplace_back</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">workerThread</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加任务</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">F</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token keyword">auto</span> <span class="token function">execute</span><span class="token punctuation">(</span>F<span class="token operator">&amp;&amp;</span> task<span class="token punctuation">)</span> <span class="token operator">-&gt;</span> std<span class="token double-colon punctuation">::</span>future<span class="token operator">&lt;</span><span class="token keyword">decltype</span><span class="token punctuation">(</span><span class="token function">task</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token operator">&gt;</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> pTask <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">make_shared</span><span class="token generic class-name"><span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>packaged_task<span class="token operator">&lt;</span>F<span class="token operator">&gt;&gt;</span></span></span><span class="token punctuation">(</span>task<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> future <span class="token operator">=</span> pTask<span class="token operator">-&gt;</span><span class="token function">get_future</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            taskQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>pTask<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        queueCV_<span class="token punctuation">.</span><span class="token function">notify_one</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> future<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">workerThread</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>shared_ptr<span class="token operator">&lt;</span>Task<span class="token operator">&gt;</span> task<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token punctuation">{</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                queueCV_<span class="token punctuation">.</span><span class="token function">wait</span><span class="token punctuation">(</span>lock<span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token operator">!</span>taskQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> <span class="token operator">!</span>running_<span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>running_<span class="token punctuation">)</span> <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>taskQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    task <span class="token operator">=</span> taskQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    taskQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>task<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                task<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>thread<span class="token operator">&gt;</span> threads_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_ptr<span class="token operator">&lt;</span>Task<span class="token operator">&gt;&gt;</span> taskQueue_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex queueMutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>condition_variable queueCV_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> running_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-并发设计原则" tabindex="-1"><a class="header-anchor" href="#_6-1-并发设计原则"><span>6.1 并发设计原则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                减少锁竞争的设计原则                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 避免共享状态                                          │</span>
<span class="line">│     ├── 每个线程独立数据                                  │</span>
<span class="line">│     ├── 使用线程本地存储 (TLS)                             │</span>
<span class="line">│     └── 消除共享需求                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 最小化锁范围                                           │</span>
<span class="line">│     ├── 只锁必要数据                                       │</span>
<span class="line">│     ├── 减小锁持有时间                                     │</span>
<span class="line">│     └── 使用分段锁/读写锁                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 使用无锁数据结构                                       │</span>
<span class="line">│     ├── 原子操作 (atomic)                                  │</span>
<span class="line">│     ├── 无锁队列                                           │</span>
<span class="line">│     └── RCU                                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 消息传递并发                                           │</span>
<span class="line">│     ├── Actor 模型                                          │</span>
<span class="line">│     ├── CSP 模型                                            │</span>
<span class="line">│     └── 消息队列                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-工具推荐" tabindex="-1"><a class="header-anchor" href="#_6-2-工具推荐"><span>6.2 工具推荐</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 并发分析工具</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 1. ThreadSanitizer</span></span>
<span class="line"><span class="token comment">// 编译: -fsanitize=thread -g</span></span>
<span class="line"><span class="token comment">// 检测: 数据竞争、死锁等</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 2. Helgrind (Valgrind 工具)</span></span>
<span class="line"><span class="token comment">// 运行: valgrind --tool=helgrind ./server</span></span>
<span class="line"><span class="token comment">// 功能: 锁错误检测、死锁检测</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 3. perf lock</span></span>
<span class="line"><span class="token comment">// 运行: perf lock ./server</span></span>
<span class="line"><span class="token comment">// 功能: 锁竞争分析</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 4. Intel VTune</span></span>
<span class="line"><span class="token comment">// 功能: 并发分析、热点识别</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="锁优化技术总结" tabindex="-1"><a class="header-anchor" href="#锁优化技术总结"><span>锁优化技术总结</span></a></h3><table><thead><tr><th>技术</th><th>复杂度</th><th>效果</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>分段锁</strong></td><td>中</td><td>中</td><td>哈表/Map</td></tr><tr><td><strong>读写锁</strong></td><td>低</td><td>中</td><td>读多写少</td></tr><tr><td><strong>无锁队列</strong></td><td>高</td><td>高</td><td>SPS/MPMC</td></tr><tr><td><strong>RCU</strong></td><td>高</td><td>高</td><td>读多写少</td></tr><tr><td><strong>Actor</strong></td><td>高</td><td>高</td><td>消息传递</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 优先避免锁</span>
<span class="line">   - 使用线程本地存储</span>
<span class="line">   - 采用无锁数据结构</span>
<span class="line">   - 消除共享状态</span>
<span class="line"></span>
<span class="line">2. 无法避免时减小锁粒度</span>
<span class="line">   - 缩小锁范围</span>
<span class="line">   - 使用读写锁</span>
<span class="line">   - 减少持有时间</span>
<span class="line"></span>
<span class="line">3. 考虑消息传递</span>
<span class="line">   - Actor 模型</span>
<span class="line">   - CSP 模型</span>
<span class="line">   - 消息队列</span>
<span class="line"></span>
<span class="line">4. 工具检测</span>
<span class="line">   - ThreadSanitizer</span>
<span class="line">   - Helgrind</span>
<span class="line">   - perf lock</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kkengine/kbengine/tree/master/kbe/src/lib/thread" target="_blank" rel="noopener noreferrer">KBEngine GitHub - ThreadPool</a></li><li><a href="https://www.kernel.org/doc/Documentation/core-api/lockdep.html" target="_blank" rel="noopener noreferrer">Lock-Free Programming</a></li><li><a href="https://www.semanticscholar.org/paper/5369310/" target="_blank" rel="noopener noreferrer">Actor Model 论文</a></li></ul>`,54)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};