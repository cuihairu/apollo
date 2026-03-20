import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q26-data-persistence.html","title":"Q26: 玩家数据什么时候存数据库？全量保存 vs 增量保存？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q26-data-persistence.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q26-data-persistence.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q26-玩家数据什么时候存数据库-全量保存-vs-增量保存" tabindex="-1"><a class="header-anchor" href="#q26-玩家数据什么时候存数据库-全量保存-vs-增量保存"><span>Q26: 玩家数据什么时候存数据库？全量保存 vs 增量保存？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对数据持久化策略的理解：</p><ul><li>保存时机的选择</li><li>全量保存 vs 增量保存</li><li>KBEngine 的数据保存机制</li><li>数据一致性保证</li></ul><hr><h2 id="一、保存时机" tabindex="-1"><a class="header-anchor" href="#一、保存时机"><span>一、保存时机</span></a></h2><h3 id="_1-1-触发条件" tabindex="-1"><a class="header-anchor" href="#_1-1-触发条件"><span>1.1 触发条件</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    数据保存触发时机                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 定时保存                                                │</span>
<span class="line">│     ├── 固定时间间隔 (如每 5 分钟)                          │</span>
<span class="line">│     ├── 节省数据库压力                                      │</span>
<span class="line">│     └── 可能丢失数据                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 事件保存                                                │</span>
<span class="line">│     ├── 玩家下线时                                          │</span>
<span class="line">│     ├── 获得重要物品                                        │</span>
<span class="line">│     ├── 完成关键任务                                        │</span>
<span class="line">│     └── 角色升级                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 变化保存                                                │</span>
<span class="line">│     ├── 数据变化时立即保存                                  │</span>
<span class="line">│     ├── 数据安全但数据库压力大                              │</span>
<span class="line">│     └── 需要优化合并                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 标记保存                                                │</span>
<span class="line">│     ├── 脏标记机制                                          │</span>
<span class="line">│     ├── 定期检查并保存                                      │</span>
<span class="line">│     └── 平衡安全与性能                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-保存时机对比" tabindex="-1"><a class="header-anchor" href="#_1-2-保存时机对比"><span>1.2 保存时机对比</span></a></h3><table><thead><tr><th>时机</th><th>优点</th><th>缺点</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>定时保存</strong></td><td>压力均匀</td><td>可能丢数据</td><td>非关键数据</td></tr><tr><td><strong>事件保存</strong></td><td>数据安全</td><td>瞬时压力高</td><td>关键操作</td></tr><tr><td><strong>变化保存</strong></td><td>最安全</td><td>压力极大</td><td>不推荐</td></tr><tr><td><strong>标记保存</strong></td><td>平衡</td><td>实现复杂</td><td>推荐</td></tr></tbody></table><hr><h2 id="二、全量-vs-增量" tabindex="-1"><a class="header-anchor" href="#二、全量-vs-增量"><span>二、全量 vs 增量</span></a></h2><h3 id="_2-1-全量保存" tabindex="-1"><a class="header-anchor" href="#_2-1-全量保存"><span>2.1 全量保存</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    全量保存                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  每次保存完整的玩家数据：                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  玩家数据：                                         │       │</span>
<span class="line">│  │  ├── 基本信息 (id, name, level)                   │       │</span>
<span class="line">│  │  ├── 属性信息 (hp, mp, exp)                        │       │</span>
<span class="line">│  │  ├── 位置信息 (x, y, z)                            │       │</span>
<span class="line">│  │  ├── 背包数据 (20 个格子)                          │       │</span>
<span class="line">│  │  ├── 技能数据 (30 个技能)                          │       │</span>
<span class="line">│  │  ├── 任务数据 (50 个任务)                          │       │</span>
<span class="line">│  │  └── ... 更多数据                                  │       │</span>
<span class="line">│  │                                                    │       │</span>
<span class="line">│  │  每次保存：所有数据 → 约 10KB                      │       │</span>
<span class="line">│  │  每 5 分钟保存一次：120KB/小时                      │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优点：简单、数据完整、恢复容易                              │</span>
<span class="line">│  缺点：数据量大、写入慢、IO 压力高                          │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-增量保存" tabindex="-1"><a class="header-anchor" href="#_2-2-增量保存"><span>2.2 增量保存</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    增量保存                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  只保存变化的数据：                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  玩家获得 100 金币：                                 │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────────┐     │       │</span>
<span class="line">│  │  │  增量数据：                              │     │       │</span>
<span class="line">│  │  │  {                                     │     │       │</span>
<span class="line">│  │  │    &quot;player_id&quot;: 12345,                │     │       │</span>
<span class="line">│  │  │    &quot;changes&quot;: {                        │     │       │</span>
<span class="line">│  │  │      &quot;gold&quot;: &quot;+100&quot;                    │     │       │</span>
<span class="line">│  │  │    },                                   │     │       │</span>
<span class="line">│  │  │    &quot;timestamp&quot;: 1640000000             │     │       │</span>
<span class="line">│  │  │  }                                     │     │       │</span>
<span class="line">│  │  └─────────────────────────────────────────┘     │       │</span>
<span class="line">│  │                                                    │       │</span>
<span class="line">│  │  增量数据：约 50 bytes                             │       │</span>
<span class="line">│  │  比全量节省 99.5%！                                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优点：数据量小、写入快、节省 IO                            │</span>
<span class="line">│  缺点：需要合并、恢复复杂、可能有冗余                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-对比总结" tabindex="-1"><a class="header-anchor" href="#_2-3-对比总结"><span>2.3 对比总结</span></a></h3><table><thead><tr><th>维度</th><th>全量保存</th><th>增量保存</th></tr></thead><tbody><tr><td><strong>数据量</strong></td><td>大 (10KB)</td><td>小 (50B-500B)</td></tr><tr><td><strong>写入时间</strong></td><td>长</td><td>短</td></tr><tr><td><strong>IO 压力</strong></td><td>高</td><td>低</td></tr><tr><td><strong>恢复复杂度</strong></td><td>简单</td><td>复杂</td></tr><tr><td><strong>数据冗余</strong></td><td>无</td><td>有</td></tr><tr><td><strong>实现复杂度</strong></td><td>简单</td><td>复杂</td></tr></tbody></table><hr><h2 id="三、kbengine-的数据保存" tabindex="-1"><a class="header-anchor" href="#三、kbengine-的数据保存"><span>三、KBEngine 的数据保存</span></a></h2><h3 id="_3-1-自动保存机制" tabindex="-1"><a class="header-anchor" href="#_3-1-自动保存机制"><span>3.1 自动保存机制</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine 源码</a>：</p><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 自动保存配置</span></span>
<span class="line"><span class="token comment"># kbengine_defs.xml</span></span>
<span class="line"></span>
<span class="line"><span class="token operator">&lt;</span>Baseapp<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 自动存档间隔 <span class="token punctuation">(</span>秒<span class="token punctuation">)</span> <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>autoArchiveTime<span class="token operator">&gt;</span><span class="token number">300</span><span class="token operator">&lt;</span><span class="token operator">/</span>autoArchiveTime<span class="token operator">&gt;</span>  <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> <span class="token number">5</span> 分钟 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 是否自动保存 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>autoLoadEntity<span class="token operator">&gt;</span>true<span class="token operator">&lt;</span><span class="token operator">/</span>autoLoadEntity<span class="token operator">&gt;</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>Baseapp<span class="token operator">&gt;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-baseapp-保存实现" tabindex="-1"><a class="header-anchor" href="#_3-2-baseapp-保存实现"><span>3.2 BaseApp 保存实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine BaseApp 保存实现</span></span>
<span class="line"><span class="token comment">// src/server/baseapp/baseapp.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Baseapp</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 自动保存定时器</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> autoArchiveTimerID_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> autoArchiveTime_<span class="token punctuation">;</span>  <span class="token comment">// 默认 300 秒</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 初始化自动保存</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">initializeAutoArchive</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        autoArchiveTimerID_ <span class="token operator">=</span> <span class="token keyword">this</span><span class="token operator">-&gt;</span><span class="token function">addTimer</span><span class="token punctuation">(</span></span>
<span class="line">            autoArchiveTime_ <span class="token operator">*</span> <span class="token number">1000000</span><span class="token punctuation">,</span>  <span class="token comment">// 微秒</span></span>
<span class="line">            <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">this</span><span class="token operator">-&gt;</span><span class="token function">autoArchive</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 自动存档</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">autoArchive</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 遍历所有实体</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>entityID<span class="token punctuation">,</span> entity<span class="token punctuation">]</span> <span class="token operator">:</span> entities_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">hasDB</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 检查是否需要保存</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">isDirty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token comment">// 异步保存到数据库</span></span>
<span class="line">                    entity<span class="token operator">-&gt;</span><span class="token function">writeToDB</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-脏标记机制" tabindex="-1"><a class="header-anchor" href="#_3-3-脏标记机制"><span>3.3 脏标记机制</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 脏标记实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Account</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 脏标记</span></span>
<span class="line">        self<span class="token punctuation">.</span>_isDirty <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 属性变化时设置脏标记</span></span>
<span class="line">        self<span class="token punctuation">.</span>onPropertyChanged <span class="token operator">=</span> self<span class="token punctuation">.</span>_onPropertyChanged</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">_onPropertyChanged</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> propName<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        属性变化回调</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 设置脏标记</span></span>
<span class="line">        self<span class="token punctuation">.</span>_isDirty <span class="token operator">=</span> <span class="token boolean">True</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 通知 BaseApp 需要保存</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>baseAppCall<span class="token punctuation">(</span><span class="token string">&quot;onEntityDirty&quot;</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">writeToDB</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        写入数据库</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>_isDirty<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 保存到数据库</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>executeRawDatabaseCommand<span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;UPDATE players SET ... WHERE id = {}&quot;</span><span class="token punctuation">.</span><span class="token builtin">format</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 清除脏标记</span></span>
<span class="line">        self<span class="token punctuation">.</span>_isDirty <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、保存策略设计" tabindex="-1"><a class="header-anchor" href="#四、保存策略设计"><span>四、保存策略设计</span></a></h2><h3 id="_4-1-分级保存策略" tabindex="-1"><a class="header-anchor" href="#_4-1-分级保存策略"><span>4.1 分级保存策略</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  分级保存策略设计                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  一级数据（关键）：立即保存                                  │</span>
<span class="line">│  ├── 玩家充值                                              │</span>
<span class="line">│  ├── 珍贵物品获得                                          │</span>
<span class="line">│  ├── 重要任务完成                                          │</span>
<span class="line">│  └── 保存方式：同步写库 + 确认                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  二级数据（重要）：短延迟保存                                │</span>
<span class="line">│  ├── 金币/钻石变化                                         │</span>
<span class="line">│  ├── 角色升级                                              │</span>
<span class="line">│  ├── 装备强化                                              │</span>
<span class="line">│  └── 保存方式：异步队列 + 30秒内保存                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  三级数据（普通）：定时保存                                  │</span>
<span class="line">│  ├── 经验值变化                                            │</span>
<span class="line">│  ├── 任务进度                                              │</span>
<span class="line">│  ├── 副本进度                                              │</span>
<span class="line">│  └── 保存方式：定时器 + 5分钟保存                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  四级数据（临时）：内存缓存                                  │</span>
<span class="line">│  ├── 位置信息                                              │</span>
<span class="line">│  ├── 临时状态                                              │</span>
<span class="line">│  ├── Buff 状态                                             │</span>
<span class="line">│  └── 保存方式：仅下线时保存                                │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-混合保存实现" tabindex="-1"><a class="header-anchor" href="#_4-2-混合保存实现"><span>4.2 混合保存实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 混合保存策略实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">HybridSaveStrategy</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 数据类型</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">SavePriority</span> <span class="token punctuation">{</span></span>
<span class="line">        CRITICAL<span class="token punctuation">,</span>      <span class="token comment">// 立即同步保存</span></span>
<span class="line">        HIGH<span class="token punctuation">,</span>          <span class="token comment">// 短延迟异步保存</span></span>
<span class="line">        NORMAL<span class="token punctuation">,</span>        <span class="token comment">// 定时保存</span></span>
<span class="line">        TEMPORARY      <span class="token comment">// 仅下线保存</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 数据变更</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onDataChanged</span><span class="token punctuation">(</span>EntityID entityId<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">,</span></span>
<span class="line">                       <span class="token keyword">const</span> SaveVariant<span class="token operator">&amp;</span> value<span class="token punctuation">,</span> SavePriority priority<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>priority<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> SavePriority<span class="token double-colon punctuation">::</span>CRITICAL<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 立即同步保存</span></span>
<span class="line">                <span class="token function">saveSync</span><span class="token punctuation">(</span>entityId<span class="token punctuation">,</span> key<span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> SavePriority<span class="token double-colon punctuation">::</span>HIGH<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 添加到高优先级队列</span></span>
<span class="line">                highPriorityQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span><span class="token punctuation">{</span>entityId<span class="token punctuation">,</span> key<span class="token punctuation">,</span> value<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> SavePriority<span class="token double-colon punctuation">::</span>NORMAL<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 设置脏标记，等待定时保存</span></span>
<span class="line">                <span class="token function">markDirty</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> SavePriority<span class="token double-colon punctuation">::</span>TEMPORARY<span class="token operator">:</span></span>
<span class="line">                <span class="token comment">// 仅内存缓存</span></span>
<span class="line">                memoryCache_<span class="token punctuation">[</span>entityId<span class="token punctuation">]</span><span class="token punctuation">[</span>key<span class="token punctuation">]</span> <span class="token operator">=</span> value<span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 高优先级队列处理</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processHighPriorityQueue</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>highPriorityQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span> task <span class="token operator">=</span> highPriorityQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            highPriorityQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 异步保存</span></span>
<span class="line">            <span class="token function">saveAsync</span><span class="token punctuation">(</span>task<span class="token punctuation">.</span>entityId<span class="token punctuation">,</span> task<span class="token punctuation">.</span>key<span class="token punctuation">,</span> task<span class="token punctuation">.</span>value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 定时保存</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onTimer</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID entityId <span class="token operator">:</span> dirtyEntities_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>entity <span class="token operator">&amp;&amp;</span> entity<span class="token operator">-&gt;</span><span class="token function">isDirty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 全量保存</span></span>
<span class="line">                <span class="token function">saveFull</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        dirtyEntities_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>SaveTask<span class="token operator">&gt;</span> highPriorityQueue_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_set<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span> dirtyEntities_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>EntityID<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> SaveVariant<span class="token operator">&gt;&gt;</span> memoryCache_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、数据一致性" tabindex="-1"><a class="header-anchor" href="#五、数据一致性"><span>五、数据一致性</span></a></h2><h3 id="_5-1-事务保证" tabindex="-1"><a class="header-anchor" href="#_5-1-事务保证"><span>5.1 事务保证</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 数据库事务保存</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TransactionalSave</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 保存多个实体（事务）</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">saveEntities</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        db_<span class="token operator">-&gt;</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">try</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">:</span> entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 序列化数据</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>string data <span class="token operator">=</span> <span class="token function">serializeEntity</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment">// 更新数据库</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>string sql <span class="token operator">=</span> <span class="token function">format</span><span class="token punctuation">(</span></span>
<span class="line">                    <span class="token string">&quot;UPDATE players SET data = &#39;%s&#39; WHERE id = %lu&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                    <span class="token function">escape</span><span class="token punctuation">(</span>data<span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">c_str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                    entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>db_<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span>sql<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">throw</span> std<span class="token double-colon punctuation">::</span><span class="token function">runtime_error</span><span class="token punctuation">(</span><span class="token string">&quot;Database error&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 提交事务</span></span>
<span class="line">            db_<span class="token operator">-&gt;</span><span class="token function">commit</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">catch</span> <span class="token punctuation">(</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 回滚事务</span></span>
<span class="line">            db_<span class="token operator">-&gt;</span><span class="token function">rollback</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-备份机制" tabindex="-1"><a class="header-anchor" href="#_5-2-备份机制"><span>5.2 备份机制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 数据备份机制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BackupManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 保存前备份</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">saveWithBackup</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 备份旧数据</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string oldData <span class="token operator">=</span> <span class="token function">loadFromDB</span><span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 保存新数据</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">saveToDB</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 保存备份</span></span>
<span class="line">        <span class="token function">saveBackup</span><span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> oldData<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 恢复备份</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">restoreFromBackup</span><span class="token punctuation">(</span>EntityID entityId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 获取备份数据</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string backupData <span class="token operator">=</span> <span class="token function">loadBackup</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>backupData<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 恢复数据</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">restoreData</span><span class="token punctuation">(</span>entityId<span class="token punctuation">,</span> backupData<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、性能优化" tabindex="-1"><a class="header-anchor" href="#六、性能优化"><span>六、性能优化</span></a></h2><h3 id="_6-1-批量保存" tabindex="-1"><a class="header-anchor" href="#_6-1-批量保存"><span>6.1 批量保存</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 批量保存优化</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BatchSaveManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 待保存队列</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> pendingSave_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加待保存实体</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addPendingSave</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        pendingSave_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 达到批量大小或超时时触发保存</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pendingSave_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;=</span> BATCH_SIZE <span class="token operator">||</span></span>
<span class="line">            lastFlushTime_ <span class="token operator">-</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;=</span> FLUSH_INTERVAL<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">flush</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 批量保存</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">flush</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pendingSave_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 构建 SQL</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string sql <span class="token operator">=</span> <span class="token string">&quot;INSERT INTO player_data (id, data) VALUES &quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> pendingSave_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>i <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> sql <span class="token operator">+=</span> <span class="token string">&quot;, &quot;</span><span class="token punctuation">;</span></span>
<span class="line">            Entity<span class="token operator">*</span> entity <span class="token operator">=</span> pendingSave_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            sql <span class="token operator">+=</span> <span class="token function">format</span><span class="token punctuation">(</span><span class="token string">&quot;(%lu, &#39;%s&#39;)&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                           entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                           <span class="token function">escape</span><span class="token punctuation">(</span><span class="token function">serialize</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">c_str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        sql <span class="token operator">+=</span> <span class="token string">&quot; ON DUPLICATE KEY UPDATE data = VALUES(data)&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 执行</span></span>
<span class="line">        db_<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span>sql<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        pendingSave_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        lastFlushTime_ <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t BATCH_SIZE <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> FLUSH_INTERVAL <span class="token operator">=</span> <span class="token number">5000</span><span class="token punctuation">;</span>  <span class="token comment">// 5 秒</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> lastFlushTime_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-异步保存" tabindex="-1"><a class="header-anchor" href="#_6-2-异步保存"><span>6.2 异步保存</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 异步保存实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AsyncSaveManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 保存线程</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>thread saveThread_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>SaveTask<span class="token operator">&gt;</span> saveQueue_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex queueMutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>condition_variable queueCV_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 异步保存</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">saveAsync</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token punctuation">(</span><span class="token keyword">bool</span><span class="token punctuation">)</span><span class="token operator">&gt;</span> callback<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        SaveTask task<span class="token punctuation">;</span></span>
<span class="line">        task<span class="token punctuation">.</span>entityId <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        task<span class="token punctuation">.</span>data <span class="token operator">=</span> <span class="token function">serialize</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        task<span class="token punctuation">.</span>callback <span class="token operator">=</span> callback<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            saveQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>task<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        queueCV_<span class="token punctuation">.</span><span class="token function">notify_one</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 保存线程主循环</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">saveThreadMain</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>running_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            SaveTask task<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token punctuation">{</span></span>
<span class="line">                std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>queueMutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                queueCV_<span class="token punctuation">.</span><span class="token function">wait</span><span class="token punctuation">(</span>lock<span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">]</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token operator">!</span>saveQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> <span class="token operator">!</span>running_<span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>running_<span class="token punctuation">)</span> <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                task <span class="token operator">=</span> saveQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                saveQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 保存到数据库</span></span>
<span class="line">            <span class="token keyword">bool</span> success <span class="token operator">=</span> <span class="token function">saveToDB</span><span class="token punctuation">(</span>task<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 回调</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>task<span class="token punctuation">.</span>callback<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                task<span class="token punctuation">.</span><span class="token function">callback</span><span class="token punctuation">(</span>success<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> running_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、最佳实践" tabindex="-1"><a class="header-anchor" href="#七、最佳实践"><span>七、最佳实践</span></a></h2><h3 id="_7-1-推荐策略" tabindex="-1"><a class="header-anchor" href="#_7-1-推荐策略"><span>7.1 推荐策略</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  推荐的数据保存策略                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 采用混合策略                                            │</span>
<span class="line">│     ├── 关键数据：立即同步保存                              │</span>
<span class="line">│     ├── 重要数据：异步队列保存                              │</span>
<span class="line">│     ├── 普通数据：定时批量保存                              │</span>
<span class="line">│     └── 临时数据：仅下线保存                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 使用脏标记机制                                          │</span>
<span class="line">│     ├── 只保存变化的数据                                    │</span>
<span class="line">│     ├── 定期合并保存                                        │</span>
<span class="line">│     └── 减少数据库压力                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 异步 + 备份                                            │</span>
<span class="line">│     ├── 主流程异步保存                                      │</span>
<span class="line">│     ├── 保存前备份旧数据                                    │</span>
<span class="line">│     └── 确保数据可恢复                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 监控保存状态                                            │</span>
<span class="line">│     ├── 记录保存失败                                        │</span>
<span class="line">│     ├── 定期检查一致性                                      │</span>
<span class="line">│     └── 及时处理异常                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-2-kbengine-配置建议" tabindex="-1"><a class="header-anchor" href="#_7-2-kbengine-配置建议"><span>7.2 KBEngine 配置建议</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 数据保存配置建议</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># kbengine_defs.xml</span></span>
<span class="line"></span>
<span class="line"><span class="token operator">&lt;</span>Baseapp<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 自动保存间隔：<span class="token number">300</span> 秒 <span class="token punctuation">(</span><span class="token number">5</span> 分钟<span class="token punctuation">)</span> <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>autoArchiveTime<span class="token operator">&gt;</span><span class="token number">300</span><span class="token operator">&lt;</span><span class="token operator">/</span>autoArchiveTime<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 启用实体备份 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>shouldAutoArchive<span class="token operator">&gt;</span>true<span class="token operator">&lt;</span><span class="token operator">/</span>shouldAutoArchive<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 备份间隔：<span class="token number">600</span> 秒 <span class="token punctuation">(</span><span class="token number">10</span> 分钟<span class="token punctuation">)</span> <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>autoArchiveTime<span class="token operator">*</span><span class="token number">0.5</span><span class="token operator">&gt;</span><span class="token number">150</span><span class="token operator">&lt;</span><span class="token operator">/</span>autoArchiveTime<span class="token operator">*</span><span class="token number">0.5</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>Baseapp<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 脚本中添加脏标记</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Player</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>__dirty <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onPropertyChanged</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> propName<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 设置脏标记</span></span>
<span class="line">        self<span class="token punctuation">.</span>__dirty <span class="token operator">=</span> <span class="token boolean">True</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="保存策略总结" tabindex="-1"><a class="header-anchor" href="#保存策略总结"><span>保存策略总结</span></a></h3><table><thead><tr><th>数据类型</th><th>保存时机</th><th>保存方式</th><th>优先级</th></tr></thead><tbody><tr><td><strong>充值数据</strong></td><td>立即</td><td>同步全量</td><td>最高</td></tr><tr><td><strong>重要物品</strong></td><td>立即</td><td>异步全量</td><td>高</td></tr><tr><td><strong>角色升级</strong></td><td>短延迟</td><td>异步增量</td><td>中</td></tr><tr><td><strong>普通数据</strong></td><td>定时</td><td>批量增量</td><td>中</td></tr><tr><td><strong>临时数据</strong></td><td>下线时</td><td>全量</td><td>低</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 合理选择保存时机</span>
<span class="line">   - 关键操作立即保存</span>
<span class="line">   - 普通变化定时保存</span>
<span class="line">   - 临时数据下线保存</span>
<span class="line"></span>
<span class="line">2. 优化保存方式</span>
<span class="line">   - 使用脏标记减少保存</span>
<span class="line">   - 批量保存减少 IO</span>
<span class="line">   - 异步保存提高响应</span>
<span class="line"></span>
<span class="line">3. 确保数据安全</span>
<span class="line">   - 重要数据备份</span>
<span class="line">   - 事务保证一致性</span>
<span class="line">   - 监控保存状态</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/tree/master/kbe/src/server/dbmgr" target="_blank" rel="noopener noreferrer">KBEngine GitHub - DBMgr</a></li><li><a href="https://dev.mysql.com/doc/refman/8.0/en/optimization-bulk-load.html" target="_blank" rel="noopener noreferrer">MySQL 批量插入优化</a></li><li><a href="https://dev.mysql.com/doc/refman/8.0/en/innodb-transaction-isolation.html" target="_blank" rel="noopener noreferrer">数据库事务隔离级别</a></li></ul>`,60)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};