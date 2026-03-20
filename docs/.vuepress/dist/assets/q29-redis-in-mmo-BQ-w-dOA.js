import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q29-redis-in-mmo.html","title":"Q29: Redis 在 MMO 中有哪些应用场景？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q29-redis-in-mmo.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q29-redis-in-mmo.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q29-redis-在-mmo-中有哪些应用场景" tabindex="-1"><a class="header-anchor" href="#q29-redis-在-mmo-中有哪些应用场景"><span>Q29: Redis 在 MMO 中有哪些应用场景？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对 Redis 在游戏服务器中的应用理解：</p><ul><li>Redis 的数据类型和特性</li><li>MMO 中的典型应用场景</li><li>KBEngine 与 Redis 的集成</li><li>最佳实践和注意事项</li></ul><hr><h2 id="一、redis-基础" tabindex="-1"><a class="header-anchor" href="#一、redis-基础"><span>一、Redis 基础</span></a></h2><h3 id="_1-1-redis-特性" tabindex="-1"><a class="header-anchor" href="#_1-1-redis-特性"><span>1.1 Redis 特性</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      Redis 特性                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  核心特性：                                                  │</span>
<span class="line">│  ├── 内存存储 - 极快的读写速度                              │</span>
<span class="line">│  ├── 丰富数据结构 - String, Hash, List, Set, ZSet           │</span>
<span class="line">│  ├── 持久化支持 - RDB/AOF                                   │</span>
<span class="line">│  ├── 主从复制 - 高可用                                     │</span>
<span class="line">│  ├── 集群支持 - 水平扩展                                   │</span>
<span class="line">│  └── 事务支持 - MULTI/EXEC                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  性能指标：                                                 │</span>
<span class="line">│  ├── QPS: 100,000+ (单机)                                 │</span>
<span class="line">│  ├── 延迟: &lt; 1ms                                            │</span>
<span class="line">│  └── 并发: 10,000+ 连接                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-数据类型与用途" tabindex="-1"><a class="header-anchor" href="#_1-2-数据类型与用途"><span>1.2 数据类型与用途</span></a></h3><table><thead><tr><th>数据类型</th><th>MMO 应用场景</th><th>操作命令</th></tr></thead><tbody><tr><td><strong>String</strong></td><td>玩家状态、锁</td><td>GET, SET, INCR</td></tr><tr><td><strong>Hash</strong></td><td>玩家数据缓存</td><td>HGET, HSET, HMGET</td></tr><tr><td><strong>List</strong></td><td>消息队列、日志</td><td>LPUSH, RPOP, LRANGE</td></tr><tr><td><strong>Set</strong></td><td>在线玩家、好友</td><td>SADD, SREM, SMEMBERS</td></tr><tr><td><strong>ZSet</strong></td><td>排行榜</td><td>ZADD, ZRANGE, ZINCRBY</td></tr><tr><td><strong>Bitmap</strong></td><td>功能开关、签到</td><td>SETBIT, GETBIT, BITCOUNT</td></tr><tr><td><strong>Geo</strong></td><td>位置服务</td><td>GEOADD, GEORADIUS</td></tr></tbody></table><hr><h2 id="二、mmo-应用场景" tabindex="-1"><a class="header-anchor" href="#二、mmo-应用场景"><span>二、MMO 应用场景</span></a></h2><h3 id="_2-1-排行榜系统" tabindex="-1"><a class="header-anchor" href="#_2-1-排行榜系统"><span>2.1 排行榜系统</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  Redis 排行榜实现                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  使用 ZSET (Sorted Set) 实现排行榜：                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  添加/更新分数：                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  ZADD rank:level 50 player:1001                   │       │</span>
<span class="line">│  │  ZADD rank:level 45 player:1002                   │       │</span>
<span class="line">│  │  ZADD rank:level 60 player:1003                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  获取 TOP 100：                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  ZREVRANGE rank:level 0 99 WITHSCORES            │       │</span>
<span class="line">│  │  返回：[(1003, 60), (1001, 50), (1002, 45)]     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  获取玩家排名：                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  ZREVRANK rank:level player:1001                 │       │</span>
<span class="line">│  │  返回：1 (第 2 名)                                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// C++ Redis 排行榜实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RedisRankManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 更新玩家分数</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">updateScore</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> rankKey<span class="token punctuation">,</span></span>
<span class="line">                     EntityID playerId<span class="token punctuation">,</span> <span class="token keyword">int64_t</span> score<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string member <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> redis_<span class="token operator">-&gt;</span><span class="token function">zadd</span><span class="token punctuation">(</span>rankKey<span class="token punctuation">,</span> member<span class="token punctuation">,</span> score<span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取排行榜</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>RankEntry<span class="token operator">&gt;</span> <span class="token function">getTopRank</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> rankKey<span class="token punctuation">,</span></span>
<span class="line">                                       <span class="token keyword">int</span> topN<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>RankEntry<span class="token operator">&gt;</span> result<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// ZREVRANGE key 0 (topN-1) WITHSCORES</span></span>
<span class="line">        <span class="token keyword">auto</span> members <span class="token operator">=</span> redis_<span class="token operator">-&gt;</span><span class="token function">zrevrange</span><span class="token punctuation">(</span>rankKey<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> topN <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">,</span> <span class="token boolean">true</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">int</span> rank <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>member<span class="token punctuation">,</span> score<span class="token punctuation">]</span> <span class="token operator">:</span> members<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            result<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span><span class="token function">stoull</span><span class="token punctuation">(</span>member<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">int64_t</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>score<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                rank<span class="token operator">++</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> result<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取玩家排名</span></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">getPlayerRank</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> rankKey<span class="token punctuation">,</span> EntityID playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string member <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// ZREVRANK key member</span></span>
<span class="line">        <span class="token keyword">auto</span> rank <span class="token operator">=</span> redis_<span class="token operator">-&gt;</span><span class="token function">zrevrank</span><span class="token punctuation">(</span>rankKey<span class="token punctuation">,</span> member<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> rank<span class="token punctuation">.</span><span class="token function">has_value</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">?</span> <span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">int</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token operator">*</span>rank<span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token number">1</span> <span class="token operator">:</span> <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-在线玩家管理" tabindex="-1"><a class="header-anchor" href="#_2-2-在线玩家管理"><span>2.2 在线玩家管理</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  在线玩家管理 (Set + Hash)                   │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  使用 Set 存储在线玩家列表：                                │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  SADD online_players player:1001                 │       │</span>
<span class="line">│  │  SADD online_players player:1002                 │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  使用 Hash 存储玩家详细信息：                               │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  HSET player:1001 name &quot;Player1&quot;                 │       │</span>
<span class="line">│  │  HSET player:1001 level 50                       │       │</span>
<span class="line">│  │  HSET player:1001 map &quot;main_city&quot;                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  玩家上线/下线：                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  上线：                                           │       │</span>
<span class="line">│  │  ├── SADD online_players player:1001            │       │</span>
<span class="line">│  │  ├── HSET player:1001 online_time now()          │       │</span>
<span class="line">│  │  └── HSET player:1001 server_id server_1         │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  下线：                                           │       │</span>
<span class="line">│  │  ├── SREM online_players player:1001            │       │</span>
<span class="line">│  │  ├── HSET player:1001 offline_time now()         │       │</span>
<span class="line">│  │  └── EXPIRE player:1001 3600 (1小时后过期)      │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-分布式锁" tabindex="-1"><a class="header-anchor" href="#_2-3-分布式锁"><span>2.3 分布式锁</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  Redis 分布式锁                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景：防止玩家数据重复处理                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  获取锁：                                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  SET lock:player:1001 unique_id NX EX 30        │       │</span>
<span class="line">│  │  ├── NX: 只在不存在时设置                          │       │</span>
<span class="line">│  │  ├── EX: 设置 30 秒过期                            │       │</span>
<span class="line">│  │  └── unique_id: 确保只释放自己的锁                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  释放锁：                                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  Lua 脚本 (原子操作)：                            │       │</span>
<span class="line">│  │  if redis.call(&quot;get&quot;, KEYS[1]) == ARGV[1] then   │       │</span>
<span class="line">│  │      return redis.call(&quot;del&quot;, KEYS[1])            │       │</span>
<span class="line">│  │  else                                              │       │</span>
<span class="line">│  │      return 0                                      │       │</span>
<span class="line">│  │  end                                               │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Redis 分布式锁实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RedisLock</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 尝试获取锁</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">tryLock</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> timeoutMs<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string value <span class="token operator">=</span> <span class="token function">generateUniqueID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// SET key value NX EX timeout</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string cmd <span class="token operator">=</span> <span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;SET %s %s NX EX %d&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            key<span class="token punctuation">.</span><span class="token function">c_str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            value<span class="token punctuation">.</span><span class="token function">c_str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            timeoutMs <span class="token operator">/</span> <span class="token number">1000</span></span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> result <span class="token operator">=</span> redis_<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span>cmd<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        lockValue_ <span class="token operator">=</span> value<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> result<span class="token punctuation">.</span><span class="token function">has_value</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> <span class="token operator">*</span>result <span class="token operator">==</span> <span class="token string">&quot;OK&quot;</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 释放锁</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">unlock</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// Lua 脚本确保只删除自己的锁</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string luaScript <span class="token operator">=</span></span>
<span class="line">            <span class="token string">&quot;if redis.call(&#39;get&#39;, KEYS[1]) == ARGV[1] then &quot;</span></span>
<span class="line">            <span class="token string">&quot;    return redis.call(&#39;del&#39;, KEYS[1]) &quot;</span></span>
<span class="line">            <span class="token string">&quot;else &quot;</span></span>
<span class="line">            <span class="token string">&quot;    return 0 &quot;</span></span>
<span class="line">            <span class="token string">&quot;end&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> result <span class="token operator">=</span> redis_<span class="token operator">-&gt;</span><span class="token function">eval</span><span class="token punctuation">(</span>luaScript<span class="token punctuation">,</span> <span class="token punctuation">{</span>key<span class="token punctuation">}</span><span class="token punctuation">,</span> <span class="token punctuation">{</span>lockValue_<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> result<span class="token punctuation">.</span><span class="token function">has_value</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> <span class="token operator">*</span>result <span class="token operator">==</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 自动锁 (RAII)</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">Guard</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">        <span class="token function">Guard</span><span class="token punctuation">(</span>RedisLock<span class="token operator">*</span> lock<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> timeout<span class="token punctuation">)</span></span>
<span class="line">            <span class="token operator">:</span> <span class="token function">lock_</span><span class="token punctuation">(</span>lock<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">key_</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">acquired_</span><span class="token punctuation">(</span><span class="token boolean">false</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            acquired_ <span class="token operator">=</span> lock_<span class="token operator">-&gt;</span><span class="token function">tryLock</span><span class="token punctuation">(</span>key_<span class="token punctuation">,</span> timeout<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">~</span><span class="token function">Guard</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>acquired_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                lock_<span class="token operator">-&gt;</span><span class="token function">unlock</span><span class="token punctuation">(</span>key_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">bool</span> <span class="token function">acquired</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> acquired_<span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">        RedisLock<span class="token operator">*</span> lock_<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string key_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">bool</span> acquired_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string lockValue_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-4-限流和防刷" tabindex="-1"><a class="header-anchor" href="#_2-4-限流和防刷"><span>2.4 限流和防刷</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  Redis 限流实现                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  滑动窗口限流：                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  每 60 秒最多 100 次请求                         │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  执行：                                            │       │</span>
<span class="line">│  │  key = &quot;limit:player:1001&quot;                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  1. 移除 60 秒前的记录                           │       │</span>
<span class="line">│  │  ZREMRANGEBYSCORE key 0 (now() - 60)             │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  2. 统计当前请求数                               │       │</span>
<span class="line">│  │  count = ZCARD key                                │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  3. 判断是否限流                                 │       │</span>
<span class="line">│  │  if count &lt; 100:                                 │       │</span>
<span class="line">│  │      ZADD key now() request_id                   │       │</span>
<span class="line">│  │      return true (允许)                          │       │</span>
<span class="line">│  │  else:                                            │       │</span>
<span class="line">│  │      return false (拒绝)                          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  令牌桶限流：                                               │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  初始化：                                          │       │</span>
<span class="line">│  │  令牌数: 10, 添加速率: 1个/秒                      │       │</span>
<span class="line">│  │  EVAL script 10 rate_limit:player:1001            │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  请求时：                                          │       │</span>
<span class="line">│  │  if 令牌 &gt; 0:                                     │       │</span>
<span class="line">│  │      消耗一个令牌                                │       │</span>
<span class="line">│  │      return true                                  │       │</span>
<span class="line">│  │  else:                                            │       │</span>
<span class="line">│  │      return false                                 │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、kbengine-与-redis" tabindex="-1"><a class="header-anchor" href="#三、kbengine-与-redis"><span>三、KBEngine 与 Redis</span></a></h2><h3 id="_3-1-集成方案" tabindex="-1"><a class="header-anchor" href="#_3-1-集成方案"><span>3.1 集成方案</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 集成 Redis</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> redis</span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"></span>
<span class="line"><span class="token comment"># Redis 连接池</span></span>
<span class="line">redis_pool <span class="token operator">=</span> redis<span class="token punctuation">.</span>ConnectionPool<span class="token punctuation">(</span></span>
<span class="line">    host<span class="token operator">=</span><span class="token string">&#39;localhost&#39;</span><span class="token punctuation">,</span></span>
<span class="line">    port<span class="token operator">=</span><span class="token number">6379</span><span class="token punctuation">,</span></span>
<span class="line">    db<span class="token operator">=</span><span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">    max_connections<span class="token operator">=</span><span class="token number">50</span></span>
<span class="line"><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">redis_client <span class="token operator">=</span> redis<span class="token punctuation">.</span>Redis<span class="token punctuation">(</span>connection_pool<span class="token operator">=</span>redis_pool<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RedisManager</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">setPlayerOnline</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> playerId<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;设置玩家在线&quot;&quot;&quot;</span></span>
<span class="line">        redis_client<span class="token punctuation">.</span>sadd<span class="token punctuation">(</span><span class="token string">&#39;online_players&#39;</span><span class="token punctuation">,</span> playerId<span class="token punctuation">)</span></span>
<span class="line">        redis_client<span class="token punctuation">.</span>hset<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&#39;player:</span><span class="token interpolation"><span class="token punctuation">{</span>playerId<span class="token punctuation">}</span></span><span class="token string">&#39;</span></span><span class="token punctuation">,</span> <span class="token string">&#39;online&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;1&#39;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">setPlayerOffline</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> playerId<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;设置玩家离线&quot;&quot;&quot;</span></span>
<span class="line">        redis_client<span class="token punctuation">.</span>srem<span class="token punctuation">(</span><span class="token string">&#39;online_players&#39;</span><span class="token punctuation">,</span> playerId<span class="token punctuation">)</span></span>
<span class="line">        redis_client<span class="token punctuation">.</span>hset<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&#39;player:</span><span class="token interpolation"><span class="token punctuation">{</span>playerId<span class="token punctuation">}</span></span><span class="token string">&#39;</span></span><span class="token punctuation">,</span> <span class="token string">&#39;online&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;0&#39;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 1 小时后过期</span></span>
<span class="line">        redis_client<span class="token punctuation">.</span>expire<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&#39;player:</span><span class="token interpolation"><span class="token punctuation">{</span>playerId<span class="token punctuation">}</span></span><span class="token string">&#39;</span></span><span class="token punctuation">,</span> <span class="token number">3600</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">updateRank</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> rankKey<span class="token punctuation">,</span> playerId<span class="token punctuation">,</span> score<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;更新排行榜&quot;&quot;&quot;</span></span>
<span class="line">        redis_client<span class="token punctuation">.</span>zadd<span class="token punctuation">(</span>rankKey<span class="token punctuation">,</span> <span class="token punctuation">{</span>playerId<span class="token punctuation">:</span> score<span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">getTopRank</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> rankKey<span class="token punctuation">,</span> n<span class="token operator">=</span><span class="token number">100</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取排行榜&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> redis_client<span class="token punctuation">.</span>zrevrange<span class="token punctuation">(</span>rankKey<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> n<span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">,</span> withscores<span class="token operator">=</span><span class="token boolean">True</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-缓存策略" tabindex="-1"><a class="header-anchor" href="#_3-2-缓存策略"><span>3.2 缓存策略</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  KBEngine + Redis 缓存架构                   │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────┐         ┌─────────────┐                  │</span>
<span class="line">│  │  BaseApp    │         │  Redis      │                  │</span>
<span class="line">│  │             │◄──────►│             │                  │</span>
<span class="line">│  │  数据缓存    │         │  热点数据    │                  │</span>
<span class="line">│  │  - 玩家列表   │         │  - 排行榜    │                  │</span>
<span class="line">│  │  - 排行榜    │         │  - 在线列表   │                  │</span>
<span class="line">│  │  - 通知队列   │         │  - 分布式锁   │                  │</span>
<span class="line">│  └──────┬──────┘         └──────┬──────┘                  │</span>
<span class="line">│         │                        │                          │</span>
<span class="line">│         │    缓存未命中          │                          │</span>
<span class="line">│         ▼                        ▼                          │</span>
<span class="line">│  ┌─────────────────────────────────────────┐               │</span>
<span class="line">│  │              DBMgr                     │               │</span>
<span class="line">│  │                                          │               │</span>
<span class="line">│  │  ┌─────────────────────────────────┐   │               │</span>
<span class="line">│  │  │        MySQL 数据库              │   │               │</span>
<span class="line">│  │  │  - 玩家完整数据                  │   │               │</span>
<span class="line">│  │  │  - 持久化存储                    │   │               │</span>
<span class="line">│  │  └─────────────────────────────────┘   │               │</span>
<span class="line">│  └─────────────────────────────────────────┘               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、高级应用" tabindex="-1"><a class="header-anchor" href="#四、高级应用"><span>四、高级应用</span></a></h2><h3 id="_4-1-hyperloglog-统计" tabindex="-1"><a class="header-anchor" href="#_4-1-hyperloglog-统计"><span>4.1 HyperLogLog 统计</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// HyperLogLog 用于唯一统计</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RedisUVCounter</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 记录访问</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">recordAccess</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">,</span> EntityID playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string member <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        redis_<span class="token operator">-&gt;</span><span class="token function">pfadd</span><span class="token punctuation">(</span>key<span class="token punctuation">,</span> <span class="token punctuation">{</span>member<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取唯一访问数</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> <span class="token function">getUniqueCount</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> count <span class="token operator">=</span> redis_<span class="token operator">-&gt;</span><span class="token function">pfcount</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> count<span class="token punctuation">.</span><span class="token function">value_or</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 合并统计（多个 Key）</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> <span class="token function">mergeAndCount</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token operator">&gt;</span><span class="token operator">&amp;</span> keys<span class="token punctuation">,</span></span>
<span class="line">                             <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> destKey<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        redis_<span class="token operator">-&gt;</span><span class="token function">pfmerge</span><span class="token punctuation">(</span>destKey<span class="token punctuation">,</span> keys<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> keys<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">getUniqueCount</span><span class="token punctuation">(</span>destKey<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-pub-sub-消息" tabindex="-1"><a class="header-anchor" href="#_4-2-pub-sub-消息"><span>4.2 Pub/Sub 消息</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Redis Pub/Sub 跨服通信</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RedisPubSub</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 发布消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">publish</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> channel<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> message<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        redis_<span class="token operator">-&gt;</span><span class="token function">publish</span><span class="token punctuation">(</span>channel<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 订阅消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">subscribe</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> channel<span class="token punctuation">,</span></span>
<span class="line">                  std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span><span class="token punctuation">)</span><span class="token operator">&gt;</span> callback<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        subscribers_<span class="token punctuation">[</span>channel<span class="token punctuation">]</span> <span class="token operator">=</span> callback<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 在后台线程中订阅</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span><span class="token function">thread</span><span class="token punctuation">(</span><span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">,</span> channel<span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">auto</span> msg <span class="token operator">=</span> redis_<span class="token operator">-&gt;</span><span class="token function">subscribe</span><span class="token punctuation">(</span><span class="token punctuation">{</span>channel<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>msg <span class="token operator">&amp;&amp;</span> msg<span class="token operator">-&gt;</span>channel <span class="token operator">==</span> channel<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">if</span> <span class="token punctuation">(</span>subscribers_<span class="token punctuation">.</span><span class="token function">count</span><span class="token punctuation">(</span>channel<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                        subscribers_<span class="token punctuation">[</span>channel<span class="token punctuation">]</span><span class="token punctuation">(</span>msg<span class="token operator">-&gt;</span>payload<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token punctuation">}</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">detach</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span><span class="token punctuation">)</span><span class="token operator">&gt;&gt;</span> subscribers_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> running_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、最佳实践" tabindex="-1"><a class="header-anchor" href="#五、最佳实践"><span>五、最佳实践</span></a></h2><h3 id="_5-1-数据结构选择" tabindex="-1"><a class="header-anchor" href="#_5-1-数据结构选择"><span>5.1 数据结构选择</span></a></h3><table><thead><tr><th>场景</th><th>Redis 类型</th><th>原因</th></tr></thead><tbody><tr><td><strong>排行榜</strong></td><td>ZSet</td><td>自动排序，范围查询</td></tr><tr><td><strong>玩家数据</strong></td><td>Hash</td><td>结构化存储，字段访问</td></tr><tr><td><strong>在线列表</strong></td><td>Set</td><td>去重，集合操作</td></tr><tr><td><strong>消息队列</strong></td><td>List</td><td>FIFO，阻塞操作</td></tr><tr><td><strong>计数器</strong></td><td>String + INCR</td><td>原子递增</td></tr><tr><td><strong>功能开关</strong></td><td>Bitmap</td><td>空间效率高</td></tr><tr><td><strong>位置服务</strong></td><td>Geo</td><td>距离计算，范围查询</td></tr></tbody></table><h3 id="_5-2-性能优化" tabindex="-1"><a class="header-anchor" href="#_5-2-性能优化"><span>5.2 性能优化</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  Redis 性能优化                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 使用 Pipeline 批量操作                                 │</span>
<span class="line">│     ├── 减少网络往返                                      │</span>
<span class="line">│     └── 提升吞吐量                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 合理设置过期时间                                       │</span>
<span class="line">│     ├── 避免内存无限增长                                  │</span>
<span class="line">│     └── 自动清理过期数据                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 使用连接池                                             │</span>
<span class="line">│     ├── 减少连接创建开销                                  │</span>
<span class="line">│     └── 复用连接                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 选择合适的数据结构                                     │</span>
<span class="line">│     ├── ZSet vs Hash + sort                               │</span>
<span class="line">│     ├── Set vs List (去重需求)                             │</span>
<span class="line">│     └── Bitmap vs Set (空间效率)                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 使用 Lua 脚本                                          │</span>
<span class="line">│     ├── 原子操作                                           │</span>
<span class="line">│     └── 减少网络传输                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-3-注意事项" tabindex="-1"><a class="header-anchor" href="#_5-3-注意事项"><span>5.3 注意事项</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  Redis 使用注意                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 数据持久化                                               │</span>
<span class="line">│     ├── Redis 不是最终存储                                 │</span>
<span class="line">│     ├── 需要定期同步到 MySQL                               │</span>
<span class="line">│     └── 使用 RDB/AOF 持久化                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 内存管理                                                │</span>
<span class="line">│     ├── 设置 maxmemory 限制                                │</span>
<span class="line">│     ├── 使用 allkeys-lru 淘汰策略                          │</span>
<span class="line">│     └── 监控内存使用                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 高可用配置                                               │</span>
<span class="line">│     ├── 使用 Redis Sentinel 主从                           │</span>
<span class="line">│     ├── 配置自动故障转移                                   │</span>
<span class="line">│     └── 监控节点状态                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 安全设置                                                │</span>
<span class="line">│     ├── 设置密码认证                                        │</span>
<span class="line">│     ├── 绑定特定 IP                                        │</span>
<span class="line">│     └── 禁用危险命令                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、总结" tabindex="-1"><a class="header-anchor" href="#六、总结"><span>六、总结</span></a></h2><h3 id="redis-应用场景总结" tabindex="-1"><a class="header-anchor" href="#redis-应用场景总结"><span>Redis 应用场景总结</span></a></h3><table><thead><tr><th>应用场景</th><th>Redis 类型</th><th>复杂度</th><th>重要性</th></tr></thead><tbody><tr><td><strong>排行榜</strong></td><td>ZSet</td><td>低</td><td>高</td></tr><tr><td><strong>在线管理</strong></td><td>Set + Hash</td><td>低</td><td>高</td></tr><tr><td><strong>分布式锁</strong></td><td>String + Lua</td><td>中</td><td>中</td></tr><tr><td><strong>限流防刷</strong></td><td>ZSet/String</td><td>中</td><td>高</td></tr><tr><td><strong>消息队列</strong></td><td>List/PubSub</td><td>低</td><td>中</td></tr><tr><td><strong>位置服务</strong></td><td>Geo</td><td>中</td><td>中</td></tr><tr><td><strong>统计去重</strong></td><td>HyperLogLog</td><td>低</td><td>低</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. Redis 作为缓存层</span>
<span class="line">   - 热点数据缓存</span>
<span class="line">   - 减轻数据库压力</span>
<span class="line">   - 定期同步持久化</span>
<span class="line"></span>
<span class="line">2. 合理使用数据类型</span>
<span class="line">   - 根据场景选择类型</span>
<span class="line">   - 注意内存占用</span>
<span class="line">   - 优化数据结构</span>
<span class="line"></span>
<span class="line">3. 保证数据安全</span>
<span class="line">   - 主从复制</span>
<span class="line">   - 持久化配置</span>
<span class="line">   - 监控告警</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://redis.io/documentation" target="_blank" rel="noopener noreferrer">Redis 官方文档</a></li><li><a href="https://www.kbelab.com/manual/dbmgr.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 数据库管理</a></li><li><a href="https://redis.io/topics/cluster-tutorial" target="_blank" rel="noopener noreferrer">Redis 集群教程</a></li></ul>`,51)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};