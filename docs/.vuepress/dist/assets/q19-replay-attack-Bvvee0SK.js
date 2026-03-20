import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q19-replay-attack.html","title":"Q19: 如何防止消息重放攻击？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q19-replay-attack.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q19-replay-attack.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q19-如何防止消息重放攻击" tabindex="-1"><a class="header-anchor" href="#q19-如何防止消息重放攻击"><span>Q19: 如何防止消息重放攻击？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对网络安全和防作弊的理解：</p><ul><li>重放攻击的原理和危害</li><li>消息认证和防重放机制</li><li>序列号、时间戳、Nonce 等技术</li><li>KBEngine 的安全措施</li></ul><hr><h2 id="一、重放攻击原理" tabindex="-1"><a class="header-anchor" href="#一、重放攻击原理"><span>一、重放攻击原理</span></a></h2><h3 id="_1-1-什么是重放攻击" tabindex="-1"><a class="header-anchor" href="#_1-1-什么是重放攻击"><span>1.1 什么是重放攻击</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    重放攻击原理                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  正常流程:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  客户端 ──请求──► 服务器 ──处理──► 返回结果       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  重放攻击流程:                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  攻击者捕获请求                                   │       │</span>
<span class="line">│  │       │                                          │       │</span>
<span class="line">│  │       ▼                                          │       │</span>
<span class="line">│  │  ┌─────────────┐                                │       │</span>
<span class="line">│  │  │  捕获的请求  │ &quot;login:user=abc,pass=123&quot;      │       │</span>
<span class="line">│  │  └─────────────┘                                │       │</span>
<span class="line">│  │       │                                          │       │</span>
<span class="line">│  │       │ 重复发送                                  │       │</span>
<span class="line">│  │       ▼                                          │       │</span>
<span class="line">│  │  服务器 ──再次处理──► 可能导致危害                 │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  危害:                                                     │</span>
<span class="line">│  ├── 重复消费 (刷金币、刷道具)                              │</span>
<span class="line">│  ├── 绕过验证 (重放登录包)                                  │</span>
<span class="line">│  ├── 恶意刷数据 (刷排行榜、刷评论)                          │</span>
<span class="line">│  └── 破坏业务逻辑 (重复操作)                                │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-常见重放攻击场景" tabindex="-1"><a class="header-anchor" href="#_1-2-常见重放攻击场景"><span>1.2 常见重放攻击场景</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  重放攻击常见场景                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 1: 重复消费                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 玩家购买道具，发送购买请求                     │       │</span>
<span class="line">│  │  2. 服务器扣款，发放道具                           │       │</span>
<span class="line">│  │  3. 攻击者捕获购买请求                             │       │</span>
<span class="line">│  │  4. 重复发送购买请求                               │       │</span>
<span class="line">│  │  5. 服务器再次扣款，再次发放道具 → 刷道具！         │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 2: 登录重放                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 玩家 A 登录成功                               │       │</span>
<span class="line">│  │  2. 攻击者捕获登录包                              │       │</span>
<span class="line">│  │  3. 玩家 A 下线                                   │       │</span>
<span class="line">│  │  4. 攻击者重放登录包                               │       │</span>
<span class="line">│  │  5. 伪装成玩家 A 登录 → 盗号！                      │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 3: 战斗操作重放                                        │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 玩家发送攻击请求                               │       │</span>
<span class="line">│  │  2. 服务器处理，造成伤害                           │       │</span>
<span class="line">│  │  3. 攻击者重放攻击请求                             │       │</span>
<span class="line">│  │  4. 服务器重复处理，重复伤害 → 秒怪！              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 4: 交易重放                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 玩家 A 向 B 转账                               │       │</span>
<span class="line">│  │  2. 攻击者捕获转账请求                             │       │</span>
<span class="line">│  │  3. 重复发送转账请求                               │       │</span>
<span class="line">│  │  4. 重复转账 → 刷金币！                            │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、防重放机制" tabindex="-1"><a class="header-anchor" href="#二、防重放机制"><span>二、防重放机制</span></a></h2><h3 id="_2-1-消息序列号" tabindex="-1"><a class="header-anchor" href="#_2-1-消息序列号"><span>2.1 消息序列号</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    消息序列号机制                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  原理:                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  每个会话维护一个递增的序列号                       │       │</span>
<span class="line">│  │  每条消息携带序列号                                 │       │</span>
<span class="line">│  │  服务器只接受更大序列号的消息                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  流程:                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  客户端                         服务器           │       │</span>
<span class="line">│  │  ┌─────┐                     ┌─────┐            │       │</span>
<span class="line">│  │  │Seq=1│ ─────────────────────→│接受 │            │       │</span>
<span class="line">│  │  └─────┘                     └─────┘            │       │</span>
<span class="line">│  │  ┌─────┐                     ┌─────┐            │       │</span>
<span class="line">│  │  │Seq=2│ ─────────────────────→│接受 │            │       │</span>
<span class="line">│  │  └─────┘                     └─────┘            │       │</span>
<span class="line">│  │  ┌─────┐                     ┌─────┐            │       │</span>
<span class="line">│  │  │Seq=2│ ─(重放)─────────────→│拒绝！│            │       │</span>
<span class="line">│  │  └─────┘   (序列号不递增)       └─────┘            │       │</span>
<span class="line">│  │  ┌─────┐                     ┌─────┐            │       │</span>
<span class="line">│  │  │Seq=3│ ─────────────────────→│接受 │            │       │</span>
<span class="line">│  │  └─────┘                     └─────┘            │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优势: 简单、高效                                           │</span>
<span class="line">│  劣势: 需要维护状态、掉包导致空洞                            │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-序列号实现" tabindex="-1"><a class="header-anchor" href="#_2-2-序列号实现"><span>2.2 序列号实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 消息序列号防重放</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SequenceNumberManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 生成下一个序列号</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">nextSequence</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token operator">++</span>currentSequence_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 验证序列号</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">validateSequence</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> seq<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 首次连接</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>lastSequence_ <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            lastSequence_ <span class="token operator">=</span> seq<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否递增</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>seq <span class="token operator">&gt;</span> lastSequence_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 检查跳跃是否过大（可能的重放攻击）</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>seq <span class="token operator">-</span> lastSequence_ <span class="token operator">&gt;</span> MAX_SEQUENCE_GAP<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            lastSequence_ <span class="token operator">=</span> seq<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 序列号回退，可能是重放</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理乱序（允许一定窗口内的乱序）</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">validateSequenceWithWindow</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> seq<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 在窗口内的直接接受</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>seq <span class="token operator">&gt;</span> lastSequence_ <span class="token operator">&amp;&amp;</span></span>
<span class="line">            seq <span class="token operator">-</span> lastSequence__ <span class="token operator">&lt;=</span> SEQUENCE_WINDOW<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            lastSequence_ <span class="token operator">=</span> seq<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否在乱序窗口内</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>outOfOrderBuffer_<span class="token punctuation">.</span><span class="token function">contains</span><span class="token punctuation">(</span>seq<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span> <span class="token comment">// 已经处理过</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>seq <span class="token operator">&lt;</span> lastSequence_ <span class="token operator">&amp;&amp;</span></span>
<span class="line">            lastSequence_ <span class="token operator">-</span> seq <span class="token operator">&lt;=</span> SEQUENCE_WINDOW<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 在乱序窗口内，缓存</span></span>
<span class="line">            outOfOrderBuffer_<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>seq<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> currentSequence_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> lastSequence_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_set<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> outOfOrderBuffer_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> MAX_SEQUENCE_GAP <span class="token operator">=</span> <span class="token number">1000</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> SEQUENCE_WINDOW <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 消息封装</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">SequencedMessage</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> messageId<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> data<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span>     <span class="token comment">// 额外：时间戳</span></span>
<span class="line">    <span class="token keyword">uint8_t</span> signature<span class="token punctuation">[</span><span class="token number">32</span><span class="token punctuation">]</span><span class="token punctuation">;</span>  <span class="token comment">// 额外：签名</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 序列化</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> <span class="token function">serialize</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> buffer<span class="token punctuation">;</span></span>
<span class="line">        buffer<span class="token punctuation">.</span><span class="token function">reserve</span><span class="token punctuation">(</span><span class="token keyword">sizeof</span><span class="token punctuation">(</span>SequencedMessage<span class="token punctuation">)</span> <span class="token operator">+</span> data<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">appendUint32</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> sequence<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">appendUint16</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> messageId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">appendUint32</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> timestamp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        buffer<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> signature<span class="token punctuation">,</span> signature <span class="token operator">+</span> <span class="token number">32</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
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
<span class="line">        msg<span class="token punctuation">.</span>sequence <span class="token operator">=</span> <span class="token function">readUint32</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>messageId <span class="token operator">=</span> <span class="token function">readUint16</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> <span class="token function">readUint32</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">memcpy</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>signature<span class="token punctuation">,</span> data <span class="token operator">+</span> offset<span class="token punctuation">,</span> <span class="token number">32</span><span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">32</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">uint16_t</span> dataLen <span class="token operator">=</span> <span class="token function">readUint16</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>data<span class="token punctuation">.</span><span class="token function">assign</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">,</span> data <span class="token operator">+</span> offset <span class="token operator">+</span> dataLen<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> msg<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-时间戳机制" tabindex="-1"><a class="header-anchor" href="#_2-3-时间戳机制"><span>2.3 时间戳机制</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    时间戳防重放                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  原理:                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  每条消息携带发送时间戳                             │       │</span>
<span class="line">│  │  服务器检查时间戳是否在合理窗口内                   │       │</span>
<span class="line">│  │  过期消息被拒绝                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  窗口计算:                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  服务器时间 = 2024-01-01 10:00:00                 │       │</span>
<span class="line">│  │  时间窗口 = ±5 秒                                   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  接受范围: 09:59:55 ~ 10:00:05                    │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  消息时间戳 = 09:55:00 → 拒绝！(太旧)              │       │</span>
<span class="line">│  │  消息时间戳 = 09:59:58 → 接受                      │       │</span>
<span class="line">│  │  消息时间戳 = 10:00:00 → 接受                      │       │</span>
<span class="line">│  │  消息时间戳 = 10:00:03 → 接受                      │       │</span>
<span class="line">│  │  消息时间戳 = 10:00:10 → 拒绝！(太新，时钟偏差大)    │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优势: 无状态、实现简单                                     │</span>
<span class="line">│  劣势: 依赖时钟同步、窗口大小难以平衡                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 时间戳防重放实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TimestampValidator</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 验证时间戳</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">validateTimestamp</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> messageTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> currentTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int32_t</span> timeDiff <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">int32_t</span><span class="token punctuation">)</span><span class="token punctuation">(</span>currentTime <span class="token operator">-</span> messageTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查时间差是否在允许窗口内</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">abs</span><span class="token punctuation">(</span>timeDiff<span class="token punctuation">)</span> <span class="token operator">&gt;</span> TIME_WINDOW_SECONDS<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 带时钟偏差的时间戳验证</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">validateTimestampWithSkew</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> messageTime<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> clientClockSkew<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> currentTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> adjustedTime <span class="token operator">=</span> messageTime <span class="token operator">+</span> clientClockSkew<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int32_t</span> timeDiff <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">int32_t</span><span class="token punctuation">)</span><span class="token punctuation">(</span>currentTime <span class="token operator">-</span> adjustedTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">abs</span><span class="token punctuation">(</span>timeDiff<span class="token punctuation">)</span> <span class="token operator">&lt;=</span> TIME_WINDOW_SECONDS<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 计算时钟偏差（基于 NTP 或多次消息）</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">calculateClockSkew</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> clientTime<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> serverTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> serverTime <span class="token operator">-</span> clientTime<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> std<span class="token double-colon punctuation">::</span><span class="token function">time</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int32_t</span> TIME_WINDOW_SECONDS <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-4-nonce-机制" tabindex="-1"><a class="header-anchor" href="#_2-4-nonce-机制"><span>2.4 Nonce 机制</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    Nonce 防重放                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  原理:                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  Nonce = Number Used Once                        │       │</span>
<span class="line">│  │  每条消息使用一个唯一的随机数                       │       │</span>
<span class="line">│  │  服务器记录已使用的 Nonce                          │       │</span>
<span class="line">│  │  重复的 Nonce 被拒绝                               │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  流程:                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 客户端生成随机 Nonce                           │       │</span>
<span class="line">│  │  2. 发送消息 {data, nonce, signature}             │       │</span>
<span class="line">│  │  3. 服务器检查 Nonce 是否已使用                    │       │</span>
<span class="line">│  │  4. 如果未使用，记录并处理                          │       │</span>
<span class="line">│  │  5. 如果已使用，拒绝                                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Nonce 管理:                                               │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 使用 Bloom Filter 快速检测                     │       │</span>
<span class="line">│  │  - 定期清理过期 Nonce                              │       │</span>
<span class="line">│  │  - 分布式环境使用 Redis                            │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优势: 完全防重放                                           │</span>
<span class="line">│  劣势: 需要存储、有一定开销                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Nonce 防重放实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">NonceManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 生成 Nonce</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">generateNonce</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint8_t</span> nonceBytes<span class="token punctuation">[</span><span class="token number">16</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">RAND_bytes</span><span class="token punctuation">(</span>nonceBytes<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>nonceBytes<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">char</span> nonceStr<span class="token punctuation">[</span><span class="token number">33</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> <span class="token number">16</span><span class="token punctuation">;</span> i<span class="token operator">++</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">sprintf</span><span class="token punctuation">(</span>nonceStr <span class="token operator">+</span> i <span class="token operator">*</span> <span class="token number">2</span><span class="token punctuation">,</span> <span class="token string">&quot;%02x&quot;</span><span class="token punctuation">,</span> nonceBytes<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        nonceStr<span class="token punctuation">[</span><span class="token number">32</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token char">&#39;\\0&#39;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> std<span class="token double-colon punctuation">::</span><span class="token function">string</span><span class="token punctuation">(</span>nonceStr<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 验证并使用 Nonce</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">useNonce</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> nonce<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 检查 Bloom Filter</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>nonceBloomFilter_<span class="token punctuation">.</span><span class="token function">contains</span><span class="token punctuation">(</span>nonce<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 可能已使用，精确检查</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>usedNonces_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>nonce<span class="token punctuation">)</span> <span class="token operator">!=</span> usedNonces_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span> <span class="token comment">// 确认已使用</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 记录 Nonce</span></span>
<span class="line">        usedNonces_<span class="token punctuation">[</span>nonce<span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        nonceBloomFilter_<span class="token punctuation">.</span><span class="token function">add</span><span class="token punctuation">(</span>nonce<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        nonceCount_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 定期清理</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>nonceCount_ <span class="token operator">&gt;</span> CLEANUP_THRESHOLD<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">cleanupExpiredNonces</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 清理过期 Nonce</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">cleanupExpiredNonces</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> expireTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> NONCE_EXPIRE_SECONDS<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> usedNonces_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> usedNonces_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>second <span class="token operator">&lt;</span> expireTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                it <span class="token operator">=</span> usedNonces_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                nonceCount_<span class="token operator">--</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token operator">++</span>it<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 重建 Bloom Filter</span></span>
<span class="line">        nonceBloomFilter_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>nonce<span class="token punctuation">,</span> time<span class="token punctuation">]</span> <span class="token operator">:</span> usedNonces_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            nonceBloomFilter_<span class="token punctuation">.</span><span class="token function">add</span><span class="token punctuation">(</span>nonce<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> usedNonces_<span class="token punctuation">;</span></span>
<span class="line">    BloomFilter nonceBloomFilter_<span class="token punctuation">;</span></span>
<span class="line">    size_t nonceCount_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> NONCE_EXPIRE_SECONDS <span class="token operator">=</span> <span class="token number">300</span><span class="token punctuation">;</span> <span class="token comment">// 5分钟</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t CLEANUP_THRESHOLD <span class="token operator">=</span> <span class="token number">10000</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> std<span class="token double-colon punctuation">::</span><span class="token function">time</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 简化的 Bloom Filter</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">BloomFilter</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">        <span class="token function">BloomFilter</span><span class="token punctuation">(</span>size_t size <span class="token operator">=</span> <span class="token number">10000</span><span class="token punctuation">,</span> size_t hashes <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token operator">:</span> <span class="token function">bits_</span><span class="token punctuation">(</span>size<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">hashCount_</span><span class="token punctuation">(</span>hashes<span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">add</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> hashCount_<span class="token punctuation">;</span> i<span class="token operator">++</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                size_t index <span class="token operator">=</span> <span class="token function">hash</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> i<span class="token punctuation">)</span> <span class="token operator">%</span> bits_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                bits_<span class="token punctuation">[</span>index<span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">bool</span> <span class="token function">contains</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> hashCount_<span class="token punctuation">;</span> i<span class="token operator">++</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                size_t index <span class="token operator">=</span> <span class="token function">hash</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> i<span class="token punctuation">)</span> <span class="token operator">%</span> bits_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>bits_<span class="token punctuation">[</span>index<span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span><span class="token function">fill</span><span class="token punctuation">(</span>bits_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> bits_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token boolean">false</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">bool</span><span class="token operator">&gt;</span> bits_<span class="token punctuation">;</span></span>
<span class="line">        size_t hashCount_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        size_t <span class="token function">hash</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">,</span> size_t seed<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">            size_t h <span class="token operator">=</span> seed<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">char</span> c <span class="token operator">:</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                h <span class="token operator">=</span> h <span class="token operator">*</span> <span class="token number">31</span> <span class="token operator">+</span> c<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token keyword">return</span> h<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、综合防重放方案" tabindex="-1"><a class="header-anchor" href="#三、综合防重放方案"><span>三、综合防重放方案</span></a></h2><h3 id="_3-1-组合机制" tabindex="-1"><a class="header-anchor" href="#_3-1-组合机制"><span>3.1 组合机制</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              组合防重放方案 (推荐)                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  消息格式:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  struct SecureMessage {                          │       │</span>
<span class="line">│  │      uint32_t sequence;      // 序列号           │       │</span>
<span class="line">│  │      uint32_t timestamp;     // 时间戳           │       │</span>
<span class="line">│  │      uint8_t  nonce[16];     // Nonce            │       │</span>
<span class="line">│  │      uint16_t messageId;     // 消息 ID          │       │</span>
<span class="line">│  │      uint8_t  signature[32];// 签名              │       │</span>
<span class="line">│  │      uint8_t  data[];        // 消息数据         │       │</span>
<span class="line">│  │  };                                                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  验证流程:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 验证签名 (防止篡改)                            │       │</span>
<span class="line">│  │  2. 检查时间戳 (拒绝过期消息)                       │       │</span>
<span class="line">│  │  3. 检查序列号 (拒绝重复/乱序)                      │       │</span>
<span class="line">│  │  4. 检查 Nonce (双重保险)                          │       │</span>
<span class="line">│  │  5. 全部通过则处理消息                             │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  各层作用:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  签名     → 防止消息被篡改                          │       │</span>
<span class="line">│  │  时间戳   → 拒绝明显过期的消息                      │       │</span>
<span class="line">│  │  序列号   → 防止短期重放、检测乱序                  │       │</span>
<span class="line">│  │  Nonce    → 防止长期重放                           │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-完整实现" tabindex="-1"><a class="header-anchor" href="#_3-2-完整实现"><span>3.2 完整实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 完整的防重放消息处理器</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SecureMessageHandler</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 发送安全消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendSecureMessage</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> messageId<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> data<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        SecureMessage msg<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>sequence <span class="token operator">=</span> sequenceManager_<span class="token punctuation">.</span><span class="token function">nextSequence</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>messageId <span class="token operator">=</span> messageId<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>data <span class="token operator">=</span> data<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 生成 Nonce</span></span>
<span class="line">        msg<span class="token punctuation">.</span>nonce <span class="token operator">=</span> nonceManager_<span class="token punctuation">.</span><span class="token function">generateNonce</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 序列化并签名</span></span>
<span class="line">        <span class="token keyword">auto</span> serialized <span class="token operator">=</span> msg<span class="token punctuation">.</span><span class="token function">serializeWithoutSignature</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">sign</span><span class="token punctuation">(</span>serialized<span class="token punctuation">,</span> msg<span class="token punctuation">.</span>signature<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送</span></span>
<span class="line">        network_<span class="token punctuation">.</span><span class="token function">send</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span><span class="token function">serialize</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收并验证消息</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">onReceiveMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        SecureMessage msg<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>msg<span class="token punctuation">.</span><span class="token function">deserialize</span><span class="token punctuation">(</span>data<span class="token punctuation">,</span> len<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 验证签名</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">verifySignature</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Invalid signature&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 验证时间戳</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>timestampValidator_<span class="token punctuation">.</span><span class="token function">validateTimestamp</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>timestamp<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Invalid timestamp&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 验证序列号</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>sequenceManager_<span class="token punctuation">.</span><span class="token function">validateSequence</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>sequence<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Invalid sequence number&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 验证 Nonce</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>nonceManager_<span class="token punctuation">.</span><span class="token function">useNonce</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>nonce<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_WARNING</span><span class="token punctuation">(</span><span class="token string">&quot;Duplicate nonce&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 处理消息</span></span>
<span class="line">        <span class="token function">handleMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>messageId<span class="token punctuation">,</span> msg<span class="token punctuation">.</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">SecureMessage</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string nonce<span class="token punctuation">;</span>      <span class="token comment">// 16 字节 hex</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> messageId<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint8_t</span> signature<span class="token punctuation">[</span><span class="token number">32</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> data<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> <span class="token function">serializeWithoutSignature</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> buffer<span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">appendUint32</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> sequence<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">appendUint32</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> timestamp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">appendString</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> nonce<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">appendUint16</span><span class="token punctuation">(</span>buffer<span class="token punctuation">,</span> messageId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            buffer<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> data<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> data<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> buffer<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> <span class="token function">serialize</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span> buffer <span class="token operator">=</span> <span class="token function">serializeWithoutSignature</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            buffer<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> signature<span class="token punctuation">,</span> signature <span class="token operator">+</span> <span class="token number">32</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> buffer<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">bool</span> <span class="token function">deserialize</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t len<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            size_t offset <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            sequence <span class="token operator">=</span> <span class="token function">readUint32</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">            timestamp <span class="token operator">=</span> <span class="token function">readUint32</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            size_t nonceLen <span class="token operator">=</span> <span class="token number">32</span><span class="token punctuation">;</span> <span class="token comment">// 16 字节 = 32 hex</span></span>
<span class="line">            nonce<span class="token punctuation">.</span><span class="token function">assign</span><span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token keyword">char</span><span class="token operator">*</span><span class="token punctuation">)</span>data <span class="token operator">+</span> offset<span class="token punctuation">,</span> nonceLen<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            offset <span class="token operator">+=</span> nonceLen<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            messageId <span class="token operator">=</span> <span class="token function">readUint16</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token function">memcpy</span><span class="token punctuation">(</span>signature<span class="token punctuation">,</span> data <span class="token operator">+</span> offset<span class="token punctuation">,</span> <span class="token number">32</span><span class="token punctuation">)</span><span class="token punctuation">;</span> offset <span class="token operator">+=</span> <span class="token number">32</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            size_t dataLen <span class="token operator">=</span> len <span class="token operator">-</span> offset<span class="token punctuation">;</span></span>
<span class="line">            data<span class="token punctuation">.</span><span class="token function">assign</span><span class="token punctuation">(</span>data <span class="token operator">+</span> offset<span class="token punctuation">,</span> data <span class="token operator">+</span> offset <span class="token operator">+</span> dataLen<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    SequenceNumberManager sequenceManager_<span class="token punctuation">;</span></span>
<span class="line">    TimestampValidator timestampValidator_<span class="token punctuation">;</span></span>
<span class="line">    NonceManager nonceManager_<span class="token punctuation">;</span></span>
<span class="line">    Network<span class="token operator">&amp;</span> network_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sign</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> data<span class="token punctuation">,</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> signature<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 使用 HMAC-SHA256 或 Ed25519</span></span>
<span class="line">        <span class="token function">HMAC</span><span class="token punctuation">(</span><span class="token function">EVP_sha256</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> secretKey_<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> secretKey_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">             data<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> data<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> signature<span class="token punctuation">,</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">verifySignature</span><span class="token punctuation">(</span>SecureMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> data <span class="token operator">=</span> msg<span class="token punctuation">.</span><span class="token function">serializeWithoutSignature</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint8_t</span> computedSig<span class="token punctuation">[</span><span class="token number">32</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">HMAC</span><span class="token punctuation">(</span><span class="token function">EVP_sha256</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> secretKey_<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> secretKey_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">             data<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> data<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> computedSig<span class="token punctuation">,</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">memcmp</span><span class="token punctuation">(</span>computedSig<span class="token punctuation">,</span> msg<span class="token punctuation">.</span>signature<span class="token punctuation">,</span> <span class="token number">32</span><span class="token punctuation">)</span> <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> secretKey_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-安全措施" tabindex="-1"><a class="header-anchor" href="#四、kbengine-安全措施"><span>四、KBEngine 安全措施</span></a></h2><h3 id="_4-1-kbengine-消息加密" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-消息加密"><span>4.1 KBEngine 消息加密</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 消息加密机制</span></span>
<span class="line"><span class="token comment">// src/lib/network/bundle.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Bundle</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">MemoryStream</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 消息封装时添加加密</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">newMessage</span><span class="token punctuation">(</span>MessageID msgID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span><span class="token keyword">this</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> msgID<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 如果启用了加密，添加加密标记</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pChannel_<span class="token operator">-&gt;</span><span class="token function">isEncrypted</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">(</span><span class="token operator">*</span><span class="token keyword">this</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> <span class="token punctuation">(</span>uint8<span class="token punctuation">)</span><span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">encrypt</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">(</span><span class="token operator">*</span><span class="token keyword">this</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> <span class="token punctuation">(</span>uint8<span class="token punctuation">)</span><span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 加密数据</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">encrypt</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pChannel_<span class="token operator">-&gt;</span><span class="token function">encryptionType</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">==</span> ENCRYPTION_TYPE_AES<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// AES 加密</span></span>
<span class="line">            <span class="token function">aesEncrypt</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>pChannel_<span class="token operator">-&gt;</span><span class="token function">encryptionType</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">==</span> ENCRYPTION_TYPE_BLOWFISH<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// Blowfish 加密 (KBEngine 默认)</span></span>
<span class="line">            <span class="token function">blowfishEncrypt</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    Channel<span class="token operator">*</span> pChannel_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">blowfishEncrypt</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 使用 Blowfish 加密</span></span>
<span class="line">        <span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> key <span class="token operator">=</span> pChannel_<span class="token operator">-&gt;</span><span class="token function">encryptionKey</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> keyLen <span class="token operator">=</span> pChannel_<span class="token operator">-&gt;</span><span class="token function">encryptionKeyLength</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// KBEngine 使用 Blowfish 进行消息加密</span></span>
<span class="line">        <span class="token comment">// src/lib/network/blowfish_box.h</span></span>
<span class="line">        <span class="token function">blowfish_box_encrypt</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token function">wpos</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> <span class="token function">wpos</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            key<span class="token punctuation">,</span></span>
<span class="line">            keyLen</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-kbengine-登录安全" tabindex="-1"><a class="header-anchor" href="#_4-2-kbengine-登录安全"><span>4.2 KBEngine 登录安全</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 登录防重放</span></span>
<span class="line"><span class="token comment">// src/server/loginapp/loginapp_interface.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LoginApp</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 登录请求处理</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">login</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> accountName<span class="token punctuation">,</span></span>
<span class="line">               <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> password<span class="token punctuation">,</span></span>
<span class="line">               <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> datas<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 检查是否已有活跃会话</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">hasActiveSession</span><span class="token punctuation">(</span>accountName<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 防止重复登录</span></span>
<span class="line">            <span class="token function">sendLoginError</span><span class="token punctuation">(</span>accountName<span class="token punctuation">,</span> <span class="token string">&quot;Already logged in&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 验证密码</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">verifyPassword</span><span class="token punctuation">(</span>accountName<span class="token punctuation">,</span> password<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">sendLoginError</span><span class="token punctuation">(</span>accountName<span class="token punctuation">,</span> <span class="token string">&quot;Invalid password&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 创建登录令牌 (带时间戳和随机数)</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string token <span class="token operator">=</span> <span class="token function">generateLoginToken</span><span class="token punctuation">(</span>accountName<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 记录登录信息</span></span>
<span class="line">        <span class="token function">recordLogin</span><span class="token punctuation">(</span>accountName<span class="token punctuation">,</span> token<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 返回令牌</span></span>
<span class="line">        <span class="token function">sendLoginSuccess</span><span class="token punctuation">(</span>accountName<span class="token punctuation">,</span> token<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">generateLoginToken</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> accountName<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 令牌格式: account:timestamp:random:signature</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp <span class="token operator">=</span> <span class="token function">time</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint8_t</span> random<span class="token punctuation">[</span><span class="token number">16</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">RAND_bytes</span><span class="token punctuation">(</span>random<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>random<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string tokenData <span class="token operator">=</span> accountName <span class="token operator">+</span> <span class="token string">&quot;:&quot;</span> <span class="token operator">+</span></span>
<span class="line">                               std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>timestamp<span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token string">&quot;:&quot;</span> <span class="token operator">+</span></span>
<span class="line">                               <span class="token function">hexEncode</span><span class="token punctuation">(</span>random<span class="token punctuation">,</span> <span class="token number">16</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 签名</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string signature <span class="token operator">=</span> <span class="token function">hmacSha256</span><span class="token punctuation">(</span>tokenData<span class="token punctuation">,</span> secretKey_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> tokenData <span class="token operator">+</span> <span class="token string">&quot;:&quot;</span> <span class="token operator">+</span> signature<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">verifyLoginToken</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> token<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 解析 token</span></span>
<span class="line">        <span class="token keyword">auto</span> parts <span class="token operator">=</span> <span class="token function">split</span><span class="token punctuation">(</span>token<span class="token punctuation">,</span> <span class="token char">&#39;:&#39;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>parts<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">!=</span> <span class="token number">4</span><span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string account <span class="token operator">=</span> parts<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">stoul</span><span class="token punctuation">(</span>parts<span class="token punctuation">[</span><span class="token number">1</span><span class="token punctuation">]</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string random <span class="token operator">=</span> parts<span class="token punctuation">[</span><span class="token number">2</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string signature <span class="token operator">=</span> parts<span class="token punctuation">[</span><span class="token number">3</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 检查时间戳 (5分钟窗口)</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> currentTime <span class="token operator">=</span> <span class="token function">time</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">abs</span><span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token keyword">int32_t</span><span class="token punctuation">)</span><span class="token punctuation">(</span>currentTime <span class="token operator">-</span> timestamp<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">300</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 验证签名</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string tokenData <span class="token operator">=</span> account <span class="token operator">+</span> <span class="token string">&quot;:&quot;</span> <span class="token operator">+</span> parts<span class="token punctuation">[</span><span class="token number">1</span><span class="token punctuation">]</span> <span class="token operator">+</span> <span class="token string">&quot;:&quot;</span> <span class="token operator">+</span> random<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string computedSig <span class="token operator">=</span> <span class="token function">hmacSha256</span><span class="token punctuation">(</span>tokenData<span class="token punctuation">,</span> secretKey_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> computedSig <span class="token operator">==</span> signature<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string secretKey_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、特殊场景处理" tabindex="-1"><a class="header-anchor" href="#五、特殊场景处理"><span>五、特殊场景处理</span></a></h2><h3 id="_5-1-关键操作幂等性" tabindex="-1"><a class="header-anchor" href="#_5-1-关键操作幂等性"><span>5.1 关键操作幂等性</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                 关键操作幂等性设计                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  原则：即使消息被重放，也不应产生负面影响                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  幂等操作设计:                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 为每个关键操作分配唯一 ID                      │       │</span>
<span class="line">│  │  2. 服务器记录已处理的操作 ID                       │       │</span>
<span class="line">│  │  3. 重复 ID 的操作只执行一次                        │       │</span>
<span class="line">│  │  4. 返回之前的结果                                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  示例：购买道具                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  客户端请求:                                       │       │</span>
<span class="line">│  │  {                                                │       │</span>
<span class="line">│  │    &quot;messageId&quot;: &quot;buyItem&quot;,                       │       │</span>
<span class="line">│  │    &quot;operationId&quot;: &quot;uuid-12345&quot;,                  │       │</span>
<span class="line">│  │    &quot;itemId&quot;: 1001,                               │       │</span>
<span class="line">│  │    &quot;count&quot;: 1                                    │       │</span>
<span class="line">│  │  }                                                │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  服务器处理:                                       │       │</span>
<span class="line">│  │  1. 检查 operationId 是否已处理                     │       │</span>
<span class="line">│  │  2. 如果已处理，返回之前的结果                       │       │</span>
<span class="line">│  │  3. 如果未处理，执行购买                             │       │</span>
<span class="line">│  │  4. 记录 operationId                               │       │</span>
<span class="line">│  │  5. 返回结果                                        │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 幂等操作处理器</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">Result</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">IdempotentOperation</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Operation</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string operationId<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string request<span class="token punctuation">;</span></span>
<span class="line">        Result result<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 执行或获取操作结果</span></span>
<span class="line">    Result <span class="token function">execute</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> operationId<span class="token punctuation">,</span></span>
<span class="line">                   <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> request<span class="token punctuation">,</span></span>
<span class="line">                   std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token function">Result</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">&gt;</span> handler<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否已处理</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> operations_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>operationId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> operations_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">LOG_INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Returning cached result for operation: &quot;</span> <span class="token operator">+</span> operationId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>result<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 执行操作</span></span>
<span class="line">        Result result <span class="token operator">=</span> <span class="token function">handler</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 记录结果</span></span>
<span class="line">        Operation op<span class="token punctuation">;</span></span>
<span class="line">        op<span class="token punctuation">.</span>operationId <span class="token operator">=</span> operationId<span class="token punctuation">;</span></span>
<span class="line">        op<span class="token punctuation">.</span>request <span class="token operator">=</span> request<span class="token punctuation">;</span></span>
<span class="line">        op<span class="token punctuation">.</span>result <span class="token operator">=</span> result<span class="token punctuation">;</span></span>
<span class="line">        op<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        operations_<span class="token punctuation">[</span>operationId<span class="token punctuation">]</span> <span class="token operator">=</span> op<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 清理过期记录</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>operations_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> CLEANUP_THRESHOLD<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">cleanupExpiredOperations</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> result<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> Operation<span class="token operator">&gt;</span> operations_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">cleanupExpiredOperations</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> expireTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> OPERATION_EXPIRE_SECONDS<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> operations_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> operations_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>timestamp <span class="token operator">&lt;</span> expireTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                it <span class="token operator">=</span> operations_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token operator">++</span>it<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> OPERATION_EXPIRE_SECONDS <span class="token operator">=</span> <span class="token number">3600</span><span class="token punctuation">;</span> <span class="token comment">// 1小时</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t CLEANUP_THRESHOLD <span class="token operator">=</span> <span class="token number">10000</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ShopService</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    BuyResult <span class="token function">buyItem</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> playerId<span class="token punctuation">,</span></span>
<span class="line">                      <span class="token keyword">uint32_t</span> itemId<span class="token punctuation">,</span></span>
<span class="line">                      <span class="token keyword">uint32_t</span> count<span class="token punctuation">,</span></span>
<span class="line">                      <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> operationId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> idempotentBuy_<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>operationId<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;buyItem:&quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>itemId<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">[</span><span class="token operator">&amp;</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 实际购买逻辑</span></span>
<span class="line">                Player<span class="token operator">*</span> player <span class="token operator">=</span> <span class="token function">getPlayer</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>player<span class="token punctuation">)</span> <span class="token keyword">return</span> BuyResult<span class="token double-colon punctuation">::</span>PlayerNotFound<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>player<span class="token operator">-&gt;</span><span class="token function">getGold</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> <span class="token function">getItemPrice</span><span class="token punctuation">(</span>itemId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> BuyResult<span class="token double-colon punctuation">::</span>InsufficientGold<span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">                player<span class="token operator">-&gt;</span><span class="token function">deductGold</span><span class="token punctuation">(</span><span class="token function">getItemPrice</span><span class="token punctuation">(</span>itemId<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                player<span class="token operator">-&gt;</span><span class="token function">addItem</span><span class="token punctuation">(</span>itemId<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">return</span> BuyResult<span class="token double-colon punctuation">::</span>Success<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    IdempotentOperation<span class="token operator">&lt;</span>BuyResult<span class="token operator">&gt;</span> idempotentBuy_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-防重放设计原则" tabindex="-1"><a class="header-anchor" href="#_6-1-防重放设计原则"><span>6.1 防重放设计原则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  防重放设计原则                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 深度防御                                                │</span>
<span class="line">│     ├── 多层验证 (签名 + 时间戳 + 序列号 + Nonce)            │</span>
<span class="line">│     ├── 不同场景使用不同策略                                  │</span>
<span class="line">│     └── 任何一层失败都拒绝消息                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 权威服务器                                              │</span>
<span class="line">│     ├── 所有关键操作在服务器验证                              │</span>
<span class="line">│     ├── 客户端只发送请求                                     │</span>
<span class="line">│     └── 服务器计算所有结果                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 幂等设计                                                │</span>
<span class="line">│     ├── 关键操作设计为幂等                                   │</span>
<span class="line">│     ├── 使用唯一操作 ID                                      │</span>
<span class="line">│     └── 重复请求返回相同结果                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 合理窗口                                                │</span>
<span class="line">│     ├── 时间窗口: 根据网络延迟调整                            │</span>
<span class="line">│     ├── 序列号窗口: 允许一定乱序                              │</span>
<span class="line">│     └── Nonce 过期: 平衡安全和存储                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 监控告警                                                │</span>
<span class="line">│     ├── 记录重放攻击尝试                                      │</span>
<span class="line">│     ├── 异常模式检测                                          │</span>
<span class="line">│     └── 及时响应处理                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-不同场景的策略" tabindex="-1"><a class="header-anchor" href="#_6-2-不同场景的策略"><span>6.2 不同场景的策略</span></a></h3><table><thead><tr><th>场景</th><th>推荐策略</th><th>理由</th></tr></thead><tbody><tr><td><strong>登录</strong></td><td>时间戳 + Nonce + 签名</td><td>防止盗号、会话劫持</td></tr><tr><td><strong>战斗操作</strong></td><td>序列号 + 时间戳</td><td>高频、低延迟要求</td></tr><tr><td><strong>交易</strong></td><td>幂等 ID + Nonce + 签名</td><td>涉及资产、高安全</td></tr><tr><td><strong>聊天</strong></td><td>时间戳 + 频率限制</td><td>防刷屏、低安全要求</td></tr><tr><td><strong>移动</strong></td><td>序列号</td><td>纯粹的防重放</td></tr><tr><td><strong>道具使用</strong></td><td>幂等 ID + 签名</td><td>防刷道具</td></tr></tbody></table><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="防重放技术对比" tabindex="-1"><a class="header-anchor" href="#防重放技术对比"><span>防重放技术对比</span></a></h3><table><thead><tr><th>技术</th><th>优势</th><th>劣势</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>序列号</strong></td><td>简单、高效</td><td>需要状态、掉包敏感</td><td>实时操作</td></tr><tr><td><strong>时间戳</strong></td><td>无状态</td><td>时钟依赖</td><td>通用场景</td></tr><tr><td><strong>Nonce</strong></td><td>完全防重放</td><td>需要存储</td><td>关键操作</td></tr><tr><td><strong>签名</strong></td><td>防篡改</td><td>计算开销</td><td>所有场景</td></tr><tr><td><strong>幂等 ID</strong></td><td>业务安全</td><td>需要业务配合</td><td>交易、消费</td></tr></tbody></table><h3 id="kbengine-安全机制" tabindex="-1"><a class="header-anchor" href="#kbengine-安全机制"><span>KBEngine 安全机制</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 的安全措施:</span>
<span class="line">1. Blowfish 消息加密</span>
<span class="line">2. 登录令牌验证</span>
<span class="line">3. 会话管理</span>
<span class="line">4. 账号绑定连接</span>
<span class="line"></span>
<span class="line">开发者需要:</span>
<span class="line">1. 实现业务层幂等性</span>
<span class="line">2. 添加防外挂验证</span>
<span class="line">3. 监控异常行为</span>
<span class="line">4. 定期更新密钥</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/lib/network/bundle.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - Bundle 加密</a></li><li><a href="https://owasp.org/www-community/attacks/Replay_Attack" target="_blank" rel="noopener noreferrer">OWASP 防重放攻击指南</a></li><li><a href="https://www.cisecurity.org/controls/" target="_blank" rel="noopener noreferrer">网络安全最佳实践</a></li></ul>`,54)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};