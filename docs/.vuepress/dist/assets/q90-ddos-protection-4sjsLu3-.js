import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q90-ddos-protection.html","title":"Q90: 如何应对 DDoS 攻击？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q90-ddos-protection.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q90-ddos-protection.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q90-如何应对-ddos-攻击" tabindex="-1"><a class="header-anchor" href="#q90-如何应对-ddos-攻击"><span>Q90: 如何应对 DDoS 攻击？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对 DDoS 防护的理解：</p><ul><li>DDoS 攻击类型</li><li>防护策略</li><li>流量清洗</li><li>游戏服务器防护</li></ul><hr><h2 id="一、ddos-类型" tabindex="-1"><a class="header-anchor" href="#一、ddos-类型"><span>一、DDoS 类型</span></a></h2><h3 id="_1-1-攻击分类" tabindex="-1"><a class="header-anchor" href="#_1-1-攻击分类"><span>1.1 攻击分类</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    DDoS 攻击类型                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 容量攻击                                                │</span>
<span class="line">│  ├── UDP Flood                                             │</span>
<span class="line">│  ├── ICMP Flood                                            │</span>
<span class="line">│  └── 目标: 耗尽带宽                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 协议攻击                                                │</span>
<span class="line">│  ├── SYN Flood                                             │</span>
<span class="line">│  ├── ACK Flood                                             │</span>
<span class="line">│  └── 目标: 耗尽连接资源                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 应用攻击                                                │</span>
<span class="line">│  ├── HTTP Flood                                            │</span>
<span class="line">│  ├── 慢速 POST                                             │</span>
<span class="line">│  └── 目标: 耗尽应用资源                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 游戏攻击                                                │</span>
<span class="line">│  ├── 登录 Flood                                            │</span>
<span class="line">│  ├── 假人攻击                                               │</span>
<span class="line">│  └── 目标: 逻辑漏洞                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、防护策略" tabindex="-1"><a class="header-anchor" href="#二、防护策略"><span>二、防护策略</span></a></h2><h3 id="_2-1-多层防护" tabindex="-1"><a class="header-anchor" href="#_2-1-多层防护"><span>2.1 多层防护</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// DDoS 防护架构</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">DDoSProtection</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 1. 接入层防护</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">checkIP</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> ip<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 检查黑名单</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isBlacklisted</span><span class="token punctuation">(</span>ip<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查白名单</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isWhitelisted</span><span class="token punctuation">(</span>ip<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查威胁情报</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isThreatIP</span><span class="token punctuation">(</span>ip<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">addToGraylist</span><span class="token punctuation">(</span>ip<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 2. 速率限制</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">checkRateLimit</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> ip<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> counter <span class="token operator">=</span> ipCounters_<span class="token punctuation">[</span>ip<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">uint64_t</span> now <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 滑动窗口</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>counter<span class="token punctuation">.</span>slots<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> counter<span class="token punctuation">.</span>slots<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> now <span class="token operator">-</span> <span class="token number">60000</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            counter<span class="token punctuation">.</span>slots<span class="token punctuation">.</span><span class="token function">pop_front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1秒内最多 100 个请求</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>counter<span class="token punctuation">.</span>slots<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">100</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 超限，升级防御</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>counter<span class="token punctuation">.</span>slots<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">500</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 严重超限，加入黑名单</span></span>
<span class="line">                <span class="token function">blacklistIP</span><span class="token punctuation">(</span>ip<span class="token punctuation">,</span> <span class="token number">3600</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        counter<span class="token punctuation">.</span>slots<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>now<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 3. 连接限制</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">checkConnectionLimit</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> ip<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> info <span class="token operator">=</span> ipInfo_<span class="token punctuation">[</span>ip<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 单 IP 连接数限制</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>info<span class="token punctuation">.</span>connections <span class="token operator">&gt;</span> <span class="token number">100</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 新连接速率限制</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> now <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>now <span class="token operator">-</span> info<span class="token punctuation">.</span>lastConnect <span class="token operator">&lt;</span> <span class="token number">100</span><span class="token punctuation">)</span> <span class="token punctuation">{</span>  <span class="token comment">// 100ms 内</span></span>
<span class="line">            info<span class="token punctuation">.</span>rapidCount<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>info<span class="token punctuation">.</span>rapidCount <span class="token operator">&gt;</span> <span class="token number">50</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            info<span class="token punctuation">.</span>rapidCount <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        info<span class="token punctuation">.</span>lastConnect <span class="token operator">=</span> now<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 4. 挑战验证</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">challengeClient</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> ip<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 发送 JS 挑战</span></span>
<span class="line">        <span class="token function">sendChallenge</span><span class="token punctuation">(</span>ip<span class="token punctuation">,</span> <span class="token function">generateChallenge</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 等待响应</span></span>
<span class="line">        <span class="token comment">// 验证通过才放行</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">IPCounter</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>deque<span class="token operator">&lt;</span><span class="token keyword">uint64_t</span><span class="token operator">&gt;</span> slots<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">IPInfo</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> connections <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> lastConnect <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> rapidCount <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> IPCounter<span class="token operator">&gt;</span> ipCounters_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> IPInfo<span class="token operator">&gt;</span> ipInfo_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、游戏服务器防护" tabindex="-1"><a class="header-anchor" href="#三、游戏服务器防护"><span>三、游戏服务器防护</span></a></h2><h3 id="_3-1-登录保护" tabindex="-1"><a class="header-anchor" href="#_3-1-登录保护"><span>3.1 登录保护</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 游戏登录 DDoS 防护</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LoginProtection</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 处理登录请求</span></span>
<span class="line">    LoginResult <span class="token function">handleLogin</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> account<span class="token punctuation">,</span></span>
<span class="line">                            <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> ip<span class="token punctuation">,</span></span>
<span class="line">                            <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> fingerprint<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 检查 IP 信誉</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">checkIPReputation</span><span class="token punctuation">(</span>ip<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 需要验证码</span></span>
<span class="line">            <span class="token keyword">return</span> LoginResult<span class="token double-colon punctuation">::</span>NEED_CAPTCHA<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 检查账号状态</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isAccountLocked</span><span class="token punctuation">(</span>account<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> LoginResult<span class="token double-colon punctuation">::</span>ACCOUNT_LOCKED<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 检查登录频率</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">checkLoginRate</span><span class="token punctuation">(</span>account<span class="token punctuation">,</span> ip<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> LoginResult<span class="token double-colon punctuation">::</span>TOO_FREQUENT<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 验证指纹 (防止多开)</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">checkFingerprint</span><span class="token punctuation">(</span>account<span class="token punctuation">,</span> fingerprint<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> LoginResult<span class="token double-colon punctuation">::</span>FINGERPRINT_MISMATCH<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 执行登录</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">performLogin</span><span class="token punctuation">(</span>account<span class="token punctuation">,</span> ip<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">checkLoginRate</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> account<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> ip<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 检查账号级别限制</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> accountCounter <span class="token operator">=</span> loginCounters_<span class="token punctuation">[</span>account<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">uint64_t</span> now <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> windowStart <span class="token operator">=</span> now <span class="token operator">-</span> <span class="token number">300000</span><span class="token punctuation">;</span>  <span class="token comment">// 5分钟窗口</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 清理过期记录</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>accountCounter<span class="token punctuation">.</span>attempts<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span></span>
<span class="line">               accountCounter<span class="token punctuation">.</span>attempts<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>timestamp <span class="token operator">&lt;</span> windowStart<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            accountCounter<span class="token punctuation">.</span>attempts<span class="token punctuation">.</span><span class="token function">pop_front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5分钟内最多 10 次失败</span></span>
<span class="line">        <span class="token keyword">int</span> failCount <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> attempt <span class="token operator">:</span> accountCounter<span class="token punctuation">.</span>attempts<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>attempt<span class="token punctuation">.</span>success<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                failCount<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>failCount <span class="token operator">&gt;</span> <span class="token number">10</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 账号临时锁定</span></span>
<span class="line">            <span class="token function">lockAccount</span><span class="token punctuation">(</span>account<span class="token punctuation">,</span> <span class="token number">300</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">LoginAttempt</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">bool</span> success<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">AccountCounter</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>deque<span class="token operator">&lt;</span>LoginAttempt<span class="token operator">&gt;</span> attempts<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> AccountCounter<span class="token operator">&gt;</span> loginCounters_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、最佳实践" tabindex="-1"><a class="header-anchor" href="#四、最佳实践"><span>四、最佳实践</span></a></h2><h3 id="_4-1-防护层次" tabindex="-1"><a class="header-anchor" href="#_4-1-防护层次"><span>4.1 防护层次</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">DDoS 防护 = 多层防御 + 流量清洗 + 游戏层防护</span>
<span class="line">- 接入层过滤</span>
<span class="line">- CDN 分流</span>
<span class="line">- 游戏层限流</span>
<span class="line">- 应急预案</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、总结" tabindex="-1"><a class="header-anchor" href="#五、总结"><span>五、总结</span></a></h2><h3 id="ddos-防护核心" tabindex="-1"><a class="header-anchor" href="#ddos-防护核心"><span>DDoS 防护核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">防 DDoS = 预防 + 检测 + 响应 + 恢复</span>
<span class="line">- 接入层防护</span>
<span class="line">- 速率限制</span>
<span class="line">- 挑战验证</span>
<span class="line">- 专业服务 (Cloudflare 等)</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.cloudflare.com/learning/ddos/" target="_blank" rel="noopener noreferrer">DDoS Protection Best Practices</a></li><li><a href="https://www.akamai.com/" target="_blank" rel="noopener noreferrer">Game DDoS Protection</a></li></ul>`,27)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};