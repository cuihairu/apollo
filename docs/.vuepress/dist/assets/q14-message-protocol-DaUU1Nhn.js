import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q14-message-protocol.html","title":"Q14: 如何设计消息协议？Protobuf vs JSON vs 自定义协议？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q14-message-protocol.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q14-message-protocol.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q14-如何设计消息协议-protobuf-vs-json-vs-自定义协议" tabindex="-1"><a class="header-anchor" href="#q14-如何设计消息协议-protobuf-vs-json-vs-自定义协议"><span>Q14: 如何设计消息协议？Protobuf vs JSON vs 自定义协议？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对消息协议设计的理解：</p><ul><li>消息协议的核心需求</li><li>Protobuf、JSON、自定义协议的对比</li><li>KBEngine 的消息协议设计</li><li>不同场景的最佳选择</li></ul><hr><h2 id="一、消息协议需求" tabindex="-1"><a class="header-anchor" href="#一、消息协议需求"><span>一、消息协议需求</span></a></h2><h3 id="_1-1-核心需求" tabindex="-1"><a class="header-anchor" href="#_1-1-核心需求"><span>1.1 核心需求</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  消息协议的核心需求                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 性能需求                                                │</span>
<span class="line">│     ├── 序列化/反序列化速度                                 │</span>
<span class="line">│     ├── 数据大小（带宽占用）                                │</span>
<span class="line">│     └── 内存占用                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 开发效率                                                │</span>
<span class="line">│     ├── 可读性（调试方便）                                   │</span>
<span class="line">│     ├── 易用性（开发体验）                                   │</span>
<span class="line">│     └── 工具支持（代码生成）                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 兼容性                                                  │</span>
<span class="line">│     ├── 向后兼容（老版本能解析新版本数据）                    │</span>
<span class="line">│     ├── 跨语言支持                                          │</span>
<span class="line">│     └── 平台支持                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 安全性                                                  │</span>
<span class="line">│     ├── 数据验证                                           │</span>
<span class="line">│     ├── 防篡改                                             │</span>
<span class="line">│     └── 加密支持                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-协议对比概览" tabindex="-1"><a class="header-anchor" href="#_1-2-协议对比概览"><span>1.2 协议对比概览</span></a></h3><table><thead><tr><th>维度</th><th>JSON</th><th>XML</th><th>Protobuf</th><th>FlatBuffers</th><th>MsgPack</th></tr></thead><tbody><tr><td><strong>可读性</strong></td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐⭐</td><td>⭐</td><td>⭐</td><td>⭐⭐</td></tr><tr><td><strong>序列化速度</strong></td><td>⭐⭐</td><td>⭐</td><td>⭐⭐⭐⭐</td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐</td></tr><tr><td><strong>数据大小</strong></td><td>⭐</td><td>⭐</td><td>⭐⭐⭐⭐</td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐</td></tr><tr><td><strong>向后兼容</strong></td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐</td><td>⭐⭐⭐⭐</td></tr><tr><td><strong>跨语言</strong></td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐</td><td>⭐⭐⭐</td><td>⭐⭐⭐⭐</td></tr><tr><td><strong>工具支持</strong></td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐</td><td>⭐⭐⭐⭐</td><td>⭐⭐⭐</td><td>⭐⭐⭐</td></tr></tbody></table><hr><h2 id="二、json-协议" tabindex="-1"><a class="header-anchor" href="#二、json-协议"><span>二、JSON 协议</span></a></h2><h3 id="_2-1-json-示例" tabindex="-1"><a class="header-anchor" href="#_2-1-json-示例"><span>2.1 JSON 示例</span></a></h3><div class="language-json line-numbers-mode" data-highlighter="prismjs" data-ext="json"><pre><code class="language-json"><span class="line"><span class="token comment">// 玩家登录请求</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line">  <span class="token property">&quot;msgId&quot;</span><span class="token operator">:</span> <span class="token number">1001</span><span class="token punctuation">,</span></span>
<span class="line">  <span class="token property">&quot;seq&quot;</span><span class="token operator">:</span> <span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">  <span class="token property">&quot;timestamp&quot;</span><span class="token operator">:</span> <span class="token number">1640000000</span><span class="token punctuation">,</span></span>
<span class="line">  <span class="token property">&quot;data&quot;</span><span class="token operator">:</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token property">&quot;username&quot;</span><span class="token operator">:</span> <span class="token string">&quot;player1&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token property">&quot;password&quot;</span><span class="token operator">:</span> <span class="token string">&quot;hashed_password&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token property">&quot;version&quot;</span><span class="token operator">:</span> <span class="token string">&quot;1.0.0&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token property">&quot;device&quot;</span><span class="token operator">:</span> <span class="token punctuation">{</span></span>
<span class="line">      <span class="token property">&quot;type&quot;</span><span class="token operator">:</span> <span class="token string">&quot;ios&quot;</span><span class="token punctuation">,</span></span>
<span class="line">      <span class="token property">&quot;model&quot;</span><span class="token operator">:</span> <span class="token string">&quot;iPhone 12&quot;</span><span class="token punctuation">,</span></span>
<span class="line">      <span class="token property">&quot;osVersion&quot;</span><span class="token operator">:</span> <span class="token string">&quot;15.0&quot;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line">  <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 玩家移动请求</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line">  <span class="token property">&quot;msgId&quot;</span><span class="token operator">:</span> <span class="token number">2001</span><span class="token punctuation">,</span></span>
<span class="line">  <span class="token property">&quot;seq&quot;</span><span class="token operator">:</span> <span class="token number">2</span><span class="token punctuation">,</span></span>
<span class="line">  <span class="token property">&quot;timestamp&quot;</span><span class="token operator">:</span> <span class="token number">1640000100</span><span class="token punctuation">,</span></span>
<span class="line">  <span class="token property">&quot;data&quot;</span><span class="token operator">:</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token property">&quot;entityId&quot;</span><span class="token operator">:</span> <span class="token number">12345</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token property">&quot;position&quot;</span><span class="token operator">:</span> <span class="token punctuation">{</span></span>
<span class="line">      <span class="token property">&quot;x&quot;</span><span class="token operator">:</span> <span class="token number">100.5</span><span class="token punctuation">,</span></span>
<span class="line">      <span class="token property">&quot;y&quot;</span><span class="token operator">:</span> <span class="token number">0.0</span><span class="token punctuation">,</span></span>
<span class="line">      <span class="token property">&quot;z&quot;</span><span class="token operator">:</span> <span class="token number">200.3</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token property">&quot;rotation&quot;</span><span class="token operator">:</span> <span class="token number">45.0</span></span>
<span class="line">  <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-json-优缺点" tabindex="-1"><a class="header-anchor" href="#_2-2-json-优缺点"><span>2.2 JSON 优缺点</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">优点：</span>
<span class="line">✅ 可读性强 - 人类可读，调试方便</span>
<span class="line">✅ 易于使用 - 所有语言都有成熟库</span>
<span class="line">✅ 灵活性高 - 动态添加字段</span>
<span class="line">✅ Web 友好 - 前后端统一格式</span>
<span class="line"></span>
<span class="line">缺点：</span>
<span class="line">❌ 数据量大 - 大量重复的键名和引号</span>
<span class="line">❌ 解析慢 - 需要完整的解析过程</span>
<span class="line">❌ 无类型 - 类型信息丢失</span>
<span class="line">❌ 不支持二进制数据</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-性能测试" tabindex="-1"><a class="header-anchor" href="#_2-3-性能测试"><span>2.3 性能测试</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">测试：序列化 10000 次玩家对象</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  格式   │ 序列化(ms) │ 反序列化(ms) │ 数据大小(KB) │         │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│  JSON   │    45      │     52      │     125      │         │</span>
<span class="line">│  Protobuf│    8       │     12      │      35      │         │</span>
<span class="line">│  MsgPack│    12      │     18      │      42      │         │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、protobuf-协议" tabindex="-1"><a class="header-anchor" href="#三、protobuf-协议"><span>三、Protobuf 协议</span></a></h2><h3 id="_3-1-protobuf-示例" tabindex="-1"><a class="header-anchor" href="#_3-1-protobuf-示例"><span>3.1 Protobuf 示例</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// player.proto</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> game<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 玩家信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">PlayerInfo</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> username <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> level <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int64</span> exp <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> hp <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> max_hp <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">message</span> <span class="token class-name">Position</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token builtin">float</span> x <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token builtin">float</span> y <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token builtin">float</span> z <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token positional-class-name class-name">Position</span> position <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 登录请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">LoginRequest</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">string</span> username <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> password <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> version <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 登录响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">LoginResponse</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int32</span> code <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> message <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token positional-class-name class-name">PlayerInfo</span> player_info <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 移动请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">MoveRequest</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">uint64</span> entity_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token positional-class-name class-name">Position</span> position <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">float</span> rotation <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-protobuf-编码原理" tabindex="-1"><a class="header-anchor" href="#_3-2-protobuf-编码原理"><span>3.2 Protobuf 编码原理</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  Protobuf 编码结构                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  每个 Tag-Length-Value (TLV):                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  Tag (1-5 bytes) │ Length │ Value (n bytes)    │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Tag 结构：                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  Field Number (高位) │ Wire Type (低位)        │       │</span>
<span class="line">│  │  (field_id &gt;&gt; 3)      │ (field_id &amp; 0x07)      │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Wire Type:                                                │</span>
<span class="line">│  ├── 0: Varint (变长整数)                                  │</span>
<span class="line">│  ├── 1: 64-bit (固定 8 字节)                               │</span>
<span class="line">│  ├── 2: Length-delimited (字符串、嵌套消息)                 │</span>
<span class="line">│  ├── 5: 32-bit (固定 4 字节)                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  示例：int32 x = 150;                                      │</span>
<span class="line">│  ├── field_id = 1, wire_type = 0 (Varint)                 │</span>
<span class="line">│  ├── tag = (1 &lt;&lt; 3) | 0 = 0x08                            │</span>
<span class="line">│  ├── value = 150 = 0x96 0x01 (Varint 编码)                │</span>
<span class="line">│  └── 编码结果: 0x08 0x96 0x01 (3 bytes)                   │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-protobuf-优缺点" tabindex="-1"><a class="header-anchor" href="#_3-3-protobuf-优缺点"><span>3.3 Protobuf 优缺点</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">优点：</span>
<span class="line">✅ 高效 - 编码紧凑，解析快速</span>
<span class="line">✅ 跨语言 - 支持所有主流语言</span>
<span class="line">✅ 向后兼容 - 可安全添加/删除字段</span>
<span class="line">✅ 强类型 - 有完整的类型定义</span>
<span class="line">✅ 代码生成 - 自动生成序列化代码</span>
<span class="line"></span>
<span class="line">缺点：</span>
<span class="line">❌ 不可读 - 二进制格式，调试困难</span>
<span class="line">❌ 需要 .proto 文件 - 增加编译步骤</span>
<span class="line">❌ 不支持动态结构 - 修改需要重新编译</span>
<span class="line">❌ 学习成本 - 需要了解 proto 语法</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-消息协议" tabindex="-1"><a class="header-anchor" href="#四、kbengine-消息协议"><span>四、KBEngine 消息协议</span></a></h2><h3 id="_4-1-kbengine-协议格式" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-协议格式"><span>4.1 KBEngine 协议格式</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine 源码</a>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  KBEngine 消息协议                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  消息头 (Message Header)                         │       │</span>
<span class="line">│  │  ┌────────┬────────┬────────┬────────┐          │       │</span>
<span class="line">│  │  │MsgType │ MsgID  │ Length │ ...    │          │       │</span>
<span class="line">│  │  │(2bytes)│(2bytes)│(2bytes)│        │          │       │</span>
<span class="line">│  │  └────────┴────────┴────────┴────────┘          │       │</span>
<span class="line">│  ├─────────────────────────────────────────────────┤       │</span>
<span class="line">│  │  消息体 (Message Body)                           │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────────┐    │       │</span>
<span class="line">│  │  │  实体ID │ 参数列表 │ ...                 │    │       │</span>
<span class="line">│  │  │(4 bytes)│ (变长)   │                     │    │       │</span>
<span class="line">│  │  └─────────────────────────────────────────┘    │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  消息类型 (MsgType):                                        │</span>
<span class="line">│  ├── 0x01: 客户端 → 服务器                                  │</span>
<span class="line">│  ├── 0x02: 服务器 → 客户端                                  │</span>
<span class="line">│  ├── 0x03: 服务器内部                                       │</span>
<span class="line">│  └── 0x04: 广播消息                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-kbengine-源码实现" tabindex="-1"><a class="header-anchor" href="#_4-2-kbengine-源码实现"><span>4.2 KBEngine 源码实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 消息定义</span></span>
<span class="line"><span class="token comment">// src/server/network/message.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Message</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 消息 ID</span></span>
<span class="line">    MessageID id_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 消息类型</span></span>
<span class="line">    MessageType msgType_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 消息长度</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> length_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 实体 ID</span></span>
<span class="line">    EntityID entityID_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 参数列表</span></span>
<span class="line">    MemoryStream args_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 消息打包器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Bundle</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">MemoryStream</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 开始写入新消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">newMessage</span><span class="token punctuation">(</span>MessageID msgID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 写入消息 ID</span></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span><span class="token keyword">this</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> msgID<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 写入消息长度（占位）</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> length <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span><span class="token operator">*</span> lengthPos <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">uint16_t</span><span class="token operator">*</span><span class="token punctuation">)</span><span class="token punctuation">(</span><span class="token function">wpos</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>msgID<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span><span class="token keyword">this</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> length<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 结束消息写入</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">finishMessage</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 回填消息长度</span></span>
<span class="line">        <span class="token keyword">uint16_t</span><span class="token operator">*</span> lengthPos <span class="token operator">=</span> <span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token operator">*</span>lengthPos <span class="token operator">=</span> <span class="token function">wpos</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> startPos_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-kbengine-参数序列化" tabindex="-1"><a class="header-anchor" href="#_4-3-kbengine-参数序列化"><span>4.3 KBEngine 参数序列化</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 参数序列化</span></span>
<span class="line"><span class="token comment">// src/lib/python/Serialization/PyMemberDef.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PyMemberDef</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 类型枚举</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">DataType</span> <span class="token punctuation">{</span></span>
<span class="line">        UINT8<span class="token punctuation">,</span> UINT16<span class="token punctuation">,</span> UINT32<span class="token punctuation">,</span> UINT64<span class="token punctuation">,</span></span>
<span class="line">        INT8<span class="token punctuation">,</span> INT16<span class="token punctuation">,</span> INT32<span class="token punctuation">,</span> INT64<span class="token punctuation">,</span></span>
<span class="line">        FLOAT<span class="token punctuation">,</span> DOUBLE<span class="token punctuation">,</span></span>
<span class="line">        STRING<span class="token punctuation">,</span> UNICODE<span class="token punctuation">,</span></span>
<span class="line">        PYTHON<span class="token punctuation">,</span> BLOB<span class="token punctuation">,</span></span>
<span class="line">        ARRAY<span class="token punctuation">,</span> FIXED_DICT<span class="token punctuation">,</span></span>
<span class="line">        ENTITYCALL<span class="token punctuation">,</span> MAILBOX</span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 序列化</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addToStream</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">*</span> stream<span class="token punctuation">,</span> PyObject<span class="token operator">*</span> value<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>type_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> UINT8<span class="token operator">:</span></span>
<span class="line">                stream<span class="token operator">-&gt;</span><span class="token function">writeUint8</span><span class="token punctuation">(</span><span class="token function">PyLong_AsLong</span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> UINT16<span class="token operator">:</span></span>
<span class="line">                stream<span class="token operator">-&gt;</span><span class="token function">writeUint16</span><span class="token punctuation">(</span><span class="token function">PyLong_AsLong</span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> UINT32<span class="token operator">:</span></span>
<span class="line">                stream<span class="token operator">-&gt;</span><span class="token function">writeUint32</span><span class="token punctuation">(</span><span class="token function">PyLong_AsUnsignedLongMask</span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> STRING<span class="token operator">:</span></span>
<span class="line">                stream<span class="token operator">-&gt;</span><span class="token function">writeString</span><span class="token punctuation">(</span><span class="token function">PyUnicode_AsUTF8</span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token comment">// ... 其他类型</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 反序列化</span></span>
<span class="line">    PyObject<span class="token operator">*</span> <span class="token function">createFromStream</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">*</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>type_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> UINT8<span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token function">PyLong_FromLong</span><span class="token punctuation">(</span>stream<span class="token operator">-&gt;</span><span class="token function">readUint8</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">case</span> STRING<span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token function">PyUnicode_FromString</span><span class="token punctuation">(</span>stream<span class="token operator">-&gt;</span><span class="token function">readString</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">c_str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token comment">// ... 其他类型</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、自定义协议设计" tabindex="-1"><a class="header-anchor" href="#五、自定义协议设计"><span>五、自定义协议设计</span></a></h2><h3 id="_5-1-混合协议设计" tabindex="-1"><a class="header-anchor" href="#_5-1-混合协议设计"><span>5.1 混合协议设计</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              混合协议设计（推荐）                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  协议分层：                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  应用层 (Application)                            │       │</span>
<span class="line">│  │  ├── 业务逻辑 (Protobuf 定义)                     │       │</span>
<span class="line">│  │  └── 类型安全 (代码生成)                          │       │</span>
<span class="line">│  ├─────────────────────────────────────────────────┤       │</span>
<span class="line">│  │  传输层 (Transport)                               │       │</span>
<span class="line">│  │  ├── 消息 ID (uint16)                             │       │</span>
<span class="line">│  │  ├── 序列号 (uint16)                              │       │</span>
<span class="line">│  │  ├── 时间戳 (uint32)                              │       │</span>
<span class="line">│  │  └── 数据体 (Protobuf binary)                    │       │</span>
<span class="line">│  ├─────────────────────────────────────────────────┤       │</span>
<span class="line">│  │  网络层 (Network)                                 │       │</span>
<span class="line">│  │  └── TCP/UDP/KCP                                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-协议头设计" tabindex="-1"><a class="header-anchor" href="#_5-2-协议头设计"><span>5.2 协议头设计</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 统一消息头</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">pragma</span> <span class="token expression"><span class="token function">pack</span><span class="token punctuation">(</span>push<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span></span></span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">MessageHeader</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 魔数 (用于校验)</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> magic<span class="token punctuation">;</span>        <span class="token comment">// 0x4D534747 (&quot;MSGG&quot;)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 协议版本</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> version<span class="token punctuation">;</span>      <span class="token comment">// 当前版本 1</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 消息类型</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> msgType<span class="token punctuation">;</span>      <span class="token comment">// 请求/响应/推送</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 消息 ID</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> msgId<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 序列号 (用于匹配请求响应)</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 时间戳</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 会话 ID</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> sessionId<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 数据长度</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> bodyLength<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 校验和 (CRC16)</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> checksum<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 保留字段</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> reserved<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">pragma</span> <span class="token expression"><span class="token function">pack</span><span class="token punctuation">(</span>pop<span class="token punctuation">)</span></span></span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 消息类型枚举</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">MsgType</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint16_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 客户端请求</span></span>
<span class="line">    REQUEST <span class="token operator">=</span> <span class="token number">0x0001</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 服务器响应</span></span>
<span class="line">    RESPONSE <span class="token operator">=</span> <span class="token number">0x0002</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 服务器推送</span></span>
<span class="line">    PUSH <span class="token operator">=</span> <span class="token number">0x0003</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播消息</span></span>
<span class="line">    BROADCAST <span class="token operator">=</span> <span class="token number">0x0004</span><span class="token punctuation">,</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 消息 ID 定义</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">MessageID</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint16_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 认证相关 (1000-1999)</span></span>
<span class="line">    LOGIN_REQUEST <span class="token operator">=</span> <span class="token number">1001</span><span class="token punctuation">,</span></span>
<span class="line">    LOGIN_RESPONSE <span class="token operator">=</span> <span class="token number">1002</span><span class="token punctuation">,</span></span>
<span class="line">    LOGOUT_REQUEST <span class="token operator">=</span> <span class="token number">1003</span><span class="token punctuation">,</span></span>
<span class="line">    LOGOUT_RESPONSE <span class="token operator">=</span> <span class="token number">1004</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 玩家相关 (2000-2999)</span></span>
<span class="line">    PLAYER_INFO_REQUEST <span class="token operator">=</span> <span class="token number">2001</span><span class="token punctuation">,</span></span>
<span class="line">    PLAYER_INFO_RESPONSE <span class="token operator">=</span> <span class="token number">2002</span><span class="token punctuation">,</span></span>
<span class="line">    PLAYER_MOVE_REQUEST <span class="token operator">=</span> <span class="token number">2003</span><span class="token punctuation">,</span></span>
<span class="line">    PLAYER_MOVE_NOTIFY <span class="token operator">=</span> <span class="token number">2004</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 战斗相关 (3000-3999)</span></span>
<span class="line">    SKILL_CAST_REQUEST <span class="token operator">=</span> <span class="token number">3001</span><span class="token punctuation">,</span></span>
<span class="line">    SKILL_CAST_NOTIFY <span class="token operator">=</span> <span class="token number">3002</span><span class="token punctuation">,</span></span>
<span class="line">    DAMAGE_NOTIFY <span class="token operator">=</span> <span class="token number">3003</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// ... 更多消息</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-3-消息编解码器" tabindex="-1"><a class="header-anchor" href="#_5-3-消息编解码器"><span>5.3 消息编解码器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 消息编解码器</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MessageCodec</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 编码消息</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> <span class="token function">encode</span><span class="token punctuation">(</span>MsgType type<span class="token punctuation">,</span></span>
<span class="line">                               MessageID msgId<span class="token punctuation">,</span></span>
<span class="line">                               <span class="token keyword">uint16_t</span> sequence<span class="token punctuation">,</span></span>
<span class="line">                               <span class="token keyword">uint64_t</span> sessionId<span class="token punctuation">,</span></span>
<span class="line">                               <span class="token keyword">const</span> google<span class="token double-colon punctuation">::</span>protobuf<span class="token double-colon punctuation">::</span>Message<span class="token operator">&amp;</span> body<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 序列化消息体</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string bodyData<span class="token punctuation">;</span></span>
<span class="line">        body<span class="token punctuation">.</span><span class="token function">SerializeToString</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>bodyData<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 构建消息头</span></span>
<span class="line">        MessageHeader header<span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>magic <span class="token operator">=</span> <span class="token number">0x4D534747</span><span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>version <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>msgType <span class="token operator">=</span> <span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">uint16_t</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>type<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>msgId <span class="token operator">=</span> <span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">uint16_t</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>msgId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>sequence <span class="token operator">=</span> sequence<span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> <span class="token function">getTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>sessionId <span class="token operator">=</span> sessionId<span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>bodyLength <span class="token operator">=</span> bodyData<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>checksum <span class="token operator">=</span> <span class="token function">calculateChecksum</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>header<span class="token punctuation">,</span> bodyData<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        header<span class="token punctuation">.</span>reserved <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 组合完整消息</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> buffer<span class="token punctuation">;</span></span>
<span class="line">        buffer<span class="token punctuation">.</span><span class="token function">resize</span><span class="token punctuation">(</span><span class="token keyword">sizeof</span><span class="token punctuation">(</span>MessageHeader<span class="token punctuation">)</span> <span class="token operator">+</span> bodyData<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">memcpy</span><span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token operator">&amp;</span>header<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>MessageHeader<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">memcpy</span><span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>MessageHeader<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">               bodyData<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> bodyData<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> buffer<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 解码消息</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">decode</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> buffer<span class="token punctuation">,</span></span>
<span class="line">                MessageHeader<span class="token operator">&amp;</span> outHeader<span class="token punctuation">,</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> outBody<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>MessageHeader<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 解析消息头</span></span>
<span class="line">        <span class="token function">memcpy</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>outHeader<span class="token punctuation">,</span> buffer<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>MessageHeader<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 校验魔数</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>outHeader<span class="token punctuation">.</span>magic <span class="token operator">!=</span> <span class="token number">0x4D534747</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 校验长度</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">!=</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>MessageHeader<span class="token punctuation">)</span> <span class="token operator">+</span> outHeader<span class="token punctuation">.</span>bodyLength<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 校验校验和</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> calculatedChecksum <span class="token operator">=</span> <span class="token function">calculateChecksum</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token operator">&amp;</span>outHeader<span class="token punctuation">,</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span><span class="token function">string</span><span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>MessageHeader<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                       buffer<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>calculatedChecksum <span class="token operator">!=</span> outHeader<span class="token punctuation">.</span>checksum<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 提取消息体</span></span>
<span class="line">        outBody<span class="token punctuation">.</span><span class="token function">assign</span><span class="token punctuation">(</span>buffer<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>MessageHeader<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                       buffer<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 计算 CRC16 校验和</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">uint16_t</span> <span class="token function">calculateChecksum</span><span class="token punctuation">(</span><span class="token keyword">const</span> MessageHeader<span class="token operator">*</span> header<span class="token punctuation">,</span></span>
<span class="line">                                      <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> body<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 简化的 CRC16 计算</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> crc <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span> data <span class="token operator">=</span> <span class="token generic-function"><span class="token function">reinterpret_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">const</span> <span class="token keyword">uint8_t</span><span class="token operator">*</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>header<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        size_t len <span class="token operator">=</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>MessageHeader<span class="token punctuation">)</span> <span class="token operator">-</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> len<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            crc <span class="token operator">^=</span> <span class="token punctuation">(</span>data<span class="token punctuation">[</span>i<span class="token punctuation">]</span> <span class="token operator">&lt;&lt;</span> <span class="token number">8</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> j <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> j <span class="token operator">&lt;</span> <span class="token number">8</span><span class="token punctuation">;</span> <span class="token operator">++</span>j<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>crc <span class="token operator">&amp;</span> <span class="token number">0x8000</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    crc <span class="token operator">=</span> <span class="token punctuation">(</span>crc <span class="token operator">&lt;&lt;</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">^</span> <span class="token number">0x1021</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                    crc <span class="token operator">=</span> crc <span class="token operator">&lt;&lt;</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> crc<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、协议选择策略" tabindex="-1"><a class="header-anchor" href="#六、协议选择策略"><span>六、协议选择策略</span></a></h2><h3 id="_6-1-决策树" tabindex="-1"><a class="header-anchor" href="#_6-1-决策树"><span>6.1 决策树</span></a></h3>`,46),i(d,{code:`eJxLy8kvT85ILCpRCHHhUgCC4BIgJ/plQ+ez7pXPtnU8a1z/tLf/xbp1sQq6unYKgYbVL+c0vFjW+GTXrucbdz/tX/9i/W77Wi6w1kBDkJqaZzPW1yh4Bfv7RYOIWGSppxOW1SgEGlU/a1j+onkv0JhnG5vguo3ASp7saHjRsQZiALL4y9UzgFqNq5+27nm6bifIAFTdxhAL9uyqUQgoyi/JTypNi4YxoG6AKdlYo+BcWlySnxv9on3V03WznuzsfLKr58X+2U87tsVCjAPZDvYwiOGalwIMkKbnU1ZYKbxs7X2+d93jhsYXG5pfrJ8KZISnJkHMh9kG1gfmoGhcPePloomQEAVp377i6c7NT/s3QPRC3APWCWEia302r/FF+xZImAG1Pu/c+WxdFzAagN6HOre4pDInFeZWhbTMnBwr5bS05OSkZCRpmJOg8skWqWbJlkjycIuhCpKMLVNSkrgA71DqVQ==`}),o[1]||=e(`<h3 id="_6-2-场景推荐" tabindex="-1"><a class="header-anchor" href="#_6-2-场景推荐"><span>6.2 场景推荐</span></a></h3><table><thead><tr><th>场景</th><th>推荐协议</th><th>原因</th></tr></thead><tbody><tr><td><strong>配置文件</strong></td><td>JSON</td><td>可读、易编辑</td></tr><tr><td><strong>日志输出</strong></td><td>JSON</td><td>结构化、可读</td></tr><tr><td><strong>Web API</strong></td><td>JSON</td><td>前后端通用</td></tr><tr><td><strong>客户端通信</strong></td><td>Protobuf</td><td>高效、跨平台</td></tr><tr><td><strong>服务器内部</strong></td><td>自定义</td><td>极致性能</td></tr><tr><td><strong>调试接口</strong></td><td>JSON</td><td>可读、通用</td></tr><tr><td><strong>高频位置更新</strong></td><td>自定义</td><td>最小开销</td></tr></tbody></table><hr><h2 id="七、实战建议" tabindex="-1"><a class="header-anchor" href="#七、实战建议"><span>七、实战建议</span></a></h2><h3 id="_7-1-混合使用策略" tabindex="-1"><a class="header-anchor" href="#_7-1-混合使用策略"><span>7.1 混合使用策略</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">实际项目中的混合策略：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                   消息类型分类处理                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  高频消息 → 自定义二进制                                     │</span>
<span class="line">│  ├── 位置更新 (100ms/次)                                    │</span>
<span class="line">│  ├── 状态同步 (50ms/次)                                     │</span>
<span class="line">│  └── AOI 广播 (实时)                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  中频消息 → Protobuf                                         │</span>
<span class="line">│  ├── 技能释放                                              │</span>
<span class="line">│  ├── 伤害结算                                              │</span>
<span class="line">│  └── 物品操作                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">│  低频消息 → JSON                                            │</span>
<span class="line">│  ├── 登录认证                                              │</span>
<span class="line">│  ├── 配置加载                                              │</span>
<span class="line">│  └── GM 命令                                               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-2-版本兼容" tabindex="-1"><a class="header-anchor" href="#_7-2-版本兼容"><span>7.2 版本兼容</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// Protobuf 向后兼容示例</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">PlayerInfo</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>          <span class="token comment">// 不要删除已有字段</span></span>
<span class="line">    <span class="token builtin">string</span> username <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>           <span class="token comment">// 保留字段序号</span></span>
<span class="line">    <span class="token builtin">int32</span> level <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// V2 添加新字段</span></span>
<span class="line">    <span class="token builtin">int32</span> vip_level <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span>           <span class="token comment">// 新字段不影响老版本</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// V3 添加嵌套消息</span></span>
<span class="line">    <span class="token keyword">message</span> <span class="token class-name">Equipment</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token builtin">uint64</span> item_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token builtin">int32</span> slot <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">Equipment</span> equipments <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// V4 标记旧字段为废弃</span></span>
<span class="line">    <span class="token builtin">int32</span> deprecated_field <span class="token operator">=</span> <span class="token number">6</span> <span class="token punctuation">[</span><span class="token annotation">deprecated</span> <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// V5 添加新字段</span></span>
<span class="line">    <span class="token builtin">string</span> avatar_url <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="协议对比总结" tabindex="-1"><a class="header-anchor" href="#协议对比总结"><span>协议对比总结</span></a></h3><table><thead><tr><th>维度</th><th>JSON</th><th>Protobuf</th><th>自定义</th></tr></thead><tbody><tr><td><strong>开发效率</strong></td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐</td><td>⭐⭐</td></tr><tr><td><strong>运行效率</strong></td><td>⭐⭐</td><td>⭐⭐⭐⭐</td><td>⭐⭐⭐⭐⭐</td></tr><tr><td><strong>可调试性</strong></td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐</td><td>⭐⭐</td></tr><tr><td><strong>兼容性</strong></td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐⭐⭐⭐</td><td>⭐⭐</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 默认选择 Protobuf</span>
<span class="line">   - 平衡性能和开发效率</span>
<span class="line">   - 良好的工具支持</span>
<span class="line">   - 自然的版本兼容</span>
<span class="line"></span>
<span class="line">2. 特殊场景使用 JSON</span>
<span class="line">   - 配置文件</span>
<span class="line">   - 调试接口</span>
<span class="line">   - Web 兼容</span>
<span class="line"></span>
<span class="line">3. 极致性能考虑自定义</span>
<span class="line">   - 高频位置更新</span>
<span class="line">   - 有足够开发资源</span>
<span class="line">   - 愿意承担维护成本</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/tree/master/kbe/src/server/messages" target="_blank" rel="noopener noreferrer">KBEngine GitHub - 消息定义</a></li><li><a href="https://developers.google.com/protocol-buffers" target="_blank" rel="noopener noreferrer">Protobuf 官方文档</a></li><li><a href="https://google.github.io/flatbuffers/" target="_blank" rel="noopener noreferrer">FlatBuffers 对比</a></li><li><a href="https://msgpack.org/index.html" target="_blank" rel="noopener noreferrer">MessagePack 规范</a></li></ul>`,17)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};