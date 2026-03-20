import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q84-memory-hack.html","title":"Q84: 如何防止内存修改？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q84-memory-hack.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q84-memory-hack.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q84-如何防止内存修改" tabindex="-1"><a class="header-anchor" href="#q84-如何防止内存修改"><span>Q84: 如何防止内存修改？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对内存修改防护的理解：</p><ul><li>内存修改原理</li><li>关键数据保护</li><li>完整性检查</li><li>混淆与加密</li></ul><hr><h2 id="一、内存修改原理" tabindex="-1"><a class="header-anchor" href="#一、内存修改原理"><span>一、内存修改原理</span></a></h2><h3 id="_1-1-常见手法" tabindex="-1"><a class="header-anchor" href="#_1-1-常见手法"><span>1.1 常见手法</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    内存修改手法                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 直接修改内存                                             │</span>
<span class="line">│  ├── 使用 Cheat Engine 搜索数值                             │</span>
<span class="line">│  ├── 修改血量、金币、物品数量等                              │</span>
<span class="line">│  └── 实时修改游戏状态                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 指针扫描                                                 │</span>
<span class="line">│  ├── 找到动态地址                                           │</span>
<span class="line">│  ├── 通过指针链修改                                         │</span>
<span class="line">│  └── 绕过简单的地址随机化                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 代码注入                                                 │</span>
<span class="line">│  ├── DLL 注入                                               │</span>
<span class="line">│  ├── Hook 游戏函数                                          │</span>
<span class="line">│  └── 修改游戏逻辑                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 内存读写                                                 │</span>
<span class="line">│  ├── ReadProcessMemory / WriteProcessMemory                 │</span>
<span class="line">│  ├── 外挂读写游戏内存                                        │</span>
<span class="line">│  └── 需要进程权限                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、数据保护" tabindex="-1"><a class="header-anchor" href="#二、数据保护"><span>二、数据保护</span></a></h2><h3 id="_2-1-服务端权威" tabindex="-1"><a class="header-anchor" href="#_2-1-服务端权威"><span>2.1 服务端权威</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 服务端权威 - 根本防护</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ServerAuthoritative</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 客户端请求攻击</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onAttackRequest</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> targetId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 服务端计算伤害</span></span>
<span class="line">        Player<span class="token operator">*</span> attacker <span class="token operator">=</span> <span class="token function">getPlayer</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        Entity<span class="token operator">*</span> target <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>targetId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>attacker <span class="token operator">||</span> <span class="token operator">!</span>target<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 服务端计算伤害</span></span>
<span class="line">        DamageResult result <span class="token operator">=</span> <span class="token function">calculateDamage</span><span class="token punctuation">(</span>attacker<span class="token punctuation">,</span> target<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 服务端扣血</span></span>
<span class="line">        target<span class="token operator">-&gt;</span><span class="token function">setHP</span><span class="token punctuation">(</span>result<span class="token punctuation">.</span>finalDamage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知客户端结果</span></span>
<span class="line">        <span class="token function">sendDamageResult</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;targetId&quot;</span><span class="token punctuation">,</span> targetId<span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;damage&quot;</span><span class="token punctuation">,</span> result<span class="token punctuation">.</span>finalDamage<span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;isCritical&quot;</span><span class="token punctuation">,</span> result<span class="token punctuation">.</span>isCritical<span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 客户端只是显示，无法修改结果</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 定期同步状态</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">syncState</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Player<span class="token operator">*</span> player <span class="token operator">=</span> <span class="token function">getPlayer</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>player<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 定期发送服务端权威状态</span></span>
<span class="line">        <span class="token function">sendStateUpdate</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;hp&quot;</span><span class="token punctuation">,</span> player<span class="token operator">-&gt;</span><span class="token function">getHP</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;mp&quot;</span><span class="token punctuation">,</span> player<span class="token operator">-&gt;</span><span class="token function">getMP</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;position&quot;</span><span class="token punctuation">,</span> player<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;level&quot;</span><span class="token punctuation">,</span> player<span class="token operator">-&gt;</span><span class="token function">getLevel</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;exp&quot;</span><span class="token punctuation">,</span> player<span class="token operator">-&gt;</span><span class="token function">getExp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-关键数据加密" tabindex="-1"><a class="header-anchor" href="#_2-2-关键数据加密"><span>2.2 关键数据加密</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 加密关键数据</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EncryptedGameData</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 加密存储</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setGold</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> gold<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 加密</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> encrypted <span class="token operator">=</span> <span class="token function">encryptValue</span><span class="token punctuation">(</span>gold<span class="token punctuation">,</span> playerKey_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 添加校验和</span></span>
<span class="line">        encrypted <span class="token operator">=</span> <span class="token function">appendChecksum</span><span class="token punctuation">(</span>encrypted<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 存储</span></span>
<span class="line">        gold_ <span class="token operator">=</span> encrypted<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 服务端也需要存储真实值</span></span>
<span class="line">        serverGold_ <span class="token operator">=</span> gold<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 读取</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> <span class="token function">getGold</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 从加密值解密</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> encrypted <span class="token operator">=</span> gold_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 验证校验和</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">verifyChecksum</span><span class="token punctuation">(</span>encrypted<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 被修改过</span></span>
<span class="line">            <span class="token keyword">return</span> serverGold_<span class="token punctuation">;</span>  <span class="token comment">// 返回服务端真实值</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">decryptValue</span><span class="token punctuation">(</span>encrypted<span class="token punctuation">,</span> playerKey_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 与服务端同步验证</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">validateWithServer</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> clientGold <span class="token operator">=</span> <span class="token function">getGold</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> serverGold <span class="token operator">=</span> <span class="token function">queryServerGold</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> clientGold <span class="token operator">==</span> serverGold<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> <span class="token function">encryptValue</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> value<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 简单加密 (实际应使用更强的加密)</span></span>
<span class="line">        <span class="token keyword">return</span> value <span class="token operator">^</span> key<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint64_t</span> <span class="token function">decryptValue</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> encrypted<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> encrypted <span class="token operator">^</span> key<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint64_t</span> <span class="token function">appendChecksum</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> value<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> checksum <span class="token operator">=</span> <span class="token function">calculateChecksum</span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span>value <span class="token operator">&lt;&lt;</span> <span class="token number">32</span><span class="token punctuation">)</span> <span class="token operator">|</span> checksum<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">verifyChecksum</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> value<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> checksum <span class="token operator">=</span> value <span class="token operator">&amp;</span> <span class="token number">0xFFFFFFFF</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> data <span class="token operator">=</span> value <span class="token operator">&gt;&gt;</span> <span class="token number">32</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">calculateChecksum</span><span class="token punctuation">(</span>data<span class="token punctuation">)</span> <span class="token operator">==</span> checksum<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">calculateChecksum</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> value<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// CRC32 或其他校验算法</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">crc32</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>value<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">uint64_t</span> gold_<span class="token punctuation">;</span>        <span class="token comment">// 加密存储</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> serverGold_<span class="token punctuation">;</span>  <span class="token comment">// 服务端真实值</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> playerKey_<span class="token punctuation">;</span>   <span class="token comment">// 玩家密钥</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、完整性检查" tabindex="-1"><a class="header-anchor" href="#三、完整性检查"><span>三、完整性检查</span></a></h2><h3 id="_3-1-内存校验" tabindex="-1"><a class="header-anchor" href="#_3-1-内存校验"><span>3.1 内存校验</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 内存完整性检查</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MemoryIntegrityChecker</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 初始化时保存关键数据哈希</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">initialize</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 获取关键数据区</span></span>
<span class="line">        keyDataStart_ <span class="token operator">=</span> <span class="token function">getKeyDataAddress</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        keyDataSize_ <span class="token operator">=</span> <span class="token function">getKeyDataSize</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 计算初始哈希</span></span>
<span class="line">        originalHash_ <span class="token operator">=</span> <span class="token function">calculateHash</span><span class="token punctuation">(</span>keyDataStart_<span class="token punctuation">,</span> keyDataSize_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 定期检查完整性</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">checkIntegrity</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 计算当前哈希</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> currentHash <span class="token operator">=</span> <span class="token function">calculateHash</span><span class="token punctuation">(</span>keyDataStart_<span class="token punctuation">,</span> keyDataSize_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 对比</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>currentHash <span class="token operator">!=</span> originalHash_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 内存被修改</span></span>
<span class="line">            <span class="token function">onMemoryTampered</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 客户端随机检查</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">randomCheck</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">checkIntegrity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 上报服务器</span></span>
<span class="line">            <span class="token function">reportToServer</span><span class="token punctuation">(</span><span class="token string">&quot;Memory tampering detected&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token comment">// 可以选择退出游戏</span></span>
<span class="line">            <span class="token function">exit</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">calculateHash</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 使用 SHA256 或 CRC32</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> crc <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint8_t</span><span class="token operator">*</span> bytes <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">uint8_t</span><span class="token operator">*</span><span class="token punctuation">)</span>data<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> size<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            crc <span class="token operator">=</span> crc32Table<span class="token punctuation">[</span><span class="token punctuation">(</span>crc <span class="token operator">^</span> bytes<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token operator">&amp;</span> <span class="token number">0xFF</span><span class="token punctuation">]</span> <span class="token operator">^</span> <span class="token punctuation">(</span>crc <span class="token operator">&gt;&gt;</span> <span class="token number">8</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> crc<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span><span class="token operator">*</span> keyDataStart_<span class="token punctuation">;</span></span>
<span class="line">    size_t keyDataSize_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> originalHash_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">const</span> <span class="token keyword">uint32_t</span> crc32Table<span class="token punctuation">[</span><span class="token number">256</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-代码校验" tabindex="-1"><a class="header-anchor" href="#_3-2-代码校验"><span>3.2 代码校验</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 代码完整性检查</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CodeIntegrityChecker</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 初始化时保存代码段哈希</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">void</span> <span class="token function">initialize</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        HMODULE <span class="token keyword">module</span> <span class="token operator">=</span> <span class="token function">GetModuleHandleA</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        MODULEINFO modInfo<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">GetModuleInformation</span><span class="token punctuation">(</span><span class="token function">GetCurrentProcess</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token keyword">module</span><span class="token punctuation">,</span> <span class="token operator">&amp;</span>modInfo<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>modInfo<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 计算代码段哈希</span></span>
<span class="line">        codeHash_ <span class="token operator">=</span> <span class="token function">calculateHash</span><span class="token punctuation">(</span><span class="token punctuation">(</span>BYTE<span class="token operator">*</span><span class="token punctuation">)</span><span class="token keyword">module</span><span class="token punctuation">,</span> modInfo<span class="token punctuation">.</span>SizeOfImage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 保存到安全位置</span></span>
<span class="line">        <span class="token function">saveHashToSecureLocation</span><span class="token punctuation">(</span>codeHash_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 验证代码完整性</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">bool</span> <span class="token function">verifyIntegrity</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        HMODULE <span class="token keyword">module</span> <span class="token operator">=</span> <span class="token function">GetModuleHandleA</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        MODULEINFO modInfo<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">GetModuleInformation</span><span class="token punctuation">(</span><span class="token function">GetCurrentProcess</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token keyword">module</span><span class="token punctuation">,</span> <span class="token operator">&amp;</span>modInfo<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>modInfo<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">uint32_t</span> currentHash <span class="token operator">=</span> <span class="token function">calculateHash</span><span class="token punctuation">(</span><span class="token punctuation">(</span>BYTE<span class="token operator">*</span><span class="token punctuation">)</span><span class="token keyword">module</span><span class="token punctuation">,</span> modInfo<span class="token punctuation">.</span>SizeOfImage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> currentHash <span class="token operator">==</span> codeHash_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">uint32_t</span> <span class="token function">calculateHash</span><span class="token punctuation">(</span><span class="token keyword">const</span> BYTE<span class="token operator">*</span> data<span class="token punctuation">,</span> size_t size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> hash <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> size<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            hash <span class="token operator">=</span> hash <span class="token operator">*</span> <span class="token number">31</span> <span class="token operator">+</span> data<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> hash<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">uint32_t</span> codeHash_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、混淆与保护" tabindex="-1"><a class="header-anchor" href="#四、混淆与保护"><span>四、混淆与保护</span></a></h2><h3 id="_4-1-代码混淆" tabindex="-1"><a class="header-anchor" href="#_4-1-代码混淆"><span>4.1 代码混淆</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 代码混淆技术</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ObfuscatedValue</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 混淆后的血量获取</span></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">getHP</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 不直接返回 hp_</span></span>
<span class="line">        <span class="token comment">// 而是通过一系列运算</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span>hp_ <span class="token operator">^</span> XOR_KEY<span class="token punctuation">)</span> <span class="token operator">*</span> MULTIPLIER <span class="token operator">+</span> ADDEND<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setHP</span><span class="token punctuation">(</span><span class="token keyword">int</span> hp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 同样混淆写入</span></span>
<span class="line">        hp_ <span class="token operator">=</span> <span class="token punctuation">(</span>hp <span class="token operator">-</span> ADDEND<span class="token punctuation">)</span> <span class="token operator">/</span> MULTIPLIER <span class="token operator">^</span> XOR_KEY<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">int</span> hp_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">const</span> <span class="token keyword">int</span> XOR_KEY <span class="token operator">=</span> <span class="token number">0x12345678</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">const</span> <span class="token keyword">int</span> MULTIPLIER <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">const</span> <span class="token keyword">int</span> ADDEND <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 多层指针</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MultiPointerValue</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">getValue</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 通过多级指针访问</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token operator">*</span><span class="token operator">*</span>ptr3_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setValue</span><span class="token punctuation">(</span><span class="token keyword">int</span> value<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token operator">*</span><span class="token operator">*</span>ptr3_ <span class="token operator">=</span> value<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">int</span><span class="token operator">*</span> value_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span><span class="token operator">*</span><span class="token operator">*</span> ptr1_ <span class="token operator">=</span> <span class="token operator">&amp;</span>value_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span><span class="token operator">*</span><span class="token operator">*</span><span class="token operator">*</span> ptr2_ <span class="token operator">=</span> <span class="token operator">&amp;</span>ptr1_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span><span class="token operator">*</span><span class="token operator">*</span><span class="token operator">*</span><span class="token operator">*</span> ptr3_ <span class="token operator">=</span> <span class="token operator">&amp;</span>ptr2_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-字符串加密" tabindex="-1"><a class="header-anchor" href="#_4-2-字符串加密"><span>4.2 字符串加密</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 字符串加密 (防止内存搜索)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EncryptedString</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">EncryptedString</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">char</span><span class="token operator">*</span> str<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t len <span class="token operator">=</span> <span class="token function">strlen</span><span class="token punctuation">(</span>str<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        data_<span class="token punctuation">.</span><span class="token function">resize</span><span class="token punctuation">(</span>len<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 加密存储</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> len<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            data_<span class="token punctuation">[</span>i<span class="token punctuation">]</span> <span class="token operator">=</span> str<span class="token punctuation">[</span>i<span class="token punctuation">]</span> <span class="token operator">^</span> <span class="token punctuation">(</span>XOR_KEY <span class="token operator">+</span> i<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">decrypt</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string result<span class="token punctuation">;</span></span>
<span class="line">        result<span class="token punctuation">.</span><span class="token function">reserve</span><span class="token punctuation">(</span>data_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> data_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            result <span class="token operator">+=</span> data_<span class="token punctuation">[</span>i<span class="token punctuation">]</span> <span class="token operator">^</span> <span class="token punctuation">(</span>XOR_KEY <span class="token operator">+</span> i<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> result<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 运行时解密使用</span></span>
<span class="line">    <span class="token keyword">operator</span> std<span class="token double-colon punctuation">::</span><span class="token function">string</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">decrypt</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">char</span><span class="token operator">&gt;</span> data_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">const</span> <span class="token keyword">char</span> XOR_KEY <span class="token operator">=</span> <span class="token number">0x7A</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">checkPassword</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    EncryptedString expected<span class="token punctuation">{</span><span class="token string">&quot;CorrectPassword&quot;</span><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string input <span class="token operator">=</span> <span class="token function">getUserInput</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span>input <span class="token operator">==</span> expected<span class="token punctuation">)</span> <span class="token punctuation">{</span>  <span class="token comment">// 自动解密比较</span></span>
<span class="line">        <span class="token comment">// 密码正确</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、反调试" tabindex="-1"><a class="header-anchor" href="#五、反调试"><span>五、反调试</span></a></h2><h3 id="_5-1-调试检测" tabindex="-1"><a class="header-anchor" href="#_5-1-调试检测"><span>5.1 调试检测</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 反调试保护</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AntiDebug</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">bool</span> <span class="token function">isDebuggerPresent</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 方法 1: IsDebuggerPresent API</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">IsDebuggerPresent</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 方法 2: CheckRemoteDebuggerPresent</span></span>
<span class="line">        BOOL isRemotePresent <span class="token operator">=</span> FALSE<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">CheckRemoteDebuggerPresent</span><span class="token punctuation">(</span><span class="token function">GetCurrentProcess</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token operator">&amp;</span>isRemotePresent<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>isRemotePresent<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 方法 3: 检查调试标志</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isDebugFlagSet</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 方法 4: 检查调试器进程</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isDebuggerProcessRunning</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">void</span> <span class="token function">triggerAntiDebug</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isDebuggerPresent</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 可以采取多种措施</span></span>
<span class="line">            <span class="token comment">// 1. 静默退出</span></span>
<span class="line">            <span class="token function">exit</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 2. 崩溃 (误导调试者)</span></span>
<span class="line">            <span class="token comment">// *((int*)nullptr) = 0;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 3. 上报服务器</span></span>
<span class="line">            <span class="token comment">// reportToServer(&quot;Debugger detected&quot;);</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">bool</span> <span class="token function">isDebugFlagSet</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">ifdef</span> <span class="token expression">_WIN32</span></span></span>
<span class="line">        <span class="token keyword">typedef</span> <span class="token function">NTSTATUS</span> <span class="token punctuation">(</span>NTAPI<span class="token operator">*</span> NtQueryInformationProcess<span class="token punctuation">)</span><span class="token punctuation">(</span></span>
<span class="line">            HANDLE<span class="token punctuation">,</span> DWORD<span class="token punctuation">,</span> PVOID<span class="token punctuation">,</span> ULONG<span class="token punctuation">,</span> PULONG<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        HMODULE ntdll <span class="token operator">=</span> <span class="token function">GetModuleHandleA</span><span class="token punctuation">(</span><span class="token string">&quot;ntdll.dll&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>ntdll<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span> NtQueryInformationProcess <span class="token operator">=</span> <span class="token punctuation">(</span>NtQueryInformationProcess<span class="token punctuation">)</span></span>
<span class="line">                <span class="token function">GetProcAddress</span><span class="token punctuation">(</span>ntdll<span class="token punctuation">,</span> <span class="token string">&quot;NtQueryInformationProcess&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>NtQueryInformationProcess<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                DWORD dwDebugPort <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">                NTSTATUS status <span class="token operator">=</span> <span class="token function">NtQueryInformationProcess</span><span class="token punctuation">(</span></span>
<span class="line">                    <span class="token function">GetCurrentProcess</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                    <span class="token number">0x7</span><span class="token punctuation">,</span>  <span class="token comment">// ProcessDebugPort</span></span>
<span class="line">                    <span class="token operator">&amp;</span>dwDebugPort<span class="token punctuation">,</span></span>
<span class="line">                    <span class="token keyword">sizeof</span><span class="token punctuation">(</span>dwDebugPort<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                    <span class="token keyword">nullptr</span></span>
<span class="line">                <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">return</span> dwDebugPort <span class="token operator">!=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">endif</span></span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">bool</span> <span class="token function">isDebuggerProcessRunning</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">const</span> <span class="token keyword">wchar_t</span><span class="token operator">*</span> debuggerProcesses<span class="token punctuation">[</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            L<span class="token string">&quot;ollydbg.exe&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            L<span class="token string">&quot;Wireshark.exe&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            L<span class="token string">&quot;IDA.exe&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            L<span class="token string">&quot;ida64.exe&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            L<span class="token string">&quot;idaq.exe&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            L<span class="token string">&quot;x64dbg.exe&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            L<span class="token string">&quot;x32dbg.exe&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            L<span class="token string">&quot;cheatengine.exe&quot;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        HANDLE snapshot <span class="token operator">=</span> <span class="token function">CreateToolhelp32Snapshot</span><span class="token punctuation">(</span>TH32CS_SNAPPROCESS<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        PROCESSENTRY32W pe32<span class="token punctuation">;</span></span>
<span class="line">        pe32<span class="token punctuation">.</span>dwSize <span class="token operator">=</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>pe32<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">Process32FirstW</span><span class="token punctuation">(</span>snapshot<span class="token punctuation">,</span> <span class="token operator">&amp;</span>pe32<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">do</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> proc <span class="token operator">:</span> debuggerProcesses<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">_wcsicmp</span><span class="token punctuation">(</span>pe32<span class="token punctuation">.</span>szExeFile<span class="token punctuation">,</span> proc<span class="token punctuation">)</span> <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                        <span class="token function">CloseHandle</span><span class="token punctuation">(</span>snapshot<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token punctuation">}</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token function">Process32NextW</span><span class="token punctuation">(</span>snapshot<span class="token punctuation">,</span> <span class="token operator">&amp;</span>pe32<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">CloseHandle</span><span class="token punctuation">(</span>snapshot<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、kbengine-防护" tabindex="-1"><a class="header-anchor" href="#六、kbengine-防护"><span>六、KBEngine 防护</span></a></h2><h3 id="_6-1-kbengine-数据保护" tabindex="-1"><a class="header-anchor" href="#_6-1-kbengine-数据保护"><span>6.1 KBEngine 数据保护</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 数据保护</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    KBEngine 实体数据保护:</span>
<span class="line">    1. 服务端存储真实值</span>
<span class="line">    2. 客户端只是显示</span>
<span class="line">    3. 定期同步验证</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 服务端权威属性</span></span>
<span class="line">        self<span class="token punctuation">.</span>__hp <span class="token operator">=</span> <span class="token number">100</span></span>
<span class="line">        self<span class="token punctuation">.</span>__max_hp <span class="token operator">=</span> <span class="token number">100</span></span>
<span class="line">        self<span class="token punctuation">.</span>__mp <span class="token operator">=</span> <span class="token number">50</span></span>
<span class="line">        self<span class="token punctuation">.</span>__max_mp <span class="token operator">=</span> <span class="token number">50</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 客户端可读属性 (显示用)</span></span>
<span class="line">        self<span class="token punctuation">.</span>client_hp <span class="token operator">=</span> <span class="token number">100</span></span>
<span class="line">        self<span class="token punctuation">.</span>client_mp <span class="token operator">=</span> <span class="token number">50</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">modifyHP</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> delta<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;服务端修改血量&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 服务端计算</span></span>
<span class="line">        old_hp <span class="token operator">=</span> self<span class="token punctuation">.</span>__hp</span>
<span class="line">        self<span class="token punctuation">.</span>__hp <span class="token operator">=</span> <span class="token builtin">max</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">,</span> <span class="token builtin">min</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>__max_hp<span class="token punctuation">,</span> self<span class="token punctuation">.</span>__hp <span class="token operator">+</span> delta<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 如果客户端显示与服务端不一致，强制同步</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>client_hp <span class="token operator">!=</span> self<span class="token punctuation">.</span>__hp<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>client_hp <span class="token operator">=</span> self<span class="token punctuation">.</span>__hp</span>
<span class="line">            self<span class="token punctuation">.</span>syncToClient<span class="token punctuation">(</span><span class="token string">&quot;hp&quot;</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>__hp<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 触发事件</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>__hp <span class="token operator">!=</span> old_hp<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>onHPChanged<span class="token punctuation">(</span>old_hp<span class="token punctuation">,</span> self<span class="token punctuation">.</span>__hp<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">getHP</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取血量&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 返回服务端真实值</span></span>
<span class="line">        <span class="token keyword">return</span> self<span class="token punctuation">.</span>__hp</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onClientModifyHP</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> new_hp<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;客户端请求修改血量 (拒绝)&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 记录可疑行为</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>warning<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Player </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> attempted to modify HP&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 强制同步正确值</span></span>
<span class="line">        self<span class="token punctuation">.</span>syncToClient<span class="token punctuation">(</span><span class="token string">&quot;hp&quot;</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>__hp<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="防内存修改核心" tabindex="-1"><a class="header-anchor" href="#防内存修改核心"><span>防内存修改核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">防内存修改 = 服务端权威 + 数据加密 + 完整性检查 + 混淆</span>
<span class="line">- 关键数据服务端存储</span>
<span class="line">- 客户端只负责显示</span>
<span class="line">- 定期验证完整性</span>
<span class="line">- 混淆增加破解难度</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.sans.org/" target="_blank" rel="noopener noreferrer">Memory Protection Techniques</a></li><li><a href="https://www.valvesoftware.com/" target="_blank" rel="noopener noreferrer">Anti-Cheat Development</a></li></ul>`,41)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};