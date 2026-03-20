import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q33-hot-data.html","title":"Q33: 如何处理热点数据？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q33-hot-data.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q33-hot-data.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q33-如何处理热点数据" tabindex="-1"><a class="header-anchor" href="#q33-如何处理热点数据"><span>Q33: 如何处理热点数据？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对热点数据处理的理解：</p><ul><li>热点数据的定义和危害</li><li>缓存策略</li><li>数据分片</li><li>多级缓存架构</li></ul><hr><h2 id="一、热点数据问题" tabindex="-1"><a class="header-anchor" href="#一、热点数据问题"><span>一、热点数据问题</span></a></h2><h3 id="_1-1-什么是热点数据" tabindex="-1"><a class="header-anchor" href="#_1-1-什么是热点数据"><span>1.1 什么是热点数据</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    热点数据定义                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  热点数据: 被频繁访问的数据                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  MMO 中的热点数据:                                          │</span>
<span class="line">│  ├── 玩家基本信息 (登录时频繁查询)                          │</span>
<span class="line">│  ├── 排行榜 Top 数据 (大量玩家查询)                         │</span>
<span class="line">│  ├── 热门道具价格 (交易行查询)                              │</span>
<span class="line">│  ├── 系统配置数据 (频繁读取)                                │</span>
<span class="line">│  ├── 公会信息 (成员频繁访问)                                │</span>
<span class="line">│  └── 活动状态 (活动期间高频查询)                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题:                                                     │</span>
<span class="line">│  ├── 数据库压力过大                                        │</span>
<span class="line">│  ├── 响应延迟增加                                            │</span>
<span class="line">│  ├── 可能导致数据库宕机                                     │</span>
<span class="line">│  └── 影响玩家体验                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-热点数据分类" tabindex="-1"><a class="header-anchor" href="#_1-2-热点数据分类"><span>1.2 热点数据分类</span></a></h3><table><thead><tr><th>类型</th><th>示例</th><th>访问特点</th><th>解决方案</th></tr></thead><tbody><tr><td><strong>读多写少</strong></td><td>配置、排行榜</td><td>高频读取</td><td>Redis 缓存</td></tr><tr><td><strong>读多写多</strong></td><td>金币、积分</td><td>高频读写</td><td>Redis + 队列</td></tr><tr><td><strong>读写均衡</strong></td><td>背包、装备</td><td>中频访问</td><td>分表 + 缓存</td></tr><tr><td><strong>突发热点</strong></td><td>活动数据</td><td>短时高峰</td><td>预热 + 扩容</td></tr></tbody></table><hr><h2 id="二、缓存策略" tabindex="-1"><a class="header-anchor" href="#二、缓存策略"><span>二、缓存策略</span></a></h2><h3 id="_2-1-多级缓存" tabindex="-1"><a class="header-anchor" href="#_2-1-多级缓存"><span>2.1 多级缓存</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    多级缓存架构                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  L1: 进程内缓存 (单机)                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 存储在进程内存中                                  │       │</span>
<span class="line">│  │  - 访问速度最快 (&lt;1ms)                              │       │</span>
<span class="line">│  │  - 容量有限 (MB 级别)                              │       │</span>
<span class="line">│  │  - 多实例数据不一致                                 │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适合: 配置数据、静态表                             │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼                                  │</span>
<span class="line">│  L2: Redis 缓存 (分布式)                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 分布式共享                                      │       │</span>
<span class="line">│  │  - 速度快 (~1-5ms)                               │       │</span>
<span class="line">│  │  - 容量大 (GB 级别)                               │       │</span>
<span class="line">│  │  - 支持过期策略                                    │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适合: 玩家数据、排行榜                             │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼                                  │</span>
<span class="line">│  L3: 数据库 (持久化)                                        │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 数据最终存储                                    │       │</span>
<span class="line">│  │  - 速度慢 (~10-100ms)                             │       │</span>
<span class="line">│  │  - 容量最大 (TB 级别)                              │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适合: 所有数据的持久化                             │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-cache-aside-模式" tabindex="-1"><a class="header-anchor" href="#_2-2-cache-aside-模式"><span>2.2 Cache-Aside 模式</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Cache-Aside 缓存模式</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CacheAside</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 获取数据</span></span>
<span class="line">    T <span class="token function">get</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 先查缓存</span></span>
<span class="line">        T value<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>cache_<span class="token punctuation">.</span><span class="token function">get</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 缓存命中</span></span>
<span class="line">            <span class="token keyword">return</span> value<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 缓存未命中，查数据库</span></span>
<span class="line">        value <span class="token operator">=</span> database_<span class="token operator">-&gt;</span><span class="token generic-function"><span class="token function">load</span><span class="token generic class-name"><span class="token operator">&lt;</span>T<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>value<span class="token punctuation">.</span><span class="token function">isNull</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> value<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 写入缓存</span></span>
<span class="line">        cache_<span class="token punctuation">.</span><span class="token function">set</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> value<span class="token punctuation">,</span> cacheTTL_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> value<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新数据</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">set</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">,</span> <span class="token keyword">const</span> T<span class="token operator">&amp;</span> value<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 更新数据库</span></span>
<span class="line">        database_<span class="token operator">-&gt;</span><span class="token function">save</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 更新缓存</span></span>
<span class="line">        cache_<span class="token punctuation">.</span><span class="token function">set</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> value<span class="token punctuation">,</span> cacheTTL_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 删除其他服务器的缓存（如果需要）</span></span>
<span class="line">        <span class="token function">invalidateOtherCaches</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 删除数据</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">remove</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 删除数据库</span></span>
<span class="line">        database_<span class="token operator">-&gt;</span><span class="token function">remove</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 删除缓存</span></span>
<span class="line">        cache_<span class="token punctuation">.</span><span class="token function">del</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 通知其他服务器</span></span>
<span class="line">        <span class="token function">invalidateOtherCaches</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">invalidateOtherCaches</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 通过 Redis Pub/Sub 通知</span></span>
<span class="line">        redis_<span class="token punctuation">.</span><span class="token function">publish</span><span class="token punctuation">(</span><span class="token string">&quot;cache:invalidate&quot;</span><span class="token punctuation">,</span> key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    Cache<span class="token operator">&amp;</span> cache_<span class="token punctuation">;</span></span>
<span class="line">    Database<span class="token operator">*</span> database_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> cacheTTL_ <span class="token operator">=</span> <span class="token number">300</span><span class="token punctuation">;</span> <span class="token comment">// 5分钟</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、数据分片" tabindex="-1"><a class="header-anchor" href="#三、数据分片"><span>三、数据分片</span></a></h2><h3 id="_3-1-分片策略" tabindex="-1"><a class="header-anchor" href="#_3-1-分片策略"><span>3.1 分片策略</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 数据分片路由</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">DataSharding</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 获取分片</span></span>
<span class="line">    size_t <span class="token function">getShard</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> size_t shardCount<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 一致性哈希</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">hash</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span> <span class="token operator">%</span> shardCount<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取分片数据库</span></span>
<span class="line">    Database<span class="token operator">*</span> <span class="token function">getShardDatabase</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t shardId <span class="token operator">=</span> <span class="token function">getShard</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> shardCount_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> databases_<span class="token punctuation">[</span>shardId<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 查询玩家数据</span></span>
<span class="line">    PlayerData <span class="token function">getPlayerData</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Database<span class="token operator">*</span> db <span class="token operator">=</span> <span class="token function">getShardDatabase</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> result <span class="token operator">=</span> db<span class="token operator">-&gt;</span><span class="token function">query</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;SELECT * FROM player_{} WHERE id = {}&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token function">getShard</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> shardCount_<span class="token punctuation">)</span><span class="token punctuation">,</span> playerId</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>result<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token function">parsePlayerData</span><span class="token punctuation">(</span>result<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    size_t <span class="token function">hash</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// MurmurHash 或 FNV</span></span>
<span class="line">        <span class="token keyword">return</span> std<span class="token double-colon punctuation">::</span>hash<span class="token operator">&lt;</span><span class="token keyword">uint64_t</span><span class="token operator">&gt;</span><span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Database<span class="token operator">*</span><span class="token operator">&gt;</span> databases_<span class="token punctuation">;</span></span>
<span class="line">    size_t shardCount_ <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、缓存更新策略" tabindex="-1"><a class="header-anchor" href="#四、缓存更新策略"><span>四、缓存更新策略</span></a></h2><h3 id="_4-1-更新策略对比" tabindex="-1"><a class="header-anchor" href="#_4-1-更新策略对比"><span>4.1 更新策略对比</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  缓存更新策略                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  Cache-Aside (推荐):                                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  读: 先读缓存，miss 再读 DB                          │       │</span>
<span class="line">│  │  写: 先写 DB，再更新/删除缓存                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 简单、可靠                                    │       │</span>
<span class="line">│  │  缺点: 可能短时间不一致                               │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Write-Through:                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  写: 同时写缓存和 DB                               │       │</span>
<span class="line">│  │  读: 先读缓存                                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 强一致性                                     │       │</span>
<span class="line">│  │  缺点: 写入延迟大                                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Write-Behind:                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  写: 先写缓存，异步写 DB                             │       │</span>
<span class="line">│  │  读: 先读缓存                                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 写入速度快                                   │       │</span>
<span class="line">│  │  缺点: 可能丢失数据                                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、总结" tabindex="-1"><a class="header-anchor" href="#五、总结"><span>五、总结</span></a></h2><h3 id="热点数据处理方案" tabindex="-1"><a class="header-anchor" href="#热点数据处理方案"><span>热点数据处理方案</span></a></h3><table><thead><tr><th>方案</th><th>适用场景</th><th>复杂度</th></tr></thead><tbody><tr><td><strong>Redis 缓存</strong></td><td>读多写少</td><td>低</td></tr><tr><td><strong>本地缓存</strong></td><td>配置数据</td><td>中</td></tr><tr><td><strong>数据分片</strong></td><td>大量玩家</td><td>高</td></tr><tr><td><strong>读写分离</strong></td><td>查询多</td><td>中</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 多级缓存降低 DB 压力</span>
<span class="line">2. 合理设置 TTL 避免雪崩</span>
<span class="line">3. 分片分散热点</span>
<span class="line">4. 监控热点数据</span>
<span class="line">5. 预热准备热点数据</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://redis.io/docs/manual/patterns/" target="_blank" rel="noopener noreferrer">Redis 缓存设计</a></li><li><a href="https://www.alibabacloud.com/blog/" target="_blank" rel="noopener noreferrer">热点数据处理</a></li></ul>`,33)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};