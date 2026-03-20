import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q114-open-source-frameworks.html","title":"Q114: 你对哪些开源游戏服务器框架有了解？各有什么特点？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q114-open-source-frameworks.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q114-open-source-frameworks.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q114-你对哪些开源游戏服务器框架有了解-各有什么特点" tabindex="-1"><a class="header-anchor" href="#q114-你对哪些开源游戏服务器框架有了解-各有什么特点"><span>Q114: 你对哪些开源游戏服务器框架有了解？各有什么特点？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察知识广度：</p><ul><li>主流框架</li><li>架构特点</li><li>适用场景</li><li>对比分析</li></ul><hr><h2 id="一、主流框架概览" tabindex="-1"><a class="header-anchor" href="#一、主流框架概览"><span>一、主流框架概览</span></a></h2><h3 id="_1-1-框架分类" tabindex="-1"><a class="header-anchor" href="#_1-1-框架分类"><span>1.1 框架分类</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    开源游戏服务器框架                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  C/C++ 框架:                                                 │</span>
<span class="line">│  ├── KBEngine - Python 脚本                                 │</span>
<span class="line">│  ├── Skynet - Lua/ C                                       │</span>
<span class="line">│  ├── Pomelo - Node.js                                     │</span>
<span class="line">│  └── Colyseus - Node.js                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Go 框架:                                                   │</span>
<span class="line">│  ├── Leaf - 轻量级                                         │</span>
<span class="line">│  ├── Nano - 最小化                                         │</span>
<span class="line">│  └── Go-World - MMO                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Java 框架:                                                 │</span>
<span class="line">│  ├── Netty + 自研                                          │</span>
<span class="line">│  ├── Wild World Open                                      │</span>
<span class="line">│  └── Zoe - 多人游戏框架                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  其他框架:                                                   │</span>
<span class="line">│  ├── Photon (C#)                                           │</span>
<span class="line">│  ├── SmartFox (Java)                                       │</span>
<span class="line">│  └── RedDwarf (Java)                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、kbengine" tabindex="-1"><a class="header-anchor" href="#二、kbengine"><span>二、KBEngine</span></a></h2><h3 id="_2-1-架构特点" tabindex="-1"><a class="header-anchor" href="#_2-1-架构特点"><span>2.1 架构特点</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 特点</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">KBEngineOverview</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;KBEngine 概述&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    info <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&#39;语言&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;C++ + Python&#39;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;架构&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;分布式微服务&#39;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;特点&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">            <span class="token string">&#39;组件化架构 (LoginApp/BaseApp/CellApp/DBMgr)&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;Python 脚本热更新&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;内置 AOI 系统&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;实体组件系统&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;Watcher 监控系统&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;丰富的文档&#39;</span></span>
<span class="line">        <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;优势&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">            <span class="token string">&#39;成熟稳定&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;文档完善&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;社区活跃&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;快速开发&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;学习成本低&#39;</span></span>
<span class="line">        <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;劣势&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">            <span class="token string">&#39;性能一般&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;Python GIL 限制&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;扩展性有限&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;架构固定&#39;</span></span>
<span class="line">        <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;适用场景&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">            <span class="token string">&#39;中小型 MMO&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;2D/3D 网游&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;卡牌游戏&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;策略游戏&#39;</span></span>
<span class="line">        <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;承载能力&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;单服 5000-8000 CCU&#39;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;开源协议&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;GPLv2&#39;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    architecture <span class="token operator">=</span> <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    KBEngine 架构:</span>
<span class="line"></span>
<span class="line">    LoginApp: 登录认证，负载均衡</span>
<span class="line">       ↓</span>
<span class="line">    BaseAppMgr: 管理 BaseApp</span>
<span class="line">       ↓</span>
<span class="line">    BaseApp: 玩家数据管理，非实时逻辑</span>
<span class="line">       ↓</span>
<span class="line">    CellAppMgr: 管理 CellApp</span>
<span class="line">       ↓</span>
<span class="line">    CellApp: 游戏逻辑，场景管理，AOI</span>
<span class="line">       ↕</span>
<span class="line">    DBMgr: 数据库操作，缓存管理</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、skynet" tabindex="-1"><a class="header-anchor" href="#三、skynet"><span>三、Skynet</span></a></h2><h3 id="_3-1-架构特点" tabindex="-1"><a class="header-anchor" href="#_3-1-架构特点"><span>3.1 架构特点</span></a></h3><div class="language-lua line-numbers-mode" data-highlighter="prismjs" data-ext="lua"><pre><code class="language-lua"><span class="line"><span class="token comment">-- Skynet 特点</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">local</span> SkynetOverview <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">    语言 <span class="token operator">=</span> <span class="token string">&quot;C + Lua&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    架构 <span class="token operator">=</span> <span class="token string">&quot;Actor 模型&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    特点 <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;Actor 消息传递&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;轻量级服务&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;高并发支持&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;热更新支持&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;集群支持&quot;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    优势 <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;性能优秀&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;内存占用小&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;并发能力强&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;扩展灵活&quot;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    劣势 <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;学习曲线陡&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;文档较少&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;调试困难&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;Lua 性能限制&quot;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    适用场景 <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;高并发游戏&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;实时战斗&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;SLG 游戏&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;卡牌游戏&quot;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    承载能力 <span class="token operator">=</span> <span class="token string">&quot;单机 10000+ 服务&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    开源协议 <span class="token operator">=</span> <span class="token string">&quot;MIT&quot;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- Skynet 服务示例</span></span>
<span class="line">skynet<span class="token punctuation">.</span><span class="token function">start</span><span class="token punctuation">(</span><span class="token keyword">function</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token comment">-- 启动一个服务</span></span>
<span class="line">    <span class="token keyword">local</span> service <span class="token operator">=</span> skynet<span class="token punctuation">.</span><span class="token function">newservice</span><span class="token punctuation">(</span><span class="token string">&quot;simpledb&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">-- 发送消息</span></span>
<span class="line">    skynet<span class="token punctuation">.</span><span class="token function">call</span><span class="token punctuation">(</span>service<span class="token punctuation">,</span> <span class="token string">&quot;lua&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;SET&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;hello&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;world&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">-- 获取响应</span></span>
<span class="line">    <span class="token keyword">local</span> r <span class="token operator">=</span> skynet<span class="token punctuation">.</span><span class="token function">call</span><span class="token punctuation">(</span>service<span class="token punctuation">,</span> <span class="token string">&quot;lua&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;GET&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;hello&quot;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token function">print</span><span class="token punctuation">(</span>r<span class="token punctuation">)</span>  <span class="token comment">-- world</span></span>
<span class="line"><span class="token keyword">end</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、pomelo" tabindex="-1"><a class="header-anchor" href="#四、pomelo"><span>四、Pomelo</span></a></h2><h3 id="_4-1-架构特点" tabindex="-1"><a class="header-anchor" href="#_4-1-架构特点"><span>4.1 架构特点</span></a></h3><div class="language-javascript line-numbers-mode" data-highlighter="prismjs" data-ext="js"><pre><code class="language-javascript"><span class="line"><span class="token comment">// Pomelo 特点</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">const</span> PomeloOverview <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token literal-property property">语言</span><span class="token operator">:</span> <span class="token string">&quot;Node.js&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token literal-property property">架构</span><span class="token operator">:</span> <span class="token string">&quot;分布式&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token literal-property property">特点</span><span class="token operator">:</span> <span class="token punctuation">[</span></span>
<span class="line">        <span class="token string">&quot;基于 Node.js 异步&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;组件化设计&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;HTTP/WS 协议&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;热更新支持&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;快速开发&quot;</span></span>
<span class="line">    <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token literal-property property">优势</span><span class="token operator">:</span> <span class="token punctuation">[</span></span>
<span class="line">        <span class="token string">&quot;JavaScript 全栈&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;生态丰富&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;开发效率高&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;社区活跃&quot;</span></span>
<span class="line">    <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token literal-property property">劣势</span><span class="token operator">:</span> <span class="token punctuation">[</span></span>
<span class="line">        <span class="token string">&quot;单线程限制&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;性能一般&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;V8 内存开销&quot;</span></span>
<span class="line">    <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token literal-property property">适用场景</span><span class="token operator">:</span> <span class="token punctuation">[</span></span>
<span class="line">        <span class="token string">&quot;网页游戏&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;H5 游戏&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;卡牌游戏&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;休闲游戏&quot;</span></span>
<span class="line">    <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token literal-property property">承载能力</span><span class="token operator">:</span> <span class="token string">&quot;单服 3000-5000 CCU&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token literal-property property">开源协议</span><span class="token operator">:</span> <span class="token string">&quot;MIT&quot;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Pomelo 路由示例</span></span>
<span class="line">app<span class="token punctuation">.</span><span class="token function">route</span><span class="token punctuation">(</span><span class="token string">&#39;area.playerHandler.enterScene&#39;</span><span class="token punctuation">,</span> <span class="token keyword">function</span><span class="token punctuation">(</span><span class="token parameter">msg<span class="token punctuation">,</span> session<span class="token punctuation">,</span> next</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">var</span> playerId <span class="token operator">=</span> session<span class="token punctuation">.</span>uid<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">var</span> areaId <span class="token operator">=</span> msg<span class="token punctuation">.</span>areaId<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 路由到具体的 connector</span></span>
<span class="line">    <span class="token function">next</span><span class="token punctuation">(</span><span class="token keyword">null</span><span class="token punctuation">,</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token literal-property property">playerId</span><span class="token operator">:</span> playerId<span class="token punctuation">,</span></span>
<span class="line">        <span class="token literal-property property">areaId</span><span class="token operator">:</span> areaId<span class="token punctuation">,</span></span>
<span class="line">        <span class="token literal-property property">host</span><span class="token operator">:</span> utils<span class="token punctuation">.</span><span class="token function">getHostByAreaId</span><span class="token punctuation">(</span>areaId<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token literal-property property">port</span><span class="token operator">:</span> utils<span class="token punctuation">.</span><span class="token function">getPortByAreaId</span><span class="token punctuation">(</span>areaId<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、go-框架" tabindex="-1"><a class="header-anchor" href="#五、go-框架"><span>五、Go 框架</span></a></h2><h3 id="_5-1-leaf" tabindex="-1"><a class="header-anchor" href="#_5-1-leaf"><span>5.1 Leaf</span></a></h3><div class="language-go line-numbers-mode" data-highlighter="prismjs" data-ext="go"><pre><code class="language-go"><span class="line"><span class="token comment">// Leaf 特点</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">type</span> LeafOverview <span class="token keyword">struct</span> <span class="token punctuation">{</span></span>
<span class="line">    语言     <span class="token builtin">string</span></span>
<span class="line">    架构     <span class="token builtin">string</span></span>
<span class="line">    特点     <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span></span>
<span class="line">    优势     <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span></span>
<span class="line">    劣势     <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span></span>
<span class="line">    适用场景 <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">var</span> leaf <span class="token operator">=</span> LeafOverview<span class="token punctuation">{</span></span>
<span class="line">    语言<span class="token punctuation">:</span> <span class="token string">&quot;Go&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    架构<span class="token punctuation">:</span> <span class="token string">&quot;简洁实用&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    特点<span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span><span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;简洁易用&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;高性能&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;并发支持&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;模块化&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    优势<span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span><span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;性能优秀&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;内存占用小&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;学习成本低&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;部署简单&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    劣势<span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span><span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;功能较少&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;需要自己扩展&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;文档有限&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    适用场景<span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span><span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;棋牌游戏&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;休闲游戏&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;卡牌游戏&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Leaf 示例</span></span>
<span class="line"><span class="token keyword">func</span> <span class="token function">main</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    leaf<span class="token punctuation">.</span><span class="token function">Run</span><span class="token punctuation">(</span><span class="token string">&quot;leaf&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;conf/server.json&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 注册消息处理器</span></span>
<span class="line">    skeleton<span class="token punctuation">.</span><span class="token function">RegisterChan</span><span class="token punctuation">(</span>time<span class="token punctuation">.</span>Second<span class="token operator">*</span><span class="token number">10</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token comment">// 处理器</span></span>
<span class="line">        <span class="token function">new</span><span class="token punctuation">(</span>HelloHandler<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token comment">// 接收器</span></span>
<span class="line">        <span class="token operator">&amp;</span>HelloReceiver<span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token comment">// 发送器</span></span>
<span class="line">        <span class="token operator">&amp;</span>HelloSender<span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">)</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-go-world" tabindex="-1"><a class="header-anchor" href="#_5-2-go-world"><span>5.2 Go-World</span></a></h3><div class="language-go line-numbers-mode" data-highlighter="prismjs" data-ext="go"><pre><code class="language-go"><span class="line"><span class="token comment">// Go-World 特点</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">var</span> goWorld <span class="token operator">=</span> <span class="token keyword">struct</span> <span class="token punctuation">{</span></span>
<span class="line">    语言     <span class="token builtin">string</span></span>
<span class="line">    架构     <span class="token builtin">string</span></span>
<span class="line">    特点     <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">{</span></span>
<span class="line">    语言<span class="token punctuation">:</span> <span class="token string">&quot;Go&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    架构<span class="token punctuation">:</span> <span class="token string">&quot;ECS + 空间分割&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    特点<span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span><span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;ECS 架构&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;空间分区&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;AOI 支持&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;状态同步&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;高并发&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    适用场景<span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token builtin">string</span><span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;MMO&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;MOBA&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;大世界游戏&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Go-World 示例</span></span>
<span class="line"><span class="token keyword">type</span> PlayerEntity <span class="token keyword">struct</span> <span class="token punctuation">{</span></span>
<span class="line">    BaseEntity</span>
<span class="line">    Position Vector3</span>
<span class="line">    HP       <span class="token builtin">int</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">func</span> <span class="token punctuation">(</span>e <span class="token operator">*</span>PlayerEntity<span class="token punctuation">)</span> On <span class="token function">interest</span><span class="token punctuation">(</span>hooks <span class="token punctuation">[</span><span class="token punctuation">]</span>EntityHook<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 进入 AOI 时调用</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">func</span> <span class="token punctuation">(</span>e <span class="token operator">*</span>PlayerEntity<span class="token punctuation">)</span> <span class="token function">Update</span><span class="token punctuation">(</span>dt <span class="token builtin">float64</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 每帧更新</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、框架对比" tabindex="-1"><a class="header-anchor" href="#六、框架对比"><span>六、框架对比</span></a></h2><h3 id="_6-1-对比表" tabindex="-1"><a class="header-anchor" href="#_6-1-对比表"><span>6.1 对比表</span></a></h3><table><thead><tr><th>框架</th><th>语言</th><th>性能</th><th>学习成本</th><th>承载</th><th>适用场景</th></tr></thead><tbody><tr><td>KBEngine</td><td>C++/Python</td><td>中</td><td>低</td><td>5K</td><td>中小型 MMO</td></tr><tr><td>Skynet</td><td>C/Lua</td><td>高</td><td>高</td><td>10K+</td><td>高并发游戏</td></tr><tr><td>Pomelo</td><td>Node.js</td><td>中</td><td>低</td><td>3-5K</td><td>网页游戏</td></tr><tr><td>Leaf</td><td>Go</td><td>高</td><td>中</td><td>5K+</td><td>棋牌/休闲</td></tr><tr><td>Go-World</td><td>Go</td><td>高</td><td>中</td><td>10K+</td><td>MMO/MOBA</td></tr><tr><td>Photon</td><td>C#</td><td>高</td><td>中</td><td>10K+</td><td>商业项目</td></tr></tbody></table><h3 id="_6-2-选择建议" tabindex="-1"><a class="header-anchor" href="#_6-2-选择建议"><span>6.2 选择建议</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 框架选择建议</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">FrameworkSelector</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;框架选择器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">select</span><span class="token punctuation">(</span>requirements<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;根据需求选择&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 快速开发，中小规模</span></span>
<span class="line">        <span class="token keyword">if</span> requirements<span class="token punctuation">[</span><span class="token string">&#39;team_size&#39;</span><span class="token punctuation">]</span> <span class="token operator">&lt;</span> <span class="token number">5</span> <span class="token keyword">and</span> \\</span>
<span class="line">           requirements<span class="token punctuation">[</span><span class="token string">&#39;target_ccu&#39;</span><span class="token punctuation">]</span> <span class="token operator">&lt;</span> <span class="token number">5000</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token string">&#39;KBEngine&#39;</span> <span class="token keyword">if</span> requirements<span class="token punctuation">[</span><span class="token string">&#39;language&#39;</span><span class="token punctuation">]</span> <span class="token operator">==</span> <span class="token string">&#39;Python&#39;</span> <span class="token keyword">else</span> <span class="token string">&#39;Pomelo&#39;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 高性能，大规模</span></span>
<span class="line">        <span class="token keyword">if</span> requirements<span class="token punctuation">[</span><span class="token string">&#39;target_ccu&#39;</span><span class="token punctuation">]</span> <span class="token operator">&gt;</span> <span class="token number">10000</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token string">&#39;Skynet&#39;</span> <span class="token keyword">if</span> requirements<span class="token punctuation">[</span><span class="token string">&#39;language&#39;</span><span class="token punctuation">]</span> <span class="token operator">==</span> <span class="token string">&#39;Lua&#39;</span> <span class="token keyword">else</span> <span class="token string">&#39;Go-World&#39;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 简单游戏</span></span>
<span class="line">        <span class="token keyword">if</span> requirements<span class="token punctuation">[</span><span class="token string">&#39;complexity&#39;</span><span class="token punctuation">]</span> <span class="token operator">==</span> <span class="token string">&#39;simple&#39;</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token string">&#39;Leaf&#39;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 商业项目</span></span>
<span class="line">        <span class="token keyword">if</span> requirements<span class="token punctuation">[</span><span class="token string">&#39;budget&#39;</span><span class="token punctuation">]</span> <span class="token operator">==</span> <span class="token string">&#39;high&#39;</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token string">&#39;Photon (商业授权)&#39;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 默认推荐</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token string">&#39;KBEngine (易上手)&#39;</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line">selection <span class="token operator">=</span> FrameworkSelector<span class="token punctuation">.</span>select<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">    <span class="token string">&#39;team_size&#39;</span><span class="token punctuation">:</span> <span class="token number">10</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token string">&#39;target_ccu&#39;</span><span class="token punctuation">:</span> <span class="token number">5000</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token string">&#39;language&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;Python&#39;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token string">&#39;complexity&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;medium&#39;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token string">&#39;budget&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;medium&#39;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token comment"># 结果: KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、其他框架" tabindex="-1"><a class="header-anchor" href="#七、其他框架"><span>七、其他框架</span></a></h2><h3 id="_7-1-列表" tabindex="-1"><a class="header-anchor" href="#_7-1-列表"><span>7.1 列表</span></a></h3><table><thead><tr><th>框架</th><th>语言</th><th>特点</th></tr></thead><tbody><tr><td><strong>Colyseus</strong></td><td>Node.js</td><td>现代 API，Aris 框架</td></tr><tr><td><strong>Zoe</strong></td><td>Java</td><td>专注多人游戏</td></tr><tr><td><strong>RedDwarf</strong></td><td>Java</td><td>老牌框架，稳定</td></tr><tr><td><strong>SmartFox</strong></td><td>Java</td><td>商业方案</td></tr><tr><td><strong>Photon</strong></td><td>C#</td><td>Unity 友好，商业</td></tr><tr><td><strong>GameSparks</strong></td><td>-</td><td>BaaS 方案</td></tr></tbody></table><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="框架选择核心" tabindex="-1"><a class="header-anchor" href="#框架选择核心"><span>框架选择核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">框架选择 = 团队技能 + 项目需求 + 长期维护</span>
<span class="line">- 团队熟悉优先</span>
<span class="line">- 考虑承载需求</span>
<span class="line">- 评估学习成本</span>
<span class="line">- 关注社区活跃度</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine GitHub</a></li><li><a href="https://github.com/cloudwu/skynet" target="_blank" rel="noopener noreferrer">Skynet GitHub</a></li><li><a href="https://github.com/NetEase/pomelo" target="_blank" rel="noopener noreferrer">Pomelo GitHub</a></li><li><a href="https://github.com/name5566/leaf" target="_blank" rel="noopener noreferrer">Leaf GitHub</a></li></ul>`,43)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};