import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q22-out-of-order.html","title":"Q22: 如何处理网络消息的乱序问题？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q22-out-of-order.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q22-out-of-order.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q22-如何处理网络消息的乱序问题" tabindex="-1"><a class="header-anchor" href="#q22-如何处理网络消息的乱序问题"><span>Q22: 如何处理网络消息的乱序问题？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对网络消息乱序问题的理解：</p><ul><li>消息乱序的原因和影响</li><li>序列号机制</li><li>消息重排序和缓冲</li><li>KBEngine 的消息处理机制</li></ul><hr><h2 id="一、消息乱序问题" tabindex="-1"><a class="header-anchor" href="#一、消息乱序问题"><span>一、消息乱序问题</span></a></h2><h3 id="_1-1-为什么会乱序" tabindex="-1"><a class="header-anchor" href="#_1-1-为什么会乱序"><span>1.1 为什么会乱序</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    消息乱序的原因                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  原因 1: 网络路由不同                                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  消息 1: 客户端 → 路由 A → 服务器 (快)            │       │</span>
<span class="line">│  │  消息 2: 客户端 → 路由 B → 服务器 (慢)            │       │</span>
<span class="line">│  │  结果: 消息 2 先于消息 1 到达！                    │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  原因 2: 数据包分片传输                                      │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  大消息被分成多个包                                │       │</span>
<span class="line">│  │  不同包可能走不同路径                              │       │</span>
<span class="line">│  │  到达顺序可能打乱                                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  原因 3: 重传机制                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  消息 3 丢失                                       │       │</span>
<span class="line">│  │  消息 4 正常到达                                   │       │</span>
<span class="line">│  │  消息 3 重传后到达                                 │       │</span>
<span class="line">│  │  顺序: 4 → 3 (乱序！)                              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  原因 4: 多路径传输                                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  使用多个网络接口                                   │       │</span>
<span class="line">│  │  或多线程发送                                      │       │</span>
<span class="line">│  │  可能导致乱序                                      │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-乱序的影响" tabindex="-1"><a class="header-anchor" href="#_1-2-乱序的影响"><span>1.2 乱序的影响</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    消息乱序的影响                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  影响 1: 状态不一致                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  正确顺序: 移动 A → 移动 B → 攻击                  │       │</span>
<span class="line">│  │  乱序到达: 攻击 → 移动 A → 移动 B                  │       │</span>
<span class="line">│  │  结果: 在错误位置进行攻击！                          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  影响 2: 逻辑错误                                           │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  正确顺序: 装备武器 → 使用技能                     │       │</span>
<span class="line">│  │  乱序到达: 使用技能 → 装备武器                     │       │</span>
<span class="line">│  │  结果: 用错误武器释放技能！                         │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  影响 3: 依赖关系破坏                                        │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  消息 A 依赖消息 B 的结果                          │       │</span>
<span class="line">│  │  但 A 先到达，无法正确处理                          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、序列号机制" tabindex="-1"><a class="header-anchor" href="#二、序列号机制"><span>二、序列号机制</span></a></h2><h3 id="_2-1-序列号设计" tabindex="-1"><a class="header-anchor" href="#_2-1-序列号设计"><span>2.1 序列号设计</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    序列号机制设计                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  序列号格式:                                                │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  struct Message {                                │       │</span>
<span class="line">│  │      uint16_t messageId;      // 消息类型         │       │</span>
<span class="line">│  │      uint32_t sequence;      // 序列号            │       │</span>
<span class="line">│  │      uint8_t  flags;         // 标志位            │       │</span>
<span class="line">│  │      uint8_t  data[];        // 数据              │       │</span>
<span class="line">│  │  };                                                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  序列号分配策略:                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 全局递增                                       │       │</span>
<span class="line">│  │     - 所有消息共享一个序列号空间                    │       │</span>
<span class="line">│  │     - 简单但粒度粗                                 │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  2. 按消息类型递增                                  │       │</span>
<span class="line">│  │     - 每种消息类型独立序列号                        │       │</span>
<span class="line">│  │     - 允许不同类型消息乱序                          │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  3. 按通道递增 (推荐)                              │       │</span>
<span class="line">│  │     - 每个逻辑通道独立序列号                        │       │</span>
<span class="line">│  │     - 不同通道可乱序，同通道必须有序                │       │</span>
<span class="line">│  │     - 例如: 移动通道、战斗通道、聊天通道              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-序列号实现" tabindex="-1"><a class="header-anchor" href="#_2-2-序列号实现"><span>2.2 序列号实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 序列号管理器</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SequenceManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 生成下一个序列号</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">nextSequence</span><span class="token punctuation">(</span>ChannelType channel <span class="token operator">=</span> ChannelType<span class="token double-colon punctuation">::</span>Default<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token operator">++</span>sequenceNumbers_<span class="token punctuation">[</span>channel<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取当前序列号</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">currentSequence</span><span class="token punctuation">(</span>ChannelType channel <span class="token operator">=</span> ChannelType<span class="token double-colon punctuation">::</span>Default<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> sequenceNumbers_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>channel<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> sequenceNumbers_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 验证序列号</span></span>
<span class="line">    SequenceStatus <span class="token function">validateSequence</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> seq<span class="token punctuation">,</span> ChannelType channel<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> expected <span class="token operator">=</span> expectedSequence_<span class="token punctuation">[</span>channel<span class="token punctuation">]</span> <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>seq <span class="token operator">==</span> expected<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 正常顺序</span></span>
<span class="line">            expectedSequence_<span class="token punctuation">[</span>channel<span class="token punctuation">]</span> <span class="token operator">=</span> seq<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> SequenceStatus<span class="token double-colon punctuation">::</span>InOrder<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>seq <span class="token operator">&gt;</span> expected<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 未来消息（有丢失）</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>seq <span class="token operator">-</span> expected <span class="token operator">&lt;=</span> MAX_SEQUENCE_GAP<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 在允许范围内，可能只是乱序</span></span>
<span class="line">                <span class="token keyword">return</span> SequenceStatus<span class="token double-colon punctuation">::</span>Future<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 差距太大，可能是错误</span></span>
<span class="line">                <span class="token keyword">return</span> SequenceStatus<span class="token double-colon punctuation">::</span>Error<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 旧消息（已处理或重复）</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>expected <span class="token operator">-</span> seq <span class="token operator">&lt;=</span> MAX_SEQUENCE_GAP<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 在窗口内，可能是乱序</span></span>
<span class="line">                <span class="token keyword">return</span> SequenceStatus<span class="token double-colon punctuation">::</span>Past<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 太旧，拒绝</span></span>
<span class="line">                <span class="token keyword">return</span> SequenceStatus<span class="token double-colon punctuation">::</span>TooOld<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">SequenceStatus</span> <span class="token punctuation">{</span></span>
<span class="line">        InOrder<span class="token punctuation">,</span>    <span class="token comment">// 正常顺序</span></span>
<span class="line">        Future<span class="token punctuation">,</span>     <span class="token comment">// 未来消息（先缓存）</span></span>
<span class="line">        Past<span class="token punctuation">,</span>       <span class="token comment">// 过去消息（可能从缓存中取）</span></span>
<span class="line">        Error<span class="token punctuation">,</span>      <span class="token comment">// 错误序列号</span></span>
<span class="line">        TooOld      <span class="token comment">// 太旧，丢弃</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">ChannelType</span> <span class="token punctuation">{</span></span>
<span class="line">        Default<span class="token punctuation">,</span></span>
<span class="line">        Movement<span class="token punctuation">,</span></span>
<span class="line">        Combat<span class="token punctuation">,</span></span>
<span class="line">        Chat<span class="token punctuation">,</span></span>
<span class="line">        System</span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>ChannelType<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> sequenceNumbers_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>ChannelType<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> expectedSequence_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> MAX_SEQUENCE_GAP <span class="token operator">=</span> <span class="token number">1000</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 带序列号的消息</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">SequencedMessage</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> messageId<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line">    ChannelType channel<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint8_t</span> flags<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> data<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 序列化</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> <span class="token function">serialize</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> buffer<span class="token punctuation">;</span></span>
<span class="line">        buffer<span class="token punctuation">.</span><span class="token function">reserve</span><span class="token punctuation">(</span><span class="token keyword">sizeof</span><span class="token punctuation">(</span>SequencedMessage<span class="token punctuation">)</span> <span class="token operator">+</span> data<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">appendUint16</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> messageId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">appendUint32</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> sequence<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">appendUint8</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> <span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>channel<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">appendUint8</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> flags<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">appendUint16</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> data<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        buffer<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> data<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> data<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> buffer<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 反序列化</span></span>
<span class="line">    <span class="token keyword">static</span> SequencedMessage <span class="token function">deserialize</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        SequencedMessage msg<span class="token punctuation">;</span></span>
<span class="line">        size_t offset <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        msg<span class="token punctuation">.</span>messageId <span class="token operator">=</span> <span class="token function">readUint16</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>sequence <span class="token operator">=</span> <span class="token function">readUint32</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>channel <span class="token operator">=</span> <span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span>ChannelType<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>data<span class="token punctuation">[</span>offset<span class="token operator">++</span><span class="token punctuation">]</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>flags <span class="token operator">=</span> data<span class="token punctuation">[</span>offset<span class="token operator">++</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">uint16_t</span> dataLen <span class="token operator">=</span> <span class="token function">readUint16</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>data<span class="token punctuation">.</span><span class="token function">assign</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">,</span> data <span class="token operator">+</span> offset <span class="token operator">+</span> dataLen<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> msg<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、消息重排序" tabindex="-1"><a class="header-anchor" href="#三、消息重排序"><span>三、消息重排序</span></a></h2><h3 id="_3-1-排序缓冲区" tabindex="-1"><a class="header-anchor" href="#_3-1-排序缓冲区"><span>3.1 排序缓冲区</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    排序缓冲区机制                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  工作原理:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 收到消息时检查序列号                            │       │</span>
<span class="line">│  │  2. 如果是期望的序列号，立即处理                      │       │</span>
<span class="line">│  │  3. 如果是未来的序列号，放入缓冲区                    │       │</span>
<span class="line">│  │  4. 检查缓冲区是否有可以处理的连续消息                │       │</span>
<span class="line">│  │  5. 定期清理过期的缓冲消息                            │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  示例:                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  期望序列号: 5                                     │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  收到消息 7:                                       │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────┐         │       │</span>
<span class="line">│  │  │ 缓冲区: [7]                           │         │       │</span>
<span class="line">│  │  └─────────────────────────────────────┘         │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  收到消息 5:                                       │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────┐         │       │</span>
<span class="line">│  │  │ 处理 5, 期望变为 6                     │         │       │</span>
<span class="line">│  │  │ 缓冲区: [7]                           │         │       │</span>
<span class="line">│  │  └─────────────────────────────────────┘         │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  收到消息 6:                                       │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────┐         │       │</span>
<span class="line">│  │  │ 处理 6, 期望变为 7                     │         │       │</span>
<span class="line">│  │  │ 检查缓冲区: 7 可用!                   │         │       │</span>
<span class="line">│  │  │ 处理 7, 期望变为 8                     │         │       │</span>
<span class="line">│  │  │ 缓冲区: []                           │         │       │</span>
<span class="line">│  │  └─────────────────────────────────────┘         │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-排序缓冲区实现" tabindex="-1"><a class="header-anchor" href="#_3-2-排序缓冲区实现"><span>3.2 排序缓冲区实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 排序缓冲区实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ReorderBuffer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">BufferedMessage</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line">        SequencedMessage message<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> arrivalTime<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加消息到缓冲区</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">add</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> sequence<span class="token punctuation">,</span> <span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 检查是否已存在</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>buffer_<span class="token punctuation">.</span><span class="token function">count</span><span class="token punctuation">(</span>sequence<span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span> <span class="token comment">// 重复消息</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        BufferedMessage buffered<span class="token punctuation">;</span></span>
<span class="line">        buffered<span class="token punctuation">.</span>sequence <span class="token operator">=</span> sequence<span class="token punctuation">;</span></span>
<span class="line">        buffered<span class="token punctuation">.</span>message <span class="token operator">=</span> msg<span class="token punctuation">;</span></span>
<span class="line">        buffered<span class="token punctuation">.</span>arrivalTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        buffer_<span class="token punctuation">[</span>sequence<span class="token punctuation">]</span> <span class="token operator">=</span> buffered<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 限制缓冲区大小</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>buffer_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> MAX_BUFFER_SIZE<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 删除最旧的消息</span></span>
<span class="line">            <span class="token keyword">auto</span> oldest <span class="token operator">=</span> buffer_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            buffer_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>oldest<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否可以处理</span></span>
<span class="line">        <span class="token function">tryProcess</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理消息（返回是否处理）</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">process</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> sequence<span class="token punctuation">,</span> <span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">,</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span><span class="token punctuation">)</span><span class="token operator">&gt;</span> handler<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>sequence <span class="token operator">==</span> expectedSequence_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 正好是期望的序列号</span></span>
<span class="line">            <span class="token function">handler</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            expectedSequence_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 检查缓冲区是否有可处理的</span></span>
<span class="line">            <span class="token function">tryProcess</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>sequence <span class="token operator">&gt;</span> expectedSequence_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 未来的消息，放入缓冲区</span></span>
<span class="line">            <span class="token function">add</span><span class="token punctuation">(</span>sequence<span class="token punctuation">,</span> msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 旧消息，检查是否在缓冲区窗口内</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>expectedSequence_ <span class="token operator">-</span> sequence <span class="token operator">&lt;=</span> MAX_SEQUENCE_GAP<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 可能是从缓冲区来的</span></span>
<span class="line">                <span class="token keyword">auto</span> it <span class="token operator">=</span> buffer_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>sequence<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> buffer_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">handler</span><span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    buffer_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    expectedSequence_ <span class="token operator">=</span> sequence <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token function">tryProcess</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token comment">// 太旧，丢弃</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 尝试处理缓冲区中的消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">tryProcess</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>buffer_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span> it <span class="token operator">=</span> buffer_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>expectedSequence_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> buffer_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 处理该消息</span></span>
<span class="line">                <span class="token function">handleMessage</span><span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                buffer_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                expectedSequence_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 清理过期消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">cleanup</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> now <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> expireTime <span class="token operator">=</span> now <span class="token operator">-</span> BUFFER_TIMEOUT<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> buffer_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> buffer_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>arrivalTime <span class="token operator">&lt;</span> expireTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 超时，记录丢包</span></span>
<span class="line">                <span class="token function">onPacketTimeout</span><span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>first<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                it <span class="token operator">=</span> buffer_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token operator">++</span>it<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 强制设置期望序列号（跳过丢失的消息）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">skipTo</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 清理缓冲区中 &lt;= sequence 的消息</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> buffer_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> buffer_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>first <span class="token operator">&lt;=</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                it <span class="token operator">=</span> buffer_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token operator">++</span>it<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        expectedSequence_ <span class="token operator">=</span> sequence <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>messageHandler_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">messageHandler_</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onPacketTimeout</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>timeoutHandler_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">timeoutHandler_</span><span class="token punctuation">(</span>sequence<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> BufferedMessage<span class="token operator">&gt;</span> buffer_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> expectedSequence_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span><span class="token punctuation">)</span><span class="token operator">&gt;</span> messageHandler_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span><span class="token punctuation">)</span><span class="token operator">&gt;</span> timeoutHandler_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t MAX_BUFFER_SIZE <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> MAX_SEQUENCE_GAP <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> BUFFER_TIMEOUT <span class="token operator">=</span> <span class="token number">1000</span><span class="token punctuation">;</span> <span class="token comment">// 1秒</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 消息处理器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">OrderedMessageHandler</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">OrderedMessageHandler</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 设置消息处理回调</span></span>
<span class="line">        reorderBuffer_<span class="token punctuation">.</span><span class="token function">setMessageHandler</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">handleMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 设置超时回调</span></span>
<span class="line">        reorderBuffer_<span class="token punctuation">.</span><span class="token function">setTimeoutHandler</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">onSequenceTimeout</span><span class="token punctuation">(</span>sequence<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">receiveMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        SequencedMessage msg <span class="token operator">=</span> <span class="token class-name">SequencedMessage</span><span class="token double-colon punctuation">::</span><span class="token function">deserialize</span><span class="token punctuation">(</span>data<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 处理消息（可能进入缓冲区）</span></span>
<span class="line">        reorderBuffer_<span class="token punctuation">.</span><span class="token function">process</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>sequence<span class="token punctuation">,</span> msg<span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> m<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">handleMessage</span><span class="token punctuation">(</span>m<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 定期清理</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        reorderBuffer_<span class="token punctuation">.</span><span class="token function">cleanup</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 根据消息类型分发</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>msg<span class="token punctuation">.</span>messageId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> MSG_MOVE<span class="token operator">:</span></span>
<span class="line">                <span class="token function">handleMoveMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> MSG_ATTACK<span class="token operator">:</span></span>
<span class="line">                <span class="token function">handleAttackMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> MSG_CHAT<span class="token operator">:</span></span>
<span class="line">                <span class="token function">handleChatMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">default</span><span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleMoveMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 处理移动消息</span></span>
<span class="line">        MoveData move <span class="token operator">=</span> <span class="token function">parseMoveData</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">updateEntityPosition</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>sequence<span class="token punctuation">,</span> move<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleAttackMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 处理攻击消息</span></span>
<span class="line">        AttackData attack <span class="token operator">=</span> <span class="token function">parseAttackData</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">processAttack</span><span class="token punctuation">(</span>attack<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleChatMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 聊天消息不严格要求顺序</span></span>
<span class="line">        ChatData chat <span class="token operator">=</span> <span class="token function">parseChatData</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">displayChat</span><span class="token punctuation">(</span>chat<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onSequenceTimeout</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Message sequence timeout: &quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>sequence<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 请求重传</span></span>
<span class="line">        <span class="token function">requestRetransmit</span><span class="token punctuation">(</span>sequence<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">requestRetransmit</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 发送重传请求</span></span>
<span class="line">        RetransmitRequest req<span class="token punctuation">;</span></span>
<span class="line">        req<span class="token punctuation">.</span>missingSequence <span class="token operator">=</span> sequence<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">sendToServer</span><span class="token punctuation">(</span>req<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    ReorderBuffer reorderBuffer_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-消息处理" tabindex="-1"><a class="header-anchor" href="#四、kbengine-消息处理"><span>四、KBEngine 消息处理</span></a></h2><h3 id="_4-1-kbengine-bundle-机制" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-bundle-机制"><span>4.1 KBEngine Bundle 机制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Bundle 消息处理</span></span>
<span class="line"><span class="token comment">// src/lib/network/bundle.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Bundle</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">MemoryStream</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 消息序列化时自动添加消息 ID</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">newMessage</span><span class="token punctuation">(</span>MessageID msgID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 写入消息 ID</span></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span><span class="token keyword">this</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> msgID<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 记录消息位置（用于重传）</span></span>
<span class="line">        messageStartPos_ <span class="token operator">=</span> <span class="token function">wpos</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取消息数据</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> std<span class="token double-colon punctuation">::</span><span class="token function">string</span><span class="token punctuation">(</span><span class="token function">str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    size_t messageStartPos_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// KBEngine 通道消息处理</span></span>
<span class="line"><span class="token comment">// src/lib/network/channel.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 发送消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">send</span><span class="token punctuation">(</span>Bundle<span class="token operator">*</span> pBundle<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 添加到发送队列</span></span>
<span class="line">        sendQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 尝试发送</span></span>
<span class="line">        <span class="token function">processSend</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理接收到的消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processRecv</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>stream<span class="token punctuation">.</span><span class="token function">remaining</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 读取消息 ID</span></span>
<span class="line">            MessageID msgID<span class="token punctuation">;</span></span>
<span class="line">            stream <span class="token operator">&gt;&gt;</span> msgID<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 处理消息</span></span>
<span class="line">            <span class="token function">handleMessage</span><span class="token punctuation">(</span>msgID<span class="token punctuation">,</span> stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleMessage</span><span class="token punctuation">(</span>MessageID msgID<span class="token punctuation">,</span> MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 查找消息处理器</span></span>
<span class="line">        <span class="token keyword">auto</span> handler <span class="token operator">=</span> <span class="token class-name">MessageHandlers</span><span class="token double-colon punctuation">::</span><span class="token function">find</span><span class="token punctuation">(</span>msgID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>handler<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            handler<span class="token operator">-&gt;</span><span class="token function">process</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Unknown message ID: &quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>msgID<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>Bundle<span class="token operator">*</span><span class="token operator">&gt;</span> sendQueue_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-kbengine-可靠消息" tabindex="-1"><a class="header-anchor" href="#_4-2-kbengine-可靠消息"><span>4.2 KBEngine 可靠消息</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 可靠消息机制</span></span>
<span class="line"><span class="token comment">// src/lib/network/channel.hpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 请求确认</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">requestAck</span><span class="token punctuation">(</span>Bundle<span class="token operator">*</span> pBundle<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 设置需要确认标志</span></span>
<span class="line">        pBundle<span class="token operator">-&gt;</span><span class="token function">setNeedAck</span><span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 分配序列号</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> seq <span class="token operator">=</span> nextSequence_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">        pBundle<span class="token operator">-&gt;</span><span class="token function">setSequence</span><span class="token punctuation">(</span>seq<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 保存到未确认队列</span></span>
<span class="line">        unackedBundles_<span class="token punctuation">[</span>seq<span class="token punctuation">]</span> <span class="token operator">=</span> pBundle<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送</span></span>
<span class="line">        <span class="token function">send</span><span class="token punctuation">(</span>pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理确认</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onAck</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> unackedBundles_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>sequence<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> unackedBundles_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 已确认，删除</span></span>
<span class="line">            <span class="token keyword">delete</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">            unackedBundles_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 更新滑动窗口</span></span>
<span class="line">            <span class="token function">updateSlidingWindow</span><span class="token punctuation">(</span>sequence<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 超时重传</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">checkRetransmit</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> now <span class="token operator">=</span> <span class="token function">timeStamp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>seq<span class="token punctuation">,</span> bundle<span class="token punctuation">]</span> <span class="token operator">:</span> unackedBundles_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>now <span class="token operator">-</span> bundle<span class="token operator">-&gt;</span><span class="token function">sendTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> RETRANSMIT_TIMEOUT<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 重传</span></span>
<span class="line">                <span class="token function">LOG_DEBUG</span><span class="token punctuation">(</span><span class="token string">&quot;Retransmitting bundle, seq=&quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>seq<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">send</span><span class="token punctuation">(</span>bundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment">// 更新发送时间</span></span>
<span class="line">                bundle<span class="token operator">-&gt;</span><span class="token function">updateSendTime</span><span class="token punctuation">(</span>now<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment">// 增加重传计数</span></span>
<span class="line">                bundle<span class="token operator">-&gt;</span><span class="token function">incrementRetransmitCount</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment">// 检查是否超过最大重传次数</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>bundle<span class="token operator">-&gt;</span><span class="token function">retransmitCount</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> MAX_RETRANSMITS<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">onRetransmitTimeout</span><span class="token punctuation">(</span>seq<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateSlidingWindow</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 移动窗口，删除已确认的消息</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> unackedBundles_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> unackedBundles_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> it<span class="token operator">-&gt;</span>first <span class="token operator">&lt;=</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">delete</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">            it <span class="token operator">=</span> unackedBundles_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onRetransmitTimeout</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> sequence<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">LOG_ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Bundle retransmit timeout, seq=&quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>sequence<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 连接可能有问题，考虑断开</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>unackedBundles_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> MAX_UNACKED_COUNT<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">onClose</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span><span class="token keyword">uint16_t</span><span class="token punctuation">,</span> Bundle<span class="token operator">*</span><span class="token operator">&gt;</span> unackedBundles_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> nextSequence_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> RETRANSMIT_TIMEOUT <span class="token operator">=</span> <span class="token number">1000</span><span class="token punctuation">;</span> <span class="token comment">// 1秒</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_RETRANSMITS <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t MAX_UNACKED_COUNT <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、高级处理策略" tabindex="-1"><a class="header-anchor" href="#五、高级处理策略"><span>五、高级处理策略</span></a></h2><h3 id="_5-1-分通道处理" tabindex="-1"><a class="header-anchor" href="#_5-1-分通道处理"><span>5.1 分通道处理</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    分通道乱序处理                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  设计思想: 不同类型的消息可以乱序                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  通道划分:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  通道              │  消息类型     │  是否需要严格顺序 │       │</span>
<span class="line">│  │  ├─────────────────────────────────────────────┤       │</span>
<span class="line">│  │  │ Movement      │  移动/转向     │  是          │       │</span>
<span class="line">│  │  │ Combat        │  攻击/技能     │  是          │       │</span>
<span class="line">│  │  │ Chat          │  聊天消息     │  否          │       │</span>
<span class="line">│  │  │ System        │  系统通知     │  否          │       │</span>
<span class="line">│  │  │ Reliable      │  重要消息     │  是          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优势:                                                     │</span>
<span class="line">│  ├── 降低缓冲区需求                                         │</span>
<span class="line">│  ├── 减少等待延迟                                           │</span>
<span class="line">│  └── 提高处理效率                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  实现:                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  每个通道独立维护序列号和缓冲区                      │       │</span>
<span class="line">│  │  不同通道的消息可以乱序到达和处理                   │       │</span>
<span class="line">│  │  同一通道内保持顺序                                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-分通道实现" tabindex="-1"><a class="header-anchor" href="#_5-2-分通道实现"><span>5.2 分通道实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 分通道消息处理</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ChannelMessageHandler</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token punctuation">{</span></span>
<span class="line">        Movement<span class="token punctuation">,</span>   <span class="token comment">// 移动消息，严格顺序</span></span>
<span class="line">        Combat<span class="token punctuation">,</span>     <span class="token comment">// 战斗消息，严格顺序</span></span>
<span class="line">        Chat<span class="token punctuation">,</span>       <span class="token comment">// 聊天消息，可乱序</span></span>
<span class="line">        System<span class="token punctuation">,</span>     <span class="token comment">// 系统消息，可乱序</span></span>
<span class="line">        Reliable    <span class="token comment">// 可靠消息，严格顺序</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">send</span><span class="token punctuation">(</span>Channel channel<span class="token punctuation">,</span> <span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 为消息分配通道序列号</span></span>
<span class="line">        msg<span class="token punctuation">.</span>sequence <span class="token operator">=</span> sequenceManagers_<span class="token punctuation">[</span>channel<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">nextSequence</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>channel <span class="token operator">=</span> channel<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送</span></span>
<span class="line">        network_<span class="token punctuation">.</span><span class="token function">send</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span><span class="token function">serialize</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">receive</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Channel channel <span class="token operator">=</span> msg<span class="token punctuation">.</span>channel<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否需要严格顺序</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">requiresOrdering</span><span class="token punctuation">(</span>channel<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 需要排序，放入缓冲区</span></span>
<span class="line">            reorderBuffers_<span class="token punctuation">[</span>channel<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">process</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>sequence<span class="token punctuation">,</span> msg<span class="token punctuation">,</span></span>
<span class="line">                <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">,</span> channel<span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> m<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">processMessage</span><span class="token punctuation">(</span>channel<span class="token punctuation">,</span> m<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 不需要排序，直接处理</span></span>
<span class="line">            <span class="token function">processMessage</span><span class="token punctuation">(</span>channel<span class="token punctuation">,</span> msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">requiresOrdering</span><span class="token punctuation">(</span>Channel channel<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> channel <span class="token operator">==</span> Channel<span class="token double-colon punctuation">::</span>Movement <span class="token operator">||</span></span>
<span class="line">               channel <span class="token operator">==</span> Channel<span class="token double-colon punctuation">::</span>Combat <span class="token operator">||</span></span>
<span class="line">               channel <span class="token operator">==</span> Channel<span class="token double-colon punctuation">::</span>Reliable<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processMessage</span><span class="token punctuation">(</span>Channel channel<span class="token punctuation">,</span> <span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>channel<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> Channel<span class="token double-colon punctuation">::</span>Movement<span class="token operator">:</span></span>
<span class="line">                <span class="token function">processMovement</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> Channel<span class="token double-colon punctuation">::</span>Combat<span class="token operator">:</span></span>
<span class="line">                <span class="token function">processCombat</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> Channel<span class="token double-colon punctuation">::</span>Chat<span class="token operator">:</span></span>
<span class="line">                <span class="token function">processChat</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> Channel<span class="token double-colon punctuation">::</span>System<span class="token operator">:</span></span>
<span class="line">                <span class="token function">processSystem</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> Channel<span class="token double-colon punctuation">::</span>Reliable<span class="token operator">:</span></span>
<span class="line">                <span class="token function">processReliable</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processMovement</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 严格按顺序处理移动</span></span>
<span class="line">        MoveData move <span class="token operator">=</span> <span class="token function">parseMoveData</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">updatePosition</span><span class="token punctuation">(</span>move<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processChat</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 聊天不需要严格顺序</span></span>
<span class="line">        ChatData chat <span class="token operator">=</span> <span class="token function">parseChatData</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">displayChat</span><span class="token punctuation">(</span>chat<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>Channel<span class="token punctuation">,</span> SequenceManager<span class="token operator">&gt;</span> sequenceManagers_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>Channel<span class="token punctuation">,</span> ReorderBuffer<span class="token operator">&gt;</span> reorderBuffers_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-3-时间戳排序" tabindex="-1"><a class="header-anchor" href="#_5-3-时间戳排序"><span>5.3 时间戳排序</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 基于时间戳的排序（用于不可靠消息）</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TimestampMessageHandler</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">receive</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 计算消息的期望显示时间</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> displayTime <span class="token operator">=</span> <span class="token function">calculateDisplayTime</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 加入优先级队列</span></span>
<span class="line">        messageQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span><span class="token punctuation">{</span>msg<span class="token punctuation">,</span> displayTime<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> currentTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 处理到期的消息</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>messageQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> item <span class="token operator">=</span> messageQueue_<span class="token punctuation">.</span><span class="token function">top</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>item<span class="token punctuation">.</span>displayTime <span class="token operator">&lt;=</span> currentTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 处理消息</span></span>
<span class="line">                <span class="token function">processMessage</span><span class="token punctuation">(</span>item<span class="token punctuation">.</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                messageQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 还没到时间</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">QueueItem</span> <span class="token punctuation">{</span></span>
<span class="line">        SequencedMessage msg<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> displayTime<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">bool</span> <span class="token keyword">operator</span><span class="token operator">&gt;</span><span class="token punctuation">(</span><span class="token keyword">const</span> QueueItem<span class="token operator">&amp;</span> other<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> displayTime <span class="token operator">&gt;</span> other<span class="token punctuation">.</span>displayTime<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">calculateDisplayTime</span><span class="token punctuation">(</span><span class="token keyword">const</span> SequencedMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 基于服务器时间戳 + 延迟补偿</span></span>
<span class="line">        <span class="token keyword">return</span> msg<span class="token punctuation">.</span>timestamp <span class="token operator">+</span> latencyCompensation_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>priority_queue<span class="token operator">&lt;</span>QueueItem<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>QueueItem<span class="token operator">&gt;</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>greater<span class="token operator">&lt;</span><span class="token operator">&gt;&gt;</span> messageQueue_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> latencyCompensation_ <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span> <span class="token comment">// 100ms 补偿</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-处理策略选择" tabindex="-1"><a class="header-anchor" href="#_6-1-处理策略选择"><span>6.1 处理策略选择</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              消息乱序处理策略选择                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  消息类型           │  处理策略          │  理由              │</span>
<span class="line">│  ├───────────────────────────────────────────────────────┤ │</span>
<span class="line">│  │ 移动/位置更新     │ 严格排序 + 插值    │ 位置连续性重要  │ │</span>
<span class="line">│  │ 战斗操作         │ 严格排序           │ 逻辑依赖强      │ │</span>
<span class="line">│  │ 聊天消息         │ 可乱序             │ 无依赖          │ │</span>
<span class="line">│  │ 系统通知         │ 可乱序 + 时间戳排序 │ 显示顺序可调整  │ │</span>
<span class="line">│  │ 状态同步         │ 可乱序 + 最终一致   │ 取最新状态      │ │</span>
<span class="line">│  │ 动画触发         │ 可乱序             │ 独立播放        │ │</span>
<span class="line">│  │ 交易操作         │ 严格排序 + 幂等    │ 防止重复        │ │</span>
<span class="line">│  └───────────────────────────────────────────────────────┘ │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-性能考虑" tabindex="-1"><a class="header-anchor" href="#_6-2-性能考虑"><span>6.2 性能考虑</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              乱序处理性能优化                                 │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 缓冲区大小控制                                          │</span>
<span class="line">│     ├── 设置合理的缓冲区大小上限                            │</span>
<span class="line">│     ├── 超过上限时丢弃最旧的消息                             │</span>
<span class="line">│     └── 使用环形缓冲区提高效率                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 超时处理                                                │</span>
<span class="line">│     ├── 设置缓冲超时时间                                    │</span>
<span class="line">│     ├── 超时后请求重传或跳过                                 │</span>
<span class="line">│     └── 避免无限等待                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 分通道处理                                              │</span>
<span class="line">│     ├── 不同类型消息独立通道                                 │</span>
<span class="line">│     ├── 减少跨通道依赖                                       │</span>
<span class="line">│     └── 提高并行处理能力                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 智能跳过                                                │</span>
<span class="line">│     ├── 检测到大量丢包时跳过                                 │</span>
<span class="line">│     ├── 请求完整状态同步                                     │</span>
<span class="line">│     └── 避免缓冲区溢出                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="消息乱序解决方案对比" tabindex="-1"><a class="header-anchor" href="#消息乱序解决方案对比"><span>消息乱序解决方案对比</span></a></h3><table><thead><tr><th>方案</th><th>复杂度</th><th>延迟</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>忽略乱序</strong></td><td>低</td><td>低</td><td>无依赖消息</td></tr><tr><td><strong>简单排序</strong></td><td>中</td><td>中</td><td>小流量</td></tr><tr><td><strong>分通道排序</strong></td><td>中</td><td>低</td><td>混合场景</td></tr><tr><td><strong>智能缓冲</strong></td><td>高</td><td>中</td><td>高可靠要求</td></tr><tr><td><strong>时间戳排序</strong></td><td>中</td><td>可调</td><td>显示类消息</td></tr></tbody></table><h3 id="kbengine-消息处理机制" tabindex="-1"><a class="header-anchor" href="#kbengine-消息处理机制"><span>KBEngine 消息处理机制</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 特点:</span>
<span class="line">1. 使用 Bundle 消息封装</span>
<span class="line">2. 支持可靠消息（带序列号）</span>
<span class="line">3. 内置重传机制</span>
<span class="line">4. 按消息类型分发处理</span>
<span class="line"></span>
<span class="line">开发者建议:</span>
<span class="line">1. 重要操作使用可靠消息</span>
<span class="line">2. 实时消息做好缓冲和插值</span>
<span class="line">3. 合理划分消息通道</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/lib/network/bundle.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - Bundle 实现</a></li><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/lib/network/channel.cpp" target="_blank" rel="noopener noreferrer">KBEngine GitHub - Channel 通信</a></li><li><a href="https://en.wikipedia.org/wiki/Transmission_Control_Protocol" target="_blank" rel="noopener noreferrer">TCP 序列号与乱序处理</a></li></ul>`,51)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};