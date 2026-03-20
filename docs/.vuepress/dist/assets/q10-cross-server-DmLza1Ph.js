import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q10-cross-server.html","title":"Q10: 如何实现跨服功能（如跨服战场、跨服聊天）？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q10-cross-server.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q10-cross-server.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q10-如何实现跨服功能-如跨服战场、跨服聊天" tabindex="-1"><a class="header-anchor" href="#q10-如何实现跨服功能-如跨服战场、跨服聊天"><span>Q10: 如何实现跨服功能（如跨服战场、跨服聊天）？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对跨服功能设计的理解：</p><ul><li>跨服功能的常见场景</li><li>不同跨服场景的实现方案</li><li>KBEngine 如何支持跨服</li><li>跨服架构的权衡与挑战</li></ul><hr><h2 id="一、跨服场景分析" tabindex="-1"><a class="header-anchor" href="#一、跨服场景分析"><span>一、跨服场景分析</span></a></h2><h3 id="_1-1-常见跨服功能" tabindex="-1"><a class="header-anchor" href="#_1-1-常见跨服功能"><span>1.1 常见跨服功能</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      跨服功能分类                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 跨服聊天                                                │</span>
<span class="line">│     ├── 全服公告                                           │</span>
<span class="line">│     ├── 世界频道                                           │</span>
<span class="line">│     └── 跨服私聊                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 跨服社交                                                │</span>
<span class="line">│     ├── 好友系统                                           │</span>
<span class="line">│     ├── 公会系统                                           │</span>
<span class="line">│     └── 跨服组队                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 跨服玩法                                                │</span>
<span class="line">│     ├── 跨服战场                                           │</span>
<span class="line">│     ├── 跨服竞技场                                         │</span>
<span class="line">│     ├── 跨服副本                                           │</span>
<span class="line">│     └── 跨服活动                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 跨服交易                                                │</span>
<span class="line">│     ├── 跨服拍卖行                                         │</span>
<span class="line">│     ├── 跨服商城                                           │</span>
<span class="line">│     └── 跨服交易                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 跨服排行                                                │</span>
<span class="line">│     ├── 全服排行榜                                         │</span>
<span class="line">│     ├── 战力排行                                           │</span>
<span class="line">│     └── 成就排行                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-跨服需求分析" tabindex="-1"><a class="header-anchor" href="#_1-2-跨服需求分析"><span>1.2 跨服需求分析</span></a></h3><table><thead><tr><th>功能</th><th>实时性要求</th><th>一致性要求</th><th>实现复杂度</th></tr></thead><tbody><tr><td><strong>跨服聊天</strong></td><td>低（秒级）</td><td>低</td><td>简单</td></tr><tr><td><strong>跨服好友</strong></td><td>中</td><td>高</td><td>中等</td></tr><tr><td><strong>跨服组队</strong></td><td>高</td><td>高</td><td>复杂</td></tr><tr><td><strong>跨服战场</strong></td><td>高</td><td>中</td><td>复杂</td></tr><tr><td><strong>跨服排行</strong></td><td>低</td><td>高</td><td>中等</td></tr><tr><td><strong>跨服交易</strong></td><td>中</td><td>高</td><td>复杂</td></tr></tbody></table><hr><h2 id="二、跨服架构设计" tabindex="-1"><a class="header-anchor" href="#二、跨服架构设计"><span>二、跨服架构设计</span></a></h2><h3 id="_2-1-整体架构图" tabindex="-1"><a class="header-anchor" href="#_2-1-整体架构图"><span>2.1 整体架构图</span></a></h3>`,13),i(d,{code:`eJyVkk9LwmAcx+++igeD0IPIti6FCG3H8OCf2+jwTLccPqg8m0VH6SJSHuqQkRCCkEHRrULI3oyb9i56/rj5MFvQDnu23/fzPJ9nvz0Wap1V6xC7oKImALmcjnGCYbsOyiY+NbGkJ/3Rldcfe3dTIIGUd/9J3tPJYwbTSz2UdBU6Jmy3gbQpa6SsmQhFyoQGuUwmT3NWNJu1xG9iWRTLILWcD7bEciiWRbEcioUyoQOx/KdYEcUKSPnPj1tiJRQrolgJxUKZ0IFYiRFruOU43K4nV+9T4lt8vHhfF4JUq0NX59mq2/cmT3yTG6DcqtoQrZHlZL6YTaKICl0XmWvE7w290SyKlGCzEQCD69X4MgpUMKwFS1DF8CZKFIq6/9bzu6/fwwevd5szcDZ/BK0GzJagYdhuoSjozJrt6Ck2AA11HNfEaR6HbaJHZpf9P3pXAOsl6UZsyDsRG/MuxMa0A7Eh+3q+LboFknAZRdiy5IEuQAaGskmF4j8nsHasz4h7ThjhgADLRuhgx7Ks/epe4gd09TXk`}),o[1]||=e(`<h3 id="_2-2-通信方式" tabindex="-1"><a class="header-anchor" href="#_2-2-通信方式"><span>2.2 通信方式</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">跨服通信的三种方式：</span>
<span class="line"></span>
<span class="line">1. 直接 TCP 连接</span>
<span class="line">   ┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">   │                                                             │</span>
<span class="line">   │   Server A                          Server B               │</span>
<span class="line">   │  ┌────────┐  ─────────────────────►  ┌────────┐           │</span>
<span class="line">   │  │Client  │      TCP 直连             │Client  │           │</span>
<span class="line">   │  └────────┘                          └────────┘           │</span>
<span class="line">   │                                                             │</span>
<span class="line">   │  优点：实时性好                                             │</span>
<span class="line">   │  缺点：连接管理复杂                                         │</span>
<span class="line">   └─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">2. 消息队列</span>
<span class="line">   ┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">   │                                                             │</span>
<span class="line">   │   Server A              MQ              Server B           │</span>
<span class="line">   │  ┌────────┐   ─────────►┌────┐◄─────────  ┌────────┐      │</span>
<span class="line">   │  │Client  │             │Kafka│             │Client  │      │</span>
<span class="line">   │  └────────┘   ◄─────────└────┘───────────  └────────┘      │</span>
<span class="line">   │                                                             │</span>
<span class="line">   │  优点：解耦、可靠                                            │</span>
<span class="line">   │  缺点：有延迟                                               │</span>
<span class="line">   └─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">3. 中心服务转发</span>
<span class="line">   ┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">   │                                                             │</span>
<span class="line">   │   Server A                                        Server B  │</span>
<span class="line">   │  ┌────────┐                                        ┌────────┐│</span>
<span class="line">   │  │Client  │◄───────────┐         ┌────────────►│Client  ││</span>
<span class="line">   │  └────────┘            │         │             └────────┘│</span>
<span class="line">   │                         ▼         ▼                       │</span>
<span class="line">   │                  ┌───────────────────┐                     │</span>
<span class="line">   │                  │   跨服中心服务    │                     │</span>
<span class="line">   │                  └───────────────────┘                     │</span>
<span class="line">   │                                                             │</span>
<span class="line">   │  优点：统一管理                                             │</span>
<span class="line">   │  缺点：中心服务是瓶颈                                       │</span>
<span class="line">   └─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、具体功能实现" tabindex="-1"><a class="header-anchor" href="#三、具体功能实现"><span>三、具体功能实现</span></a></h2><h3 id="_3-1-跨服聊天" tabindex="-1"><a class="header-anchor" href="#_3-1-跨服聊天"><span>3.1 跨服聊天</span></a></h3><h4 id="架构设计" tabindex="-1"><a class="header-anchor" href="#架构设计"><span>架构设计</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    跨服聊天架构                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│   Server A          Server B          Server C              │</span>
<span class="line">│  ┌────────┐        ┌────────┐        ┌────────┐            │</span>
<span class="line">│  │Player1 │        │Player2 │        │Player3 │            │</span>
<span class="line">│  │说话:&quot;   │        │说话:&quot;   │        │说话:&quot;   │            │</span>
<span class="line">│  │大家好&quot;  │        │大家好&quot;  │        │大家好&quot;  │            │</span>
<span class="line">│  └────┬───┘        └────┬───┘        └────┬───┘            │</span>
<span class="line">│       │                 │                 │                 │</span>
<span class="line">│       └─────────────────┼─────────────────┘                 │</span>
<span class="line">│                         ▼                                   │</span>
<span class="line">│                  ┌─────────────┐                            │</span>
<span class="line">│                  │  聊天服务   │                            │</span>
<span class="line">│                  │  ChatApp    │                            │</span>
<span class="line">│                  └──────┬──────┘                            │</span>
<span class="line">│                         │                                   │</span>
<span class="line">│                  ┌──────┴──────┐                            │</span>
<span class="line">│                  │             │                            │</span>
<span class="line">│                  ▼             ▼                            │</span>
<span class="line">│            ┌─────────┐  ┌─────────┐                         │</span>
<span class="line">│            │  Redis  │  │  Kafka  │                         │</span>
<span class="line">│            │ Pub/Sub │  │  Topic  │                         │</span>
<span class="line">│            └─────────┘  └─────────┘                         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="实现代码" tabindex="-1"><a class="header-anchor" href="#实现代码"><span>实现代码</span></a></h4><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 跨服聊天服务实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ChatService</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    跨服聊天服务</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># Redis 发布订阅</span></span>
<span class="line">        self<span class="token punctuation">.</span>redis_pubsub <span class="token operator">=</span> RedisPubSub<span class="token punctuation">(</span><span class="token string">&quot;chat:cross_server&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 消息队列</span></span>
<span class="line">        self<span class="token punctuation">.</span>message_queue <span class="token operator">=</span> KafkaTopic<span class="token punctuation">(</span><span class="token string">&quot;cross_server_chat&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 服务器注册表</span></span>
<span class="line">        self<span class="token punctuation">.</span>server_registry <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">on_chat_message</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> channel_type<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        处理跨服聊天消息</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 1. 验证玩家</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>validate_player<span class="token punctuation">(</span>server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 2. 敏感词过滤</span></span>
<span class="line">        filtered_message <span class="token operator">=</span> self<span class="token punctuation">.</span>filter_sensitive_words<span class="token punctuation">(</span>message<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 3. 构建跨服消息</span></span>
<span class="line">        cross_msg <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;server_id&quot;</span><span class="token punctuation">:</span> server_id<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;player_id&quot;</span><span class="token punctuation">:</span> player_id<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;player_name&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>get_player_name<span class="token punctuation">(</span>server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;channel&quot;</span><span class="token punctuation">:</span> channel_type<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;message&quot;</span><span class="token punctuation">:</span> filtered_message<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;timestamp&quot;</span><span class="token punctuation">:</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 4. 广播到所有服务器</span></span>
<span class="line">        self<span class="token punctuation">.</span>broadcast_to_servers<span class="token punctuation">(</span>cross_msg<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">broadcast_to_servers</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        广播消息到所有服务器</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 方式 1: Redis Pub/Sub</span></span>
<span class="line">        self<span class="token punctuation">.</span>redis_pubsub<span class="token punctuation">.</span>publish<span class="token punctuation">(</span>message<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 方式 2: Kafka Topic</span></span>
<span class="line">        self<span class="token punctuation">.</span>message_queue<span class="token punctuation">.</span>send<span class="token punctuation">(</span>message<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 方式 3: 直接转发（如果服务器列表不大）</span></span>
<span class="line">        <span class="token keyword">for</span> server_id<span class="token punctuation">,</span> server_info <span class="token keyword">in</span> self<span class="token punctuation">.</span>server_registry<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>send_to_server<span class="token punctuation">(</span>server_info<span class="token punctuation">[</span><span class="token string">&quot;host&quot;</span><span class="token punctuation">]</span><span class="token punctuation">,</span> message<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="频道设计" tabindex="-1"><a class="header-anchor" href="#频道设计"><span>频道设计</span></a></h4><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 跨服聊天频道设计</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CrossServerChannel</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    跨服聊天频道</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token comment"># 频道类型</span></span>
<span class="line">    CHANNEL_WORLD <span class="token operator">=</span> <span class="token number">1</span>        <span class="token comment"># 世界频道（全服）</span></span>
<span class="line">    CHANNEL_SYSTEM <span class="token operator">=</span> <span class="token number">2</span>       <span class="token comment"># 系统公告</span></span>
<span class="line">    CHANNEL_GUILD <span class="token operator">=</span> <span class="token number">3</span>        <span class="token comment"># 公会频道（跨服公会）</span></span>
<span class="line">    CHANNEL_TEAM <span class="token operator">=</span> <span class="token number">4</span>         <span class="token comment"># 队伍频道（跨服组队）</span></span>
<span class="line">    CHANNEL_PRIVATE <span class="token operator">=</span> <span class="token number">5</span>      <span class="token comment"># 私聊（需要好友关系）</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">can_send_to_channel</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> player<span class="token punctuation">,</span> channel_type<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        检查玩家是否可以发送到指定频道</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 等级限制</span></span>
<span class="line">        <span class="token keyword">if</span> player<span class="token punctuation">.</span>level <span class="token operator">&lt;</span> <span class="token number">10</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 冷却时间</span></span>
<span class="line">        <span class="token keyword">if</span> channel_type <span class="token operator">==</span> self<span class="token punctuation">.</span>CHANNEL_WORLD<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>check_cooldown<span class="token punctuation">(</span>player<span class="token punctuation">,</span> <span class="token number">30</span><span class="token punctuation">)</span><span class="token punctuation">:</span>  <span class="token comment"># 30秒冷却</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># VIP 权限</span></span>
<span class="line">        <span class="token keyword">if</span> channel_type <span class="token operator">==</span> self<span class="token punctuation">.</span>CHANNEL_SYSTEM<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> player<span class="token punctuation">.</span>is_vip<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">True</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-跨服战场" tabindex="-1"><a class="header-anchor" href="#_3-2-跨服战场"><span>3.2 跨服战场</span></a></h3><h4 id="架构设计-1" tabindex="-1"><a class="header-anchor" href="#架构设计-1"><span>架构设计</span></a></h4>`,13),i(d,{code:`eJyVkM1KAlEYhvddxVnWIkH7WUgEM7Q1hLmCgxxKKLVxCtpZJGj+lBRq+TO4kAxCjUDRKb0Zv+O46hY6Z874gzObzvI87/d+7/fGycUliYTIURifqPh8A7EXw6oWDoVjOKKhoBfhOAqe4WuibipEvSIqkrYcMlniMhnHCY7FkOTkAcE17YwEsBY6JapToyw1YpMzjc+ZRt7asHTHUY2gKP8Jev3I60HT/Du0e2anTz9vzX6LVnM0VYbq0FIHvduHh7LkR+a4AskmZAezZG5W1iFVsrgscR5g3JoUXHitr+MqnwcJyfTLmBo6dAyaHkFnQNMZYRew7aje5ERoX+9ExhUJS8R5qiskv983btftsHWpChhDx10it+K3uSDQrk9GdhBlwWvwloFscXVY2Z5nEHPVLtQSbgF2F/Va9a0H4BLz5wMeCuwSR+tsuzmu07w9xuu9b8BLy7HIx2JaHcJjlpZ6k4Eu1gkv37+8WKI9D7+LFkvMxWxk7UQHto1As4Rhjgr2AlfkYrw/N54aT7SmrxQtqrRJvbrW0PgZKnXI68vUi0l+uwv/A3PPmeQ=`}),o[2]||=e(`<h4 id="实现代码-1" tabindex="-1"><a class="header-anchor" href="#实现代码-1"><span>实现代码</span></a></h4><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 跨服战场匹配器</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CrossServerBattleMatcher</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    跨服战场匹配器</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 匹配队列（按战力分段）</span></span>
<span class="line">        self<span class="token punctuation">.</span>match_queues <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;low&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token punctuation">,</span>      <span class="token comment"># 战力 0-5000</span></span>
<span class="line">            <span class="token string">&quot;mid&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token punctuation">,</span>      <span class="token comment"># 战力 5000-10000</span></span>
<span class="line">            <span class="token string">&quot;high&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token punctuation">,</span>     <span class="token comment"># 战力 10000+</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 战场服务器池</span></span>
<span class="line">        self<span class="token punctuation">.</span>battle_servers <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">join_match</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> battle_power<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        玩家加入匹配队列</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        player_info <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;server_id&quot;</span><span class="token punctuation">:</span> server_id<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;player_id&quot;</span><span class="token punctuation">:</span> player_id<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;battle_power&quot;</span><span class="token punctuation">:</span> battle_power<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;join_time&quot;</span><span class="token punctuation">:</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 根据战力加入对应队列</span></span>
<span class="line">        <span class="token keyword">if</span> battle_power <span class="token operator">&lt;</span> <span class="token number">5000</span><span class="token punctuation">:</span></span>
<span class="line">            queue <span class="token operator">=</span> self<span class="token punctuation">.</span>match_queues<span class="token punctuation">[</span><span class="token string">&quot;low&quot;</span><span class="token punctuation">]</span></span>
<span class="line">        <span class="token keyword">elif</span> battle_power <span class="token operator">&lt;</span> <span class="token number">10000</span><span class="token punctuation">:</span></span>
<span class="line">            queue <span class="token operator">=</span> self<span class="token punctuation">.</span>match_queues<span class="token punctuation">[</span><span class="token string">&quot;mid&quot;</span><span class="token punctuation">]</span></span>
<span class="line">        <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">            queue <span class="token operator">=</span> self<span class="token punctuation">.</span>match_queues<span class="token punctuation">[</span><span class="token string">&quot;high&quot;</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        queue<span class="token punctuation">.</span>append<span class="token punctuation">(</span>player_info<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 尝试匹配</span></span>
<span class="line">        self<span class="token punctuation">.</span>try_match<span class="token punctuation">(</span>queue<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">try_match</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> queue<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        尝试匹配玩家</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token builtin">len</span><span class="token punctuation">(</span>queue<span class="token punctuation">)</span> <span class="token operator">&gt;=</span> <span class="token number">2</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 简单的 FIFO 匹配</span></span>
<span class="line">            player1 <span class="token operator">=</span> queue<span class="token punctuation">.</span>pop<span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">            player2 <span class="token operator">=</span> queue<span class="token punctuation">.</span>pop<span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 创建战场</span></span>
<span class="line">            self<span class="token punctuation">.</span>create_battle<span class="token punctuation">(</span><span class="token punctuation">[</span>player1<span class="token punctuation">,</span> player2<span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">create_battle</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> players<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        创建跨服战场</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 选择负载最低的战场服务器</span></span>
<span class="line">        battle_server <span class="token operator">=</span> self<span class="token punctuation">.</span>select_battle_server<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 创建战场实例</span></span>
<span class="line">        battle_id <span class="token operator">=</span> battle_server<span class="token punctuation">.</span>create_battle_instance<span class="token punctuation">(</span></span>
<span class="line">            players<span class="token operator">=</span>players<span class="token punctuation">,</span></span>
<span class="line">            battle_type<span class="token operator">=</span><span class="token string">&quot;arena&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            map_id<span class="token operator">=</span><span class="token number">1</span></span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 通知玩家</span></span>
<span class="line">        <span class="token keyword">for</span> player <span class="token keyword">in</span> players<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>notify_player_matched<span class="token punctuation">(</span></span>
<span class="line">                player<span class="token punctuation">[</span><span class="token string">&quot;server_id&quot;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">                player<span class="token punctuation">[</span><span class="token string">&quot;player_id&quot;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">                battle_server<span class="token punctuation">.</span>address<span class="token punctuation">,</span></span>
<span class="line">                battle_id</span>
<span class="line">            <span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="战场服务器设计" tabindex="-1"><a class="header-anchor" href="#战场服务器设计"><span>战场服务器设计</span></a></h4><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 战场服务器实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BattleServer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 战场实例</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">BattleInstance</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> battleId<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>PlayerInfo<span class="token operator">&gt;</span> players<span class="token punctuation">;</span></span>
<span class="line">        BattleState state<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> startTime<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 创建战场实例</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">createBattleInstance</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>PlayerInfo<span class="token operator">&gt;</span><span class="token operator">&amp;</span> players<span class="token punctuation">,</span></span>
<span class="line">                                   BattleType type<span class="token punctuation">,</span></span>
<span class="line">                                   <span class="token keyword">uint32_t</span> mapId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        BattleInstance instance<span class="token punctuation">;</span></span>
<span class="line">        instance<span class="token punctuation">.</span>battleId <span class="token operator">=</span> <span class="token function">generateBattleId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        instance<span class="token punctuation">.</span>players <span class="token operator">=</span> players<span class="token punctuation">;</span></span>
<span class="line">        instance<span class="token punctuation">.</span>state <span class="token operator">=</span> BattleState<span class="token double-colon punctuation">::</span>WAITING<span class="token punctuation">;</span></span>
<span class="line">        instance<span class="token punctuation">.</span>startTime <span class="token operator">=</span> <span class="token function">getTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 存储实例</span></span>
<span class="line">        battles_<span class="token punctuation">[</span>instance<span class="token punctuation">.</span>battleId<span class="token punctuation">]</span> <span class="token operator">=</span> instance<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> instance<span class="token punctuation">.</span>battleId<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 玩家连接到战场</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onPlayerConnect</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> battleId<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> battle <span class="token operator">=</span> battles_<span class="token punctuation">[</span>battleId<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否所有玩家都连接了</span></span>
<span class="line">        <span class="token keyword">bool</span> allConnected <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> player <span class="token operator">:</span> battle<span class="token punctuation">.</span>players<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>player<span class="token punctuation">.</span>connected<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                allConnected <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 所有玩家连接完毕，开始战斗</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>allConnected<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">startBattle</span><span class="token punctuation">(</span>battleId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 开始战斗</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">startBattle</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> battleId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> battle <span class="token operator">=</span> battles_<span class="token punctuation">[</span>battleId<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        battle<span class="token punctuation">.</span>state <span class="token operator">=</span> BattleState<span class="token double-colon punctuation">::</span>FIGHTING<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知所有玩家战斗开始</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> player <span class="token operator">:</span> battle<span class="token punctuation">.</span>players<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">sendToPlayer</span><span class="token punctuation">(</span>player<span class="token punctuation">.</span>serverId<span class="token punctuation">,</span> player<span class="token punctuation">.</span>playerId<span class="token punctuation">,</span></span>
<span class="line">                       <span class="token string">&quot;BATTLE_START&quot;</span><span class="token punctuation">,</span> battleId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 设置战斗超时</span></span>
<span class="line">        <span class="token function">schedule</span><span class="token punctuation">(</span>battleId<span class="token punctuation">,</span> BATTLE_TIMEOUT<span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">,</span> battleId<span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">endBattle</span><span class="token punctuation">(</span>battleId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-跨服好友" tabindex="-1"><a class="header-anchor" href="#_3-3-跨服好友"><span>3.3 跨服好友</span></a></h3><h4 id="数据结构" tabindex="-1"><a class="header-anchor" href="#数据结构"><span>数据结构</span></a></h4><div class="language-sql line-numbers-mode" data-highlighter="prismjs" data-ext="sql"><pre><code class="language-sql"><span class="line"><span class="token comment">-- 跨服好友表</span></span>
<span class="line"><span class="token keyword">CREATE</span> <span class="token keyword">TABLE</span> cross_server_friends <span class="token punctuation">(</span></span>
<span class="line">    id <span class="token keyword">BIGINT</span> <span class="token keyword">PRIMARY</span> <span class="token keyword">KEY</span> <span class="token keyword">AUTO_INCREMENT</span><span class="token punctuation">,</span></span>
<span class="line">    player_server_id <span class="token keyword">INT</span> <span class="token operator">NOT</span> <span class="token boolean">NULL</span><span class="token punctuation">,</span>          <span class="token comment">-- 玩家所在服务器</span></span>
<span class="line">    player_id <span class="token keyword">BIGINT</span> <span class="token operator">NOT</span> <span class="token boolean">NULL</span><span class="token punctuation">,</span>              <span class="token comment">-- 玩家 ID</span></span>
<span class="line">    friend_server_id <span class="token keyword">INT</span> <span class="token operator">NOT</span> <span class="token boolean">NULL</span><span class="token punctuation">,</span>          <span class="token comment">-- 好友所在服务器</span></span>
<span class="line">    friend_id <span class="token keyword">BIGINT</span> <span class="token operator">NOT</span> <span class="token boolean">NULL</span><span class="token punctuation">,</span>              <span class="token comment">-- 好友 ID</span></span>
<span class="line">    friend_name <span class="token keyword">VARCHAR</span><span class="token punctuation">(</span><span class="token number">64</span><span class="token punctuation">)</span> <span class="token operator">NOT</span> <span class="token boolean">NULL</span><span class="token punctuation">,</span>       <span class="token comment">-- 好友名称</span></span>
<span class="line">    is_online <span class="token keyword">TINYINT</span> <span class="token keyword">DEFAULT</span> <span class="token number">0</span><span class="token punctuation">,</span>            <span class="token comment">-- 是否在线</span></span>
<span class="line">    last_login_time <span class="token keyword">TIMESTAMP</span><span class="token punctuation">,</span>              <span class="token comment">-- 最后登录时间</span></span>
<span class="line">    create_time <span class="token keyword">TIMESTAMP</span> <span class="token keyword">DEFAULT</span> <span class="token keyword">CURRENT_TIMESTAMP</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token keyword">UNIQUE</span> <span class="token keyword">KEY</span> uk_player <span class="token punctuation">(</span>player_server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> friend_server_id<span class="token punctuation">,</span> friend_id<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token keyword">KEY</span> idx_friend <span class="token punctuation">(</span>friend_server_id<span class="token punctuation">,</span> friend_id<span class="token punctuation">)</span></span>
<span class="line"><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="实现代码-2" tabindex="-1"><a class="header-anchor" href="#实现代码-2"><span>实现代码</span></a></h4><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 跨服好友服务</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CrossServerFriendService</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    跨服好友服务</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">add_friend</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> my_server_id<span class="token punctuation">,</span> my_player_id<span class="token punctuation">,</span></span>
<span class="line">                   friend_server_id<span class="token punctuation">,</span> friend_player_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        添加跨服好友</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 1. 检查是否已经是好友</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>is_friend<span class="token punctuation">(</span>my_server_id<span class="token punctuation">,</span> my_player_id<span class="token punctuation">,</span></span>
<span class="line">                         friend_server_id<span class="token punctuation">,</span> friend_player_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token string">&quot;code&quot;</span><span class="token punctuation">:</span> <span class="token number">1</span><span class="token punctuation">,</span> <span class="token string">&quot;msg&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;已经是好友&quot;</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 2. 获取好友信息（跨服查询）</span></span>
<span class="line">        friend_info <span class="token operator">=</span> self<span class="token punctuation">.</span>get_player_info_cross_server<span class="token punctuation">(</span></span>
<span class="line">            friend_server_id<span class="token punctuation">,</span> friend_player_id</span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> friend_info<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token string">&quot;code&quot;</span><span class="token punctuation">:</span> <span class="token number">2</span><span class="token punctuation">,</span> <span class="token string">&quot;msg&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;玩家不存在&quot;</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 3. 添加好友记录（双向）</span></span>
<span class="line">        self<span class="token punctuation">.</span>add_friend_record<span class="token punctuation">(</span>my_server_id<span class="token punctuation">,</span> my_player_id<span class="token punctuation">,</span></span>
<span class="line">                              friend_server_id<span class="token punctuation">,</span> friend_player_id<span class="token punctuation">,</span></span>
<span class="line">                              friend_info<span class="token punctuation">[</span><span class="token string">&quot;name&quot;</span><span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 4. 发送好友申请通知</span></span>
<span class="line">        self<span class="token punctuation">.</span>send_friend_request_notification<span class="token punctuation">(</span></span>
<span class="line">            friend_server_id<span class="token punctuation">,</span> friend_player_id<span class="token punctuation">,</span></span>
<span class="line">            my_server_id<span class="token punctuation">,</span> my_player_id</span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token string">&quot;code&quot;</span><span class="token punctuation">:</span> <span class="token number">0</span><span class="token punctuation">,</span> <span class="token string">&quot;msg&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;success&quot;</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_player_info_cross_server</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        跨服获取玩家信息</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 检查本地缓存</span></span>
<span class="line">        cache_key <span class="token operator">=</span> <span class="token string-interpolation"><span class="token string">f&quot;player_info:</span><span class="token interpolation"><span class="token punctuation">{</span>server_id<span class="token punctuation">}</span></span><span class="token string">:</span><span class="token interpolation"><span class="token punctuation">{</span>player_id<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span></span>
<span class="line">        cached <span class="token operator">=</span> self<span class="token punctuation">.</span>redis<span class="token punctuation">.</span>get<span class="token punctuation">(</span>cache_key<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> cached<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> json<span class="token punctuation">.</span>loads<span class="token punctuation">(</span>cached<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 缓存未命中，查询目标服务器</span></span>
<span class="line">        server_addr <span class="token operator">=</span> self<span class="token punctuation">.</span>get_server_address<span class="token punctuation">(</span>server_id<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> server_addr<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 远程调用获取玩家信息</span></span>
<span class="line">        player_info <span class="token operator">=</span> self<span class="token punctuation">.</span>rpc_call<span class="token punctuation">(</span></span>
<span class="line">            server_addr<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;getPlayerInfo&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            player_id</span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 缓存结果</span></span>
<span class="line">        self<span class="token punctuation">.</span>redis<span class="token punctuation">.</span>setex<span class="token punctuation">(</span>cache_key<span class="token punctuation">,</span> <span class="token number">3600</span><span class="token punctuation">,</span> json<span class="token punctuation">.</span>dumps<span class="token punctuation">(</span>player_info<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> player_info</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-4-跨服排行" tabindex="-1"><a class="header-anchor" href="#_3-4-跨服排行"><span>3.4 跨服排行</span></a></h3><h4 id="架构设计-2" tabindex="-1"><a class="header-anchor" href="#架构设计-2"><span>架构设计</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    跨服排行架构                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│   Server A          Server B          Server C              │</span>
<span class="line">│  ┌────────┐        ┌────────┐        ┌────────┐            │</span>
<span class="line">│  │Player1 │        │Player2 │        │Player3 │            │</span>
<span class="line">│  │战力:5000│        │战力:8000│        │战力:6000│            │</span>
<span class="line">│  └────┬───┘        └────┬───┘        └────┬───┘            │</span>
<span class="line">│       │                 │                 │                 │</span>
<span class="line">│       │   上报数据       │                 │                 │</span>
<span class="line">│       └─────────────────┼─────────────────┘                 │</span>
<span class="line">│                         ▼                                   │</span>
<span class="line">│                  ┌─────────────┐                            │</span>
<span class="line">│                  │ 排行服务    │                            │</span>
<span class="line">│                  │ RankService │                            │</span>
<span class="line">│                  └──────┬──────┘                            │</span>
<span class="line">│                         │                                   │</span>
<span class="line">│                         ▼                                   │</span>
<span class="line">│                  ┌─────────────┐                            │</span>
<span class="line">│                  │  Redis      │                            │</span>
<span class="line">│                  │ Sorted Set  │                            │</span>
<span class="line">│                  └─────────────┘                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│                  ┌─────────────┐                            │</span>
<span class="line">│                  │ rank:battle_power                      │</span>
<span class="line">│                  │ 1. ServerB-Player2: 8000                │</span>
<span class="line">│                  │ 2. ServerC-Player3: 6000                │</span>
<span class="line">│                  │ 3. ServerA-Player1: 5000                │</span>
<span class="line">│                  └─────────────┘                            │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="实现代码-3" tabindex="-1"><a class="header-anchor" href="#实现代码-3"><span>实现代码</span></a></h4><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 跨服排行服务</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CrossServerRankService</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    跨服排行服务</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>redis <span class="token operator">=</span> RedisCluster<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">report_score</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> rank_type<span class="token punctuation">,</span> score<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        上报玩家分数（各服务器定期上报）</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        member <span class="token operator">=</span> <span class="token string-interpolation"><span class="token string">f&quot;</span><span class="token interpolation"><span class="token punctuation">{</span>server_id<span class="token punctuation">}</span></span><span class="token string">:</span><span class="token interpolation"><span class="token punctuation">{</span>player_id<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 使用 Redis Sorted Set</span></span>
<span class="line">        rank_key <span class="token operator">=</span> <span class="token string-interpolation"><span class="token string">f&quot;rank:</span><span class="token interpolation"><span class="token punctuation">{</span>rank_type<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 更新分数</span></span>
<span class="line">        self<span class="token punctuation">.</span>redis<span class="token punctuation">.</span>zadd<span class="token punctuation">(</span>rank_key<span class="token punctuation">,</span> <span class="token punctuation">{</span>member<span class="token punctuation">:</span> score<span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 设置过期时间（避免数据无限增长）</span></span>
<span class="line">        self<span class="token punctuation">.</span>redis<span class="token punctuation">.</span>expire<span class="token punctuation">(</span>rank_key<span class="token punctuation">,</span> <span class="token number">7</span> <span class="token operator">*</span> <span class="token number">24</span> <span class="token operator">*</span> <span class="token number">3600</span><span class="token punctuation">)</span>  <span class="token comment"># 7天</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_rank</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> rank_type<span class="token punctuation">,</span> top_n<span class="token operator">=</span><span class="token number">100</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        获取排行榜</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        rank_key <span class="token operator">=</span> <span class="token string-interpolation"><span class="token string">f&quot;rank:</span><span class="token interpolation"><span class="token punctuation">{</span>rank_type<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 获取前 N 名（降序）</span></span>
<span class="line">        top_members <span class="token operator">=</span> self<span class="token punctuation">.</span>redis<span class="token punctuation">.</span>zrevrange<span class="token punctuation">(</span></span>
<span class="line">            rank_key<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> top_n <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">,</span> withscores<span class="token operator">=</span><span class="token boolean">True</span></span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 格式化结果</span></span>
<span class="line">        result <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line">        <span class="token keyword">for</span> rank<span class="token punctuation">,</span> <span class="token punctuation">(</span>member<span class="token punctuation">,</span> score<span class="token punctuation">)</span> <span class="token keyword">in</span> <span class="token builtin">enumerate</span><span class="token punctuation">(</span>top_members<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            server_id<span class="token punctuation">,</span> player_id <span class="token operator">=</span> member<span class="token punctuation">.</span>split<span class="token punctuation">(</span><span class="token string">&quot;:&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            result<span class="token punctuation">.</span>append<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&quot;rank&quot;</span><span class="token punctuation">:</span> rank<span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;server_id&quot;</span><span class="token punctuation">:</span> <span class="token builtin">int</span><span class="token punctuation">(</span>server_id<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;player_id&quot;</span><span class="token punctuation">:</span> <span class="token builtin">int</span><span class="token punctuation">(</span>player_id<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;score&quot;</span><span class="token punctuation">:</span> <span class="token builtin">int</span><span class="token punctuation">(</span>score<span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> result</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_player_rank</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> rank_type<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        获取玩家排名</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        member <span class="token operator">=</span> <span class="token string-interpolation"><span class="token string">f&quot;</span><span class="token interpolation"><span class="token punctuation">{</span>server_id<span class="token punctuation">}</span></span><span class="token string">:</span><span class="token interpolation"><span class="token punctuation">{</span>player_id<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span></span>
<span class="line">        rank_key <span class="token operator">=</span> <span class="token string-interpolation"><span class="token string">f&quot;rank:</span><span class="token interpolation"><span class="token punctuation">{</span>rank_type<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 获取玩家排名</span></span>
<span class="line">        rank <span class="token operator">=</span> self<span class="token punctuation">.</span>redis<span class="token punctuation">.</span>zrevrank<span class="token punctuation">(</span>rank_key<span class="token punctuation">,</span> member<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> rank <span class="token keyword">is</span> <span class="token boolean">None</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 获取玩家分数</span></span>
<span class="line">        score <span class="token operator">=</span> self<span class="token punctuation">.</span>redis<span class="token punctuation">.</span>zscore<span class="token punctuation">(</span>rank_key<span class="token punctuation">,</span> member<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;rank&quot;</span><span class="token punctuation">:</span> rank <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">,</span>  <span class="token comment"># Redis 排名从 0 开始</span></span>
<span class="line">            <span class="token string">&quot;score&quot;</span><span class="token punctuation">:</span> <span class="token builtin">int</span><span class="token punctuation">(</span>score<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、数据一致性" tabindex="-1"><a class="header-anchor" href="#四、数据一致性"><span>四、数据一致性</span></a></h2><h3 id="_4-1-分布式事务" tabindex="-1"><a class="header-anchor" href="#_4-1-分布式事务"><span>4.1 分布式事务</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">跨服交易的一致性保证：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   Server A              Server B           跨服交易中心       │</span>
<span class="line">│  ┌────────┐            ┌────────┐         ┌────────┐        │</span>
<span class="line">│  │Player1 │            │Player2 │         │协调器   │        │</span>
<span class="line">│  │金币:100│            │金币:50  │         │        │        │</span>
<span class="line">│  └────┬───┘            └────┬───┘         └────┬───┘        │</span>
<span class="line">│       │                     │                   │            │</span>
<span class="line">│       │   玩家1给玩家2转50金币                    │            │</span>
<span class="line">│       │                     │                   │            │</span>
<span class="line">│       │◄────────────────────┼───────────────────►│            │</span>
<span class="line">│       │                     │                   │            │</span>
<span class="line">│       │  1. 协调器创建事务                        │            │</span>
<span class="line">│       │                     │                   │            │</span>
<span class="line">│       │◄────┐               │                   │            │</span>
<span class="line">│       │    │ 扣款50         │                   │            │</span>
<span class="line">│       │────┘               │                   │            │</span>
<span class="line">│  金币:50                  │                   │            │</span>
<span class="line">│       │                     │                   │            │</span>
<span class="line">│       │                     │◄────┐             │            │</span>
<span class="line">│       │                     │    │ 加款50       │            │</span>
<span class="line">│       │                     │────┘             │            │</span>
<span class="line">│       │               金币:100                  │            │</span>
<span class="line">│       │                     │                   │            │</span>
<span class="line">│       │◄────────────────────┼───────────────────►│            │</span>
<span class="line">│       │   2. 确认提交（两阶段）                   │            │</span>
<span class="line">│       │                     │                   │            │</span>
<span class="line">│       ▼                     ▼                   ▼            │</span>
<span class="line">│   提交成功                提交成功             提交完成         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-补偿机制" tabindex="-1"><a class="header-anchor" href="#_4-2-补偿机制"><span>4.2 补偿机制</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 分布式事务补偿机制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">DistributedTransaction</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    分布式事务（补偿模式）</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">transfer_money_cross_server</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> from_server<span class="token punctuation">,</span> from_player<span class="token punctuation">,</span></span>
<span class="line">                                    to_server<span class="token punctuation">,</span> to_player<span class="token punctuation">,</span> amount<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        跨服转账</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        transaction_id <span class="token operator">=</span> self<span class="token punctuation">.</span>generate_transaction_id<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 记录事务日志（用于补偿）</span></span>
<span class="line">        self<span class="token punctuation">.</span>log_transaction<span class="token punctuation">(</span>transaction_id<span class="token punctuation">,</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;from_server&quot;</span><span class="token punctuation">:</span> from_server<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;from_player&quot;</span><span class="token punctuation">:</span> from_player<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;to_server&quot;</span><span class="token punctuation">:</span> to_server<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;to_player&quot;</span><span class="token punctuation">:</span> to_player<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;amount&quot;</span><span class="token punctuation">:</span> amount<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;status&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;pending&quot;</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 1. 扣款</span></span>
<span class="line">            result1 <span class="token operator">=</span> self<span class="token punctuation">.</span>deduct_money<span class="token punctuation">(</span>from_server<span class="token punctuation">,</span> from_player<span class="token punctuation">,</span> amount<span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> result1<span class="token punctuation">[</span><span class="token string">&quot;success&quot;</span><span class="token punctuation">]</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">raise</span> Exception<span class="token punctuation">(</span><span class="token string">&quot;扣款失败&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 2. 加款</span></span>
<span class="line">            result2 <span class="token operator">=</span> self<span class="token punctuation">.</span>add_money<span class="token punctuation">(</span>to_server<span class="token punctuation">,</span> to_player<span class="token punctuation">,</span> amount<span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> result2<span class="token punctuation">[</span><span class="token string">&quot;success&quot;</span><span class="token punctuation">]</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token comment"># 补偿：加款失败，退还款项</span></span>
<span class="line">                self<span class="token punctuation">.</span>compensate_add_money<span class="token punctuation">(</span>from_server<span class="token punctuation">,</span> from_player<span class="token punctuation">,</span> amount<span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">raise</span> Exception<span class="token punctuation">(</span><span class="token string">&quot;加款失败&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 3. 标记事务完成</span></span>
<span class="line">            self<span class="token punctuation">.</span>update_transaction_status<span class="token punctuation">(</span>transaction_id<span class="token punctuation">,</span> <span class="token string">&quot;completed&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token string">&quot;success&quot;</span><span class="token punctuation">:</span> <span class="token boolean">True</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 标记事务失败</span></span>
<span class="line">            self<span class="token punctuation">.</span>update_transaction_status<span class="token punctuation">(</span>transaction_id<span class="token punctuation">,</span> <span class="token string">&quot;failed&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 触发人工处理（记录日志）</span></span>
<span class="line">            self<span class="token punctuation">.</span>alert_transaction_failed<span class="token punctuation">(</span>transaction_id<span class="token punctuation">,</span> <span class="token builtin">str</span><span class="token punctuation">(</span>e<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token string">&quot;success&quot;</span><span class="token punctuation">:</span> <span class="token boolean">False</span><span class="token punctuation">,</span> <span class="token string">&quot;msg&quot;</span><span class="token punctuation">:</span> <span class="token builtin">str</span><span class="token punctuation">(</span>e<span class="token punctuation">)</span><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、性能优化" tabindex="-1"><a class="header-anchor" href="#五、性能优化"><span>五、性能优化</span></a></h2><h3 id="_5-1-批量处理" tabindex="-1"><a class="header-anchor" href="#_5-1-批量处理"><span>5.1 批量处理</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 批量上报优化</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BatchReporter</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    批量上报器</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>pending_data <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line">        self<span class="token punctuation">.</span>last_flush <span class="token operator">=</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>batch_size <span class="token operator">=</span> <span class="token number">100</span></span>
<span class="line">        self<span class="token punctuation">.</span>flush_interval <span class="token operator">=</span> <span class="token number">5</span>  <span class="token comment"># 5秒</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">report</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> data<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        上报数据</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>pending_data<span class="token punctuation">.</span>append<span class="token punctuation">(</span>data<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 达到批量大小时刷新</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>pending_data<span class="token punctuation">)</span> <span class="token operator">&gt;=</span> self<span class="token punctuation">.</span>batch_size<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>flush<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">flush</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        刷新数据到跨服中心</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>pending_data<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 批量发送</span></span>
<span class="line">        self<span class="token punctuation">.</span>send_batch<span class="token punctuation">(</span>self<span class="token punctuation">.</span>pending_data<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 清空缓存</span></span>
<span class="line">        self<span class="token punctuation">.</span>pending_data<span class="token punctuation">.</span>clear<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>last_flush <span class="token operator">=</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">auto_flush</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        自动刷新（定时器）</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> self<span class="token punctuation">.</span>last_flush <span class="token operator">&gt;=</span> self<span class="token punctuation">.</span>flush_interval<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>flush<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-本地缓存" tabindex="-1"><a class="header-anchor" href="#_5-2-本地缓存"><span>5.2 本地缓存</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 本地缓存跨服数据</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CrossServerCache</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    跨服数据缓存</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>local_cache <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">        self<span class="token punctuation">.</span>cache_ttl <span class="token operator">=</span> <span class="token number">300</span>  <span class="token comment"># 5分钟</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_player_info</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        获取玩家信息（优先缓存）</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        cache_key <span class="token operator">=</span> <span class="token punctuation">(</span>server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 检查缓存</span></span>
<span class="line">        <span class="token keyword">if</span> cache_key <span class="token keyword">in</span> self<span class="token punctuation">.</span>local_cache<span class="token punctuation">:</span></span>
<span class="line">            cached_data<span class="token punctuation">,</span> cached_time <span class="token operator">=</span> self<span class="token punctuation">.</span>local_cache<span class="token punctuation">[</span>cache_key<span class="token punctuation">]</span></span>
<span class="line">            <span class="token keyword">if</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> cached_time <span class="token operator">&lt;</span> self<span class="token punctuation">.</span>cache_ttl<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">return</span> cached_data</span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 缓存未命中，查询跨服</span></span>
<span class="line">        player_info <span class="token operator">=</span> self<span class="token punctuation">.</span>query_cross_server<span class="token punctuation">(</span>server_id<span class="token punctuation">,</span> player_id<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 更新缓存</span></span>
<span class="line">        self<span class="token punctuation">.</span>local_cache<span class="token punctuation">[</span>cache_key<span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">(</span>player_info<span class="token punctuation">,</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> player_info</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、总结" tabindex="-1"><a class="header-anchor" href="#六、总结"><span>六、总结</span></a></h2><h3 id="实现方案对比" tabindex="-1"><a class="header-anchor" href="#实现方案对比"><span>实现方案对比</span></a></h3><table><thead><tr><th>功能</th><th>推荐方案</th><th>优点</th><th>缺点</th></tr></thead><tbody><tr><td><strong>跨服聊天</strong></td><td>Redis Pub/Sub</td><td>简单、实时</td><td>有序性差</td></tr><tr><td><strong>跨服战场</strong></td><td>独立战场服务器</td><td>隔离好</td><td>迁移复杂</td></tr><tr><td><strong>跨服好友</strong></td><td>中心数据库</td><td>一致性好</td><td>性能瓶颈</td></tr><tr><td><strong>跨服排行</strong></td><td>Redis Sorted Set</td><td>高性能</td><td>数据有损</td></tr><tr><td><strong>跨服交易</strong></td><td>补偿事务</td><td>最终一致</td><td>复杂度高</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">跨服功能设计建议：</span>
<span class="line"></span>
<span class="line">1. 选择合适的通信方式</span>
<span class="line">   - 聊天：消息队列</span>
<span class="line">   - 战场：直连战场服务器</span>
<span class="line">   - 社交：中心数据库</span>
<span class="line">   - 排行：Redis</span>
<span class="line"></span>
<span class="line">2. 注意数据一致性</span>
<span class="line">   - 使用补偿机制</span>
<span class="line">   - 记录事务日志</span>
<span class="line">   - 提供人工介入</span>
<span class="line"></span>
<span class="line">3. 性能优化</span>
<span class="line">   - 批量处理</span>
<span class="line">   - 本地缓存</span>
<span class="line">   - 异步上报</span>
<span class="line"></span>
<span class="line">4. 容错处理</span>
<span class="line">   - 超时重试</span>
<span class="line">   - 降级服务</span>
<span class="line">   - 监控告警</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.kbelab.com/manual/balance.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 负载均衡</a></li><li><a href="https://redis.io/topics/pubsub" target="_blank" rel="noopener noreferrer">Redis Pub/Sub 官方文档</a></li><li><a href="https://kafka.apache.org/documentation/" target="_blank" rel="noopener noreferrer">Kafka 分布式消息队列</a></li><li><a href="https://www.infoq.cn/article/game-server-cross-server" target="_blank" rel="noopener noreferrer">跨服架构设计实践</a></li></ul>`,35)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};