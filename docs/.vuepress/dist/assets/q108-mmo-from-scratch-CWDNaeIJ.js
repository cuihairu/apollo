import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q108-mmo-from-scratch.html","title":"Q108: 从 0 到 1 搭建一个 MMO 服务器，你的思路是什么？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q108-mmo-from-scratch.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q108-mmo-from-scratch.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q108-从-0-到-1-搭建一个-mmo-服务器-你的思路是什么" tabindex="-1"><a class="header-anchor" href="#q108-从-0-到-1-搭建一个-mmo-服务器-你的思路是什么"><span>Q108: 从 0 到 1 搭建一个 MMO 服务器，你的思路是什么？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察系统设计能力：</p><ul><li>需求分析</li><li>架构设计</li><li>技术选型</li><li>实施路线</li></ul><hr><h2 id="一、需求分析" tabindex="-1"><a class="header-anchor" href="#一、需求分析"><span>一、需求分析</span></a></h2><h3 id="_1-1-核心需求" tabindex="-1"><a class="header-anchor" href="#_1-1-核心需求"><span>1.1 核心需求</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    MMO 服务器需求                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  功能需求:                                                   │</span>
<span class="line">│  ├── 玩家注册/登录                                           │</span>
<span class="line">│  ├── 角色创建/管理                                           │</span>
<span class="line">│  ├── 3D 场景移动                                             │</span>
<span class="line">│  ├── 聊天系统                                                │</span>
<span class="line">│  ├── 战斗系统                                                │</span>
<span class="line">│  ├── 背包/物品                                               │</span>
<span class="line">│  ├── 任务系统                                                │</span>
<span class="line">│  └── 公会系统                                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  非功能需求:                                                 │</span>
<span class="line">│  ├── 承载: 5000+ CCU                                        │</span>
<span class="line">│  ├── 延迟: &lt; 100ms (同区)                                   │</span>
<span class="line">│  ├── 可用性: 99.9%                                          │</span>
<span class="line">│  ├── 扩展性: 支持动态扩容                                     │</span>
<span class="line">│  └── 安全性: 防外挂、防刷                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、技术选型" tabindex="-1"><a class="header-anchor" href="#二、技术选型"><span>二、技术选型</span></a></h2><h3 id="_2-1-技术栈" tabindex="-1"><a class="header-anchor" href="#_2-1-技术栈"><span>2.1 技术栈</span></a></h3><table><thead><tr><th>层级</th><th>技术选择</th><th>理由</th></tr></thead><tbody><tr><td>网络层</td><td>TCP/KCP</td><td>可靠传输</td></tr><tr><td>协议层</td><td>Protobuf</td><td>高效序列化</td></tr><tr><td>通信模型</td><td>Actor</td><td>分布式友好</td></tr><tr><td>脚本语言</td><td>Python</td><td>快速开发</td></tr><tr><td>数据库</td><td>MySQL + Redis</td><td>持久化 + 缓存</td></tr><tr><td>消息队列</td><td>自建</td><td>游戏专用</td></tr></tbody></table><h3 id="_2-2-架构决策" tabindex="-1"><a class="header-anchor" href="#_2-2-架构决策"><span>2.2 架构决策</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 技术选型分析</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ArchitectureDecision</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;架构决策&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">choose_communication_model</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;选择通信模型&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;model&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;Actor&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;reasons&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;避免锁竞争&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;天然分布式&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;消息隔离&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;易于扩展&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;alternatives&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&#39;线程模型&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;锁竞争复杂&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;协程模型&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;阻塞风险&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;进程模型&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;通信成本高&#39;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">choose_serialization</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;选择序列化方案&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;primary&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;Protobuf&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;reasons&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;性能优异&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;跨语言&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;字段兼容&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;体积小&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;comparison&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&#39;Protobuf&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token string">&#39;speed&#39;</span><span class="token punctuation">:</span> <span class="token number">9</span><span class="token punctuation">,</span> <span class="token string">&#39;size&#39;</span><span class="token punctuation">:</span> <span class="token number">8</span><span class="token punctuation">,</span> <span class="token string">&#39;readability&#39;</span><span class="token punctuation">:</span> <span class="token number">5</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;JSON&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token string">&#39;speed&#39;</span><span class="token punctuation">:</span> <span class="token number">5</span><span class="token punctuation">,</span> <span class="token string">&#39;size&#39;</span><span class="token punctuation">:</span> <span class="token number">4</span><span class="token punctuation">,</span> <span class="token string">&#39;readability&#39;</span><span class="token punctuation">:</span> <span class="token number">10</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;MsgPack&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token string">&#39;speed&#39;</span><span class="token punctuation">:</span> <span class="token number">8</span><span class="token punctuation">,</span> <span class="token string">&#39;size&#39;</span><span class="token punctuation">:</span> <span class="token number">9</span><span class="token punctuation">,</span> <span class="token string">&#39;readability&#39;</span><span class="token punctuation">:</span> <span class="token number">4</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;FlatBuffers&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token string">&#39;speed&#39;</span><span class="token punctuation">:</span> <span class="token number">10</span><span class="token punctuation">,</span> <span class="token string">&#39;size&#39;</span><span class="token punctuation">:</span> <span class="token number">9</span><span class="token punctuation">,</span> <span class="token string">&#39;readability&#39;</span><span class="token punctuation">:</span> <span class="token number">3</span><span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、架构设计" tabindex="-1"><a class="header-anchor" href="#三、架构设计"><span>三、架构设计</span></a></h2><h3 id="_3-1-系统架构" tabindex="-1"><a class="header-anchor" href="#_3-1-系统架构"><span>3.1 系统架构</span></a></h3>`,17),i(d,{code:`eJxLy8kvT85ILCpRCHHiUgAC55zM1LyS6KfrFj3r2P589fpYLiRhBV1du5oQ54AaBZ/89My8aDDpWFCg8Hzm7qd7pz6b0xsLVg0WBylWCMnPTs2Lhkg/2b3keWcPFgNBamoU3BNLUssTK6OhtMLzvROftm6GGAgTAxnplFicCrQzGkorPJ2/6/nCBrjdyEqdU3NyQEqhtMKzHTuedfSDlYLVwowAqXVxitbwrQwO9FF4NnXDs951T3dN1oSYiKwqKDUlszhaA0wpPN8z+enaGUBVEB9BLQF5ydHfswYmAJEtLk1KL0osyFBQer5u4fMJbS+6mp437VQCy8Es8U0vgvkKyIRYDjMZJAc1EEXOxQkkAyYhgql5KQjPAUWRwwzuTpg4zIlwk6BBwQUAzNS7Dg==`}),o[1]||=e(`<h3 id="_3-2-组件职责" tabindex="-1"><a class="header-anchor" href="#_3-2-组件职责"><span>3.2 组件职责</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 组件定义</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ComponentArchitecture</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;组件架构&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    components <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&#39;LoginApp&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;职责&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token string">&#39;玩家登录验证&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;令牌发放&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;负载均衡&#39;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;技术&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;Python/Tornado&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;端口&#39;</span><span class="token punctuation">:</span> <span class="token number">8000</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;实例数&#39;</span><span class="token punctuation">:</span> <span class="token number">2</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;Gateway&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;职责&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token string">&#39;连接管理&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;消息转发&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;流量控制&#39;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;技术&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;C++/Epoll&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;端口&#39;</span><span class="token punctuation">:</span> <span class="token number">9000</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;实例数&#39;</span><span class="token punctuation">:</span> <span class="token number">4</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;BaseApp&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;职责&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token string">&#39;玩家数据管理&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;背包/任务&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;跨服功能&#39;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;技术&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;Python/Actor&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;端口&#39;</span><span class="token punctuation">:</span> <span class="token number">10000</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;实例数&#39;</span><span class="token punctuation">:</span> <span class="token number">4</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;CellApp&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;职责&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token string">&#39;场景逻辑&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;战斗系统&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;AOI 广播&#39;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;技术&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;Python/Actor&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;端口&#39;</span><span class="token punctuation">:</span> <span class="token number">11000</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;实例数&#39;</span><span class="token punctuation">:</span> <span class="token number">8</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;DBMgr&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;职责&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token string">&#39;数据库操作&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;数据缓存&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;异步保存&#39;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;技术&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;Python/MySQL&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;端口&#39;</span><span class="token punctuation">:</span> <span class="token number">12000</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;实例数&#39;</span><span class="token punctuation">:</span> <span class="token number">1</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、实施路线" tabindex="-1"><a class="header-anchor" href="#四、实施路线"><span>四、实施路线</span></a></h2><h3 id="_4-1-分阶段计划" tabindex="-1"><a class="header-anchor" href="#_4-1-分阶段计划"><span>4.1 分阶段计划</span></a></h3>`,5),i(d,{code:`eJxtkk1OwkAYhvecYg4ACa0kJu5MiLvGtcsK1ZAoJDgHwBAJPyKCAQEJ2BWYmBI1KlLQy/SbmWM409KhTZl00Xae732fTHuu5zGOIb5wDl8YSNOOERk1oW7CYAarErTazDKh2nGZrI6No0LxUscInfCV0LREOh1z966MDM4V8ggmS2qWmPVLemt3g67b1B4Rs0LGX+J5sw50JY7UpJpKJBV+xZGSyro8NFvMssAaOj+1EK/GkX6GjSISk/sefJjBhSKZmTBuBNG9ICqCQ4rkeQF/ZahPWHmjOLBh3aUfNrUnwcpTPr3J4YlK0utk0w6rvVPLpPeVEC0FxZxPw2hJBvMd9NZRZEccFwtSbQUcSbVPeo9Rx4x0FImq4rXS2gs8XO+gt478zj9yx7b5F99BS0efDjvykrduwBFuXp3VMJqTlY7C1ndk3zP3VxPjIVo6CjrS6qz6cNvzzqQ05bPei2CCIftElt8Hdw2oP5HPBpt3Q7TsE3Oi7x9c4huy`}),o[2]||=e(`<h3 id="_4-2-详细计划" tabindex="-1"><a class="header-anchor" href="#_4-2-详细计划"><span>4.2 详细计划</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">第一阶段：基础设施 (4 周)</span>
<span class="line">├── Week 1-2: 网络框架</span>
<span class="line">│   ├── TCP/KCP 通信</span>
<span class="line">│   ├── Protobuf 协议</span>
<span class="line">│   ├── 消息分发</span>
<span class="line">│   └── 心跳机制</span>
<span class="line">├── Week 3: Actor 模型</span>
<span class="line">│   ├── 消息队列</span>
<span class="line">│   ├── Actor 调度</span>
<span class="line">│   ├── 邮箱机制</span>
<span class="line">│   └── 远程调用</span>
<span class="line">└── Week 4: 基础组件</span>
<span class="line">    ├── 定时器</span>
<span class="line">    ├── 日志系统</span>
<span class="line">    ├── 配置管理</span>
<span class="line">    └── 监控接口</span>
<span class="line"></span>
<span class="line">第二阶段：核心功能 (6 周)</span>
<span class="line">├── Week 5-6: 登录系统</span>
<span class="line">│   ├── 账号验证</span>
<span class="line">│   ├── 令牌机制</span>
<span class="line">│   ├── 网关接入</span>
<span class="line">│   └── 负载均衡</span>
<span class="line">├── Week 7-8: 角色系统</span>
<span class="line">│   ├── 角色创建</span>
<span class="line">│   ├── 属性系统</span>
<span class="line">│   ├── 数据持久化</span>
<span class="line">│   └── 缓存管理</span>
<span class="line">└── Week 9-10: 场景系统</span>
<span class="line">    ├── 空间管理</span>
<span class="line">    ├── 位置同步</span>
<span class="line">    ├── AOI 实现</span>
<span class="line">    └── 场景切换</span>
<span class="line"></span>
<span class="line">第三阶段：游戏功能 (8 周)</span>
<span class="line">├── Week 11-13: 战斗系统</span>
<span class="line">│   ├── 技能系统</span>
<span class="line">│   ├── 伤害计算</span>
<span class="line">│   ├── 状态效果</span>
<span class="line">│   └── AI 行为</span>
<span class="line">├── Week 14-15: 物品系统</span>
<span class="line">│   ├── 背包管理</span>
<span class="line">│   ├── 物品使用</span>
<span class="line">│   ├── 交易系统</span>
<span class="line">│   └── 拍卖行</span>
<span class="line">└── Week 16-18: 社交系统</span>
<span class="line">    ├── 好友系统</span>
<span class="line">    ├── 聊天系统</span>
<span class="line">    ├── 公会系统</span>
<span class="line">    └── 组队系统</span>
<span class="line"></span>
<span class="line">第四阶段：优化测试 (5 周)</span>
<span class="line">├── Week 19-20: 性能优化</span>
<span class="line">│   ├── 内存优化</span>
<span class="line">│   ├── 网络优化</span>
<span class="line">│   ├── 数据库优化</span>
<span class="line">│   └── 并发优化</span>
<span class="line">└── Week 21-23: 测试发布</span>
<span class="line">    ├── 单元测试</span>
<span class="line">    ├── 集成测试</span>
<span class="line">    ├── 压力测试</span>
<span class="line">    └── 试运行</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、关键设计" tabindex="-1"><a class="header-anchor" href="#五、关键设计"><span>五、关键设计</span></a></h2><h3 id="_5-1-消息协议" tabindex="-1"><a class="header-anchor" href="#_5-1-消息协议"><span>5.1 消息协议</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 消息协议定义</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 基础消息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">BaseMessage</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">uint32</span> msg_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>        <span class="token comment">// 消息 ID</span></span>
<span class="line">    <span class="token builtin">uint64</span> sequence <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>      <span class="token comment">// 序列号</span></span>
<span class="line">    <span class="token builtin">uint64</span> timestamp <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>     <span class="token comment">// 时间戳</span></span>
<span class="line">    <span class="token builtin">bytes</span> body <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span>           <span class="token comment">// 消息体</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 登录请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">LoginRequest</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">string</span> account <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> password <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> client_version <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 登录响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">LoginResponse</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int32</span> code <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> message <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> token <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">repeated</span> <span class="token builtin">string</span> servers <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span>  <span class="token comment">// 可选服务器</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 位置同步</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">PositionUpdate</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">uint64</span> entity_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">float</span> x <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">float</span> y <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">float</span> z <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">float</span> yaw <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">uint32</span> timestamp <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-数据模型" tabindex="-1"><a class="header-anchor" href="#_5-2-数据模型"><span>5.2 数据模型</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 数据模型设计</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PlayerSchema</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;玩家数据模式&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 数据库表结构</span></span>
<span class="line">    TABLE_SQL <span class="token operator">=</span> <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    CREATE TABLE players (</span>
<span class="line">        id BIGINT PRIMARY KEY AUTO_INCREMENT,</span>
<span class="line">        account_id BIGINT NOT NULL,</span>
<span class="line">        name VARCHAR(32) NOT NULL UNIQUE,</span>
<span class="line">        level INT DEFAULT 1,</span>
<span class="line">        exp BIGINT DEFAULT 0,</span>
<span class="line">        gold BIGINT DEFAULT 0,</span>
<span class="line"></span>
<span class="line">        -- 位置</span>
<span class="line">        space_id INT,</span>
<span class="line">        position_x FLOAT,</span>
<span class="line">        position_y FLOAT,</span>
<span class="line">        position_z FLOAT,</span>
<span class="line"></span>
<span class="line">        -- 时间戳</span>
<span class="line">        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,</span>
<span class="line">        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,</span>
<span class="line">        last_login_at TIMESTAMP,</span>
<span class="line"></span>
<span class="line">        -- 索引</span>
<span class="line">        INDEX idx_account (account_id),</span>
<span class="line">        INDEX idx_level (level),</span>
<span class="line">        INDEX idx_space (space_id)</span>
<span class="line">    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># Redis 缓存结构</span></span>
<span class="line">    CACHE_KEY <span class="token operator">=</span> <span class="token string">&quot;player:{player_id}&quot;</span></span>
<span class="line">    CACHE_FIELDS <span class="token operator">=</span> <span class="token punctuation">[</span></span>
<span class="line">        <span class="token string">&#39;id&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;account_id&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;name&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;level&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;exp&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;gold&#39;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;space_id&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;position_x&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;position_y&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;position_z&#39;</span></span>
<span class="line">    <span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ItemSchema</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;物品数据模式&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    TABLE_SQL <span class="token operator">=</span> <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    CREATE TABLE player_items (</span>
<span class="line">        id BIGINT PRIMARY KEY AUTO_INCREMENT,</span>
<span class="line">        player_id BIGINT NOT NULL,</span>
<span class="line">        item_id INT NOT NULL,</span>
<span class="line">        count INT DEFAULT 1,</span>
<span class="line">        quality INT DEFAULT 1,</span>
<span class="line"></span>
<span class="line">        -- 装备属性</span>
<span class="line">        enchant_level INT DEFAULT 0,</span>
<span class="line">        extra_data JSON,</span>
<span class="line"></span>
<span class="line">        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,</span>
<span class="line">        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,</span>
<span class="line"></span>
<span class="line">        INDEX idx_player (player_id),</span>
<span class="line">        INDEX idx_item (item_id)</span>
<span class="line">    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、核心代码框架" tabindex="-1"><a class="header-anchor" href="#六、核心代码框架"><span>六、核心代码框架</span></a></h2><h3 id="_6-1-actor-实现" tabindex="-1"><a class="header-anchor" href="#_6-1-actor-实现"><span>6.1 Actor 实现</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Actor 框架</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> queue</span>
<span class="line"><span class="token keyword">import</span> threading</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Actor</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;Actor 基类&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> actor_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>actor_id <span class="token operator">=</span> actor_id</span>
<span class="line">        self<span class="token punctuation">.</span>mailbox <span class="token operator">=</span> queue<span class="token punctuation">.</span>Queue<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>running <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line">        self<span class="token punctuation">.</span>thread <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">start</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;启动 Actor&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>running <span class="token operator">=</span> <span class="token boolean">True</span></span>
<span class="line">        self<span class="token punctuation">.</span>thread <span class="token operator">=</span> threading<span class="token punctuation">.</span>Thread<span class="token punctuation">(</span>target<span class="token operator">=</span>self<span class="token punctuation">.</span>_loop<span class="token punctuation">,</span> daemon<span class="token operator">=</span><span class="token boolean">True</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>thread<span class="token punctuation">.</span>start<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">stop</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;停止 Actor&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>running <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>thread<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>thread<span class="token punctuation">.</span>join<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">_loop</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;消息循环&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">while</span> self<span class="token punctuation">.</span>running<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                message <span class="token operator">=</span> self<span class="token punctuation">.</span>mailbox<span class="token punctuation">.</span>get<span class="token punctuation">(</span>timeout<span class="token operator">=</span><span class="token number">0.1</span><span class="token punctuation">)</span></span>
<span class="line">                self<span class="token punctuation">.</span>handle_message<span class="token punctuation">(</span>message<span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">except</span> queue<span class="token punctuation">.</span>Empty<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">continue</span></span>
<span class="line">            <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">                ERROR_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Actor </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>actor_id<span class="token punctuation">}</span></span><span class="token string"> error: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">send</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;发送消息&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>mailbox<span class="token punctuation">.</span>put<span class="token punctuation">(</span>message<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">handle_message</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;处理消息 (子类实现)&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">raise</span> NotImplementedError</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">ask</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> message<span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">5</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;请求-响应模式&quot;&quot;&quot;</span></span>
<span class="line">        response_queue <span class="token operator">=</span> queue<span class="token punctuation">.</span>Queue<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 添加回调地址</span></span>
<span class="line">        message<span class="token punctuation">.</span>reply_to <span class="token operator">=</span> response_queue</span>
<span class="line"></span>
<span class="line">        self<span class="token punctuation">.</span>send<span class="token punctuation">(</span>message<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> response_queue<span class="token punctuation">.</span>get<span class="token punctuation">(</span>timeout<span class="token operator">=</span>timeout<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">except</span> queue<span class="token punctuation">.</span>Empty<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">raise</span> TimeoutError<span class="token punctuation">(</span><span class="token string">&quot;Actor response timeout&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 远程 Actor</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RemoteActor</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;远程 Actor 代理&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> actor_id<span class="token punctuation">,</span> address<span class="token punctuation">,</span> port<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>actor_id <span class="token operator">=</span> actor_id</span>
<span class="line">        self<span class="token punctuation">.</span>address <span class="token operator">=</span> address</span>
<span class="line">        self<span class="token punctuation">.</span>port <span class="token operator">=</span> port</span>
<span class="line">        self<span class="token punctuation">.</span>transport <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">send</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;发送远程消息&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 通过网络发送</span></span>
<span class="line">        self<span class="token punctuation">.</span>transport<span class="token punctuation">.</span>send<span class="token punctuation">(</span>self<span class="token punctuation">.</span>actor_id<span class="token punctuation">,</span> message<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">ask</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> message<span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">5</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;远程请求&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 实现请求-响应</span></span>
<span class="line">        <span class="token keyword">pass</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="从-0-到-1-核心要点" tabindex="-1"><a class="header-anchor" href="#从-0-到-1-核心要点"><span>从 0 到 1 核心要点</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">MMO 服务器搭建 = 需求分析 + 架构设计 + 分阶段实施</span>
<span class="line">- 需求优先，避免过度设计</span>
<span class="line">- 技术选型考虑团队和场景</span>
<span class="line">- Actor 模型简化并发</span>
<span class="line">- 分阶段迭代验证</span>
<span class="line">- 性能和安全是持续工作</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.gamedev.net/blogs/entry/2264963-mmo-server-security-the-unspoken-challenges/" target="_blank" rel="noopener noreferrer">MMO Server Architecture</a></li><li><a href="https://www.gameprogrammingpatterns.com/game-server-patterns.html" target="_blank" rel="noopener noreferrer">Game Server Patterns</a></li></ul>`,19)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};