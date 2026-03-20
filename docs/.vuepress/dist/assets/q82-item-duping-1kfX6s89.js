import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q82-item-duping.html","title":"Q82: 如何防止刷物品？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q82-item-duping.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q82-item-duping.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q82-如何防止刷物品" tabindex="-1"><a class="header-anchor" href="#q82-如何防止刷物品"><span>Q82: 如何防止刷物品？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对物品刷取漏洞防护的理解：</p><ul><li>刷物品常见手法</li><li>交易验证</li><li>数据一致性</li><li>审计日志</li></ul><hr><h2 id="一、刷物品手法" tabindex="-1"><a class="header-anchor" href="#一、刷物品手法"><span>一、刷物品手法</span></a></h2><h3 id="_1-1-常见手法" tabindex="-1"><a class="header-anchor" href="#_1-1-常见手法"><span>1.1 常见手法</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    刷物品手法                                 │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 重复利用漏洞                                              │</span>
<span class="line">│  ├── 交易时利用网络延迟多次确认                               │</span>
<span class="line">│  ├── 利用时序问题                                           │</span>
<span class="line">│  └── 利用 rollback 机制                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 越权操作                                                 │</span>
<span class="line">│  ├── 修改请求参数                                           │</span>
<span class="line">│  ├── 绕过客户端限制                                         │</span>
<span class="line">│  └── 直接发送协议包                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 并发竞争                                                 │</span>
<span class="line">│  ├── 同时使用同一物品                                        │</span>
<span class="line">│  ├── 同时完成同一任务                                        │</span>
<span class="line">│  └── 转让时利用竞争                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 逻辑漏洞                                                 │</span>
<span class="line">│  ├── 删除后获得补偿                                         │</span>
<span class="line">│  ├── 兑换漏洞                                               │</span>
<span class="line">│  └── 刷任务奖励                                             │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、交易验证" tabindex="-1"><a class="header-anchor" href="#二、交易验证"><span>二、交易验证</span></a></h2><h3 id="_2-1-原子交易" tabindex="-1"><a class="header-anchor" href="#_2-1-原子交易"><span>2.1 原子交易</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 原子交易系统</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TradeSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">TradeSession</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> id<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> player1<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> player2<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>ItemOffer<span class="token operator">&gt;</span> offer1<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>ItemOffer<span class="token operator">&gt;</span> offer2<span class="token punctuation">;</span></span>
<span class="line">        TradeState state<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> version<span class="token punctuation">;</span>  <span class="token comment">// 版本号防止并发修改</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">TradeState</span> <span class="token punctuation">{</span></span>
<span class="line">        INITIATING<span class="token punctuation">,</span></span>
<span class="line">        CONFIRMING<span class="token punctuation">,</span></span>
<span class="line">        COMPLETED<span class="token punctuation">,</span></span>
<span class="line">        CANCELLED</span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 创建交易</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> <span class="token function">createTrade</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> requesterId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> targetId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 验证双方可以交易</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">canTrade</span><span class="token punctuation">(</span>requesterId<span class="token punctuation">,</span> targetId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 检查距离</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">isInRange</span><span class="token punctuation">(</span>requesterId<span class="token punctuation">,</span> targetId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 创建交易会话</span></span>
<span class="line">        TradeSession session<span class="token punctuation">;</span></span>
<span class="line">        session<span class="token punctuation">.</span>id <span class="token operator">=</span> <span class="token function">generateTradeId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        session<span class="token punctuation">.</span>player1 <span class="token operator">=</span> requesterId<span class="token punctuation">;</span></span>
<span class="line">        session<span class="token punctuation">.</span>player2 <span class="token operator">=</span> targetId<span class="token punctuation">;</span></span>
<span class="line">        session<span class="token punctuation">.</span>state <span class="token operator">=</span> TradeState<span class="token double-colon punctuation">::</span>INITIATING<span class="token punctuation">;</span></span>
<span class="line">        session<span class="token punctuation">.</span>version <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        trades_<span class="token punctuation">[</span>session<span class="token punctuation">.</span>id<span class="token punctuation">]</span> <span class="token operator">=</span> session<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 锁定双方背包</span></span>
<span class="line">        <span class="token function">lockInventory</span><span class="token punctuation">(</span>requesterId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">lockInventory</span><span class="token punctuation">(</span>targetId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> session<span class="token punctuation">.</span>id<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加物品到交易</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">addItem</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> tradeId<span class="token punctuation">,</span></span>
<span class="line">                 <span class="token keyword">const</span> ItemOffer<span class="token operator">&amp;</span> item<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        TradeSession<span class="token operator">*</span> trade <span class="token operator">=</span> <span class="token function">getTrade</span><span class="token punctuation">(</span>tradeId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>trade <span class="token operator">||</span> trade<span class="token operator">-&gt;</span>state <span class="token operator">!=</span> TradeState<span class="token double-colon punctuation">::</span>INITIATING<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 验证是否是交易参与者</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>trade<span class="token operator">-&gt;</span>player1 <span class="token operator">!=</span> playerId <span class="token operator">&amp;&amp;</span> trade<span class="token operator">-&gt;</span>player2 <span class="token operator">!=</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 验证物品所有权</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">verifyItemOwnership</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> item<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 添加到对应报价</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>trade<span class="token operator">-&gt;</span>player1 <span class="token operator">==</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            trade<span class="token operator">-&gt;</span>offer1<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>item<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            trade<span class="token operator">-&gt;</span>offer2<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>item<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 增加版本号</span></span>
<span class="line">        trade<span class="token operator">-&gt;</span>version<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 确认交易</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">confirm1</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> tradeId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        TradeSession<span class="token operator">*</span> trade <span class="token operator">=</span> <span class="token function">getTrade</span><span class="token punctuation">(</span>tradeId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>trade<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>trade<span class="token operator">-&gt;</span>mutex<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查版本号防止并发修改</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> clientVersion <span class="token operator">=</span> <span class="token function">getClientVersion</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>clientVersion <span class="token operator">!=</span> trade<span class="token operator">-&gt;</span>version<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 版本不匹配，交易内容已变更</span></span>
<span class="line">            <span class="token function">sendTradeUpdate</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> tradeId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 标记确认</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>trade<span class="token operator">-&gt;</span>player1 <span class="token operator">==</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            trade<span class="token operator">-&gt;</span>confirmed1 <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            trade<span class="token operator">-&gt;</span>confirmed2 <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 双方都确认后进入最终确认阶段</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>trade<span class="token operator">-&gt;</span>confirmed1 <span class="token operator">&amp;&amp;</span> trade<span class="token operator">-&gt;</span>confirmed2<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            trade<span class="token operator">-&gt;</span>state <span class="token operator">=</span> TradeState<span class="token double-colon punctuation">::</span>CONFIRMING<span class="token punctuation">;</span></span>
<span class="line">            trade<span class="token operator">-&gt;</span>version<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 最终确认</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">confirm2</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> tradeId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        TradeSession<span class="token operator">*</span> trade <span class="token operator">=</span> <span class="token function">getTrade</span><span class="token punctuation">(</span>tradeId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>trade <span class="token operator">||</span> trade<span class="token operator">-&gt;</span>state <span class="token operator">!=</span> TradeState<span class="token double-colon punctuation">::</span>CONFIRMING<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>trade<span class="token operator">-&gt;</span>mutex<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 最终确认</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>trade<span class="token operator">-&gt;</span>player1 <span class="token operator">==</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            trade<span class="token operator">-&gt;</span>finalConfirmed1 <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            trade<span class="token operator">-&gt;</span>finalConfirmed2 <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 双方都最终确认，执行交易</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>trade<span class="token operator">-&gt;</span>finalConfirmed1 <span class="token operator">&amp;&amp;</span> trade<span class="token operator">-&gt;</span>finalConfirmed2<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token function">executeTrade</span><span class="token punctuation">(</span><span class="token operator">*</span>trade<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">executeTrade</span><span class="token punctuation">(</span><span class="token keyword">const</span> TradeSession<span class="token operator">&amp;</span> trade<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 在数据库事务中执行所有操作</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">dbTransaction</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token operator">&amp;</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 1. 移除玩家1的物品</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> item <span class="token operator">:</span> trade<span class="token punctuation">.</span>offer1<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">removeItem</span><span class="token punctuation">(</span>trade<span class="token punctuation">.</span>player1<span class="token punctuation">,</span> item<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span>  <span class="token comment">// 回滚</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 2. 移除玩家2的物品</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> item <span class="token operator">:</span> trade<span class="token punctuation">.</span>offer2<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">removeItem</span><span class="token punctuation">(</span>trade<span class="token punctuation">.</span>player2<span class="token punctuation">,</span> item<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span>  <span class="token comment">// 回滚</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 3. 添加物品到玩家1</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> item <span class="token operator">:</span> trade<span class="token punctuation">.</span>offer2<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">addItem</span><span class="token punctuation">(</span>trade<span class="token punctuation">.</span>player1<span class="token punctuation">,</span> item<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span>  <span class="token comment">// 回滚</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 4. 添加物品到玩家2</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> item <span class="token operator">:</span> trade<span class="token punctuation">.</span>offer1<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">addItem</span><span class="token punctuation">(</span>trade<span class="token punctuation">.</span>player2<span class="token punctuation">,</span> item<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span>  <span class="token comment">// 回滚</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 5. 记录日志</span></span>
<span class="line">            <span class="token function">logTrade</span><span class="token punctuation">(</span>trade<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 6. 清理交易</span></span>
<span class="line">            trades_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>trade<span class="token punctuation">.</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">unlockInventory</span><span class="token punctuation">(</span>trade<span class="token punctuation">.</span>player1<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">unlockInventory</span><span class="token punctuation">(</span>trade<span class="token punctuation">.</span>player2<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint64_t</span><span class="token punctuation">,</span> TradeSession<span class="token operator">&gt;</span> trades_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-物品锁定" tabindex="-1"><a class="header-anchor" href="#_2-2-物品锁定"><span>2.2 物品锁定</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 物品锁定机制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ItemLockManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 锁定物品</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">lockItem</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> itemId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        ItemKey key <span class="token operator">=</span> <span class="token punctuation">{</span>playerId<span class="token punctuation">,</span> itemId<span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否已锁定</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>lockedItems_<span class="token punctuation">.</span><span class="token function">count</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        lockedItems_<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 批量锁定</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">lockItems</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint64_t</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> itemIds<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 先检查是否都能锁定</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint64_t</span> itemId <span class="token operator">:</span> itemIds<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            ItemKey key <span class="token operator">=</span> <span class="token punctuation">{</span>playerId<span class="token punctuation">,</span> itemId<span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>lockedItems_<span class="token punctuation">.</span><span class="token function">count</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span>  <span class="token comment">// 有物品已锁定</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 全部锁定</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint64_t</span> itemId <span class="token operator">:</span> itemIds<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            lockedItems_<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span><span class="token punctuation">{</span>playerId<span class="token punctuation">,</span> itemId<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 解锁物品</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">unlockItem</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> itemId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        lockedItems_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span><span class="token punctuation">{</span>playerId<span class="token punctuation">,</span> itemId<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 解锁玩家所有物品</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">unlockPlayerItems</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> lockedItems_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> lockedItems_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>playerId <span class="token operator">==</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                it <span class="token operator">=</span> lockedItems_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token operator">++</span>it<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isItemLocked</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> itemId<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> lockedItems_<span class="token punctuation">.</span><span class="token function">count</span><span class="token punctuation">(</span><span class="token punctuation">{</span>playerId<span class="token punctuation">,</span> itemId<span class="token punctuation">}</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">ItemKey</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> playerId<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> itemId<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">bool</span> <span class="token keyword">operator</span><span class="token operator">&lt;</span><span class="token punctuation">(</span><span class="token keyword">const</span> ItemKey<span class="token operator">&amp;</span> other<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> playerId <span class="token operator">!=</span> other<span class="token punctuation">.</span>playerId <span class="token operator">?</span></span>
<span class="line">                   playerId <span class="token operator">&lt;</span> other<span class="token punctuation">.</span>playerId <span class="token operator">:</span></span>
<span class="line">                   itemId <span class="token operator">&lt;</span> other<span class="token punctuation">.</span>itemId<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex mutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>set<span class="token operator">&lt;</span>ItemKey<span class="token operator">&gt;</span> lockedItems_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、数据库一致性" tabindex="-1"><a class="header-anchor" href="#三、数据库一致性"><span>三、数据库一致性</span></a></h2><h3 id="_3-1-事务操作" tabindex="-1"><a class="header-anchor" href="#_3-1-事务操作"><span>3.1 事务操作</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 数据库事务保证一致性</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">DatabaseItemManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 转移物品 (原子操作)</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">transferItem</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> fromPlayer<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> toPlayer<span class="token punctuation">,</span></span>
<span class="line">                     <span class="token keyword">uint64_t</span> itemId<span class="token punctuation">,</span> <span class="token keyword">int</span> count<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">dbTransaction</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token operator">&amp;</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 1. 读取源物品</span></span>
<span class="line">            <span class="token keyword">auto</span> sourceItem <span class="token operator">=</span> <span class="token function">queryItem</span><span class="token punctuation">(</span>fromPlayer<span class="token punctuation">,</span> itemId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>sourceItem <span class="token operator">||</span> sourceItem<span class="token operator">-&gt;</span>count <span class="token operator">&lt;</span> count<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 2. 读取目标背包</span></span>
<span class="line">            <span class="token keyword">auto</span> targetItems <span class="token operator">=</span> <span class="token function">queryInventory</span><span class="token punctuation">(</span>toPlayer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>targetItems<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;=</span> <span class="token function">getMaxInventorySlots</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 3. 更新源物品</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>sourceItem<span class="token operator">-&gt;</span>count <span class="token operator">==</span> count<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">deleteItem</span><span class="token punctuation">(</span>fromPlayer<span class="token punctuation">,</span> itemId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">updateItemCount</span><span class="token punctuation">(</span>fromPlayer<span class="token punctuation">,</span> itemId<span class="token punctuation">,</span></span>
<span class="line">                               sourceItem<span class="token operator">-&gt;</span>count <span class="token operator">-</span> count<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 4. 添加到目标背包</span></span>
<span class="line">            <span class="token function">addItem</span><span class="token punctuation">(</span>toPlayer<span class="token punctuation">,</span> sourceItem<span class="token operator">-&gt;</span>itemTemplateId<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 5. 记录转移日志</span></span>
<span class="line">            <span class="token function">logItemTransfer</span><span class="token punctuation">(</span>fromPlayer<span class="token punctuation">,</span> toPlayer<span class="token punctuation">,</span> itemId<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 删除并补偿 (防止刷)</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">deleteItemWithCompensation</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> itemId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">dbTransaction</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token operator">&amp;</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 先获取物品信息</span></span>
<span class="line">            <span class="token keyword">auto</span> item <span class="token operator">=</span> <span class="token function">queryItem</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> itemId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>item<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 删除物品</span></span>
<span class="line">            <span class="token function">deleteItem</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> itemId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 如果是付费物品，不补偿</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>item<span class="token operator">-&gt;</span>isPaid<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 检查是否已有补偿记录</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">hasCompensationRecord</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> itemId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span>  <span class="token comment">// 已补偿过，拒绝再次补偿</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 给予补偿</span></span>
<span class="line">            <span class="token function">giveCompensation</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> item<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 记录补偿</span></span>
<span class="line">            <span class="token function">recordCompensation</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> itemId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">dbTransaction</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">bool</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">&gt;</span> fn<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 开始事务</span></span>
<span class="line">        db_<span class="token operator">-&gt;</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">try</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">bool</span> result <span class="token operator">=</span> <span class="token function">fn</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>result<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                db_<span class="token operator">-&gt;</span><span class="token function">commit</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                db_<span class="token operator">-&gt;</span><span class="token function">rollback</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">catch</span> <span class="token punctuation">(</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            db_<span class="token operator">-&gt;</span><span class="token function">rollback</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    Database<span class="token operator">*</span> db_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、审计日志" tabindex="-1"><a class="header-anchor" href="#四、审计日志"><span>四、审计日志</span></a></h2><h3 id="_4-1-完整日志" tabindex="-1"><a class="header-anchor" href="#_4-1-完整日志"><span>4.1 完整日志</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 物品操作审计日志</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ItemAuditLogger</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">logItemOperation</span><span class="token punctuation">(</span><span class="token keyword">const</span> ItemOperation<span class="token operator">&amp;</span> op<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        AuditLogEntry entry<span class="token punctuation">;</span></span>
<span class="line">        entry<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        entry<span class="token punctuation">.</span>playerId <span class="token operator">=</span> op<span class="token punctuation">.</span>playerId<span class="token punctuation">;</span></span>
<span class="line">        entry<span class="token punctuation">.</span>operation <span class="token operator">=</span> op<span class="token punctuation">.</span>operation<span class="token punctuation">;</span></span>
<span class="line">        entry<span class="token punctuation">.</span>itemId <span class="token operator">=</span> op<span class="token punctuation">.</span>itemId<span class="token punctuation">;</span></span>
<span class="line">        entry<span class="token punctuation">.</span>itemTemplateId <span class="token operator">=</span> op<span class="token punctuation">.</span>itemTemplateId<span class="token punctuation">;</span></span>
<span class="line">        entry<span class="token punctuation">.</span>count <span class="token operator">=</span> op<span class="token punctuation">.</span>count<span class="token punctuation">;</span></span>
<span class="line">        entry<span class="token punctuation">.</span>from <span class="token operator">=</span> op<span class="token punctuation">.</span>from<span class="token punctuation">;</span></span>
<span class="line">        entry<span class="token punctuation">.</span>to <span class="token operator">=</span> op<span class="token punctuation">.</span>to<span class="token punctuation">;</span></span>
<span class="line">        entry<span class="token punctuation">.</span>reason <span class="token operator">=</span> op<span class="token punctuation">.</span>reason<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 序列化</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string logLine <span class="token operator">=</span> <span class="token function">serialize</span><span class="token punctuation">(</span>entry<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 写入日志文件</span></span>
<span class="line">        <span class="token function">logToFile</span><span class="token punctuation">(</span>logLine<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 异步写入数据库</span></span>
<span class="line">        <span class="token function">asyncWriteToDB</span><span class="token punctuation">(</span>entry<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查可疑操作</span></span>
<span class="line">        <span class="token function">checkSuspicious</span><span class="token punctuation">(</span>entry<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 分析日志查找异常</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>AuditLogEntry<span class="token operator">&gt;</span> <span class="token function">analyzePlayerActivity</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span></span>
<span class="line">                                                      <span class="token keyword">int</span> hours<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> entries <span class="token operator">=</span> <span class="token function">queryLogs</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> hours<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>AuditLogEntry<span class="token operator">&gt;</span> suspicious<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查短时间内大量获得物品</span></span>
<span class="line">        <span class="token keyword">auto</span> gained <span class="token operator">=</span> <span class="token function">filterByOperation</span><span class="token punctuation">(</span>entries<span class="token punctuation">,</span> <span class="token string">&quot;gain&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>gained<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">100</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            suspicious<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>suspicious<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> gained<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> gained<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查物品异常转移</span></span>
<span class="line">        <span class="token keyword">auto</span> transferred <span class="token operator">=</span> <span class="token function">filterByOperation</span><span class="token punctuation">(</span>entries<span class="token punctuation">,</span> <span class="token string">&quot;transfer&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> entry <span class="token operator">:</span> transferred<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>entry<span class="token punctuation">.</span>count <span class="token operator">&gt;</span> <span class="token number">1000</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                suspicious<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>entry<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> suspicious<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">AuditLogEntry</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> playerId<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string operation<span class="token punctuation">;</span>  <span class="token comment">// gain, lose, transfer, delete</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> itemId<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> itemTemplateId<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> count<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> from<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> to<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string reason<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、防刷策略" tabindex="-1"><a class="header-anchor" href="#五、防刷策略"><span>五、防刷策略</span></a></h2><h3 id="_5-1-速率限制" tabindex="-1"><a class="header-anchor" href="#_5-1-速率限制"><span>5.1 速率限制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 物品操作速率限制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ItemRateLimiter</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">checkOperation</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> operation<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Key key <span class="token operator">=</span> <span class="token punctuation">{</span>playerId<span class="token punctuation">,</span> operation<span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> now <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 清理过期记录</span></span>
<span class="line">        <span class="token function">cleanup</span><span class="token punctuation">(</span>now<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 获取计数</span></span>
<span class="line">        <span class="token keyword">int</span> count <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> counters_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> counters_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            count <span class="token operator">=</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>count<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查限制</span></span>
<span class="line">        <span class="token keyword">int</span> limit <span class="token operator">=</span> <span class="token function">getLimit</span><span class="token punctuation">(</span>operation<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>count <span class="token operator">&gt;=</span> limit<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">logRateLimitExceed</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> operation<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 增加计数</span></span>
<span class="line">        counters_<span class="token punctuation">[</span>key<span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">{</span>now<span class="token punctuation">,</span> count <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">getLimit</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> operation<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">static</span> std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> <span class="token keyword">int</span><span class="token operator">&gt;</span> limits <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;trade&quot;</span><span class="token punctuation">,</span> <span class="token number">10</span><span class="token punctuation">}</span><span class="token punctuation">,</span>        <span class="token comment">// 每小时10次交易</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;drop_item&quot;</span><span class="token punctuation">,</span> <span class="token number">50</span><span class="token punctuation">}</span><span class="token punctuation">,</span>    <span class="token comment">// 每小时50次丢弃</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;delete_item&quot;</span><span class="token punctuation">,</span> <span class="token number">20</span><span class="token punctuation">}</span><span class="token punctuation">,</span>  <span class="token comment">// 每小时20次删除</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;mail&quot;</span><span class="token punctuation">,</span> <span class="token number">30</span><span class="token punctuation">}</span><span class="token punctuation">,</span>         <span class="token comment">// 每小时30次邮件</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;craft&quot;</span><span class="token punctuation">,</span> <span class="token number">100</span><span class="token punctuation">}</span>        <span class="token comment">// 每小时100次制作</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> limits<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>operation<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> it <span class="token operator">!=</span> limits<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">?</span> it<span class="token operator">-&gt;</span>second <span class="token operator">:</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Key</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> playerId<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string operation<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">bool</span> <span class="token keyword">operator</span><span class="token operator">&lt;</span><span class="token punctuation">(</span><span class="token keyword">const</span> Key<span class="token operator">&amp;</span> other<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> playerId <span class="token operator">!=</span> other<span class="token punctuation">.</span>playerId <span class="token operator">?</span></span>
<span class="line">                   playerId <span class="token operator">&lt;</span> other<span class="token punctuation">.</span>playerId <span class="token operator">:</span></span>
<span class="line">                   operation <span class="token operator">&lt;</span> other<span class="token punctuation">.</span>operation<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Counter</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> windowStart<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> count<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">cleanup</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> now<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">const</span> <span class="token keyword">uint64_t</span> WINDOW <span class="token operator">=</span> <span class="token number">3600000</span><span class="token punctuation">;</span>  <span class="token comment">// 1小时</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> counters_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> counters_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>now <span class="token operator">-</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>windowStart <span class="token operator">&gt;</span> WINDOW<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                it <span class="token operator">=</span> counters_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token operator">++</span>it<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span>Key<span class="token punctuation">,</span> Counter<span class="token operator">&gt;</span> counters_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、kbengine-防刷" tabindex="-1"><a class="header-anchor" href="#六、kbengine-防刷"><span>六、KBEngine 防刷</span></a></h2><h3 id="_6-1-kbengine-物品管理" tabindex="-1"><a class="header-anchor" href="#_6-1-kbengine-物品管理"><span>6.1 KBEngine 物品管理</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 风格的防刷机制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ItemManager</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    KBEngine 物品管理:</span>
<span class="line">    1. 服务端权威</span>
<span class="line">    2. 物品锁定</span>
<span class="line">    3. 事务操作</span>
<span class="line">    4. 日志审计</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>locked_items <span class="token operator">=</span> <span class="token builtin">set</span><span class="token punctuation">(</span><span class="token punctuation">)</span>  <span class="token comment"># (player_id, item_id)</span></span>
<span class="line">        self<span class="token punctuation">.</span>pending_trades <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">add_item</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;添加物品&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 数据库操作</span></span>
<span class="line">        success <span class="token operator">=</span> db<span class="token punctuation">.</span>execute<span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;INSERT INTO items (player_id, item_id, count) &quot;</span></span>
<span class="line">            <span class="token string">&quot;VALUES (?, ?, ?)&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count</span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> success<span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 记录日志</span></span>
<span class="line">            self<span class="token punctuation">.</span>log_item_operation<span class="token punctuation">(</span><span class="token string">&quot;add&quot;</span><span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> success</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">remove_item</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;移除物品&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 先锁定物品</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>lock_item<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 检查数量</span></span>
<span class="line">        current <span class="token operator">=</span> self<span class="token punctuation">.</span>get_item_count<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> current <span class="token operator">&lt;</span> count<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>unlock_item<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 更新数据库</span></span>
<span class="line">        success <span class="token operator">=</span> db<span class="token punctuation">.</span>execute<span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;UPDATE items SET count = count - ? &quot;</span></span>
<span class="line">            <span class="token string">&quot;WHERE player_id = ? AND item_id = ? AND count &gt;= ?&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            count<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count</span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> success<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>log_item_operation<span class="token punctuation">(</span><span class="token string">&quot;remove&quot;</span><span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        self<span class="token punctuation">.</span>unlock_item<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> success</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">transfer_item</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> from_player<span class="token punctuation">,</span> to_player<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;转移物品 (原子操作)&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 事务操作</span></span>
<span class="line">        <span class="token keyword">with</span> db<span class="token punctuation">.</span>transaction<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 锁定双方物品</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>lock_item<span class="token punctuation">(</span>from_player<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                db<span class="token punctuation">.</span>rollback<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 检查数量</span></span>
<span class="line">            current <span class="token operator">=</span> self<span class="token punctuation">.</span>get_item_count<span class="token punctuation">(</span>from_player<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">if</span> current <span class="token operator">&lt;</span> count<span class="token punctuation">:</span></span>
<span class="line">                self<span class="token punctuation">.</span>unlock_item<span class="token punctuation">(</span>from_player<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span></span>
<span class="line">                db<span class="token punctuation">.</span>rollback<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 移除</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>remove_item<span class="token punctuation">(</span>from_player<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                self<span class="token punctuation">.</span>unlock_item<span class="token punctuation">(</span>from_player<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span></span>
<span class="line">                db<span class="token punctuation">.</span>rollback<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 添加</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>add_item<span class="token punctuation">(</span>to_player<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token comment"># 回滚添加</span></span>
<span class="line">                self<span class="token punctuation">.</span>add_item<span class="token punctuation">(</span>from_player<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span></span>
<span class="line">                self<span class="token punctuation">.</span>unlock_item<span class="token punctuation">(</span>from_player<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span></span>
<span class="line">                db<span class="token punctuation">.</span>rollback<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">            self<span class="token punctuation">.</span>unlock_item<span class="token punctuation">(</span>from_player<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 记录日志</span></span>
<span class="line">            self<span class="token punctuation">.</span>log_transfer<span class="token punctuation">(</span>from_player<span class="token punctuation">,</span> to_player<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            db<span class="token punctuation">.</span>commit<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">True</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="防刷物品核心" tabindex="-1"><a class="header-anchor" href="#防刷物品核心"><span>防刷物品核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">防刷 = 原子操作 + 物品锁定 + 事务保证 + 日志审计</span>
<span class="line">- 交易使用两阶段确认</span>
<span class="line">- 数据库事务保证一致性</span>
<span class="line">- 详细日志追踪</span>
<span class="line">- 速率限制异常操作</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.gamedeveloper.com/" target="_blank" rel="noopener noreferrer">Game Security Best Practices</a></li><li><a href="https://en.wikipedia.org/wiki/Isolation_(database_systems)" target="_blank" rel="noopener noreferrer">Database Transaction Isolation</a></li></ul>`,37)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};