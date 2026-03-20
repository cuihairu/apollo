import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q60-timer-system.html","title":"Q60: 如何设计定时器系统？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q60-timer-system.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q60-timer-system.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q60-如何设计定时器系统" tabindex="-1"><a class="header-anchor" href="#q60-如何设计定时器系统"><span>Q60: 如何设计定时器系统？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对定时器系统的理解：</p><ul><li>定时器数据结构</li><li>时间轮算法</li><li>最小堆实现</li><li>定时器精度与性能</li><li>KBEngine 定时器机制</li></ul><hr><h2 id="一、定时器系统架构" tabindex="-1"><a class="header-anchor" href="#一、定时器系统架构"><span>一、定时器系统架构</span></a></h2><h3 id="_1-1-系统组成" tabindex="-1"><a class="header-anchor" href="#_1-1-系统组成"><span>1.1 系统组成</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    定时器系统架构                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  API 层 (API Layer):                                        │</span>
<span class="line">│  ├── addTimer(callback, delay)      - 添加一次性定时器      │</span>
<span class="line">│  ├── addRepeatTimer(callback, interval) - 添加重复定时器    │</span>
<span class="line">│  ├── cancelTimer(timerId)           - 取消定时器            │</span>
<span class="line">│  └── updateTime()                   - 更新时间              │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│  ▼                                                          │</span>
<span class="line">│  管理层 (Manager Layer):                                    │</span>
<span class="line">│  ├── TimerManager                 - 定时器管理器           │</span>
<span class="line">│  ├── TimerIdGenerator              - ID 生成器             │</span>
<span class="line">│  └── TimerCollector                - 过期定时器收集         │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│  ▼                                                          │</span>
<span class="line">│  存储层 (Storage Layer):                                    │</span>
<span class="line">│  ├── 时间轮 (Timing Wheel)          - 低精度大量定时器      │</span>
<span class="line">│  ├── 最小堆 (Min-Heap)              - 高精度少量定时器      │</span>
<span class="line">│  └── 哈希表 (Hash Table)            - 快速查找              │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│  ▼                                                          │</span>
<span class="line">│  调度层 (Dispatch Layer):                                   │</span>
<span class="line">│  ├── 回调队列 (Callback Queue)       - 待执行回调           │</span>
<span class="line">│  ├── 线程池 (Thread Pool)           - 异步执行             │</span>
<span class="line">│  └── 事件循环 (Event Loop)          - 集成到主循环         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-定时器类型" tabindex="-1"><a class="header-anchor" href="#_1-2-定时器类型"><span>1.2 定时器类型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    定时器类型                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 一次性定时器 (One-shot Timer)                           │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 触发后自动移除                                   │       │</span>
<span class="line">│  │  - 用途: 延迟执行、超时检测                         │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 周期定时器 (Periodic Timer)                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 按固定间隔重复触发                               │       │</span>
<span class="line">│  │  - 用途: 定时保存、心跳检测、Buff 持续时间          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 条件定时器 (Conditional Timer)                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 满足条件时触发                                   │       │</span>
<span class="line">│  │  - 用途: 等待状态达成、资源累积                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 高精度定时器 (High-precision Timer)                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 毫秒级精度                                       │       │</span>
<span class="line">│  │  - 用途: 战斗技能、动画同步                         │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 低精度定时器 (Low-precision Timer)                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 秒级精度                                        │       │</span>
<span class="line">│  │  - 用途: 数据存盘、日志清理、统计上报               │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、时间轮算法" tabindex="-1"><a class="header-anchor" href="#二、时间轮算法"><span>二、时间轮算法</span></a></h2><h3 id="_2-1-时间轮原理" tabindex="-1"><a class="header-anchor" href="#_2-1-时间轮原理"><span>2.1 时间轮原理</span></a></h3>`,13),i(d,{code:`eJxLy8kvT85ILCpRCHHhUgACx+ineyc/7ex91tP+clLHk729z/eui1XQ1bVTcIp+trjh2fylUPnle4GSsWA9TmB552qI2LM5nU/XzXo2fdvTmSvsa8EKnEEKap7NWF+j4BL9rH/C0/5pcCUQIyAqnk5YVqPgGv18+e6nXSuedmx4sqP7yY4GoLEQRS5ge9yq4XqBSp7NmQ+1xA1hiXv0s87lLxb2PJ0978WGZohmiDTYBo/oZ1P2PW1d+nJm7/Ndy18APQhW4Q423rP66cQVQFPR/eCJMN4r+mV777NpG571TwKaAtEMkQYb7x39tGPBy5lLIBJeYFNdwWwPJLYrJFC5ADHdrSk=`}),o[1]||=e(`<h3 id="_2-2-分层时间轮实现" tabindex="-1"><a class="header-anchor" href="#_2-2-分层时间轮实现"><span>2.2 分层时间轮实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 分层时间轮实现 (类似 Linux Kernel 的定时器)</span></span>
<span class="line"><span class="token comment">// 参考: KBEngine 的定时器实现</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;vector&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;functional&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;unordered_map&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;queue&gt;</span></span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TimingWheel</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">using</span> Callback <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">&gt;</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">using</span> TimerId <span class="token operator">=</span> <span class="token keyword">uint64_t</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加定时器</span></span>
<span class="line">    TimerId <span class="token function">addTimer</span><span class="token punctuation">(</span>Callback cb<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> delayMs<span class="token punctuation">,</span> <span class="token keyword">bool</span> repeat <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        TimerId id <span class="token operator">=</span> <span class="token function">generateId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        Timer timer<span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>id <span class="token operator">=</span> id<span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>callback <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>cb<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>delayMs <span class="token operator">=</span> delayMs<span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>repeat <span class="token operator">=</span> repeat<span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>remaining <span class="token operator">=</span> delayMs<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 根据延迟选择轮层级</span></span>
<span class="line">        <span class="token function">insertTimer</span><span class="token punctuation">(</span>timer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        timers_<span class="token punctuation">[</span>id<span class="token punctuation">]</span> <span class="token operator">=</span> timer<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> id<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 取消定时器</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">cancelTimer</span><span class="token punctuation">(</span>TimerId id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> timers_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> timers_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>cancelled <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        timers_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新时间 (每帧调用)</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">tick</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> deltaMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        currentTime_ <span class="token operator">+=</span> deltaMs<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查第一层轮</span></span>
<span class="line">        <span class="token function">processWheel</span><span class="token punctuation">(</span>wheel0_<span class="token punctuation">,</span> slot0Size_<span class="token punctuation">,</span> slot0Ms_<span class="token punctuation">,</span> deltaMs<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 第一层轮满，进位到第二层</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>currentTime_ <span class="token operator">%</span> <span class="token punctuation">(</span>slot0Size_ <span class="token operator">*</span> slot0Ms_<span class="token punctuation">)</span> <span class="token operator">&lt;</span> deltaMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">cascade</span><span class="token punctuation">(</span>wheel0_<span class="token punctuation">,</span> wheel1_<span class="token punctuation">,</span> slot0Size_<span class="token punctuation">,</span> slot1Size_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 第二层轮满，进位到第三层</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>currentTime_ <span class="token operator">%</span> <span class="token punctuation">(</span>slot1Size_ <span class="token operator">*</span> slot0Size_ <span class="token operator">*</span> slot0Ms_<span class="token punctuation">)</span> <span class="token operator">&lt;</span> deltaMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">cascade</span><span class="token punctuation">(</span>wheel1_<span class="token punctuation">,</span> wheel2_<span class="token punctuation">,</span> slot1Size_<span class="token punctuation">,</span> slot2Size_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Timer</span> <span class="token punctuation">{</span></span>
<span class="line">        TimerId id<span class="token punctuation">;</span></span>
<span class="line">        Callback callback<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> delayMs<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> remaining<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">bool</span> repeat<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">bool</span> cancelled <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Slot</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>TimerId<span class="token operator">&gt;</span> timers<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">insertTimer</span><span class="token punctuation">(</span>Timer<span class="token operator">&amp;</span> timer<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> ticks <span class="token operator">=</span> timer<span class="token punctuation">.</span>remaining <span class="token operator">/</span> slot0Ms_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>ticks <span class="token operator">&lt;</span> slot0Size_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 放入第一层</span></span>
<span class="line">            wheel0_<span class="token punctuation">[</span>ticks <span class="token operator">%</span> slot0Size_<span class="token punctuation">]</span><span class="token punctuation">.</span>timers<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>timer<span class="token punctuation">.</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>ticks <span class="token operator">&lt;</span> slot0Size_ <span class="token operator">*</span> slot1Size_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 放入第二层</span></span>
<span class="line">            <span class="token keyword">uint64_t</span> slot <span class="token operator">=</span> <span class="token punctuation">(</span>ticks <span class="token operator">/</span> slot0Size_<span class="token punctuation">)</span> <span class="token operator">%</span> slot1Size_<span class="token punctuation">;</span></span>
<span class="line">            wheel1_<span class="token punctuation">[</span>slot<span class="token punctuation">]</span><span class="token punctuation">.</span>timers<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>timer<span class="token punctuation">.</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 放入第三层</span></span>
<span class="line">            <span class="token keyword">uint64_t</span> slot <span class="token operator">=</span> <span class="token punctuation">(</span>ticks <span class="token operator">/</span> <span class="token punctuation">(</span>slot0Size_ <span class="token operator">*</span> slot1Size_<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token operator">%</span> slot2Size_<span class="token punctuation">;</span></span>
<span class="line">            wheel2_<span class="token punctuation">[</span>slot<span class="token punctuation">]</span><span class="token punctuation">.</span>timers<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>timer<span class="token punctuation">.</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processWheel</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Slot<span class="token operator">&gt;</span><span class="token operator">&amp;</span> wheel<span class="token punctuation">,</span> size_t wheelSize<span class="token punctuation">,</span></span>
<span class="line">                      <span class="token keyword">uint64_t</span> slotMs<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> deltaMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t slots <span class="token operator">=</span> deltaMs <span class="token operator">/</span> slotMs<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> slots <span class="token operator">&amp;&amp;</span> i <span class="token operator">&lt;</span> wheelSize<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Slot<span class="token operator">&amp;</span> slot <span class="token operator">=</span> wheel<span class="token punctuation">[</span>currentSlot_ <span class="token operator">%</span> wheelSize<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>TimerId id <span class="token operator">:</span> slot<span class="token punctuation">.</span>timers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">auto</span> it <span class="token operator">=</span> timers_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> timers_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> <span class="token operator">!</span>it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>cancelled<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">executeTimer</span><span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>second<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            slot<span class="token punctuation">.</span>timers<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            currentSlot_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">executeTimer</span><span class="token punctuation">(</span>Timer<span class="token operator">&amp;</span> timer<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>timer<span class="token punctuation">.</span>callback<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            timer<span class="token punctuation">.</span><span class="token function">callback</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>timer<span class="token punctuation">.</span>repeat<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            timer<span class="token punctuation">.</span>remaining <span class="token operator">=</span> timer<span class="token punctuation">.</span>delayMs<span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">insertTimer</span><span class="token punctuation">(</span>timer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            timers_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>timer<span class="token punctuation">.</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">cascade</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Slot<span class="token operator">&gt;</span><span class="token operator">&amp;</span> from<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Slot<span class="token operator">&gt;</span><span class="token operator">&amp;</span> to<span class="token punctuation">,</span></span>
<span class="line">                 size_t fromSize<span class="token punctuation">,</span> size_t toSize<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 将下一层的定时器重新分配</span></span>
<span class="line">        <span class="token comment">// 实现省略...</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    TimerId <span class="token function">generateId</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token operator">++</span>nextId_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 时间轮配置</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t slot0Size_ <span class="token operator">=</span> <span class="token number">256</span><span class="token punctuation">;</span>   <span class="token comment">// 第一层 256 槽</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t slot1Size_ <span class="token operator">=</span> <span class="token number">64</span><span class="token punctuation">;</span>    <span class="token comment">// 第二层 64 槽</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t slot2Size_ <span class="token operator">=</span> <span class="token number">64</span><span class="token punctuation">;</span>    <span class="token comment">// 第三层 64 槽</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint64_t</span> slot0Ms_ <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span>    <span class="token comment">// 每槽 10ms</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Slot<span class="token operator">&gt;</span> wheel0_<span class="token punctuation">{</span>slot0Size_<span class="token punctuation">}</span><span class="token punctuation">;</span>     <span class="token comment">// 2.56 秒</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Slot<span class="token operator">&gt;</span> wheel1_<span class="token punctuation">{</span>slot1Size_<span class="token punctuation">}</span><span class="token punctuation">;</span>     <span class="token comment">// ~164 秒</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Slot<span class="token operator">&gt;</span> wheel2_<span class="token punctuation">{</span>slot2Size_<span class="token punctuation">}</span><span class="token punctuation">;</span>     <span class="token comment">// ~3 小时</span></span>
<span class="line"></span>
<span class="line">    size_t currentSlot_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> currentTime_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    TimerId nextId_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>TimerId<span class="token punctuation">,</span> Timer<span class="token operator">&gt;</span> timers_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、最小堆实现" tabindex="-1"><a class="header-anchor" href="#三、最小堆实现"><span>三、最小堆实现</span></a></h2><h3 id="_3-1-最小堆定时器" tabindex="-1"><a class="header-anchor" href="#_3-1-最小堆定时器"><span>3.1 最小堆定时器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 基于最小堆的定时器 (高精度场景)</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;queue&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;functional&gt;</span></span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MinHeapTimer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">using</span> Callback <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">&gt;</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">using</span> TimerId <span class="token operator">=</span> <span class="token keyword">uint64_t</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">using</span> TimePoint <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>steady_clock<span class="token double-colon punctuation">::</span>time_point<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    TimerId <span class="token function">addTimer</span><span class="token punctuation">(</span>Callback cb<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>milliseconds delay<span class="token punctuation">,</span> <span class="token keyword">bool</span> repeat <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        TimerId id <span class="token operator">=</span> <span class="token function">generateId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        Timer timer<span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>id <span class="token operator">=</span> id<span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>callback <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>cb<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>expiry <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>steady_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">+</span> delay<span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>delay <span class="token operator">=</span> delay<span class="token punctuation">;</span></span>
<span class="line">        timer<span class="token punctuation">.</span>repeat <span class="token operator">=</span> repeat<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        heap_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>timer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        timers_<span class="token punctuation">[</span>id<span class="token punctuation">]</span> <span class="token operator">=</span> timer<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> id<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">cancelTimer</span><span class="token punctuation">(</span>TimerId id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> timers_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> timers_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>cancelled <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        timers_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新定时器</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> now <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>steady_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>heap_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> heap_<span class="token punctuation">.</span><span class="token function">top</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>expiry <span class="token operator">&lt;=</span> now<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Timer timer <span class="token operator">=</span> heap_<span class="token punctuation">.</span><span class="token function">top</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            heap_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">auto</span> it <span class="token operator">=</span> timers_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>timer<span class="token punctuation">.</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> timers_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> timer<span class="token punctuation">.</span>cancelled<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">continue</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 执行回调</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>timer<span class="token punctuation">.</span>callback<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                timer<span class="token punctuation">.</span><span class="token function">callback</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 周期定时器重新插入</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>timer<span class="token punctuation">.</span>repeat <span class="token operator">&amp;&amp;</span> <span class="token operator">!</span>timer<span class="token punctuation">.</span>cancelled<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                timer<span class="token punctuation">.</span>expiry <span class="token operator">=</span> now <span class="token operator">+</span> timer<span class="token punctuation">.</span>delay<span class="token punctuation">;</span></span>
<span class="line">                heap_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>timer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                timers_<span class="token punctuation">[</span>timer<span class="token punctuation">.</span>id<span class="token punctuation">]</span> <span class="token operator">=</span> timer<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                timers_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>timer<span class="token punctuation">.</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取下次触发时间</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>optional<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>milliseconds<span class="token operator">&gt;</span> <span class="token function">getNextDelay</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>heap_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> std<span class="token double-colon punctuation">::</span>nullopt<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> now <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>steady_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> delay <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">duration_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>milliseconds<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span></span>
<span class="line">            heap_<span class="token punctuation">.</span><span class="token function">top</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>expiry <span class="token operator">-</span> now</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> std<span class="token double-colon punctuation">::</span><span class="token function">max</span><span class="token punctuation">(</span>delay<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">milliseconds</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Timer</span> <span class="token punctuation">{</span></span>
<span class="line">        TimerId id<span class="token punctuation">;</span></span>
<span class="line">        Callback callback<span class="token punctuation">;</span></span>
<span class="line">        TimePoint expiry<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>milliseconds delay<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">bool</span> repeat <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">bool</span> cancelled <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 堆比较</span></span>
<span class="line">        <span class="token keyword">bool</span> <span class="token keyword">operator</span><span class="token operator">&gt;</span><span class="token punctuation">(</span><span class="token keyword">const</span> Timer<span class="token operator">&amp;</span> other<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> expiry <span class="token operator">&gt;</span> other<span class="token punctuation">.</span>expiry<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    TimerId <span class="token function">generateId</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token operator">++</span>nextId_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>priority_queue<span class="token operator">&lt;</span>Timer<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Timer<span class="token operator">&gt;</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>greater<span class="token operator">&lt;</span>Timer<span class="token operator">&gt;&gt;</span> heap_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>TimerId<span class="token punctuation">,</span> Timer<span class="token operator">&gt;</span> timers_<span class="token punctuation">;</span></span>
<span class="line">    TimerId nextId_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-定时器" tabindex="-1"><a class="header-anchor" href="#四、kbengine-定时器"><span>四、KBEngine 定时器</span></a></h2><h3 id="_4-1-kbengine-python-定时器" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-python-定时器"><span>4.1 KBEngine Python 定时器</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine Python 定时器系统</span></span>
<span class="line"><span class="token comment"># scripts/kbe_scripts/timers.py</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"><span class="token keyword">import</span> time</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">KBE_Timer</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;KBEngine 定时器封装&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">addTimer</span><span class="token punctuation">(</span>interval<span class="token punctuation">,</span> callback<span class="token punctuation">,</span> repeat<span class="token operator">=</span><span class="token boolean">False</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        添加定时器</span>
<span class="line"></span>
<span class="line">        Args:</span>
<span class="line">            interval: 间隔时间 (秒)</span>
<span class="line">            callback: 回调函数</span>
<span class="line">            repeat: 是否重复</span>
<span class="line"></span>
<span class="line">        Returns:</span>
<span class="line">            timerId: 定时器 ID</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> repeat<span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 重复定时器</span></span>
<span class="line">            <span class="token keyword">return</span> KBEngine<span class="token punctuation">.</span>addTimer<span class="token punctuation">(</span>interval<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> callback<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 一次性定时器</span></span>
<span class="line">            <span class="token keyword">return</span> KBEngine<span class="token punctuation">.</span>addOnceTimer<span class="token punctuation">(</span>interval<span class="token punctuation">,</span> callback<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">cancelTimer</span><span class="token punctuation">(</span>timerId<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;取消定时器&quot;&quot;&quot;</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>delTimer<span class="token punctuation">(</span>timerId<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 添加一次性定时器</span></span>
<span class="line">        self<span class="token punctuation">.</span>delayTimer <span class="token operator">=</span> KBE_Timer<span class="token punctuation">.</span>addTimer<span class="token punctuation">(</span></span>
<span class="line">            <span class="token number">5.0</span><span class="token punctuation">,</span>           <span class="token comment"># 5 秒后</span></span>
<span class="line">            self<span class="token punctuation">.</span>onTimeout</span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 添加重复定时器</span></span>
<span class="line">        self<span class="token punctuation">.</span>repeatTimer <span class="token operator">=</span> KBE_Timer<span class="token punctuation">.</span>addTimer<span class="token punctuation">(</span></span>
<span class="line">            <span class="token number">1.0</span><span class="token punctuation">,</span>           <span class="token comment"># 每秒</span></span>
<span class="line">            self<span class="token punctuation">.</span>onTick<span class="token punctuation">,</span></span>
<span class="line">            repeat<span class="token operator">=</span><span class="token boolean">True</span></span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onTimeout</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;超时回调&quot;&quot;&quot;</span></span>
<span class="line">        INFO<span class="token punctuation">(</span><span class="token string">&quot;Timeout reached!&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># Buff 到期、技能冷却等</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onTick</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;定时回调&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 持续伤害、心跳检测等</span></span>
<span class="line">        <span class="token keyword">pass</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onDestroy</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;实体销毁时清理定时器&quot;&quot;&quot;</span></span>
<span class="line">        KBE_Timer<span class="token punctuation">.</span>cancelTimer<span class="token punctuation">(</span>self<span class="token punctuation">.</span>delayTimer<span class="token punctuation">)</span></span>
<span class="line">        KBE_Timer<span class="token punctuation">.</span>cancelTimer<span class="token punctuation">(</span>self<span class="token punctuation">.</span>repeatTimer<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-kbengine-c-定时器" tabindex="-1"><a class="header-anchor" href="#_4-2-kbengine-c-定时器"><span>4.2 KBEngine C++ 定时器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine C++ 定时器系统</span></span>
<span class="line"><span class="token comment">// src/lib/helpers/timer.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 定时器回调类型</span></span>
<span class="line"><span class="token keyword">using</span> TimerCallback <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">&gt;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 定时器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Timer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">Timer</span><span class="token punctuation">(</span>TimerCallback cb<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> interval<span class="token punctuation">,</span> <span class="token keyword">bool</span> repeat<span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">callback_</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>cb<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">          <span class="token function">interval_</span><span class="token punctuation">(</span>interval<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">          <span class="token function">repeat_</span><span class="token punctuation">(</span>repeat<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">          <span class="token function">remaining_</span><span class="token punctuation">(</span>interval<span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> deltaMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>remaining_ <span class="token operator">&gt;</span> deltaMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            remaining_ <span class="token operator">-=</span> deltaMs<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 触发</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>callback_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">callback_</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>repeat_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                remaining_ <span class="token operator">=</span> interval_<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                finished_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isFinished</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> finished_<span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    TimerCallback callback_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> interval_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> remaining_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> repeat_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> finished_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 定时器管理器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TimerManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    TimerId <span class="token function">addTimer</span><span class="token punctuation">(</span>TimerCallback cb<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> intervalMs<span class="token punctuation">,</span> <span class="token keyword">bool</span> repeat <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> timer <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">make_unique</span><span class="token generic class-name"><span class="token operator">&lt;</span>Timer<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>cb<span class="token punctuation">)</span><span class="token punctuation">,</span> intervalMs<span class="token punctuation">,</span> repeat</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        TimerId id <span class="token operator">=</span> <span class="token operator">++</span>nextId_<span class="token punctuation">;</span></span>
<span class="line">        timers_<span class="token punctuation">[</span>id<span class="token punctuation">]</span> <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>timer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> id<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">cancelTimer</span><span class="token punctuation">(</span>TimerId id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        timers_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> deltaMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> timers_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> timers_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            it<span class="token operator">-&gt;</span>second<span class="token operator">-&gt;</span><span class="token function">update</span><span class="token punctuation">(</span>deltaMs<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>second<span class="token operator">-&gt;</span><span class="token function">isFinished</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                it <span class="token operator">=</span> timers_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token operator">++</span>it<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">using</span> TimerId <span class="token operator">=</span> <span class="token keyword">uint64_t</span><span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>TimerId<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>Timer<span class="token operator">&gt;&gt;</span> timers_<span class="token punctuation">;</span></span>
<span class="line">    TimerId nextId_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、性能对比" tabindex="-1"><a class="header-anchor" href="#五、性能对比"><span>五、性能对比</span></a></h2><h3 id="_5-1-定时器实现对比" tabindex="-1"><a class="header-anchor" href="#_5-1-定时器实现对比"><span>5.1 定时器实现对比</span></a></h3><table><thead><tr><th>实现方式</th><th>添加复杂度</th><th>删除复杂度</th><th>触发复杂度</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>链表</strong></td><td>O(1)</td><td>O(n)</td><td>O(n)</td><td>少量定时器</td></tr><tr><td><strong>排序链表</strong></td><td>O(n)</td><td>O(n)</td><td>O(1)</td><td>少量定时器</td></tr><tr><td><strong>最小堆</strong></td><td>O(log n)</td><td>O(log n)</td><td>O(1)</td><td>大量定时器</td></tr><tr><td><strong>时间轮</strong></td><td>O(1)</td><td>O(1)</td><td>O(1)</td><td>大量定时器</td></tr></tbody></table><h3 id="_5-2-定时器选择" tabindex="-1"><a class="header-anchor" href="#_5-2-定时器选择"><span>5.2 定时器选择</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    定时器选择指南                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  定时器数量 &lt; 100:                                           │</span>
<span class="line">│  └── 使用最小堆 (简单高效)                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  定时器数量 100-1000:                                        │</span>
<span class="line">│  └── 使用单层时间轮                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  定时器数量 &gt; 1000:                                          │</span>
<span class="line">│  └── 使用分层时间轮 (Linux Kernel 方案)                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  高精度要求 (&lt;10ms):                                         │</span>
<span class="line">│  └── 使用最小堆 + 高精度时钟                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  低精度要求 (&gt;100ms):                                        │</span>
<span class="line">│  └── 使用时间轮 (性能最优)                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-定时器设计建议" tabindex="-1"><a class="header-anchor" href="#_6-1-定时器设计建议"><span>6.1 定时器设计建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>分层处理</strong></td><td>高精度用堆，低精度用轮</td></tr><tr><td><strong>避免过多定时器</strong></td><td>合并相似定时器</td></tr><tr><td><strong>及时取消</strong></td><td>不用的定时器立即取消</td></tr><tr><td><strong>回调轻量化</strong></td><td>避免回调中执行耗时操作</td></tr><tr><td><strong>周期检测</strong></td><td>定期清理僵尸定时器</td></tr></tbody></table><h3 id="_6-2-kbengine-定时器技巧" tabindex="-1"><a class="header-anchor" href="#_6-2-kbengine-定时器技巧"><span>6.2 KBEngine 定时器技巧</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 1. 使用 addTimer 的用户参数</span></span>
<span class="line">KBEngine<span class="token punctuation">.</span>addTimer<span class="token punctuation">(</span><span class="token number">1.0</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>onTimer<span class="token punctuation">,</span> userData<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. 批量处理定时器事件</span></span>
<span class="line"><span class="token comment"># 减少定时器数量，用一个定时器处理多个事件</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 3. 帧回调代替高频定时器</span></span>
<span class="line"><span class="token comment"># 对于每帧都要执行的操作，使用 onTick 而非短间隔定时器</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="定时器系统核心" tabindex="-1"><a class="header-anchor" href="#定时器系统核心"><span>定时器系统核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">定时器系统 = 时间管理 + 触发机制 + 回调执行</span>
<span class="line">- 时间轮: 大量低精度定时器</span>
<span class="line">- 最小堆: 少量高精度定时器</span>
<span class="line">- 分层处理: 平衡性能与精度</span>
<span class="line">- 与事件循环集成</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.kernel.org/doc/Documentation/timers/timers-howto.txt" target="_blank" rel="noopener noreferrer">Linux Kernel Timer Implementation</a></li><li><a href="https://github.com/kbengine/kbengine/tree/master/src/server" target="_blank" rel="noopener noreferrer">KBEngine Timer System</a></li><li><a href="https://www.gameenginebook.com/" target="_blank" rel="noopener noreferrer">Game Engine Architecture - Event Loop</a></li></ul>`,31)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};