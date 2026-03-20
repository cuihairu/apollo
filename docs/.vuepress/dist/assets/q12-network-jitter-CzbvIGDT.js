import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q12-network-jitter.html","title":"Q12: 如何处理网络抖动和丢包？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q12-network-jitter.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q12-network-jitter.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q12-如何处理网络抖动和丢包" tabindex="-1"><a class="header-anchor" href="#q12-如何处理网络抖动和丢包"><span>Q12: 如何处理网络抖动和丢包？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对网络不稳定的处理能力：</p><ul><li>网络抖动（Jitter）和丢包的本质</li><li>抖动缓冲和预测技术</li><li>KBEngine 的处理方式</li><li>前端插值与后端补偿</li></ul><hr><h2 id="一、网络问题概述" tabindex="-1"><a class="header-anchor" href="#一、网络问题概述"><span>一、网络问题概述</span></a></h2><h3 id="_1-1-网络问题分类" tabindex="-1"><a class="header-anchor" href="#_1-1-网络问题分类"><span>1.1 网络问题分类</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    网络问题分类                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 延迟 (Latency)                                          │</span>
<span class="line">│     ├── 传播延迟   = 物理距离 / 光速                        │</span>
<span class="line">│     ├── 传输延迟   = 数据大小 / 带宽                        │</span>
<span class="line">│     ├── 处理延迟   = 服务器处理时间                         │</span>
<span class="line">│     └── 排队延迟   = 网络设备排队                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 抖动 (Jitter)                                           │</span>
<span class="line">│     └── 延迟的变化 - RTT 忽高忽低                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 丢包 (Packet Loss)                                      │</span>
<span class="line">│     ├── 网络拥塞   → 路由器丢弃                             │</span>
<span class="line">│     ├── 链路错误   → 数据损坏                               │</span>
<span class="line">│     └── 缓冲溢出   → 接收方来不及处理                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 乱序 (Out of Order)                                     │</span>
<span class="line">│     └── 数据包到达顺序不一致                               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-问题影响" tabindex="-1"><a class="header-anchor" href="#_1-2-问题影响"><span>1.2 问题影响</span></a></h3><table><thead><tr><th>问题</th><th>影响</th><th>典型场景</th></tr></thead><tbody><tr><td><strong>延迟</strong></td><td>操作响应慢</td><td>跨国服务器</td></tr><tr><td><strong>抖动</strong></td><td>画面卡顿、跳跃</td><td>移动网络</td></tr><tr><td><strong>丢包</strong></td><td>操作无响应、瞬移</td><td>弱网环境</td></tr><tr><td><strong>乱序</strong></td><td>状态不一致</td><td>多路径路由</td></tr></tbody></table><hr><h2 id="二、抖动处理" tabindex="-1"><a class="header-anchor" href="#二、抖动处理"><span>二、抖动处理</span></a></h2><h3 id="_2-1-抖动缓冲-jitter-buffer" tabindex="-1"><a class="header-anchor" href="#_2-1-抖动缓冲-jitter-buffer"><span>2.1 抖动缓冲 (Jitter Buffer)</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    抖动缓冲原理                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  无抖动缓冲：                                                │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │ 到达时间: 10ms  │ 15ms │ 40ms │ 12ms │ 35ms    │       │</span>
<span class="line">│  │ 播放时间: 10ms  │ 15ms │ 40ms │ 12ms │ 35ms    │       │</span>
<span class="line">│  │ 结果:  不均匀，画面卡顿                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  有抖动缓冲：                                                │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │ 到达时间: 10ms  │ 15ms │ 40ms │ 12ms │ 35ms    │       │</span>
<span class="line">│  │         ↓         ↓      ↓      ↓      ↓       │       │</span>
<span class="line">│  │ 缓冲队列: [10] [15] [40] [12] [35]             │       │</span>
<span class="line">│  │         ↓         ↓      ↓      ↓      ↓       │       │</span>
<span class="line">│  │ 播放时间: 50ms  │ 55ms │ 60ms │ 65ms │ 70ms   │       │</span>
<span class="line">│  │ 结果:  均匀输出，流畅                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  代价：增加固定延迟 (buffer_size = 最大抖动)                │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-自适应抖动缓冲" tabindex="-1"><a class="header-anchor" href="#_2-2-自适应抖动缓冲"><span>2.2 自适应抖动缓冲</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 自适应抖动缓冲实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AdaptiveJitterBuffer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 最小/最大缓冲大小</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MIN_BUFFER_MS <span class="token operator">=</span> <span class="token number">20</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_BUFFER_MS <span class="token operator">=</span> <span class="token number">200</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 当前缓冲大小</span></span>
<span class="line">    <span class="token keyword">int</span> currentBufferSizeMs_ <span class="token operator">=</span> MIN_BUFFER_MS<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 包队列</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>deque<span class="token operator">&lt;</span>Packet<span class="token operator">&gt;</span> packetQueue_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 统计数据</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> totalJitter_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> jitterCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addPacket</span><span class="token punctuation">(</span><span class="token keyword">const</span> Packet<span class="token operator">&amp;</span> packet<span class="token punctuation">,</span> <span class="token keyword">int</span> receiveTimeMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 计算抖动</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>packetQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">int</span> expectedTime <span class="token operator">=</span> packetQueue_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>sendTimeMs <span class="token operator">+</span></span>
<span class="line">                              <span class="token punctuation">(</span>receiveTimeMs <span class="token operator">-</span> packet<span class="token punctuation">.</span>sendTimeMs<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">int</span> jitter <span class="token operator">=</span> <span class="token function">abs</span><span class="token punctuation">(</span>receiveTimeMs <span class="token operator">-</span> expectedTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 更新统计</span></span>
<span class="line">            totalJitter_ <span class="token operator">+=</span> jitter<span class="token punctuation">;</span></span>
<span class="line">            jitterCount_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 自适应调整缓冲大小</span></span>
<span class="line">            <span class="token function">adjustBufferSize</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        packetQueue_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>packet<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">adjustBufferSize</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 计算平均抖动</span></span>
<span class="line">        <span class="token keyword">int</span> avgJitter <span class="token operator">=</span> totalJitter_ <span class="token operator">/</span> jitterCount_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 缓冲大小 = 2 * 平均抖动 (经验公式)</span></span>
<span class="line">        <span class="token keyword">int</span> targetSize <span class="token operator">=</span> avgJitter <span class="token operator">*</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 限制在合理范围内</span></span>
<span class="line">        targetSize <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">clamp</span><span class="token punctuation">(</span>targetSize<span class="token punctuation">,</span></span>
<span class="line">                                MIN_BUFFER_MS<span class="token punctuation">,</span></span>
<span class="line">                                MAX_BUFFER_MS<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 平滑调整 (避免频繁变化)</span></span>
<span class="line">        currentBufferSizeMs_ <span class="token operator">=</span> currentBufferSizeMs_ <span class="token operator">*</span> <span class="token number">0.8</span> <span class="token operator">+</span></span>
<span class="line">                               targetSize <span class="token operator">*</span> <span class="token number">0.2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取当前应该播放的包</span></span>
<span class="line">    Packet<span class="token operator">*</span> <span class="token function">getPacketToPlay</span><span class="token punctuation">(</span><span class="token keyword">int</span> currentTimeMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> playTimeMs <span class="token operator">=</span> currentTimeMs <span class="token operator">+</span> currentBufferSizeMs_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>packetQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否有包应该播放</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>packetQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>sendTimeMs <span class="token operator">&gt;=</span> playTimeMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Packet<span class="token operator">&amp;</span> packet <span class="token operator">=</span> packetQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            packetQueue_<span class="token punctuation">.</span><span class="token function">pop_front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token operator">&amp;</span>packet<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-动态延迟补偿" tabindex="-1"><a class="header-anchor" href="#_2-3-动态延迟补偿"><span>2.3 动态延迟补偿</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                 动态延迟补偿策略                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  RTT 测量：                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  时刻    │ RTT   │ 加权 RTT │ 补偿延迟          │       │</span>
<span class="line">│  ├─────────────────────────────────────────────────┤       │</span>
<span class="line">│  │  T0     │ 50ms  │   50ms   │   25ms            │       │</span>
<span class="line">│  │  T1     │ 60ms  │   52ms   │   26ms            │       │</span>
<span class="line">│  │  T2     │ 200ms │   81ms   │   40ms   ↑ 抖动!  │       │</span>
<span class="line">│  │  T3     │ 55ms  │   76ms   │   38ms            │       │</span>
<span class="line">│  │  T4     │ 50ms  │   71ms   │   35ms   ↓ 恢复   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  加权公式：RTT_avg = RTT_avg * 0.8 + RTT_new * 0.2          │</span>
<span class="line">│  补偿延迟：RTT_avg / 2                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、丢包处理" tabindex="-1"><a class="header-anchor" href="#三、丢包处理"><span>三、丢包处理</span></a></h2><h3 id="_3-1-丢包检测" tabindex="-1"><a class="header-anchor" href="#_3-1-丢包检测"><span>3.1 丢包检测</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 丢包检测机制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PacketLossDetector</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 期望的序列号</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> expectedSeq_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 丢包统计</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> totalPackets_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> lostPackets_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onPacketReceived</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> seq<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        totalPackets_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检测丢包</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>seq <span class="token operator">!=</span> expectedSeq_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">uint16_t</span> gap <span class="token operator">=</span> <span class="token punctuation">(</span>seq <span class="token operator">-</span> expectedSeq_<span class="token punctuation">)</span> <span class="token operator">&amp;</span> <span class="token number">0xFFFF</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>gap <span class="token operator">&lt;</span> <span class="token number">1000</span><span class="token punctuation">)</span> <span class="token punctuation">{</span>  <span class="token comment">// 防止序列号回绕导致的误判</span></span>
<span class="line">                <span class="token comment">// 检测到丢包</span></span>
<span class="line">                lostPackets_ <span class="token operator">+=</span> gap<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment">// 请求重传</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint16_t</span> missing <span class="token operator">=</span> expectedSeq_<span class="token punctuation">;</span></span>
<span class="line">                     missing <span class="token operator">!=</span> seq<span class="token punctuation">;</span></span>
<span class="line">                     missing <span class="token operator">=</span> <span class="token punctuation">(</span>missing <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">&amp;</span> <span class="token number">0xFFFF</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">requestRetransmit</span><span class="token punctuation">(</span>missing<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        expectedSeq_ <span class="token operator">=</span> <span class="token punctuation">(</span>seq <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">&amp;</span> <span class="token number">0xFFFF</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取丢包率</span></span>
<span class="line">    <span class="token keyword">float</span> <span class="token function">getLossRate</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>totalPackets_ <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token number">0.0f</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span><span class="token keyword">float</span><span class="token punctuation">)</span>lostPackets_ <span class="token operator">/</span> totalPackets_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-重传机制" tabindex="-1"><a class="header-anchor" href="#_3-2-重传机制"><span>3.2 重传机制</span></a></h3>`,23),i(d,{code:`eJwrTi0sTc1LTnXJTEwvSszlUgCCgsSikszkzILEvBIFZ4XEYoWn6xY969j+fPV6DOlgkPSzOb1PuxY+nbmCCyzvl1+SqpBfllqk4Gyl8LR/4suGxmdTNzzrXfe0p1XBUEfBSEfBGKzQWdfOLhioBCSsoAF0ia2hJrqEEUTCSFPh0dwehSc7Fj1dshFdjTFEjbEmuv1Aebjbnk3Z9rRjA8Sup5N6oE4I1gUaA3Smo7O3giGGCNDgZ7OmP1+yC+ISiO0YtgAVP1vc8GxrN9R8mEJkZ/qBjAP65cX67c82Nr1s732yZwE250JkwKZAXQN2DDQkYPoAzAikwg==`}),o[1]||=e(`<h3 id="_3-3-fec-前向纠错" tabindex="-1"><a class="header-anchor" href="#_3-3-fec-前向纠错"><span>3.3 FEC 前向纠错</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">FEC (Forward Error Correction) 原理：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                     FEC 原理                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  发送端：                                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  原始数据: D1, D2, D3, D4                       │       │</span>
<span class="line">│  │           ↓                                     │       │</span>
<span class="line">│  │  计算 FEC: F1 = D1 ⊕ D2 ⊕ D3 ⊕ D4 (异或)        │       │</span>
<span class="line">│  │           ↓                                     │       │</span>
<span class="line">│  │  发送: D1, D2, D3, D4, F1                      │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  接收端（D2 丢失）：                                        │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  收到: D1, X, D3, D4, F1                        │       │</span>
<span class="line">│  │           ↓                                     │       │</span>
<span class="line">│  │  恢复: D2 = D1 ⊕ D3 ⊕ D4 ⊕ F1                  │       │</span>
<span class="line">│  │           ↓                                     │       │</span>
<span class="line">│  │  成功恢复丢失数据！                              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  限制：只能恢复单个包丢失，多个包丢失需要更强 FEC           │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// FEC 实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">FECCodec</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 编码（发送端）</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> <span class="token function">encode</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Packet<span class="token operator">&gt;</span><span class="token operator">&amp;</span> packets<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> fecData<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 所有包异或</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> packet <span class="token operator">:</span> packets<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>fecData<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                fecData <span class="token operator">=</span> packet<span class="token punctuation">.</span>data<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> packet<span class="token punctuation">.</span>data<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    fecData<span class="token punctuation">[</span>i<span class="token punctuation">]</span> <span class="token operator">^=</span> packet<span class="token punctuation">.</span>data<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> fecData<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 解码（接收端）</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">decode</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Packet<span class="token operator">&gt;</span><span class="token operator">&amp;</span> packets<span class="token punctuation">,</span></span>
<span class="line">                <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> fecData<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 找出丢失的包</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> packets<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>packets<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>lost<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 使用 FEC 恢复</span></span>
<span class="line">                packets<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>data <span class="token operator">=</span> fecData<span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t j <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> j <span class="token operator">&lt;</span> packets<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>j<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">if</span> <span class="token punctuation">(</span>j <span class="token operator">!=</span> i <span class="token operator">&amp;&amp;</span> <span class="token operator">!</span>packets<span class="token punctuation">[</span>j<span class="token punctuation">]</span><span class="token punctuation">.</span>lost<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t k <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> k <span class="token operator">&lt;</span> packets<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>data<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>k<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                            packets<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>data<span class="token punctuation">[</span>k<span class="token punctuation">]</span> <span class="token operator">^=</span> packets<span class="token punctuation">[</span>j<span class="token punctuation">]</span><span class="token punctuation">.</span>data<span class="token punctuation">[</span>k<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">                        <span class="token punctuation">}</span></span>
<span class="line">                    <span class="token punctuation">}</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">                packets<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>recovered <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、客户端补偿" tabindex="-1"><a class="header-anchor" href="#四、客户端补偿"><span>四、客户端补偿</span></a></h2><h3 id="_4-1-位置插值" tabindex="-1"><a class="header-anchor" href="#_4-1-位置插值"><span>4.1 位置插值</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 客户端位置插值</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PositionInterpolator</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 位置样本</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Sample</span> <span class="token punctuation">{</span></span>
<span class="line">        Vector3 position<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>deque<span class="token operator">&lt;</span>Sample<span class="token operator">&gt;</span> samples_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加位置样本</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addSample</span><span class="token punctuation">(</span><span class="token keyword">const</span> Vector3<span class="token operator">&amp;</span> pos<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        samples_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">{</span>pos<span class="token punctuation">,</span> timestamp<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 只保留最近 500ms 的样本</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> now <span class="token operator">=</span> <span class="token function">getTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>samples_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span></span>
<span class="line">               <span class="token punctuation">(</span>now <span class="token operator">-</span> samples_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>timestamp<span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">500</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            samples_<span class="token punctuation">.</span><span class="token function">pop_front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取插值位置</span></span>
<span class="line">    Vector3 <span class="token function">getInterpolatedPosition</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>samples_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> <span class="token number">2</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> samples_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">?</span> <span class="token function">Vector3</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">:</span> samples_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>position<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 找到前后两个样本</span></span>
<span class="line">        Sample<span class="token operator">*</span> prev <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">        Sample<span class="token operator">*</span> next <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> samples_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>samples_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span>timestamp <span class="token operator">&lt;=</span> timestamp <span class="token operator">&amp;&amp;</span></span>
<span class="line">                samples_<span class="token punctuation">[</span>i <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">]</span><span class="token punctuation">.</span>timestamp <span class="token operator">&gt;=</span> timestamp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                prev <span class="token operator">=</span> <span class="token operator">&amp;</span>samples_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">                next <span class="token operator">=</span> <span class="token operator">&amp;</span>samples_<span class="token punctuation">[</span>i <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>prev <span class="token operator">||</span> <span class="token operator">!</span>next<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> samples_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>position<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 线性插值</span></span>
<span class="line">        <span class="token keyword">float</span> t <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">float</span><span class="token punctuation">)</span><span class="token punctuation">(</span>timestamp <span class="token operator">-</span> prev<span class="token operator">-&gt;</span>timestamp<span class="token punctuation">)</span> <span class="token operator">/</span></span>
<span class="line">                  <span class="token punctuation">(</span>next<span class="token operator">-&gt;</span>timestamp <span class="token operator">-</span> prev<span class="token operator">-&gt;</span>timestamp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">lerp</span><span class="token punctuation">(</span>prev<span class="token operator">-&gt;</span>position<span class="token punctuation">,</span> next<span class="token operator">-&gt;</span>position<span class="token punctuation">,</span> t<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 线性插值</span></span>
<span class="line">    Vector3 <span class="token function">lerp</span><span class="token punctuation">(</span><span class="token keyword">const</span> Vector3<span class="token operator">&amp;</span> a<span class="token punctuation">,</span> <span class="token keyword">const</span> Vector3<span class="token operator">&amp;</span> b<span class="token punctuation">,</span> <span class="token keyword">float</span> t<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> a <span class="token operator">+</span> <span class="token punctuation">(</span>b <span class="token operator">-</span> a<span class="token punctuation">)</span> <span class="token operator">*</span> t<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-速度外推" tabindex="-1"><a class="header-anchor" href="#_4-2-速度外推"><span>4.2 速度外推</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 速度外推（处理丢包）</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">VelocityExtrapolator</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">State</span> <span class="token punctuation">{</span></span>
<span class="line">        Vector3 position<span class="token punctuation">;</span></span>
<span class="line">        Vector3 velocity<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    State lastState_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新状态</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token keyword">const</span> Vector3<span class="token operator">&amp;</span> pos<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>lastState_<span class="token punctuation">.</span>timestamp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            lastState_ <span class="token operator">=</span> <span class="token punctuation">{</span>pos<span class="token punctuation">,</span> <span class="token function">Vector3</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> timestamp<span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 计算速度</span></span>
<span class="line">        <span class="token keyword">float</span> dt <span class="token operator">=</span> <span class="token punctuation">(</span>timestamp <span class="token operator">-</span> lastState_<span class="token punctuation">.</span>timestamp<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">1000.0f</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>dt <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            lastState_<span class="token punctuation">.</span>velocity <span class="token operator">=</span> <span class="token punctuation">(</span>pos <span class="token operator">-</span> lastState_<span class="token punctuation">.</span>position<span class="token punctuation">)</span> <span class="token operator">/</span> dt<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        lastState_<span class="token punctuation">.</span>position <span class="token operator">=</span> pos<span class="token punctuation">;</span></span>
<span class="line">        lastState_<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 外推位置（丢包时使用）</span></span>
<span class="line">    Vector3 <span class="token function">extrapolate</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> currentTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">float</span> dt <span class="token operator">=</span> <span class="token punctuation">(</span>currentTime <span class="token operator">-</span> lastState_<span class="token punctuation">.</span>timestamp<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">1000.0f</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 限制外推时间（避免误差累积）</span></span>
<span class="line">        dt <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">min</span><span class="token punctuation">(</span>dt<span class="token punctuation">,</span> <span class="token number">0.5f</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 最多外推 500ms</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> lastState_<span class="token punctuation">.</span>position <span class="token operator">+</span> lastState_<span class="token punctuation">.</span>velocity <span class="token operator">*</span> dt<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、服务端处理" tabindex="-1"><a class="header-anchor" href="#五、服务端处理"><span>五、服务端处理</span></a></h2><h3 id="_5-1-kbengine-的可靠性机制" tabindex="-1"><a class="header-anchor" href="#_5-1-kbengine-的可靠性机制"><span>5.1 KBEngine 的可靠性机制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 的可靠传输机制</span></span>
<span class="line"><span class="token comment">// src/server/network/channel.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 发送可靠消息</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">send</span><span class="token punctuation">(</span>Message<span class="token operator">*</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>msg<span class="token operator">-&gt;</span><span class="token function">isReliable</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 添加到可靠队列</span></span>
<span class="line">            reliableQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 设置超时重传</span></span>
<span class="line">            msg<span class="token operator">-&gt;</span>timeout_ <span class="token operator">=</span> currentTime_ <span class="token operator">+</span> RTO<span class="token punctuation">;</span></span>
<span class="line">            msg<span class="token operator">-&gt;</span>retries_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> socket_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理 ACK</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onAck</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> seq<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 移除已确认的消息</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>reliableQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span></span>
<span class="line">               reliableQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">-&gt;</span>seq_ <span class="token operator">&lt;=</span> seq<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">delete</span> reliableQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            reliableQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 超时重传</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">checkRetransmit</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> now <span class="token operator">=</span> currentTime_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> msg <span class="token operator">:</span> reliableQueue_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>now <span class="token operator">&gt;=</span> msg<span class="token operator">-&gt;</span>timeout_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>msg<span class="token operator">-&gt;</span>retries_ <span class="token operator">&lt;</span> MAX_RETRIES<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token comment">// 重传</span></span>
<span class="line">                    socket_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    msg<span class="token operator">-&gt;</span>retries_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">                    msg<span class="token operator">-&gt;</span>timeout_ <span class="token operator">=</span> now <span class="token operator">+</span> RTO <span class="token operator">*</span> <span class="token punctuation">(</span><span class="token number">1</span> <span class="token operator">&lt;&lt;</span> msg<span class="token operator">-&gt;</span>retries_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token comment">// 超过最大重传次数，断开连接</span></span>
<span class="line">                    <span class="token function">onTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>Message<span class="token operator">*</span><span class="token operator">&gt;</span> reliableQueue_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint64_t</span> RTO <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span>  <span class="token comment">// 100ms</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_RETRIES <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-拥塞控制" tabindex="-1"><a class="header-anchor" href="#_5-2-拥塞控制"><span>5.2 拥塞控制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 的流量控制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">FlowController</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 发送窗口</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> sendWindow_ <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> unackedCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 判断是否可以发送</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">canSend</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> unackedCount_ <span class="token operator">&lt;</span> sendWindow_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onSend</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        unackedCount_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 收到 ACK</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onAck</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        unackedCount_<span class="token operator">--</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 动态调整窗口</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>unackedCount_ <span class="token operator">&lt;</span> sendWindow_ <span class="token operator">/</span> <span class="token number">2</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 网络空闲，增大窗口</span></span>
<span class="line">            sendWindow_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">min</span><span class="token punctuation">(</span>sendWindow_ <span class="token operator">*</span> <span class="token number">2</span><span class="token punctuation">,</span> <span class="token number">256u</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发生超时</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onTimeout</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 减小窗口</span></span>
<span class="line">        sendWindow_ <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">max</span><span class="token punctuation">(</span>sendWindow_ <span class="token operator">/</span> <span class="token number">2</span><span class="token punctuation">,</span> <span class="token number">1u</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、综合处理策略" tabindex="-1"><a class="header-anchor" href="#六、综合处理策略"><span>六、综合处理策略</span></a></h2><h3 id="_6-1-分层处理" tabindex="-1"><a class="header-anchor" href="#_6-1-分层处理"><span>6.1 分层处理</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  网络问题分层处理                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  应用层     │  预测、插值、外推、补偿               │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                    │                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  可靠层     │  ACK/NACK、重传、FEC                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                    │                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  传输层     │  TCP/KCP/UDP                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                    │                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  网络层     │  IP 路由、分片                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-自适应策略" tabindex="-1"><a class="header-anchor" href="#_6-2-自适应策略"><span>6.2 自适应策略</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 网络自适应策略</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">NetworkAdaptiveController</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">NetworkCondition</span> <span class="token punctuation">{</span></span>
<span class="line">        EXCELLENT<span class="token punctuation">,</span>  <span class="token comment">// RTT &lt; 50ms, 丢包 &lt; 1%</span></span>
<span class="line">        GOOD<span class="token punctuation">,</span>       <span class="token comment">// RTT &lt; 100ms, 丢包 &lt; 3%</span></span>
<span class="line">        FAIR<span class="token punctuation">,</span>       <span class="token comment">// RTT &lt; 200ms, 丢包 &lt; 10%</span></span>
<span class="line">        POOR<span class="token punctuation">,</span>       <span class="token comment">// RTT &gt; 200ms, 丢包 &gt; 10%</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    NetworkCondition condition_ <span class="token operator">=</span> NetworkCondition<span class="token double-colon punctuation">::</span>GOOD<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新网络状态</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateNetworkStatus</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> rtt<span class="token punctuation">,</span> <span class="token keyword">float</span> lossRate<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>rtt <span class="token operator">&lt;</span> <span class="token number">50</span> <span class="token operator">&amp;&amp;</span> lossRate <span class="token operator">&lt;</span> <span class="token number">0.01f</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            condition_ <span class="token operator">=</span> NetworkCondition<span class="token double-colon punctuation">::</span>EXCELLENT<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>rtt <span class="token operator">&lt;</span> <span class="token number">100</span> <span class="token operator">&amp;&amp;</span> lossRate <span class="token operator">&lt;</span> <span class="token number">0.03f</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            condition_ <span class="token operator">=</span> NetworkCondition<span class="token double-colon punctuation">::</span>GOOD<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>rtt <span class="token operator">&lt;</span> <span class="token number">200</span> <span class="token operator">&amp;&amp;</span> lossRate <span class="token operator">&lt;</span> <span class="token number">0.10f</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            condition_ <span class="token operator">=</span> NetworkCondition<span class="token double-colon punctuation">::</span>FAIR<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            condition_ <span class="token operator">=</span> NetworkCondition<span class="token double-colon punctuation">::</span>POOR<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 根据网络状态调整策略</span></span>
<span class="line">        <span class="token function">adjustStrategy</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">adjustStrategy</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>condition_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> NetworkCondition<span class="token double-colon punctuation">::</span>EXCELLENT<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 高质量网络：降低延迟优先</span></span>
<span class="line">                jitterBufferMs_ <span class="token operator">=</span> <span class="token number">20</span><span class="token punctuation">;</span></span>
<span class="line">                sendRate_ <span class="token operator">=</span> <span class="token number">60</span><span class="token punctuation">;</span>  <span class="token comment">// 60 fps</span></span>
<span class="line">                enableFEC_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> NetworkCondition<span class="token double-colon punctuation">::</span>GOOD<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 良好网络：平衡</span></span>
<span class="line">                jitterBufferMs_ <span class="token operator">=</span> <span class="token number">50</span><span class="token punctuation">;</span></span>
<span class="line">                sendRate_ <span class="token operator">=</span> <span class="token number">30</span><span class="token punctuation">;</span></span>
<span class="line">                enableFEC_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> NetworkCondition<span class="token double-colon punctuation">::</span>FAIR<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 一般网络：增加可靠性</span></span>
<span class="line">                jitterBufferMs_ <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">                sendRate_ <span class="token operator">=</span> <span class="token number">20</span><span class="token punctuation">;</span></span>
<span class="line">                enableFEC_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> NetworkCondition<span class="token double-colon punctuation">::</span>POOR<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 差网络：可靠性优先</span></span>
<span class="line">                jitterBufferMs_ <span class="token operator">=</span> <span class="token number">200</span><span class="token punctuation">;</span></span>
<span class="line">                sendRate_ <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span></span>
<span class="line">                enableFEC_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">int</span> jitterBufferMs_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> sendRate_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> enableFEC_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="处理策略总结" tabindex="-1"><a class="header-anchor" href="#处理策略总结"><span>处理策略总结</span></a></h3><table><thead><tr><th>问题</th><th>处理方法</th><th>适用层</th></tr></thead><tbody><tr><td><strong>抖动</strong></td><td>抖动缓冲</td><td>客户端</td></tr><tr><td><strong>丢包</strong></td><td>重传、FEC</td><td>传输层</td></tr><tr><td><strong>延迟</strong></td><td>预测、插值</td><td>应用层</td></tr><tr><td><strong>乱序</strong></td><td>序列号重排</td><td>传输层</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 客户端处理</span>
<span class="line">   - 抖动缓冲（平滑播放）</span>
<span class="line">   - 位置插值（平滑移动）</span>
<span class="line">   - 速度外推（丢包补偿）</span>
<span class="line"></span>
<span class="line">2. 服务器处理</span>
<span class="line">   - 可靠传输（ACK/重传）</span>
<span class="line">   - 流量控制（窗口机制）</span>
<span class="line">   - 拥塞避免（动态调整）</span>
<span class="line"></span>
<span class="line">3. 协议选择</span>
<span class="line">   - TCP：可靠但延迟高</span>
<span class="line">   - KCP：低延迟可靠</span>
<span class="line">   - UDP：最快但不可靠</span>
<span class="line"></span>
<span class="line">4. 自适应策略</span>
<span class="line">   - 根据网络质量调整</span>
<span class="line">   - 动态选择处理策略</span>
<span class="line">   - 平衡延迟与可靠性</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/tree/master/kbe/src/server/network" target="_blank" rel="noopener noreferrer">KBEngine GitHub - 网络层源码</a></li><li><a href="https://github.com/skywind3000/kcp" target="_blank" rel="noopener noreferrer">KCP 协议丢包处理</a></li><li><a href="https://en.wikipedia.org/wiki/Jitter_buffer" target="_blank" rel="noopener noreferrer">网络抖动处理技术</a></li><li><a href="https://en.wikipedia.org/wiki/Forward_error_correction" target="_blank" rel="noopener noreferrer">FEC 前向纠错原理</a></li></ul>`,30)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};