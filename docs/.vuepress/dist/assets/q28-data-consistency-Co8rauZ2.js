import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q28-data-consistency.html","title":"Q28: 如何解决数据一致性问题？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q28-data-consistency.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q28-data-consistency.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q28-如何解决数据一致性问题" tabindex="-1"><a class="header-anchor" href="#q28-如何解决数据一致性问题"><span>Q28: 如何解决数据一致性问题？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对分布式系统数据一致性的理解：</p><ul><li>一致性问题的分类和原因</li><li>事务处理机制</li><li>分布式事务解决方案</li><li>最终一致性实现</li><li>KBEngine 的数据一致性处理</li></ul><hr><h2 id="一、数据一致性问题" tabindex="-1"><a class="header-anchor" href="#一、数据一致性问题"><span>一、数据一致性问题</span></a></h2><h3 id="_1-1-一致性问题分类" tabindex="-1"><a class="header-anchor" href="#_1-1-一致性问题分类"><span>1.1 一致性问题分类</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    数据一致性问题分类                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 更新丢失                                                │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  场景: 两个更新同时修改同一数据                     │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  玩家 A 有 100 金币                                  │       │</span>
<span class="line">│  │  事务1: 花费 50 金币                                 │       │</span>
<span class="line">│  │  事务2: 花费 30 金币                                 │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  结果: 可能只剩 70 或 50 金币（一个更新丢失）          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 脏读                                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  场景: 读取未提交的数据                             │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  事务1: 转账 100 金币（未提交）                      │       │</span>
<span class="line">│  │  事务2: 读取余额（读到增加后的余额）                  │       │</span>
<span class="line">│  │  事务1: 回滚                                         │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  结果: 事务2 读到脏数据                              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 不可重复读                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  场景: 同一事务内两次读取结果不同                   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  事务1: 读取余额 = 100                              │       │</span>
<span class="line">│  │  事务2: 修改余额 = 80                               │       │</span>
<span class="line">│  │  事务1: 再次读取余额 = 80（与之前不同）              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 幻读                                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  场景: 查询结果集发生变化                           │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  事务1: 查询等级 &gt; 10 的玩家（3人）                  │       │</span>
<span class="line">│  │  事务2: 新增一个等级 11 的玩家                       │       │</span>
<span class="line">│  │  事务1: 再次查询（4人，多了1个）                      │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-cap-定理" tabindex="-1"><a class="header-anchor" href="#_1-2-cap-定理"><span>1.2 CAP 定理</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      CAP 定理                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  在分布式系统中，三者最多只能同时满足两点:                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│       C ────────────────────────────────────────┐          │</span>
<span class="line">│       │ 一致性 (Consistency)                      │          │</span>
<span class="line">│       │ 所有节点在同一时间看到相同的数据              │          │</span>
<span class="line">│       │                                   │          │</span>
<span class="line">│       │                            ┌────┴────┐     │          │</span>
<span class="line">│       │                            │         │     │          │</span>
<span class="line">│       │                          A ────────┐ │     │          │</span>
<span class="line">│       │                          │可用性   │ │     │          │</span>
<span class="line">│       │                          │(Availability)│    │          │</span>
<span class="line">│       │                          └─────────┘ │     │          │</span>
<span class="line">│       │                                       │     │          │</span>
<span class="line">│       │                          ┌─────────────┘     │          │</span>
<span class="line">│       │                          │                     │          │</span>
<span class="line">│       │                          P ──────────────┐    │          │</span>
<span class="line">│       │                          分区容错性 (Partition Tolerance)│</span>
<span class="line">│       │                          网络分区时系统仍能运行       │          │</span>
<span class="line">│       └──────────────────────────┴───────────────────────┘          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  MMO 中的权衡:                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  CP: 强一致性，牺牲可用性                           │       │</span>
<span class="line">│  │  - 交易系统必须保证一致                              │       │</span>
<span class="line">│  │  - 网络分区时暂停服务                                │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  AP: 高可用，牺牲一致性                             │       │</span>
<span class="line">│  │  - 聊天系统可以接受延迟                              │       │</span>
<span class="line">│  │  - 位置同步允许短暂不一致                            │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  BASE: 基本可用，软状态，最终一致                    │       │</span>
<span class="line">│  │  - 大部分游戏系统采用此方案                          │       │</span>
<span class="line">│  │  - 保证最终一致性即可                                 │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、事务处理" tabindex="-1"><a class="header-anchor" href="#二、事务处理"><span>二、事务处理</span></a></h2><h3 id="_2-1-acid-特性" tabindex="-1"><a class="header-anchor" href="#_2-1-acid-特性"><span>2.1 ACID 特性</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    ACID 事务特性                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  A - 原子性 (Atomicity)                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  事务中的操作要么全部成功，要么全部失败               │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  示例: 转账操作                                     │       │</span>
<span class="line">│  │  1. 扣除 A 的金币                                  │       │</span>
<span class="line">│  │  2. 增加 B 的金币                                  │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  如果步骤2失败，步骤1也要回滚                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  C - 一致性 (Consistency)                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  事务执行前后，数据库都处于一致状态                   │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  示例: 金币总量不变                                 │       │</span>
<span class="line">│  │  转账只是金币从一个玩家到另一个玩家                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  I - 隔离性 (Isolation)                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  并发事务之间相互隔离                               │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  隔离级别:                                         │       │</span>
<span class="line">│  │  ├── 读未提交 (READ UNCOMMITTED)                  │       │</span>
<span class="line">│  │  ├── 读已提交 (READ COMMITTED)                    │       │</span>
<span class="line">│  │  ├── 可重复读 (REPEATABLE READ) ← 推荐            │       │</span>
<span class="line">│  │  └── 串行化 (SERIALIZABLE)                        │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  D - 持久性 (Durability)                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  事务提交后，数据永久保存                           │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  即使系统崩溃，已提交的数据也不会丢失                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-事务隔离级别" tabindex="-1"><a class="header-anchor" href="#_2-2-事务隔离级别"><span>2.2 事务隔离级别</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  事务隔离级别对比                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  隔离级别           │ 脏读 │ 不可重复读 │ 幻读 │ 性能      │</span>
<span class="line">│  ├───────────────────────────────────────────────────────┤ │</span>
<span class="line">│  │ 读未提交         │  可能 │  可能    │ 可能 │ 最高      │ │</span>
<span class="line">│  │ 读已提交         │  避免 │  可能    │ 可能 │ 高        │ │</span>
<span class="line">│  │ 可重复读 (推荐)  │  避免 │  避免    │ 可能 │ 中        │ │</span>
<span class="line">│  │ 串行化           │  避免 │  避免    │ 避免 │ 低        │ │</span>
<span class="line">│                                                             │</span>
<span class="line">│  MMO 推荐设置:                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  SET GLOBAL TRANSACTION ISOLATION LEVEL          │       │</span>
<span class="line">│  │  REPEATABLE READ;                                │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  理由:                                            │       │</span>
<span class="line">│  │  - 避免脏读和不可重复读                             │       │</span>
<span class="line">│  │  - 性能影响可接受                                   │       │</span>
<span class="line">│  │  - 幻读在游戏场景较少                               │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-事务实现" tabindex="-1"><a class="header-anchor" href="#_2-3-事务实现"><span>2.3 事务实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 数据库事务封装</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">DatabaseTransaction</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">DatabaseTransaction</span><span class="token punctuation">(</span>Database<span class="token operator">*</span> db<span class="token punctuation">)</span> <span class="token operator">:</span> <span class="token function">db_</span><span class="token punctuation">(</span>db<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">committed_</span><span class="token punctuation">(</span><span class="token boolean">false</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 开始事务</span></span>
<span class="line">        db_<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span><span class="token string">&quot;START TRANSACTION&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">~</span><span class="token function">DatabaseTransaction</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>committed_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 未提交则回滚</span></span>
<span class="line">            <span class="token function">rollback</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 提交事务</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">commit</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>db_<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span><span class="token string">&quot;COMMIT&quot;</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            committed_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 回滚事务</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">rollback</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        db_<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span><span class="token string">&quot;ROLLBACK&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        committed_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span> <span class="token comment">// 标记为已处理</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 执行 SQL</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">execute</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> sql<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> db_<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span>sql<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 查询</span></span>
<span class="line">    Result <span class="token function">query</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> sql<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> db_<span class="token operator">-&gt;</span><span class="token function">query</span><span class="token punctuation">(</span>sql<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    Database<span class="token operator">*</span> db_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> committed_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">transferGold</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> fromPlayer<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> toPlayer<span class="token punctuation">,</span> <span class="token keyword">int64_t</span> amount<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    Database<span class="token operator">*</span> db <span class="token operator">=</span> <span class="token class-name">Database</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    DatabaseTransaction <span class="token function">txn</span><span class="token punctuation">(</span>db<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 1. 检查发送者余额</span></span>
<span class="line">    <span class="token keyword">auto</span> result <span class="token operator">=</span> txn<span class="token punctuation">.</span><span class="token function">query</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">        <span class="token string">&quot;SELECT gold FROM player WHERE id = {} FOR UPDATE&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        fromPlayer</span>
<span class="line">    <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span>result<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">throw</span> std<span class="token double-colon punctuation">::</span><span class="token function">runtime_error</span><span class="token punctuation">(</span><span class="token string">&quot;Player not found&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">int64_t</span> gold <span class="token operator">=</span> result<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token string">&quot;gold&quot;</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span>gold <span class="token operator">&lt;</span> amount<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">throw</span> std<span class="token double-colon punctuation">::</span><span class="token function">runtime_error</span><span class="token punctuation">(</span><span class="token string">&quot;Insufficient gold&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 2. 扣除发送者金币</span></span>
<span class="line">    txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">        <span class="token string">&quot;UPDATE player SET gold = gold - {} WHERE id = {}&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        amount<span class="token punctuation">,</span> fromPlayer</span>
<span class="line">    <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 3. 增加接收者金币</span></span>
<span class="line">    txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">        <span class="token string">&quot;UPDATE player SET gold = gold + {} WHERE id = {}&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        amount<span class="token punctuation">,</span> toPlayer</span>
<span class="line">    <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 4. 记录交易日志</span></span>
<span class="line">    txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">        <span class="token string">&quot;INSERT INTO trade_log (from_player, to_player, amount, time) &quot;</span></span>
<span class="line">        <span class="token string">&quot;VALUES ({}, {}, {}, NOW())&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        fromPlayer<span class="token punctuation">,</span> toPlayer<span class="token punctuation">,</span> amount</span>
<span class="line">    <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 提交事务</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>txn<span class="token punctuation">.</span><span class="token function">commit</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">throw</span> std<span class="token double-colon punctuation">::</span><span class="token function">runtime_error</span><span class="token punctuation">(</span><span class="token string">&quot;Transaction commit failed&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、分布式事务" tabindex="-1"><a class="header-anchor" href="#三、分布式事务"><span>三、分布式事务</span></a></h2><h3 id="_3-1-两阶段提交-2pc" tabindex="-1"><a class="header-anchor" href="#_3-1-两阶段提交-2pc"><span>3.1 两阶段提交 (2PC)</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    两阶段提交 (2PC)                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  准备阶段 (Phase 1):                                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  协调者 ──prepare──► 参与者 A                      │       │</span>
<span class="line">│  │  协调者 ──prepare──► 参与者 B                      │       │</span>
<span class="line">│  │  协调者 ──prepare──► 参与者 C                      │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  参与者 A ──yes────────► 协调者                    │       │</span>
<span class="line">│  │  参与者 B ──yes────────► 协调者                    │       │</span>
<span class="line">│  │  参与者 C ──no─────────► 协调者                    │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  提交阶段 (Phase 2):                                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  有一方拒绝，全部回滚:                              │       │</span>
<span class="line">│  │  协调者 ──rollback──► 参与者 A                     │       │</span>
<span class="line">│  │  协调者 ──rollback──► 参与者 B                     │       │</span>
<span class="line">│  │  协调者 ──rollback──► 参与者 C                     │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  全部同意，全部提交:                                │       │</span>
<span class="line">│  │  协调者 ──commit────► 参与者 A                     │       │</span>
<span class="line">│  │  协调者 ──commit────► 参与者 B                     │       │</span>
<span class="line">│  │  协调者 ──commit────► 参与者 C                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题:                                                     │</span>
<span class="line">│  ├── 协调者单点故障                                       │</span>
<span class="line">│  ├── 阻塞，性能差                                         │</span>
<span class="line">│  └── 数据不一致窗口                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-tcc-事务" tabindex="-1"><a class="header-anchor" href="#_2-2-tcc-事务"><span>2.2 TCC 事务</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    TCC (Try-Confirm-Cancel)                   │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  TCC 将业务逻辑分成三个阶段:                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Try 阶段:                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  完成业务检查                                       │       │</span>
<span class="line">│  │  预留必须资源                                       │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  示例: 转账操作                                     │       │</span>
<span class="line">│  │  - 检查账户余额                                     │       │</span>
<span class="line">│  │  - 冻结转账金额                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Confirm 阶段:                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  确认执行业务                                       │       │</span>
<span class="line">│  │  使用 Try 阶段预留的资源                           │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  示例: 转账操作                                     │       │</span>
<span class="line">│  │  - 扣除发送者冻结金额                               │       │</span>
<span class="line">│  │  - 增加接收者金额                                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Cancel 阶段:                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  取消执行业务                                       │       │</span>
<span class="line">│  │  释放 Try 阶段预留的资源                           │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  示例: 转账操作                                     │       │</span>
<span class="line">│  │  - 解冻发送者金额                                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优势:                                                     │</span>
<span class="line">│  ├── 性能比 2PC 好                                         │</span>
<span class="line">│  ├── 业务层控制，灵活                                       │</span>
<span class="line">│  └── 适合长事务                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// TCC 事务实现</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// TCC 接口</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TCCTransaction</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token keyword">try</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">confirm</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">cancel</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 转账 TCC 实现</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TransferTCC</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">TCCTransaction</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">TransferTCC</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> from<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> to<span class="token punctuation">,</span> <span class="token keyword">int64_t</span> amount<span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">from_</span><span class="token punctuation">(</span>from<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">to_</span><span class="token punctuation">(</span>to<span class="token punctuation">)</span><span class="token punctuation">,</span> <span class="token function">amount_</span><span class="token punctuation">(</span>amount<span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Try 阶段: 检查并冻结资金</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token keyword">try</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">override</span> <span class="token punctuation">{</span></span>
<span class="line">        Database<span class="token operator">*</span> db <span class="token operator">=</span> <span class="token class-name">Database</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        DatabaseTransaction <span class="token function">txn</span><span class="token punctuation">(</span>db<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 检查发送者余额</span></span>
<span class="line">        <span class="token keyword">auto</span> result <span class="token operator">=</span> txn<span class="token punctuation">.</span><span class="token function">query</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;SELECT gold FROM player WHERE id = {} FOR UPDATE&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            from_</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>result<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> result<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token string">&quot;gold&quot;</span><span class="token punctuation">]</span> <span class="token operator">&lt;</span> amount_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 冻结金额</span></span>
<span class="line">        txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;UPDATE player SET frozen_gold = frozen_gold + {} &quot;</span></span>
<span class="line">            <span class="token string">&quot;WHERE id = {}&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            amount_<span class="token punctuation">,</span> from_</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 记录冻结</span></span>
<span class="line">        txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;INSERT INTO freeze_log (player_id, amount, ref_id) &quot;</span></span>
<span class="line">            <span class="token string">&quot;VALUES ({}, {}, &#39;{}&#39;)&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            from_<span class="token punctuation">,</span> amount_<span class="token punctuation">,</span> refId_</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> txn<span class="token punctuation">.</span><span class="token function">commit</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Confirm 阶段: 完成转账</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">confirm</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">override</span> <span class="token punctuation">{</span></span>
<span class="line">        Database<span class="token operator">*</span> db <span class="token operator">=</span> <span class="token class-name">Database</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        DatabaseTransaction <span class="token function">txn</span><span class="token punctuation">(</span>db<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 扣除冻结金额</span></span>
<span class="line">        txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;UPDATE player SET gold = gold - {}, frozen_gold = frozen_gold - {} &quot;</span></span>
<span class="line">            <span class="token string">&quot;WHERE id = {}&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            amount_<span class="token punctuation">,</span> amount_<span class="token punctuation">,</span> from_</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 增加接收者</span></span>
<span class="line">        txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;UPDATE player SET gold = gold + {} WHERE id = {}&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            amount_<span class="token punctuation">,</span> to_</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 更新冻结记录</span></span>
<span class="line">        txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;UPDATE freeze_log SET status = &#39;confirmed&#39; &quot;</span></span>
<span class="line">            <span class="token string">&quot;WHERE ref_id = &#39;{}&#39;&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            refId_</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> txn<span class="token punctuation">.</span><span class="token function">commit</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Cancel 阶段: 解冻</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">cancel</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">override</span> <span class="token punctuation">{</span></span>
<span class="line">        Database<span class="token operator">*</span> db <span class="token operator">=</span> <span class="token class-name">Database</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        DatabaseTransaction <span class="token function">txn</span><span class="token punctuation">(</span>db<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 解冻金额</span></span>
<span class="line">        txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;UPDATE player SET frozen_gold = frozen_gold - {} &quot;</span></span>
<span class="line">            <span class="token string">&quot;WHERE id = {}&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            amount_<span class="token punctuation">,</span> from_</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 更新冻结记录</span></span>
<span class="line">        txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;UPDATE freeze_log SET status = &#39;cancelled&#39; &quot;</span></span>
<span class="line">            <span class="token string">&quot;WHERE ref_id = &#39;{}&#39;&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            refId_</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> txn<span class="token punctuation">.</span><span class="token function">commit</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> from_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> to_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int64_t</span> amount_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string refId_ <span class="token operator">=</span> <span class="token function">generateUUID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// TCC 协调器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TCCCoordinator</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 执行 TCC 事务</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">execute</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_ptr<span class="token operator">&lt;</span>TCCTransaction<span class="token operator">&gt;&gt;</span> transactions<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// Try 阶段</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> txn <span class="token operator">:</span> transactions<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>txn<span class="token operator">-&gt;</span><span class="token keyword">try</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 失败，取消已执行的 Try</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> t <span class="token operator">:</span> executedTries_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    t<span class="token operator">-&gt;</span><span class="token function">cancel</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            executedTries_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>txn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// Confirm 阶段</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> txn <span class="token operator">:</span> transactions<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>txn<span class="token operator">-&gt;</span><span class="token function">confirm</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// Confirm 失败，记录日志</span></span>
<span class="line">                <span class="token function">LOG_ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;TCC Confirm failed, need manual intervention&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token comment">// 生产环境需要补偿机制</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_ptr<span class="token operator">&lt;</span>TCCTransaction<span class="token operator">&gt;&gt;</span> executedTries_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、最终一致性" tabindex="-1"><a class="header-anchor" href="#四、最终一致性"><span>四、最终一致性</span></a></h2><h3 id="_4-1-base-理论" tabindex="-1"><a class="header-anchor" href="#_4-1-base-理论"><span>4.1 BASE 理论</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      BASE 理论                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  B - Basically Available (基本可用)                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  系统保证可用性，但不保证强一致                       │       │</span>
<span class="line">│  │  即使部分节点故障，系统仍能响应                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  S - Soft State (软状态)                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  允许系统中的数据存在中间状态                         │       │</span>
<span class="line">│  │  这个中间状态不影响系统可用性                         │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  E - Eventually Consistent (最终一致)                      │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  系统保证在没有新更新的情况下                       │       │</span>
<span class="line">│  │  经过一段时间后，数据最终达到一致状态                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-最终一致性实现" tabindex="-1"><a class="header-anchor" href="#_4-2-最终一致性实现"><span>4.2 最终一致性实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 最终一致性：异步同步 + 冲突解决</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EventuallyConsistentStore</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 更新玩家数据（异步写入数据库）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updatePlayer</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">,</span></span>
<span class="line">                     <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> value<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 立即写入缓存</span></span>
<span class="line">        cache_<span class="token punctuation">.</span><span class="token function">set</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> key<span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 加入同步队列</span></span>
<span class="line">        SyncTask task<span class="token punctuation">;</span></span>
<span class="line">        task<span class="token punctuation">.</span>playerId <span class="token operator">=</span> playerId<span class="token punctuation">;</span></span>
<span class="line">        task<span class="token punctuation">.</span>key <span class="token operator">=</span> key<span class="token punctuation">;</span></span>
<span class="line">        task<span class="token punctuation">.</span>value <span class="token operator">=</span> value<span class="token punctuation">;</span></span>
<span class="line">        task<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        task<span class="token punctuation">.</span>version <span class="token operator">=</span> <span class="token operator">++</span>version_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        syncQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>task<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 读取玩家数据</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">getPlayer</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 先读缓存</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string value <span class="token operator">=</span> cache_<span class="token punctuation">.</span><span class="token function">get</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>value<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> value<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 缓存未命中，读数据库</span></span>
<span class="line">        value <span class="token operator">=</span> <span class="token function">loadFromDatabase</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>value<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            cache_<span class="token punctuation">.</span><span class="token function">set</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> key<span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> value<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 同步队列处理</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">syncLoop</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            SyncTask task<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>syncQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span>task<span class="token punctuation">,</span> <span class="token number">1000</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">syncToDatabase</span><span class="token punctuation">(</span>task<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 批量刷新缓存到数据库</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>syncQueue_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;=</span> BATCH_SIZE<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">flushBatch</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">syncToDatabase</span><span class="token punctuation">(</span><span class="token keyword">const</span> SyncTask<span class="token operator">&amp;</span> task<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 使用 UPSERT</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string sql <span class="token operator">=</span> fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;INSERT INTO player_data (player_id, data_key, data_value, version) &quot;</span></span>
<span class="line">            <span class="token string">&quot;VALUES ({}, &#39;{}&#39;, &#39;{}&#39;, {}) &quot;</span></span>
<span class="line">            <span class="token string">&quot;ON DUPLICATE KEY UPDATE &quot;</span></span>
<span class="line">            <span class="token string">&quot;data_value = VALUES(data_value), &quot;</span></span>
<span class="line">            <span class="token string">&quot;version = VALUES(version)&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            task<span class="token punctuation">.</span>playerId<span class="token punctuation">,</span> task<span class="token punctuation">.</span>key<span class="token punctuation">,</span> task<span class="token punctuation">.</span>value<span class="token punctuation">,</span> task<span class="token punctuation">.</span>version</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        database_<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span>sql<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">flushBatch</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>SyncTask<span class="token operator">&gt;</span> batch<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>syncQueue_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0</span> <span class="token operator">&amp;&amp;</span> batch<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> BATCH_SIZE<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            SyncTask task<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>syncQueue_<span class="token punctuation">.</span><span class="token function">try_pop</span><span class="token punctuation">(</span>task<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                batch<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>task<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 批量写入</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>batch<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">bulkInsert</span><span class="token punctuation">(</span>batch<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">SyncTask</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> playerId<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string key<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string value<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> version<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    ThreadSafeQueue<span class="token operator">&lt;</span>SyncTask<span class="token operator">&gt;</span> syncQueue_<span class="token punctuation">;</span></span>
<span class="line">    MemoryCache cache_<span class="token punctuation">;</span></span>
<span class="line">    Database<span class="token operator">*</span> database_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t BATCH_SIZE <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> version_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 冲突解决：版本向量</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">VersionVector</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 检查是否冲突</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">hasConflict</span><span class="token punctuation">(</span><span class="token keyword">const</span> Operation<span class="token operator">&amp;</span> op1<span class="token punctuation">,</span> <span class="token keyword">const</span> Operation<span class="token operator">&amp;</span> op2<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 并发操作可能冲突</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token operator">!</span><span class="token punctuation">(</span>op1<span class="token punctuation">.</span>version <span class="token operator">&lt;</span> op2<span class="token punctuation">.</span>version <span class="token operator">||</span> op2<span class="token punctuation">.</span>version <span class="token operator">&lt;</span> op1<span class="token punctuation">.</span>version<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 合并操作</span></span>
<span class="line">    Operation <span class="token function">merge</span><span class="token punctuation">(</span><span class="token keyword">const</span> Operation<span class="token operator">&amp;</span> op1<span class="token punctuation">,</span> <span class="token keyword">const</span> Operation<span class="token operator">&amp;</span> op2<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">hasConflict</span><span class="token punctuation">(</span>op1<span class="token punctuation">,</span> op2<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 无冲突，选择较新的</span></span>
<span class="line">            <span class="token keyword">return</span> op1<span class="token punctuation">.</span>version <span class="token operator">&gt;</span> op2<span class="token punctuation">.</span>version <span class="token operator">?</span> op1 <span class="token operator">:</span> op2<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 有冲突，应用解决策略</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">resolveConflict</span><span class="token punctuation">(</span>op1<span class="token punctuation">,</span> op2<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    Operation <span class="token function">resolveConflict</span><span class="token punctuation">(</span><span class="token keyword">const</span> Operation<span class="token operator">&amp;</span> op1<span class="token punctuation">,</span> <span class="token keyword">const</span> Operation<span class="token operator">&amp;</span> op2<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 策略1: 时间戳优先</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>op1<span class="token punctuation">.</span>timestamp <span class="token operator">&gt;</span> op2<span class="token punctuation">.</span>timestamp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> op1<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> op2<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 策略2: 业务规则</span></span>
<span class="line">        <span class="token comment">// 例如: 加法操作可以合并，取最大值等</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Operation</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> playerId<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string key<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string value<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> version<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、kbengine-数据一致性" tabindex="-1"><a class="header-anchor" href="#五、kbengine-数据一致性"><span>五、KBEngine 数据一致性</span></a></h2><h3 id="_5-1-kbengine-存档机制" tabindex="-1"><a class="header-anchor" href="#_5-1-kbengine-存档机制"><span>5.1 KBEngine 存档机制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 自动存档机制</span></span>
<span class="line"><span class="token comment">// src/server/entitydef/entity_def.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 写存档（自动调用）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">writeToDB</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 检查是否需要存档</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">shouldArchive</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 序列化实体数据</span></span>
<span class="line">        MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">addToStream</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 发送到 DBMgr</span></span>
<span class="line">        Bundle<span class="token operator">*</span> pBundle <span class="token operator">=</span> <span class="token class-name">DBMgrInterface</span><span class="token double-colon punctuation">::</span><span class="token function">createBundle</span><span class="token punctuation">(</span></span>
<span class="line">            dbmgr<span class="token punctuation">,</span></span>
<span class="line">            <span class="token double-colon punctuation">::</span>onRemoteAutoLoadEntityCreateAccountMailbox</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>pBundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> id_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>pBundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> className_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>pBundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> stream<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token class-name">DBMgrInterface</span><span class="token double-colon punctuation">::</span><span class="token function">send</span><span class="token punctuation">(</span>pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新存档时间</span></span>
<span class="line">        lastArchiveTime_ <span class="token operator">=</span> <span class="token function">timeStamp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 检查是否需要存档</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">shouldArchive</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> now <span class="token operator">=</span> <span class="token function">timeStamp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> interval <span class="token operator">=</span> now <span class="token operator">-</span> lastArchiveTime_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> interval <span class="token operator">&gt;</span> g_kbeSrvConfig<span class="token punctuation">.</span><span class="token function">entityArchiveInterval</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 从数据库加载</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">loadFromDB</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 请求数据库加载数据</span></span>
<span class="line">        Bundle<span class="token operator">*</span> pBundle <span class="token operator">=</span> <span class="token class-name">DBMgrInterface</span><span class="token double-colon punctuation">::</span><span class="token function">createBundle</span><span class="token punctuation">(</span></span>
<span class="line">            dbmgr<span class="token punctuation">,</span></span>
<span class="line">            dbmgr<span class="token double-colon punctuation">::</span>queryEntity</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>pBundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> id<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token class-name">DBMgrInterface</span><span class="token double-colon punctuation">::</span><span class="token function">send</span><span class="token punctuation">(</span>pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> lastArchiveTime_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-kbengine-数据备份" tabindex="-1"><a class="header-anchor" href="#_5-2-kbengine-数据备份"><span>5.2 KBEngine 数据备份</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 数据库备份脚本</span></span>
<span class="line"><span class="token comment"># db_backups/backup_db.sh</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">#!/bin/bash</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 配置</span></span>
<span class="line">DB_USER<span class="token operator">=</span><span class="token string">&quot;kbengine&quot;</span></span>
<span class="line">DB_PASS<span class="token operator">=</span><span class="token string">&quot;password&quot;</span></span>
<span class="line">DB_NAME<span class="token operator">=</span><span class="token string">&quot;kbengine&quot;</span></span>
<span class="line">BACKUP_DIR<span class="token operator">=</span><span class="token string">&quot;/backup/kbengine&quot;</span></span>
<span class="line">DATE<span class="token operator">=</span>$<span class="token punctuation">(</span>date <span class="token operator">+</span><span class="token operator">%</span>Y<span class="token operator">%</span>m<span class="token operator">%</span>d_<span class="token operator">%</span>H<span class="token operator">%</span>M<span class="token operator">%</span>S<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 创建备份目录</span></span>
<span class="line">mkdir <span class="token operator">-</span>p $BACKUP_DIR</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 备份数据库</span></span>
<span class="line">mysqldump <span class="token operator">-</span>u$DB_USER <span class="token operator">-</span>p$DB_PASS $DB_NAME <span class="token operator">&gt;</span> $BACKUP_DIR<span class="token operator">/</span>kbengine_$DATE<span class="token punctuation">.</span>sql</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 压缩备份</span></span>
<span class="line">gzip $BACKUP_DIR<span class="token operator">/</span>kbengine_$DATE<span class="token punctuation">.</span>sql</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 删除7天前的备份</span></span>
<span class="line">find $BACKUP_DIR <span class="token operator">-</span>name <span class="token string">&quot;*.sql.gz&quot;</span> <span class="token operator">-</span>mtime <span class="token operator">+</span><span class="token number">7</span> <span class="token operator">-</span>delete</span>
<span class="line"></span>
<span class="line">echo <span class="token string">&quot;Backup completed: kbengine_$DATE.sql.gz&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-一致性策略选择" tabindex="-1"><a class="header-anchor" href="#_6-1-一致性策略选择"><span>6.1 一致性策略选择</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              不同场景的一致性策略                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  强一致性 (必须使用事务):                                   │</span>
<span class="line">│  ├── 货币/钻石交易                                          │</span>
<span class="line">│  ├── 道具购买/消耗                                          │</span>
<span class="line">│  ├── 跨服数据同步                                           │</span>
<span class="line">│  └── 重要状态变化                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  最终一致性 (可异步):                                       │</span>
<span class="line">│  ├── 玩家状态数据                                           │</span>
<span class="line">│  ├── 非重要日志                                             │</span>
<span class="line">│  ├── 统计数据                                               │</span>
<span class="line">│  └── 排行榜                                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  无需一致性 (独立操作):                                     │</span>
<span class="line">│  ├── 聊天消息                                               │</span>
<span class="line">│  ├── 临时效果                                               │</span>
<span class="line">│  └── 显示数据                                               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-异步处理模式" tabindex="-1"><a class="header-anchor" href="#_6-2-异步处理模式"><span>6.2 异步处理模式</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 异步处理最佳实践</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AsyncDataProcessor</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 关键操作：同步 + 重试</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">syncUpdate</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">int64_t</span> goldChange<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> retries <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>retries <span class="token operator">&lt;</span> MAX_RETRIES<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">try</span> <span class="token punctuation">{</span></span>
<span class="line">                DatabaseTransaction <span class="token function">txn</span><span class="token punctuation">(</span>db_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment">// 检查余额</span></span>
<span class="line">                <span class="token keyword">auto</span> result <span class="token operator">=</span> txn<span class="token punctuation">.</span><span class="token function">query</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">                    <span class="token string">&quot;SELECT gold FROM player_{} WHERE id = {} FOR UPDATE&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                    playerId <span class="token operator">%</span> TABLE_COUNT<span class="token punctuation">,</span> playerId</span>
<span class="line">                <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>result<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">int64_t</span> currentGold <span class="token operator">=</span> result<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token string">&quot;gold&quot;</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>currentGold <span class="token operator">+</span> goldChange <span class="token operator">&lt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span> <span class="token comment">// 余额不足</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment">// 更新</span></span>
<span class="line">                txn<span class="token punctuation">.</span><span class="token function">execute</span><span class="token punctuation">(</span>fmt<span class="token double-colon punctuation">::</span><span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">                    <span class="token string">&quot;UPDATE player_{} SET gold = gold + {} WHERE id = {}&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                    playerId <span class="token operator">%</span> TABLE_COUNT<span class="token punctuation">,</span> goldChange<span class="token punctuation">,</span> playerId</span>
<span class="line">                <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>txn<span class="token punctuation">.</span><span class="token function">commit</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">catch</span> <span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>exception<span class="token operator">&amp;</span> e<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">LOG_ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Sync update failed: &quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">string</span><span class="token punctuation">(</span>e<span class="token punctuation">.</span><span class="token function">what</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            retries<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>this_thread<span class="token double-colon punctuation">::</span><span class="token function">sleep_for</span><span class="token punctuation">(</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">milliseconds</span><span class="token punctuation">(</span><span class="token number">100</span> <span class="token operator">*</span> retries<span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 非关键操作：异步 + 批量</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">asyncUpdateLog</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> log<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        logQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span><span class="token punctuation">{</span>playerId<span class="token punctuation">,</span> log<span class="token punctuation">,</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>logQueue_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;=</span> BATCH_SIZE<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">flushLogs</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">flushLogs</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>LogEntry<span class="token operator">&gt;</span> batch<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>logQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> batch<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> BATCH_SIZE<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            LogEntry entry<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>logQueue_<span class="token punctuation">.</span><span class="token function">try_pop</span><span class="token punctuation">(</span>entry<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                batch<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>entry<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>batch<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">bulkInsertLogs</span><span class="token punctuation">(</span>batch<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">LogEntry</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> playerId<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string log<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    ThreadSafeQueue<span class="token operator">&lt;</span>LogEntry<span class="token operator">&gt;</span> logQueue_<span class="token punctuation">;</span></span>
<span class="line">    Database<span class="token operator">*</span> db_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> MAX_RETRIES <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t BATCH_SIZE <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t TABLE_COUNT <span class="token operator">=</span> <span class="token number">16</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="一致性方案对比" tabindex="-1"><a class="header-anchor" href="#一致性方案对比"><span>一致性方案对比</span></a></h3><table><thead><tr><th>方案</th><th>一致性</th><th>性能</th><th>复杂度</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>强事务</strong></td><td>强</td><td>低</td><td>低</td><td>关键交易</td></tr><tr><td><strong>2PC</strong></td><td>强</td><td>很低</td><td>中</td><td>跨库事务</td></tr><tr><td><strong>TCC</strong></td><td>最终</td><td>中</td><td>高</td><td>长事务</td></tr><tr><td><strong>异步同步</strong></td><td>最终</td><td>高</td><td>中</td><td>状态同步</td></tr><tr><td><strong>事件溯源</strong></td><td>最终</td><td>中</td><td>很高</td><td>审计系统</td></tr></tbody></table><h3 id="kbengine-一致性保证" tabindex="-1"><a class="header-anchor" href="#kbengine-一致性保证"><span>KBEngine 一致性保证</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 保证:</span>
<span class="line">1. 定时自动存档</span>
<span class="line">2. 实体数据序列化</span>
<span class="line">3. 数据库持久化</span>
<span class="line"></span>
<span class="line">开发者需要:</span>
<span class="line">1. 关键操作使用事务</span>
<span class="line">2. 实现重试机制</span>
<span class="line">3. 处理并发冲突</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/tree/master/kbe/src/server/dbmgr" target="_blank" rel="noopener noreferrer">KBEngine GitHub - 数据库接口</a></li><li><a href="https://www.oracle.com/java/technologies/cap.html" target="_blank" rel="noopener noreferrer">CAP 定理详解</a></li><li><a href="https://dev.mysql.com/doc/refman/8.0/en/innodb-transaction-isolation-levels.html" target="_blank" rel="noopener noreferrer">MySQL 事务隔离级别</a></li></ul>`,52)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};