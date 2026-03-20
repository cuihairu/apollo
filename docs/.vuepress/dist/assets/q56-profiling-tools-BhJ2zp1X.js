import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q56-profiling-tools.html","title":"Q56: 如何进行性能分析？有哪些工具？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q56-profiling-tools.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q56-profiling-tools.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q56-如何进行性能分析-有哪些工具" tabindex="-1"><a class="header-anchor" href="#q56-如何进行性能分析-有哪些工具"><span>Q56: 如何进行性能分析？有哪些工具？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对性能分析工具和方法的理解：</p><ul><li>性能分析的重要性</li><li>CPU 分析工具</li><li>内存分析工具</li><li>网络分析工具</li><li>KBEngine 性能分析</li></ul><hr><h2 id="一、性能分析基础" tabindex="-1"><a class="header-anchor" href="#一、性能分析基础"><span>一、性能分析基础</span></a></h2><h3 id="_1-1-性能指标" tabindex="-1"><a class="header-anchor" href="#_1-1-性能指标"><span>1.1 性能指标</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    关键性能指标                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. CPU 使用率                                              │</span>
<span class="line">│     ├── 用户空间 CPU                                        │</span>
<span class="line">│     ├── 系统空间 CPU                                        │</span>
<span class="line">│     ├── IO 等待                                             │</span>
<span class="line">│     └── 单核/多核使用                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 内存使用                                                │</span>
<span class="line">│     ├── 常驻内存                                             │</span>
<span class="line">│     ├── 虚拟内存                                             │</span>
<span class="line">│     ├── 内存泄漏                                             │</span>
<span class="line">│     └── 缓存命中率                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. I/O 性能                                                │</span>
<span class="line">│     ├── 磁盘读写速度                                        │</span>
<span class="line">│     ├── IOPS                                               │</span>
<span class="line">│     └── IO 等待时间                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 网络性能                                                │</span>
<span class="line">│     ├── 带宽使用                                            │</span>
<span class="line">│     ├── 延迟 (RTT)                                          │</span>
<span class="line">│     ├── 丢包率                                              │</span>
<span class="line">│     └── 连接数                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 应用程序性能                                          │</span>
<span class="line">│     ├── 帧率 (FPS/TPS)                                     │</span>
<span class="line">│     ├── 响应时间                                            │</span>
<span class="line">│     ├── 吞吐量 (QPS)                                       │</span>
<span class="line">│     └── 并发数                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-性能分析流程" tabindex="-1"><a class="header-anchor" href="#_1-2-性能分析流程"><span>1.2 性能分析流程</span></a></h3>`,9),i(d,{code:`eJxNz81qwkAQB/C7T7Ev4Cu0VBM/eu5tyaEUSg+FQin0koNWqiuNbUq1DY2IKCoKxpyU4NfLZGezb2Gc8eAc58efmf/949Pr3cPt8wu7MTIsnSsuv77VZwiVSVLb6r9ADzyLZbMXLMdl8B9HTRJwGtBvWJjJoee5rjThYypFHXquXI3k+4o8j25waC+1Xz/lOyG0AnID3eSUVD9LPRAkJkoBL29bqjZXb1G8Hqp+lbyAXuTxxpPOL7SjZO+TFFFKXM+cZFE9eUdAr0teQi/zZLeTIlR+kLa5JCofyZbu2Gbm2QK8hc2u01ccEK6VOQCx4Y83`}),o[1]||=e(`<hr><h2 id="二、cpu-性能分析" tabindex="-1"><a class="header-anchor" href="#二、cpu-性能分析"><span>二、CPU 性能分析</span></a></h2><h3 id="_2-1-perf-工具" tabindex="-1"><a class="header-anchor" href="#_2-1-perf-工具"><span>2.1 perf 工具</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># Linux perf 性能分析</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 1. CPU 采样分析</span></span>
<span class="line">perf <span class="token function">top</span> <span class="token parameter variable">-p</span> <span class="token operator">&lt;</span>pid<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. 记录性能数据 (60秒)</span></span>
<span class="line">perf record <span class="token parameter variable">-p</span> <span class="token operator">&lt;</span>pid<span class="token operator">&gt;</span> <span class="token parameter variable">-g</span> -- <span class="token function">sleep</span> <span class="token number">60</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 3. 报告分析</span></span>
<span class="line">perf report</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 4. 火焰图生成</span></span>
<span class="line">perf script <span class="token operator">|</span> FlameGraph/flamegraph.pl <span class="token operator">&gt;</span> flamegraph.svg</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 5. 热点函数分析</span></span>
<span class="line">perf report <span class="token parameter variable">--stdio</span> --call-graph <span class="token operator">|</span> <span class="token function">head</span> <span class="token parameter variable">-100</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 输出示例:</span></span>
<span class="line"><span class="token comment"># Samples: 100k of event &#39;cpu-clock&#39;</span></span>
<span class="line"><span class="token comment"># Event count (approx.): 10000000000</span></span>
<span class="line"><span class="token comment">#</span></span>
<span class="line"><span class="token comment"># Overhead  Command  Shared Object       Symbol</span></span>
<span class="line"><span class="token comment"># ████████  20.00%  game-server  game-server  [.] updateLoop</span></span>
<span class="line"><span class="token comment"># ██████    15.00%  game-server  game-server  [.] processPackets</span></span>
<span class="line"><span class="token comment"># ████       10.00%  game-server  game-server  [.] updateEntities</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-火焰图解读" tabindex="-1"><a class="header-anchor" href="#_2-2-火焰图解读"><span>2.2 火焰图解读</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    火焰图解读                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  火焰图：x轴 = 样本比例，y轴 = 调用栈深度                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  updateLoop (20%)                                           │</span>
<span class="line">│  ├─ processPackets (15%)                                   │</span>
<span class="line">│  │  └─ parseMessage (10%)                                 │</span>
<span class="line">│  │     └─ handleMessage (5%)                               │</span>
<span class="line">│  └─ updateEntities (5%)                                    │</span>
<span class="line">│     └─ updatePositions (2%)                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  热点识别：                                                  │</span>
<span class="line">│  ├── 扁平的函数 = 热点函数                                 │</span>
<span class="line">│  ├── 宽度 = CPU 占用比例                                   │</span>
<span class="line">│  └── 高度 = 调用链深度                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-perf-使用示例" tabindex="-1"><a class="header-anchor" href="#_2-3-perf-使用示例"><span>2.3 perf 使用示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 代码优化示例</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 优化前：热循环</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">updateEntities</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> entities<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity <span class="token operator">=</span> entities<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity <span class="token operator">&amp;&amp;</span> entity<span class="token operator">-&gt;</span><span class="token function">isActive</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span>  <span class="token comment">// 分支预测失败</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 优化后：减少分支和缓存未命中</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">updateEntities</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 过滤活跃实体</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> activeEntities<span class="token punctuation">;</span></span>
<span class="line">    activeEntities<span class="token punctuation">.</span><span class="token function">reserve</span><span class="token punctuation">(</span>entities<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity <span class="token operator">:</span> entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity <span class="token operator">&amp;&amp;</span> entity<span class="token operator">-&gt;</span><span class="token function">isActive</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            activeEntities<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 连续内存访问</span></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity <span class="token operator">:</span> activeEntities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、内存分析" tabindex="-1"><a class="header-anchor" href="#三、内存分析"><span>三、内存分析</span></a></h2><h3 id="_3-1-valgrind-工具" tabindex="-1"><a class="header-anchor" href="#_3-1-valgrind-工具"><span>3.1 valgrind 工具</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># Valgrind 内存分析</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 1. 内存泄漏检测</span></span>
<span class="line">valgrind --leak-check<span class="token operator">=</span>full --show-leak-kinds<span class="token operator">=</span>all <span class="token punctuation">\\</span></span>
<span class="line">         --log-file<span class="token operator">=</span>valgrind.log ./game-server</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. 内存分析</span></span>
<span class="line">valgrind <span class="token parameter variable">--tool</span><span class="token operator">=</span>massif ./game-server</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 3. 性能分析</span></span>
<span class="line">valgrind <span class="token parameter variable">--tool</span><span class="token operator">=</span>callgrind ./game-server</span>
<span class="line">callgrind_annotate callgrind.out.<span class="token operator">&lt;</span>pid<span class="token operator">&gt;</span> <span class="token operator">&gt;</span> callgrind.txt</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 输出示例:</span></span>
<span class="line"><span class="token comment"># ==12345== LEAK SUMMARY:</span></span>
<span class="line"><span class="token comment"># ==12345==    definitely lost: 100 bytes in 5 blocks</span></span>
<span class="line"><span class="token comment"># ==12345==    indirectly lost: 200 bytes in 10 blocks</span></span>
<span class="line"><span class="token comment"># ==12345==    possibly lost: 50 bytes in 2 blocks</span></span>
<span class="line"><span class="token comment"># ==12345==    still reachable: 10,000 bytes in 100 blocks</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-addresssanitizer" tabindex="-1"><a class="header-anchor" href="#_3-2-addresssanitizer"><span>3.2 AddressSanitizer</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># AddressSanitizer (ASan) - 编译时检查</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 编译选项</span></span>
<span class="line">g++ <span class="token parameter variable">-fsanitize</span><span class="token operator">=</span>address -fno-omit-frame-pointer <span class="token punctuation">\\</span></span>
<span class="line">    <span class="token parameter variable">-g</span> <span class="token parameter variable">-O2</span> game-server.cpp <span class="token parameter variable">-o</span> game-server</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 运行时会检测内存错误</span></span>
<span class="line">./game-server</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 输出示例:</span></span>
<span class="line"><span class="token comment"># ==12345==ERROR: AddressSanitizer: heap-use-after-free</span></span>
<span class="line"><span class="token comment">#     #0 0x7f1234567890 in operator delete</span></span>
<span class="line"><span class="token comment">#     #1 0x7f1234567890 in ~Entity()</span></span>
<span class="line"><span class="token comment">#     #2 0x7f1234567890 in updateEntities()</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-内存池分析" tabindex="-1"><a class="header-anchor" href="#_3-3-内存池分析"><span>3.3 内存池分析</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 对象池性能分析</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ObjectPoolAnalyzer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">void</span> <span class="token function">analyze</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>type_index<span class="token punctuation">,</span> PoolStats<span class="token operator">&gt;</span> stats<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 统计各类对象的分配情况</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>type<span class="token punctuation">,</span> info<span class="token punctuation">]</span> <span class="token operator">:</span> allocationStats_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            PoolStats<span class="token operator">&amp;</span> stat <span class="token operator">=</span> stats<span class="token punctuation">[</span>type<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            stat<span class="token punctuation">.</span>totalAllocations <span class="token operator">=</span> info<span class="token punctuation">.</span>count<span class="token punctuation">;</span></span>
<span class="line">            stat<span class="token punctuation">.</span>currentInUse <span class="token operator">=</span> info<span class="token punctuation">.</span>count <span class="token operator">-</span> info<span class="token punctuation">.</span>freed<span class="token punctuation">;</span></span>
<span class="line">            stat<span class="token punctuation">.</span>peakUsage <span class="token operator">=</span> info<span class="token punctuation">.</span>peak<span class="token punctuation">;</span></span>
<span class="line">            stat<span class="token punctuation">.</span>avgLifetime <span class="token operator">=</span> info<span class="token punctuation">.</span>totalLifetime <span class="token operator">/</span> info<span class="token punctuation">.</span>count<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 打印报告</span></span>
<span class="line">        <span class="token function">printReport</span><span class="token punctuation">(</span>stats<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">PoolStats</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> totalAllocations<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> currentInUse<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> peakUsage<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> avgLifetime<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、网络分析" tabindex="-1"><a class="header-anchor" href="#四、网络分析"><span>四、网络分析</span></a></h2><h3 id="_4-1-tcpdump-wireshark" tabindex="-1"><a class="header-anchor" href="#_4-1-tcpdump-wireshark"><span>4.1 tcpdump/Wireshark</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 网络抓包分析</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 1. 抓包</span></span>
<span class="line">tcpdump <span class="token parameter variable">-i</span> any <span class="token parameter variable">-w</span> game-capture.pcap port <span class="token number">9999</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. Wireshark 分析</span></span>
<span class="line">wireshark game-capture.pcap</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 3. 过滤器示例</span></span>
<span class="line"><span class="token comment"># 显示所有消息</span></span>
<span class="line">tcp.port <span class="token operator">==</span> <span class="token number">9999</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 显示特定消息类型</span></span>
<span class="line">tcp.port <span class="token operator">==</span> <span class="token number">9999</span> <span class="token operator">&amp;&amp;</span> data.len <span class="token operator">&gt;</span> <span class="token number">100</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 统计消息类型</span></span>
<span class="line">tcp.port <span class="token operator">==</span> <span class="token number">9999</span> <span class="token operator">|</span> <span class="token function">awk</span> <span class="token string">&#39;{print $NF}&#39;</span> <span class="token operator">|</span> <span class="token function">sort</span> <span class="token operator">|</span> <span class="token function">uniq</span> <span class="token parameter variable">-c</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-netstat-ss" tabindex="-1"><a class="header-anchor" href="#_4-2-netstat-ss"><span>4.2 netstat/ss</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 连接统计</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 查看连接数</span></span>
<span class="line"><span class="token function">netstat</span> <span class="token parameter variable">-an</span> <span class="token operator">|</span> <span class="token function">grep</span> ESTABLISHED <span class="token operator">|</span> <span class="token function">wc</span> <span class="token parameter variable">-l</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 查看各状态连接数</span></span>
<span class="line">ss <span class="token parameter variable">-a</span> <span class="token operator">|</span> <span class="token function">awk</span> <span class="token string">&#39;{print $1}&#39;</span> <span class="token operator">|</span> <span class="token function">sort</span> <span class="token operator">|</span> <span class="token function">uniq</span> <span class="token parameter variable">-c</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 输出:</span></span>
<span class="line"><span class="token comment"># ESTAB: 1000</span></span>
<span class="line"><span class="token comment"># TIME_WAIT: 50</span></span>
<span class="line"><span class="token comment"># LISTEN: 10</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 查看 TCP 统计</span></span>
<span class="line">ss <span class="token parameter variable">-s</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 输出:</span></span>
<span class="line"><span class="token comment"># Tcp:   Active connections: 1000</span></span>
<span class="line"><span class="token comment">#        Passive connections: 50</span></span>
<span class="line"><span class="token comment">#        Queued connections: 0</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、kbengine-性能分析" tabindex="-1"><a class="header-anchor" href="#五、kbengine-性能分析"><span>五、KBEngine 性能分析</span></a></h2><h3 id="_5-1-kbengine-内置分析" tabindex="-1"><a class="header-anchor" href="#_5-1-kbengine-内置分析"><span>5.1 KBEngine 内置分析</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 性能监控</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PerformanceMonitor</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>stats <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;tick_time&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;entity_count&#39;</span><span class="token punctuation">:</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;packet_sent&#39;</span><span class="token punctuation">:</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;packet_recv&#39;</span><span class="token punctuation">:</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;bandwidth_out&#39;</span><span class="token punctuation">:</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;bandwidth_in&#39;</span><span class="token punctuation">:</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onTick</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> deltaTime<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        每帧统计</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;tick_time&#39;</span><span class="token punctuation">]</span><span class="token punctuation">.</span>append<span class="token punctuation">(</span>deltaTime<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 只保留最近 1000 帧</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;tick_time&#39;</span><span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">1000</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;tick_time&#39;</span><span class="token punctuation">]</span><span class="token punctuation">.</span>pop<span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 计算平均帧时间</span></span>
<span class="line">        avg_time <span class="token operator">=</span> <span class="token builtin">sum</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;tick_time&#39;</span><span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;tick_time&#39;</span><span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 如果帧时间超过阈值，记录</span></span>
<span class="line">        <span class="token keyword">if</span> avg_time <span class="token operator">&gt;</span> <span class="token number">0.05</span><span class="token punctuation">:</span>  <span class="token comment"># 50ms</span></span>
<span class="line">            KBEngine<span class="token punctuation">.</span>warning<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;High tick time: </span><span class="token interpolation"><span class="token punctuation">{</span>avg_time <span class="token operator">*</span> <span class="token number">1000</span><span class="token punctuation">:</span><span class="token format-spec">.2f</span><span class="token punctuation">}</span></span><span class="token string">ms&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">report</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        生成性能报告</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">import</span> json</span>
<span class="line"></span>
<span class="line">        report <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;avg_tick_time_ms&#39;</span><span class="token punctuation">:</span> <span class="token builtin">sum</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;tick_time&#39;</span><span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;tick_time&#39;</span><span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token operator">*</span> <span class="token number">1000</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;max_tick_time_ms&#39;</span><span class="token punctuation">:</span> <span class="token builtin">max</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;tick_time&#39;</span><span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token operator">*</span> <span class="token number">1000</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;entity_count&#39;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;entity_count&#39;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;packets_sent&#39;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;packet_sent&#39;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;packets_recv&#39;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>stats<span class="token punctuation">[</span><span class="token string">&#39;packet_recv&#39;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span>json<span class="token punctuation">.</span>dumps<span class="token punctuation">(</span>report<span class="token punctuation">,</span> indent<span class="token operator">=</span><span class="token number">2</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-kbengine-性能配置" tabindex="-1"><a class="header-anchor" href="#_5-2-kbengine-性能配置"><span>5.2 KBEngine 性能配置</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># kbengine_defs.xml 性能配置</span></span>
<span class="line"></span>
<span class="line"><span class="token operator">&lt;</span>Baseapp<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 实体数量上限 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>entityDefsCount<span class="token operator">&gt;</span><span class="token number">0</span><span class="token operator">&lt;</span><span class="token operator">/</span>entityDefsCount<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 消息队列大小 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>bufferedMessages<span class="token operator">&gt;</span><span class="token number">0</span><span class="token operator">&lt;</span><span class="token operator">/</span>bufferedMessages<span class="token operator">&gt;</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>Baseapp<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line"><span class="token operator">&lt;</span>Cellapp<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 最大实体数 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>maxEntities<span class="token operator">&gt;</span><span class="token number">5000</span><span class="token operator">&lt;</span><span class="token operator">/</span>maxEntities<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 更新频率 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>updateHertz<span class="token operator">&gt;</span><span class="token number">20</span><span class="token operator">&lt;</span><span class="token operator">/</span>updateHertz<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> AOI 范围 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>aoiRadius<span class="token operator">&gt;</span><span class="token number">100</span><span class="token operator">&lt;</span><span class="token operator">/</span>aoiRadius<span class="token operator">&gt;</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>Cellapp<span class="token operator">&gt;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、性能优化工具链" tabindex="-1"><a class="header-anchor" href="#六、性能优化工具链"><span>六、性能优化工具链</span></a></h2><h3 id="_6-1-完整工具链" tabindex="-1"><a class="header-anchor" href="#_6-1-完整工具链"><span>6.1 完整工具链</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  性能分析工具链                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  开发阶段：                                                │</span>
<span class="line">│  ├── Valgrind - 内存泄漏检测                             │</span>
<span class="line">│  ├── ASan/UBSan - 内存错误检测                          │</span>
<span class="line">│  ├── perf - CPU 性能分析                                │</span>
<span class="line">│  └── gprof - 函数级性能分析                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  测试阶段：                                                │</span>
<span class="line">│  ├── Apache Bench - 压力测试                            │</span>
<span class="line">│  ├── wrk - HTTP 压力测试                                 │</span>
<span class="line">│  ├── iperf - 网络带宽测试                               │</span>
<span class="line">│  └── tcpdump/Wireshark - 网络分析                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  生产环境：                                                │</span>
<span class="line">│  ├── Prometheus - 指标采集                               │</span>
<span class="line">│  ├── Grafana - 可视化                                   │</span>
<span class="line">│  ├── Jaeger - 分布式追踪                                │</span>
<span class="line">│  └── ELK Stack - 日志分析                               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-prometheus-grafana" tabindex="-1"><a class="header-anchor" href="#_6-2-prometheus-grafana"><span>6.2 Prometheus + Grafana</span></a></h3><div class="language-yaml line-numbers-mode" data-highlighter="prismjs" data-ext="yml"><pre><code class="language-yaml"><span class="line"><span class="token comment"># Prometheus 配置</span></span>
<span class="line"></span>
<span class="line"><span class="token key atrule">global</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token key atrule">scrape_interval</span><span class="token punctuation">:</span> 15s</span>
<span class="line"></span>
<span class="line"><span class="token key atrule">scrape_configs</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token punctuation">-</span> <span class="token key atrule">job_name</span><span class="token punctuation">:</span> <span class="token string">&#39;game-server&#39;</span></span>
<span class="line">    <span class="token key atrule">static_configs</span><span class="token punctuation">:</span></span>
<span class="line">      <span class="token punctuation">-</span> <span class="token key atrule">targets</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token string">&#39;localhost:9090&#39;</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">    <span class="token key atrule">metrics_path</span><span class="token punctuation">:</span> /metrics</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 自定义指标导出</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PrometheusExporter</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 导出指标</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">exportMetrics</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>stringstream ss<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// TPS 指标</span></span>
<span class="line">        ss <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;# HELP game_tps Transactions per second\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        ss <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;# TYPE game_tps gauge\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        ss <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;game_tps &quot;</span> <span class="token operator">&lt;&lt;</span> <span class="token function">calculateTPS</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 在线玩家数</span></span>
<span class="line">        ss <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;# HELP game_online_players Online players\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        ss <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;# TYPE game_online_players gauge\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        ss <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;game_online_players &quot;</span> <span class="token operator">&lt;&lt;</span> <span class="token function">getOnlinePlayerCount</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 帧率</span></span>
<span class="line">        ss <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;# HELP game_tick_ms Tick time in ms\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        ss <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;# TYPE game_tick_ms gauge\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        ss <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;game_tick_ms &quot;</span> <span class="token operator">&lt;&lt;</span> <span class="token function">getAvgTickTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> <span class="token string">&quot;\\n&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> ss<span class="token punctuation">.</span><span class="token function">str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、最佳实践" tabindex="-1"><a class="header-anchor" href="#七、最佳实践"><span>七、最佳实践</span></a></h2><h3 id="_7-1-性能分析流程" tabindex="-1"><a class="header-anchor" href="#_7-1-性能分析流程"><span>7.1 性能分析流程</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  性能分析最佳实践                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 确定性能目标                                            │</span>
<span class="line">│     ├── 单服承载 5000 人                                   │</span>
<span class="line">│     ├── 帧率 60 TPS                                         │</span>
<span class="line">│     └── 响应时间 &lt; 100ms                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 建立基线测试                                            │</span>
<span class="line">│     ├── 空闲服务器基准                                      │</span>
<span class="line">│     ├── 单玩家基准                                          │</span>
<span class="line">│     └── 满载基准                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 识别瓶颈                                                │</span>
<span class="line">│     ├── CPU 是否饱和                                        │</span>
<span class="line">│     ├── 内存是否泄漏                                        │</span>
<span class="line">│     ├── 网络是否阻塞                                        │</span>
<span class="line">│     └── IO 是否过高                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 定位热点                                                │</span>
<span class="line">│     ├── perf top 查看 CPU 热点                             │</span>
<span class="line">│     ├── flamegraph 生成火焰图                              │</span>
<span class="line">│     ├── pprof 分析函数调用                                  │</span>
<span class="line">│     └── 自埋点统计执行时间                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 优化改进                                                │</span>
<span class="line">│     ├── 优化热点函数                                        │</span>
<span class="line">│     ├── 减少内存分配                                        │</span>
<span class="line">│     ├── 优化算法复杂度                                      │</span>
<span class="line">│     └── 异步化阻塞操作                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  6. 验证效果                                                │</span>
<span class="line">│     ├── 压力测试验证                                        │</span>
<span class="line">│     ├── 对比优化前后                                        │</span>
<span class="line">│     └── 确保达到目标                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-2-常用命令速查" tabindex="-1"><a class="header-anchor" href="#_7-2-常用命令速查"><span>7.2 常用命令速查</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># CPU 分析</span></span>
<span class="line"><span class="token function">top</span> <span class="token parameter variable">-p</span> <span class="token operator">&lt;</span>pid<span class="token operator">&gt;</span></span>
<span class="line">mpstat <span class="token parameter variable">-P</span> ALL <span class="token number">5</span></span>
<span class="line">perf <span class="token function">top</span> <span class="token parameter variable">-p</span> <span class="token operator">&lt;</span>pid<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 内存分析</span></span>
<span class="line"><span class="token function">free</span> <span class="token parameter variable">-h</span></span>
<span class="line"><span class="token function">vmstat</span> <span class="token number">1</span> <span class="token number">5</span></span>
<span class="line">pmap <span class="token parameter variable">-x</span> <span class="token operator">&lt;</span>pid<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 网络分析</span></span>
<span class="line"><span class="token function">netstat</span> <span class="token parameter variable">-an</span> <span class="token operator">|</span> <span class="token function">grep</span> ESTABLISHED <span class="token operator">|</span> <span class="token function">wc</span> <span class="token parameter variable">-l</span></span>
<span class="line">ss <span class="token parameter variable">-s</span></span>
<span class="line">tcpdump <span class="token parameter variable">-i</span> any port <span class="token number">9999</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># IO 分析</span></span>
<span class="line">iostat <span class="token parameter variable">-x</span> <span class="token number">1</span></span>
<span class="line">iotop <span class="token parameter variable">-o</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 综合监控</span></span>
<span class="line">dstat <span class="token parameter variable">-cdngy</span> <span class="token number">1</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="工具选择指南" tabindex="-1"><a class="header-anchor" href="#工具选择指南"><span>工具选择指南</span></a></h3><table><thead><tr><th>场景</th><th>推荐工具</th><th>用途</th></tr></thead><tbody><tr><td><strong>CPU 热点</strong></td><td>perf</td><td>找出 CPU 密集函数</td></tr><tr><td><strong>内存泄漏</strong></td><td>Valgrind/ASan</td><td>检测内存问题</td></tr><tr><td><strong>网络分析</strong></td><td>Wireshark/tcpdump</td><td>抓包分析</td></tr><tr><td><strong>压力测试</strong></td><td>Apache Bench/wrk</td><td>模拟高并发</td></tr><tr><td><strong>应用监控</strong></td><td>Prometheus+Grafana</td><td>生产监控</td></tr></tbody></table><h3 id="性能分析检查清单" tabindex="-1"><a class="header-anchor" href="#性能分析检查清单"><span>性能分析检查清单</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">□ 是否建立了性能基线？</span>
<span class="line">□ 是否使用性能分析工具？</span>
<span class="line">□ 是否定期进行性能测试？</span>
<span class="line">□ 是否监控生产环境指标？</span>
<span class="line">□ 是否有性能问题处理流程？</span>
<span class="line">□ 是否建立了性能优化文档？</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://perf.wiki.kernel.org/" target="_blank" rel="noopener noreferrer">Linux perf 官方文档</a></li><li><a href="https://valgrind.org/docs/manual/" target="_blank" rel="noopener noreferrer">Valgrind 用户手册</a></li><li><a href="https://prometheus.io/docs/practices/" target="_blank" rel="noopener noreferrer">Prometheus 最佳实践</a></li></ul>`,50)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};