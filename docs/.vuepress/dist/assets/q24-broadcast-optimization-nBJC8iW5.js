import{a as e,c as t,i as n,l as r,o as i,r as a,s as o,t as s,u as c}from"./app-B8uz0aqq.js";var l=JSON.parse(`{"path":"/qa/q24-broadcast-optimization.html","title":"Q24: 如何设计广播机制？如何优化大规模广播？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q24-broadcast-optimization.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),u={name:`q24-broadcast-optimization.md`};function d(s,l,u,d,f,p){let m=r(`RouteLink`);return t(),n(`div`,null,[l[3]||=e(`<h1 id="q24-如何设计广播机制-如何优化大规模广播" tabindex="-1"><a class="header-anchor" href="#q24-如何设计广播机制-如何优化大规模广播"><span>Q24: 如何设计广播机制？如何优化大规模广播？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对广播机制设计的理解：</p><ul><li>广播的使用场景和挑战</li><li>AOI 广播优化</li><li>KBEngine 的广播机制</li><li>大规模广播性能优化</li></ul><hr><h2 id="一、广播场景分析" tabindex="-1"><a class="header-anchor" href="#一、广播场景分析"><span>一、广播场景分析</span></a></h2><h3 id="_1-1-广播类型" tabindex="-1"><a class="header-anchor" href="#_1-1-广播类型"><span>1.1 广播类型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    游戏中的广播类型                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 位置广播                                                │</span>
<span class="line">│     ├── 玩家移动通知                                       │</span>
<span class="line">│     ├── 实体状态更新                                       │</span>
<span class="line">│     └── AOI 进出通知                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 事件广播                                                │</span>
<span class="line">│     ├── 技能释放效果                                       │</span>
<span class="line">│     ├── 伤害数值显示                                       │</span>
<span class="line">│     └── Buff 状态变化                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 系统广播                                                │</span>
<span class="line">│     ├── 世界聊天                                           │</span>
<span class="line">│     ├── 系统公告                                           │</span>
<span class="line">│     └── 全服通知                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 状态广播                                                │</span>
<span class="line">│     ├── 血量变化                                           │</span>
<span class="line">│     ├── 属性变更                                           │</span>
<span class="line">│     └── 状态效果                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-广播挑战" tabindex="-1"><a class="header-anchor" href="#_1-2-广播挑战"><span>1.2 广播挑战</span></a></h3><table><thead><tr><th>挑战</th><th>描述</th><th>影响</th></tr></thead><tbody><tr><td><strong>网络带宽</strong></td><td>N 个玩家需要 N² 条消息</td><td>带宽爆炸</td></tr><tr><td><strong>CPU 开销</strong></td><td>序列化大量消息</td><td>CPU 瓶颈</td></tr><tr><td><strong>消息重复</strong></td><td>同一消息可能多次发送</td><td>浪费资源</td></tr><tr><td><strong>延迟累积</strong></td><td>大量消息排队</td><td>延迟增加</td></tr></tbody></table><hr><h2 id="二、广播优化策略" tabindex="-1"><a class="header-anchor" href="#二、广播优化策略"><span>二、广播优化策略</span></a></h2><h3 id="_2-1-aoi-限制广播" tabindex="-1"><a class="header-anchor" href="#_2-1-aoi-限制广播"><span>2.1 AOI 限制广播</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                   AOI 广播优化                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  传统广播（所有玩家）：                                      │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  玩家 A 移动 → 广播给所有 100 人                  │       │</span>
<span class="line">│  │  消息数 = 100 × 100 = 10,000 条/秒               │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  AOI 广播（仅视野内）：                                      │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │          玩家 A                                  │       │</span>
<span class="line">│  │             │                                    │       │</span>
<span class="line">│  │        ┌────┴────┐                               │       │</span>
<span class="line">│  │        │ AOI 范围 │ (半径 100m)                 │       │</span>
<span class="line">│  │        │  (10人)  │                               │       │</span>
<span class="line">│  │        └────┬────┘                               │       │</span>
<span class="line">│  │             │                                    │       │</span>
<span class="line">│  │         只广播给这 10 人                          │       │</span>
<span class="line">│  │  消息数 = 100 × 10 = 1,000 条/秒                │       │</span>
<span class="line">│  │  减少 90% 的消息量！                             │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-广播去重" tabindex="-1"><a class="header-anchor" href="#_2-2-广播去重"><span>2.2 广播去重</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 广播去重机制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BroadcastDeduplicator</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 帧标记（每帧递增）</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">uint32_t</span> currentFrame_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 消息已发送标记</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">BroadcastRecord</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> lastFrame<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span> recipients<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>MessageID<span class="token punctuation">,</span> BroadcastRecord<span class="token operator">&gt;</span> records_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcast</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> sender<span class="token punctuation">,</span> Message<span class="token operator">*</span> msg<span class="token punctuation">,</span></span>
<span class="line">                  <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        MessageID msgId <span class="token operator">=</span> msg<span class="token operator">-&gt;</span>id<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否已广播</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> record <span class="token operator">=</span> records_<span class="token punctuation">[</span>msgId<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 过滤已接收的客户端</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> newRecipients<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> viewer <span class="token operator">:</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">bool</span> alreadyReceived <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID id <span class="token operator">:</span> record<span class="token punctuation">.</span>recipients<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>id <span class="token operator">==</span> viewer<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    alreadyReceived <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>alreadyReceived<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                newRecipients<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>viewer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 只发送给新接收者</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> recipient <span class="token operator">:</span> newRecipients<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            recipient<span class="token operator">-&gt;</span><span class="token function">sendMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            record<span class="token punctuation">.</span>recipients<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>recipient<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        record<span class="token punctuation">.</span>lastFrame <span class="token operator">=</span> currentFrame_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 帧更新</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        currentFrame_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 清理过期记录（超过 100 帧）</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span> it <span class="token operator">=</span> records_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> it <span class="token operator">!=</span> records_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>currentFrame_ <span class="token operator">-</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>lastFrame <span class="token operator">&gt;</span> <span class="token number">100</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                it <span class="token operator">=</span> records_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token operator">++</span>it<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">uint32_t</span> BroadcastDeduplicator<span class="token double-colon punctuation">::</span>currentFrame_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-批量打包" tabindex="-1"><a class="header-anchor" href="#_2-3-批量打包"><span>2.3 批量打包</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 批量广播打包</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BatchBroadcaster</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 待发送的消息队列</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">PendingMessage</span> <span class="token punctuation">{</span></span>
<span class="line">        EntityID senderId<span class="token punctuation">;</span></span>
<span class="line">        Message<span class="token operator">*</span> msg<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span> recipients<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>PendingMessage<span class="token operator">&gt;</span> pendingMessages_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加待广播消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addBroadcast</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> sender<span class="token punctuation">,</span> Message<span class="token operator">*</span> msg<span class="token punctuation">,</span></span>
<span class="line">                     <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span> recipientIds<span class="token punctuation">;</span></span>
<span class="line">        recipientIds<span class="token punctuation">.</span><span class="token function">reserve</span><span class="token punctuation">(</span>viewers<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> viewer <span class="token operator">:</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            recipientIds<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>viewer<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        pendingMessages_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">{</span>sender<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> msg<span class="token punctuation">,</span> recipientIds<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 批量发送</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">flush</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pendingMessages_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 按目标客户端分组</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>EntityID<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Message<span class="token operator">*</span><span class="token operator">&gt;&gt;</span> grouped<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> pending <span class="token operator">:</span> pendingMessages_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID recipientId <span class="token operator">:</span> pending<span class="token punctuation">.</span>recipients<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                grouped<span class="token punctuation">[</span>recipientId<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>pending<span class="token punctuation">.</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 批量发送给每个客户端</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>recipientId<span class="token punctuation">,</span> messages<span class="token punctuation">]</span> <span class="token operator">:</span> grouped<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Entity<span class="token operator">*</span> recipient <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>recipientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>recipient<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 创建批量消息包</span></span>
<span class="line">                BatchMessage batch<span class="token punctuation">;</span></span>
<span class="line">                batch<span class="token punctuation">.</span>messages <span class="token operator">=</span> messages<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                recipient<span class="token operator">-&gt;</span><span class="token function">sendMessage</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>batch<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        pendingMessages_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、kbengine-广播机制" tabindex="-1"><a class="header-anchor" href="#三、kbengine-广播机制"><span>三、KBEngine 广播机制</span></a></h2><h3 id="_3-1-witness-广播" tabindex="-1"><a class="header-anchor" href="#_3-1-witness-广播"><span>3.1 Witness 广播</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine 源码</a>：</p><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine Witness 广播机制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># Viewers 列表</span></span>
<span class="line">        self<span class="token punctuation">.</span>viewers <span class="token operator">=</span> <span class="token builtin">set</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">addWitness</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        添加观察者</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>viewers<span class="token punctuation">.</span>add<span class="token punctuation">(</span>entity<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">delWitness</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        移除观察者</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> entity <span class="token keyword">in</span> self<span class="token punctuation">.</span>viewers<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>viewers<span class="token punctuation">.</span>remove<span class="token punctuation">(</span>entity<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">broadcastMessage</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        广播消息给所有观察者</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">for</span> entity <span class="token keyword">in</span> self<span class="token punctuation">.</span>viewers<span class="token punctuation">:</span></span>
<span class="line">            entity<span class="token punctuation">.</span>clientEntity<span class="token punctuation">(</span>message<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-kbengine-优化技术" tabindex="-1"><a class="header-anchor" href="#_3-2-kbengine-优化技术"><span>3.2 KBEngine 优化技术</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 广播优化</span></span>
<span class="line"><span class="token comment">// src/server/cellapp/witness.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Witness</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 观察者列表</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span> viewers_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 脏标记（只广播变化的数据）</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> dirtyFlags_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">DirtyFlags</span> <span class="token punctuation">{</span></span>
<span class="line">        DIRTY_POSITION <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">        DIRTY_ROTATION <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">        DIRTY_HP <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">2</span><span class="token punctuation">,</span></span>
<span class="line">        DIRTY_MP <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">3</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token comment">// ... 其他属性</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播状态</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcastState</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>dirtyFlags_ <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span>  <span class="token comment">// 无变化，不广播</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 只打包变化的属性</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>dirtyFlags_ <span class="token operator">&amp;</span> DIRTY_POSITION<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>position_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>dirtyFlags_ <span class="token operator">&amp;</span> DIRTY_HP<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>hp_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token comment">// ...</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送给所有观察者</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID viewerId <span class="token operator">:</span> viewers_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Entity<span class="token operator">*</span> viewer <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>viewerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>viewer<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                viewer<span class="token operator">-&gt;</span><span class="token function">sendToClient</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        dirtyFlags_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、高级优化技术" tabindex="-1"><a class="header-anchor" href="#四、高级优化技术"><span>四、高级优化技术</span></a></h2><h3 id="_4-1-层次化广播" tabindex="-1"><a class="header-anchor" href="#_4-1-层次化广播"><span>4.1 层次化广播</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                   层次化广播设计                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  广播优先级分层：                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  高优先级 (可靠，立即)                           │       │</span>
<span class="line">│  │  ├── 战斗伤害                                   │       │</span>
<span class="line">│  │  ├── 死亡事件                                   │       │</span>
<span class="line">│  │  └── 关键状态变化                               │       │</span>
<span class="line">│  ├─────────────────────────────────────────────────┤       │</span>
<span class="line">│  │  中优先级 (可靠，延迟)                           │       │</span>
<span class="line">│  │  ├── 技能释放                                   │       │</span>
<span class="line">│  │  ├── Buff 状态                                  │       │</span>
<span class="line">│  │  └── 属性变化                                   │       │</span>
<span class="line">│  ├─────────────────────────────────────────────────┤       │</span>
<span class="line">│  │  低优先级 (可丢，批量)                           │       │</span>
<span class="line">│  │  ├── 位置更新                                   │       │</span>
<span class="line">│  │  ├── 动画状态                                   │       │</span>
<span class="line">│  │  └── 装饰变化                                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  处理策略：                                                 │</span>
<span class="line">│  ├── 高优先级：立即发送，TCP 保证可靠                       │</span>
<span class="line">│  ├── 中优先级：批量发送，100ms 一次                         │</span>
<span class="line">│  └── 低优先级：合并发送，200ms 一次，可丢包                 │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-空间分区优化" tabindex="-1"><a class="header-anchor" href="#_4-2-空间分区优化"><span>4.2 空间分区优化</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 空间分区广播优化</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SpatialPartitionBroadcast</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 空间格子</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Grid</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> entities<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> lastBroadcastFrame<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 空间分区</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>GridID<span class="token punctuation">,</span> Grid<span class="token operator">&gt;</span> grids_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播位置更新</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcastPositionUpdate</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> pos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        GridID gridId <span class="token operator">=</span> <span class="token function">getGridId</span><span class="token punctuation">(</span>pos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 只广播给相邻格子</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>GridID neighborId <span class="token operator">:</span> <span class="token function">getNeighborGrids</span><span class="token punctuation">(</span>gridId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> grid <span class="token operator">=</span> grids_<span class="token punctuation">[</span>neighborId<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 检查是否已广播</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>grid<span class="token punctuation">.</span>lastBroadcastFrame <span class="token operator">==</span> currentFrame_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">continue</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 广播给格子内的实体</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>Entity<span class="token operator">*</span> viewer <span class="token operator">:</span> grid<span class="token punctuation">.</span>entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isInAOI</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> viewer<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    viewer<span class="token operator">-&gt;</span><span class="token function">sendPositionUpdate</span><span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> pos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            grid<span class="token punctuation">.</span>lastBroadcastFrame <span class="token operator">=</span> currentFrame_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取相邻格子</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>GridID<span class="token operator">&gt;</span> <span class="token function">getNeighborGrids</span><span class="token punctuation">(</span>GridID centerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> cx <span class="token operator">=</span> centerId <span class="token operator">%</span> gridSize_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> cy <span class="token operator">=</span> centerId <span class="token operator">/</span> gridSize_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>GridID<span class="token operator">&gt;</span> neighbors<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> dy <span class="token operator">=</span> <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">;</span> dy <span class="token operator">&lt;=</span> <span class="token number">1</span><span class="token punctuation">;</span> <span class="token operator">++</span>dy<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> dx <span class="token operator">=</span> <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">;</span> dx <span class="token operator">&lt;=</span> <span class="token number">1</span><span class="token punctuation">;</span> <span class="token operator">++</span>dx<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">int</span> nx <span class="token operator">=</span> cx <span class="token operator">+</span> dx<span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">int</span> ny <span class="token operator">=</span> cy <span class="token operator">+</span> dy<span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>nx <span class="token operator">&gt;=</span> <span class="token number">0</span> <span class="token operator">&amp;&amp;</span> nx <span class="token operator">&lt;</span> gridSize_ <span class="token operator">&amp;&amp;</span></span>
<span class="line">                    ny <span class="token operator">&gt;=</span> <span class="token number">0</span> <span class="token operator">&amp;&amp;</span> ny <span class="token operator">&lt;</span> gridSize_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    neighbors<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>ny <span class="token operator">*</span> gridSize_ <span class="token operator">+</span> nx<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> neighbors<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-兴趣管理优化" tabindex="-1"><a class="header-anchor" href="#_4-3-兴趣管理优化"><span>4.3 兴趣管理优化</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 兴趣管理（PVSP - Priority-based Visibility）</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PriorityVisibilityManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 实体优先级</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">Priority</span> <span class="token punctuation">{</span></span>
<span class="line">        HIGH <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span>      <span class="token comment">// 玩家自己、重要 NPC</span></span>
<span class="line">        MEDIUM <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span>    <span class="token comment">// 队友、附近玩家</span></span>
<span class="line">        LOW <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span>       <span class="token comment">// 普通玩家</span></span>
<span class="line">        IGNORE <span class="token operator">=</span> <span class="token number">3</span>     <span class="token comment">// 远距离实体</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取实体优先级</span></span>
<span class="line">    Priority <span class="token function">getPriority</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> viewer<span class="token punctuation">,</span> Entity<span class="token operator">*</span> target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">float</span> distance <span class="token operator">=</span> <span class="token function">getDistance</span><span class="token punctuation">(</span>viewer<span class="token punctuation">,</span> target<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>target<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">==</span> viewer<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> Priority<span class="token double-colon punctuation">::</span>HIGH<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isTeamMate</span><span class="token punctuation">(</span>viewer<span class="token punctuation">,</span> target<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> Priority<span class="token double-colon punctuation">::</span>MEDIUM<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>distance <span class="token operator">&lt;</span> <span class="token number">50.0f</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> Priority<span class="token double-colon punctuation">::</span>MEDIUM<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>distance <span class="token operator">&lt;</span> <span class="token number">100.0f</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> Priority<span class="token double-colon punctuation">::</span>LOW<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> Priority<span class="token double-colon punctuation">::</span>IGNORE<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 选择性广播</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">selectiveBroadcast</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> sender<span class="token punctuation">,</span> Message<span class="token operator">*</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> viewers <span class="token operator">=</span> <span class="token function">getPotentialViewers</span><span class="token punctuation">(</span>sender<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 按优先级分组</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span>Priority<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;&gt;</span> grouped<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> viewer <span class="token operator">:</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Priority priority <span class="token operator">=</span> <span class="token function">getPriority</span><span class="token punctuation">(</span>viewer<span class="token punctuation">,</span> sender<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>priority <span class="token operator">!=</span> Priority<span class="token double-colon punctuation">::</span>IGNORE<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                grouped<span class="token punctuation">[</span>priority<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>viewer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 根据优先级决定广播策略</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>msg<span class="token operator">-&gt;</span><span class="token function">isCritical</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 关键消息：广播给所有优先级</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>priority<span class="token punctuation">,</span> entities<span class="token punctuation">]</span> <span class="token operator">:</span> grouped<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> viewer <span class="token operator">:</span> entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    viewer<span class="token operator">-&gt;</span><span class="token function">sendMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 普通消息：只广播给高优先级</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> viewer <span class="token operator">:</span> grouped<span class="token punctuation">[</span>Priority<span class="token double-colon punctuation">::</span>HIGH<span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                viewer<span class="token operator">-&gt;</span><span class="token function">sendMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> viewer <span class="token operator">:</span> grouped<span class="token punctuation">[</span>Priority<span class="token double-colon punctuation">::</span>MEDIUM<span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                viewer<span class="token operator">-&gt;</span><span class="token function">sendMessage</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、性能优化" tabindex="-1"><a class="header-anchor" href="#五、性能优化"><span>五、性能优化</span></a></h2><h3 id="_5-1-零拷贝广播" tabindex="-1"><a class="header-anchor" href="#_5-1-零拷贝广播"><span>5.1 零拷贝广播</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 零拷贝广播实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ZeroCopyBroadcaster</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 共享消息缓冲区</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">SharedMessageBuffer</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>shared_ptr<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;&gt;</span> data<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> msgId<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播（零拷贝）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcast</span><span class="token punctuation">(</span><span class="token keyword">const</span> SharedMessageBuffer<span class="token operator">&amp;</span> buffer<span class="token punctuation">,</span></span>
<span class="line">                  <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> viewer <span class="token operator">:</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 只传递智能指针，不拷贝数据</span></span>
<span class="line">            viewer<span class="token operator">-&gt;</span><span class="token function">sendSharedBuffer</span><span class="token punctuation">(</span>buffer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收方处理</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onReceiveSharedBuffer</span><span class="token punctuation">(</span><span class="token keyword">const</span> SharedMessageBuffer<span class="token operator">&amp;</span> buffer<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 直接访问共享数据，无需拷贝</span></span>
<span class="line">        <span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> data <span class="token operator">=</span> buffer<span class="token punctuation">.</span>data<span class="token operator">-&gt;</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        size_t size <span class="token operator">=</span> buffer<span class="token punctuation">.</span>data<span class="token operator">-&gt;</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 处理消息...</span></span>
<span class="line">        <span class="token function">processMessage</span><span class="token punctuation">(</span>buffer<span class="token punctuation">.</span>msgId<span class="token punctuation">,</span> data<span class="token punctuation">,</span> size<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-多线程广播" tabindex="-1"><a class="header-anchor" href="#_5-2-多线程广播"><span>5.2 多线程广播</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 多线程并行广播</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MultiThreadBroadcaster</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 线程池</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>thread<span class="token operator">&gt;</span> workers_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>BroadcastTask<span class="token operator">&gt;</span> taskQueue_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex queueMutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>condition_variable queueCV_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播任务</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">BroadcastTask</span> <span class="token punctuation">{</span></span>
<span class="line">        Message<span class="token operator">*</span> msg<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span> recipientIds<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加广播任务</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">asyncBroadcast</span><span class="token punctuation">(</span>Message<span class="token operator">*</span> msg<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        BroadcastTask task<span class="token punctuation">;</span></span>
<span class="line">        task<span class="token punctuation">.</span>msg <span class="token operator">=</span> msg<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> viewer <span class="token operator">:</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            task<span class="token punctuation">.</span>recipientIds<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>viewer<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            taskQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>task<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        queueCV_<span class="token punctuation">.</span><span class="token function">notify_one</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 工作线程</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">workerThread</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            BroadcastTask task<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token punctuation">{</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                queueCV_<span class="token punctuation">.</span><span class="token function">wait</span><span class="token punctuation">(</span>lock<span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token operator">!</span>taskQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> <span class="token operator">!</span>running_<span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>running_<span class="token punctuation">)</span> <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                task <span class="token operator">=</span> taskQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                taskQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 处理广播</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID recipientId <span class="token operator">:</span> task<span class="token punctuation">.</span>recipientIds<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                Entity<span class="token operator">*</span> recipient <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>recipientId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>recipient<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    recipient<span class="token operator">-&gt;</span><span class="token function">sendMessage</span><span class="token punctuation">(</span>task<span class="token punctuation">.</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、总结" tabindex="-1"><a class="header-anchor" href="#六、总结"><span>六、总结</span></a></h2><h3 id="优化技术总结" tabindex="-1"><a class="header-anchor" href="#优化技术总结"><span>优化技术总结</span></a></h3><table><thead><tr><th>技术</th><th>效果</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>AOI 限制</strong></td><td>减少 90% 消息</td><td>空间型游戏</td></tr><tr><td><strong>广播去重</strong></td><td>避免重复发送</td><td>所有场景</td></tr><tr><td><strong>批量打包</strong></td><td>减少系统调用</td><td>高频消息</td></tr><tr><td><strong>层次化广播</strong></td><td>保证关键消息</td><td>大规模场景</td></tr><tr><td><strong>空间分区</strong></td><td>优化查找效率</td><td>大世界</td></tr><tr><td><strong>零拷贝</strong></td><td>减少 CPU 开销</td><td>高性能要求</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 默认使用 AOI 限制</span>
<span class="line">   - 只广播给视野内的玩家</span>
<span class="line">   - 大幅减少消息量</span>
<span class="line"></span>
<span class="line">2. 实现脏标记机制</span>
<span class="line">   - 只广播变化的数据</span>
<span class="line">   - 避免无效广播</span>
<span class="line"></span>
<span class="line">3. 使用批量打包</span>
<span class="line">   - 合并多个小消息</span>
<span class="line">   - 减少系统调用</span>
<span class="line"></span>
<span class="line">4. 考虑多线程广播</span>
<span class="line">   - 充分利用多核 CPU</span>
<span class="line">   - 注意线程安全</span>
<span class="line"></span>
<span class="line">5. 监控广播性能</span>
<span class="line">   - 统计消息数量</span>
<span class="line">   - 分析瓶颈</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2>`,47),a(`ul`,null,[l[2]||=a(`li`,null,[a(`a`,{href:`https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/witness.h`,target:`_blank`,rel:`noopener noreferrer`},`KBEngine GitHub - Witness`)],-1),a(`li`,null,[o(m,{to:`/qa/q3-aoi-implementation.html`},{default:c(()=>[...l[0]||=[i(`AOI 九宫格系统详解`,-1)]]),_:1})]),a(`li`,null,[o(m,{to:`/qa/q31-kbengine-broadcast-dedup.html`},{default:c(()=>[...l[1]||=[i(`KBEngine 高效广播与去重`,-1)]]),_:1})])])])}var f=s(u,[[`render`,d]]);export{l as _pageData,f as default};