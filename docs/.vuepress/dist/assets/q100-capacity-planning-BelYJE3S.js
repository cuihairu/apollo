import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q100-capacity-planning.html","title":"Q100: 如何进行服务器容量规划？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q100-capacity-planning.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q100-capacity-planning.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q100-如何进行服务器容量规划" tabindex="-1"><a class="header-anchor" href="#q100-如何进行服务器容量规划"><span>Q100: 如何进行服务器容量规划？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对容量规划的理解：</p><ul><li>性能基准</li><li>容量计算</li><li>扩展策略</li><li>KBEngine 性能指标</li></ul><hr><h2 id="一、容量规划概述" tabindex="-1"><a class="header-anchor" href="#一、容量规划概述"><span>一、容量规划概述</span></a></h2><h3 id="_1-1-规划维度" tabindex="-1"><a class="header-anchor" href="#_1-1-规划维度"><span>1.1 规划维度</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    容量规划维度                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  并发用户 (CCU):                                            │</span>
<span class="line">│  ├── 在线玩家数                                             │</span>
<span class="line">│  ├── 同时活跃数                                             │</span>
<span class="line">│  └── 峰值预留                                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  请求处理 (QPS):                                            │</span>
<span class="line">│  ├── 每秒请求数                                             │</span>
<span class="line">│  ├── 消息吞吐量                                             │</span>
<span class="line">│  └── 响应时间 (RT)                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  数据存储:                                                   │</span>
<span class="line">│  ├── 玩家数据量                                             │</span>
<span class="line">│  ├── 游戏数据量                                             │</span>
<span class="line">│  └── 日志数据量                                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  网络带宽:                                                   │</span>
<span class="line">│  ├── 入站流量                                               │</span>
<span class="line">│  ├── 出站流量                                               │</span>
<span class="line">│  └── 峰值带宽                                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  系统资源:                                                   │</span>
<span class="line">│  ├── CPU 使用率                                             │</span>
<span class="line">│  ├── 内存使用量                                             │</span>
<span class="line">│  ├── 磁盘 I/O                                               │</span>
<span class="line">│  └── 网络连接数                                             │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、性能基准测试" tabindex="-1"><a class="header-anchor" href="#二、性能基准测试"><span>二、性能基准测试</span></a></h2><h3 id="_2-1-kbengine-性能指标" tabindex="-1"><a class="header-anchor" href="#_2-1-kbengine-性能指标"><span>2.1 KBEngine 性能指标</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 性能基准</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 性能基准 (参考值):</span>
<span class="line"></span>
<span class="line">硬件: Intel Xeon E5-2680 v4, 32GB RAM, SSD</span>
<span class="line"></span>
<span class="line">单 BaseApp:</span>
<span class="line">- 玩家数量: 2000-3000</span>
<span class="line">- CPU 使用: 60-80%</span>
<span class="line">- 内存使用: 2-4GB</span>
<span class="line">- 消息处理: 5000-8000 msg/s</span>
<span class="line"></span>
<span class="line">单 CellApp:</span>
<span class="line">- 实体数量: 1000-2000 (取决于复杂度)</span>
<span class="line">- CPU 使用: 60-80%</span>
<span class="line">- 内存使用: 4-8GB</span>
<span class="line">- 消息处理: 3000-5000 msg/s</span>
<span class="line"></span>
<span class="line">DBMgr:</span>
<span class="line">- 并发写入: 1000-2000/s</span>
<span class="line">- 并发读取: 5000-10000/s</span>
<span class="line">- 内存使用: 2-4GB</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PerformanceBenchmark</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;性能基准测试&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;baseapp&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&quot;max_players&quot;</span><span class="token punctuation">:</span> <span class="token number">2500</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;max_cpu&quot;</span><span class="token punctuation">:</span> <span class="token number">80</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;max_memory&quot;</span><span class="token punctuation">:</span> <span class="token number">4</span> <span class="token operator">*</span> <span class="token number">1024</span><span class="token punctuation">,</span>  <span class="token comment"># MB</span></span>
<span class="line">                <span class="token string">&quot;max_qps&quot;</span><span class="token punctuation">:</span> <span class="token number">5000</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;cellapp&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&quot;max_entities&quot;</span><span class="token punctuation">:</span> <span class="token number">1500</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;max_cpu&quot;</span><span class="token punctuation">:</span> <span class="token number">80</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;max_memory&quot;</span><span class="token punctuation">:</span> <span class="token number">8</span> <span class="token operator">*</span> <span class="token number">1024</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;max_qps&quot;</span><span class="token punctuation">:</span> <span class="token number">3000</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;dbmgr&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&quot;max_writes&quot;</span><span class="token punctuation">:</span> <span class="token number">1500</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;max_reads&quot;</span><span class="token punctuation">:</span> <span class="token number">8000</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;max_memory&quot;</span><span class="token punctuation">:</span> <span class="token number">4</span> <span class="token operator">*</span> <span class="token number">1024</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;loginapp&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&quot;max_logins_per_sec&quot;</span><span class="token punctuation">:</span> <span class="token number">200</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;max_concurrent&quot;</span><span class="token punctuation">:</span> <span class="token number">500</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_capacity</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> target_ccu<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算所需容量&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># BaseApp 容量</span></span>
<span class="line">        baseapp_count <span class="token operator">=</span> <span class="token builtin">max</span><span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> <span class="token punctuation">(</span>target_ccu <span class="token operator">+</span> self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;baseapp&quot;</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token string">&quot;max_players&quot;</span><span class="token punctuation">]</span> <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">//</span></span>
<span class="line">                           self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;baseapp&quot;</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token string">&quot;max_players&quot;</span><span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># CellApp 容量 (假设 70% 玩家在 CellApp 有实体)</span></span>
<span class="line">        active_players <span class="token operator">=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>target_ccu <span class="token operator">*</span> <span class="token number">0.7</span><span class="token punctuation">)</span></span>
<span class="line">        cellapp_count <span class="token operator">=</span> <span class="token builtin">max</span><span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> <span class="token punctuation">(</span>active_players <span class="token operator">+</span> self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;cellapp&quot;</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token string">&quot;max_entities&quot;</span><span class="token punctuation">]</span> <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">//</span></span>
<span class="line">                              self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;cellapp&quot;</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token string">&quot;max_entities&quot;</span><span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># DBMgr 通常单实例</span></span>
<span class="line">        dbmgr_count <span class="token operator">=</span> <span class="token number">1</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># LoginApp 容量</span></span>
<span class="line">        <span class="token comment"># 假设峰值登录为 CCU 的 10%</span></span>
<span class="line">        peak_logins <span class="token operator">=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>target_ccu <span class="token operator">*</span> <span class="token number">0.1</span><span class="token punctuation">)</span></span>
<span class="line">        loginapp_count <span class="token operator">=</span> <span class="token builtin">max</span><span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> <span class="token punctuation">(</span>peak_logins <span class="token operator">+</span> self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;loginapp&quot;</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token string">&quot;max_logins_per_sec&quot;</span><span class="token punctuation">]</span> <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">//</span></span>
<span class="line">                               self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;loginapp&quot;</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token string">&quot;max_logins_per_sec&quot;</span><span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;baseapp&quot;</span><span class="token punctuation">:</span> baseapp_count<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;cellapp&quot;</span><span class="token punctuation">:</span> cellapp_count<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;dbmgr&quot;</span><span class="token punctuation">:</span> dbmgr_count<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;loginapp&quot;</span><span class="token punctuation">:</span> loginapp_count<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;total_servers&quot;</span><span class="token punctuation">:</span> baseapp_count <span class="token operator">+</span> cellapp_count <span class="token operator">+</span> dbmgr_count <span class="token operator">+</span> loginapp_count</span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">estimate_hardware</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> target_ccu<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;估算硬件需求&quot;&quot;&quot;</span></span>
<span class="line">        capacity <span class="token operator">=</span> self<span class="token punctuation">.</span>calculate_capacity<span class="token punctuation">(</span>target_ccu<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 估算机器数量</span></span>
<span class="line">        <span class="token comment"># 假设每台机器可以运行: 2 BaseApp + 3 CellApp</span></span>
<span class="line">        baseapp_per_machine <span class="token operator">=</span> <span class="token number">2</span></span>
<span class="line">        cellapp_per_machine <span class="token operator">=</span> <span class="token number">3</span></span>
<span class="line"></span>
<span class="line">        game_machines <span class="token operator">=</span> <span class="token builtin">max</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token punctuation">(</span>capacity<span class="token punctuation">[</span><span class="token string">&quot;baseapp&quot;</span><span class="token punctuation">]</span> <span class="token operator">+</span> baseapp_per_machine <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">//</span> baseapp_per_machine<span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">(</span>capacity<span class="token punctuation">[</span><span class="token string">&quot;cellapp&quot;</span><span class="token punctuation">]</span> <span class="token operator">+</span> cellapp_per_machine <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">//</span> cellapp_per_machine</span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;game_servers&quot;</span><span class="token punctuation">:</span> game_machines<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;db_server&quot;</span><span class="token punctuation">:</span> <span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;login_server&quot;</span><span class="token punctuation">:</span> <span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;total_machines&quot;</span><span class="token punctuation">:</span> game_machines <span class="token operator">+</span> <span class="token number">2</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;capacity&quot;</span><span class="token punctuation">:</span> capacity</span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、容量计算模型" tabindex="-1"><a class="header-anchor" href="#三、容量计算模型"><span>三、容量计算模型</span></a></h2><h3 id="_3-1-资源计算" tabindex="-1"><a class="header-anchor" href="#_3-1-资源计算"><span>3.1 资源计算</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 容量计算模型</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CapacityCalculator</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;容量计算器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 资源系数</span></span>
<span class="line">    CPU_COEFFICIENT <span class="token operator">=</span> <span class="token number">1.2</span>  <span class="token comment"># 20% 预留</span></span>
<span class="line">    MEMORY_COEFFICIENT <span class="token operator">=</span> <span class="token number">1.3</span>  <span class="token comment"># 30% 预留</span></span>
<span class="line">    NETWORK_COEFFICIENT <span class="token operator">=</span> <span class="token number">1.5</span>  <span class="token comment"># 50% 预留</span></span>
<span class="line">    PEAK_RATIO <span class="token operator">=</span> <span class="token number">1.5</span>  <span class="token comment"># 峰值/平均比例</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_cpu_usage</span><span class="token punctuation">(</span>ccu<span class="token punctuation">,</span> player_actions_per_minute<span class="token operator">=</span><span class="token number">60</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算 CPU 使用&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 每个玩家每分钟操作数</span></span>
<span class="line">        actions_per_second <span class="token operator">=</span> <span class="token punctuation">(</span>ccu <span class="token operator">*</span> player_actions_per_minute<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">60</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 每个操作 CPU 时间 (毫秒)</span></span>
<span class="line">        cpu_time_per_action <span class="token operator">=</span> <span class="token number">0.5</span>  <span class="token comment"># 0.5ms</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 所需 CPU 核心</span></span>
<span class="line">        cpu_cores <span class="token operator">=</span> <span class="token punctuation">(</span>actions_per_second <span class="token operator">*</span> cpu_time_per_action<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">1000</span></span>
<span class="line">        cpu_cores <span class="token operator">*=</span> CapacityCalculator<span class="token punctuation">.</span>CPU_COEFFICIENT</span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> cpu_cores</span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_memory_usage</span><span class="token punctuation">(</span>ccu<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算内存使用&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 每个玩家内存占用 (MB)</span></span>
<span class="line">        memory_per_player <span class="token operator">=</span> <span class="token number">1.5</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 总内存</span></span>
<span class="line">        total_memory <span class="token operator">=</span> ccu <span class="token operator">*</span> memory_per_player</span>
<span class="line">        total_memory <span class="token operator">*=</span> CapacityCalculator<span class="token punctuation">.</span>MEMORY_COEFFICIENT</span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> total_memory  <span class="token comment"># MB</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_network_bandwidth</span><span class="token punctuation">(</span>ccu<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算网络带宽&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 每个玩家带宽 (Kbps)</span></span>
<span class="line">        bandwidth_per_player <span class="token operator">=</span> <span class="token number">5</span>  <span class="token comment"># 5 Kbps</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 总带宽</span></span>
<span class="line">        total_bandwidth <span class="token operator">=</span> ccu <span class="token operator">*</span> bandwidth_per_player</span>
<span class="line">        total_bandwidth <span class="token operator">*=</span> CapacityCalculator<span class="token punctuation">.</span>NETWORK_COEFFICIENT</span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> total_bandwidth <span class="token operator">/</span> <span class="token number">1024</span>  <span class="token comment"># Mbps</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_db_capacity</span><span class="token punctuation">(</span>ccu<span class="token punctuation">,</span> retention_days<span class="token operator">=</span><span class="token number">30</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算数据库容量&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 每个玩家数据大小 (KB)</span></span>
<span class="line">        data_per_player <span class="token operator">=</span> <span class="token number">50</span>  <span class="token comment"># 50 KB</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 每天新增玩家</span></span>
<span class="line">        new_players_per_day <span class="token operator">=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>ccu <span class="token operator">*</span> <span class="token number">0.1</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 每天数据增长</span></span>
<span class="line">        daily_growth <span class="token operator">=</span> new_players_per_day <span class="token operator">*</span> data_per_player</span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 保留期内总数据</span></span>
<span class="line">        total_data <span class="token operator">=</span> daily_growth <span class="token operator">*</span> retention_days</span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 索引开销</span></span>
<span class="line">        total_data <span class="token operator">*=</span> <span class="token number">1.5</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> total_data <span class="token operator">/</span> <span class="token number">1024</span> <span class="token operator">/</span> <span class="token number">1024</span>  <span class="token comment"># GB</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_redis_memory</span><span class="token punctuation">(</span>ccu<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算 Redis 内存&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 每个玩家缓存数据 (KB)</span></span>
<span class="line">        cache_per_player <span class="token operator">=</span> <span class="token number">10</span>  <span class="token comment"># 10 KB</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 在线玩家缓存</span></span>
<span class="line">        online_cache <span class="token operator">=</span> ccu <span class="token operator">*</span> cache_per_player</span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 总缓存 (包含离线玩家部分数据)</span></span>
<span class="line">        total_cache <span class="token operator">=</span> online_cache <span class="token operator">*</span> <span class="token number">2</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> total_cache <span class="token operator">/</span> <span class="token number">1024</span>  <span class="token comment"># MB</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 容量规划计算器</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">plan_capacity</span><span class="token punctuation">(</span>target_ccu<span class="token punctuation">,</span> growth_rate<span class="token operator">=</span><span class="token number">0.2</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;容量规划&quot;&quot;&quot;</span></span>
<span class="line">    calculator <span class="token operator">=</span> CapacityCalculator<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 当前需求</span></span>
<span class="line">    current <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;cpu_cores&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_cpu_usage<span class="token punctuation">(</span>target_ccu<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;memory_mb&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_memory_usage<span class="token punctuation">(</span>target_ccu<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;bandwidth_mbps&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_network_bandwidth<span class="token punctuation">(</span>target_ccu<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;db_gb&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_db_capacity<span class="token punctuation">(</span>target_ccu<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;redis_mb&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_redis_memory<span class="token punctuation">(</span>target_ccu<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 考虑增长</span></span>
<span class="line">    future_ccu <span class="token operator">=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>target_ccu <span class="token operator">*</span> <span class="token punctuation">(</span><span class="token number">1</span> <span class="token operator">+</span> growth_rate<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">    future <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;cpu_cores&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_cpu_usage<span class="token punctuation">(</span>future_ccu<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;memory_mb&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_memory_usage<span class="token punctuation">(</span>future_ccu<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;bandwidth_mbps&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_network_bandwidth<span class="token punctuation">(</span>future_ccu<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;db_gb&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_db_capacity<span class="token punctuation">(</span>future_ccu<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;redis_mb&quot;</span><span class="token punctuation">:</span> calculator<span class="token punctuation">.</span>calculate_redis_memory<span class="token punctuation">(</span>future_ccu<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&quot;target_ccu&quot;</span><span class="token punctuation">:</span> target_ccu<span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;current&quot;</span><span class="token punctuation">:</span> current<span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;future_ccu&quot;</span><span class="token punctuation">:</span> future_ccu<span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&quot;future&quot;</span><span class="token punctuation">:</span> future</span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、扩展策略" tabindex="-1"><a class="header-anchor" href="#四、扩展策略"><span>四、扩展策略</span></a></h2><h3 id="_4-1-水平扩展" tabindex="-1"><a class="header-anchor" href="#_4-1-水平扩展"><span>4.1 水平扩展</span></a></h3>`,19),i(d,{code:`eJxLy8kvT85ILCpR8AniUgACx+gXW+a/2Lv36dz2FwsXPp25IlZBV9dOwSnaKbE41bGgQOHl7Lbn+5bEQhSD5ZzxyLlgyIElncCSrtHOqTk56BqdwXJuWOVcwHLuGHJgSVewpEe0i5NvehFEvRtECMx2h7LBnOKSypxUoDPSMnNyrJQtDVxdLQ2QJJxxSbhAJdzcXMwNkCVccelwwyXhjmoUACFoepI=`}),o[1]||=e(`<div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 扩展策略</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ScalingStrategy</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;扩展策略&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_scaling_policy</span><span class="token punctuation">(</span>current_ccu<span class="token punctuation">,</span> max_ccu<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取扩展策略&quot;&quot;&quot;</span></span>
<span class="line">        usage_ratio <span class="token operator">=</span> current_ccu <span class="token operator">/</span> max_ccu</span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> usage_ratio <span class="token operator">&gt;</span> <span class="token number">0.9</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token string">&quot;critical_scale&quot;</span>  <span class="token comment"># 紧急扩展</span></span>
<span class="line">        <span class="token keyword">elif</span> usage_ratio <span class="token operator">&gt;</span> <span class="token number">0.75</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token string">&quot;scale_out&quot;</span>  <span class="token comment"># 扩容</span></span>
<span class="line">        <span class="token keyword">elif</span> usage_ratio <span class="token operator">&lt;</span> <span class="token number">0.3</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token string">&quot;scale_in&quot;</span>  <span class="token comment"># 缩容</span></span>
<span class="line">        <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token string">&quot;maintain&quot;</span>  <span class="token comment"># 保持</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_scale_out</span><span class="token punctuation">(</span>current<span class="token punctuation">,</span> max_ccu<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算扩容数量&quot;&quot;&quot;</span></span>
<span class="line">        target_ratio <span class="token operator">=</span> <span class="token number">0.7</span>  <span class="token comment"># 目标使用率 70%</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 计算需要的实例数</span></span>
<span class="line">        target_instances <span class="token operator">=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>current <span class="token operator">/</span> target_ratio<span class="token punctuation">)</span></span>
<span class="line">        current_instances <span class="token operator">=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>current <span class="token operator">/</span> max_ccu<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token builtin">max</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">,</span> target_instances <span class="token operator">-</span> current_instances<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AutoScaler</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;自动扩展器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>check_interval <span class="token operator">=</span> <span class="token number">60</span>  <span class="token comment"># 秒</span></span>
<span class="line">        self<span class="token punctuation">.</span>scale_out_threshold <span class="token operator">=</span> <span class="token number">0.75</span></span>
<span class="line">        self<span class="token punctuation">.</span>scale_in_threshold <span class="token operator">=</span> <span class="token number">0.3</span></span>
<span class="line">        self<span class="token punctuation">.</span>max_instances <span class="token operator">=</span> <span class="token number">20</span></span>
<span class="line">        self<span class="token punctuation">.</span>min_instances <span class="token operator">=</span> <span class="token number">2</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">start</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;启动自动扩展&quot;&quot;&quot;</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>addTimer<span class="token punctuation">(</span>self<span class="token punctuation">.</span>check_interval<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>check_and_scale<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_and_scale</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查并扩展&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 获取当前负载</span></span>
<span class="line">        baseapp_load <span class="token operator">=</span> self<span class="token punctuation">.</span>get_baseapp_load<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> instance_id<span class="token punctuation">,</span> load <span class="token keyword">in</span> baseapp_load<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> load <span class="token operator">&gt;</span> self<span class="token punctuation">.</span>scale_out_threshold<span class="token punctuation">:</span></span>
<span class="line">                self<span class="token punctuation">.</span>scale_out<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">break</span></span>
<span class="line">            <span class="token keyword">elif</span> load <span class="token operator">&lt;</span> self<span class="token punctuation">.</span>scale_in_threshold<span class="token punctuation">:</span></span>
<span class="line">                self<span class="token punctuation">.</span>scale_in<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">break</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_baseapp_load</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取 BaseApp 负载&quot;&quot;&quot;</span></span>
<span class="line">        loads <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">        baseapps <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;components/baseapp&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> baseapp_id<span class="token punctuation">,</span> data <span class="token keyword">in</span> baseapps<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            max_players <span class="token operator">=</span> data<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;maxPlayers&quot;</span><span class="token punctuation">,</span> <span class="token number">2500</span><span class="token punctuation">)</span></span>
<span class="line">            current_players <span class="token operator">=</span> data<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;playerCount&quot;</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">            loads<span class="token punctuation">[</span>baseapp_id<span class="token punctuation">]</span> <span class="token operator">=</span> current_players <span class="token operator">/</span> max_players</span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> loads</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">scale_out</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;扩容&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 检查是否可以扩容</span></span>
<span class="line">        baseapp_count <span class="token operator">=</span> <span class="token builtin">len</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;components/baseapp&quot;</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> baseapp_count <span class="token operator">&gt;=</span> self<span class="token punctuation">.</span>max_instances<span class="token punctuation">:</span></span>
<span class="line">            WARNING_MSG<span class="token punctuation">(</span><span class="token string">&quot;Max instances reached, cannot scale out&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Scaling out: starting new BaseApp&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 调用启动脚本</span></span>
<span class="line">        <span class="token comment"># os.system(&quot;./start_baseapp.sh&quot;)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">scale_in</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;缩容&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 检查是否可以缩容</span></span>
<span class="line">        baseapp_count <span class="token operator">=</span> <span class="token builtin">len</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;components/baseapp&quot;</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> baseapp_count <span class="token operator">&lt;=</span> self<span class="token punctuation">.</span>min_instances<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Scaling in: stopping idle BaseApp&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 选择负载最低的实例停止</span></span>
<span class="line">        <span class="token comment"># os.system(&quot;./stop_baseapp.sh &lt;id&gt;&quot;)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、容量监控" tabindex="-1"><a class="header-anchor" href="#五、容量监控"><span>五、容量监控</span></a></h2><h3 id="_5-1-实时监控" tabindex="-1"><a class="header-anchor" href="#_5-1-实时监控"><span>5.1 实时监控</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 容量监控</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CapacityMonitor</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;容量监控&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics_history <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line">        self<span class="token punctuation">.</span>max_history <span class="token operator">=</span> <span class="token number">1440</span>  <span class="token comment"># 24 小时 (每分钟一条)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">collect_metrics</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;收集指标&quot;&quot;&quot;</span></span>
<span class="line">        metrics <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;timestamp&quot;</span><span class="token punctuation">:</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;ccu&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>get_ccu<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;cpu&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>get_cpu_usage<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;memory&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>get_memory_usage<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;network&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>get_network_usage<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;db_connections&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>get_db_connections<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        self<span class="token punctuation">.</span>metrics_history<span class="token punctuation">.</span>append<span class="token punctuation">(</span>metrics<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 限制历史长度</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>metrics_history<span class="token punctuation">)</span> <span class="token operator">&gt;</span> self<span class="token punctuation">.</span>max_history<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>metrics_history<span class="token punctuation">.</span>pop<span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> metrics</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_ccu</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取在线人数&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token builtin">len</span><span class="token punctuation">(</span><span class="token punctuation">[</span>e <span class="token keyword">for</span> e <span class="token keyword">in</span> KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">.</span>values<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                   <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>e<span class="token punctuation">,</span> <span class="token string">&#39;playerName&#39;</span><span class="token punctuation">)</span><span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_cpu_usage</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取 CPU 使用率&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 从 KBEngine Watcher 获取</span></span>
<span class="line">        <span class="token keyword">return</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;stats/cpuUsage&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_memory_usage</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取内存使用&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;mem/allocated&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_network_usage</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取网络使用&quot;&quot;&quot;</span></span>
<span class="line">        stats <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;network/*&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;in&quot;</span><span class="token punctuation">:</span> stats<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;messageIn&quot;</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;out&quot;</span><span class="token punctuation">:</span> stats<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;messageOut&quot;</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_db_connections</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取数据库连接数&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;db/connections&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">predict_capacity</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> hours<span class="token operator">=</span><span class="token number">24</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;预测容量需求&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>metrics_history<span class="token punctuation">)</span> <span class="token operator">&lt;</span> <span class="token number">24</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 获取最近 24 小时的数据</span></span>
<span class="line">        recent <span class="token operator">=</span> self<span class="token punctuation">.</span>metrics_history<span class="token punctuation">[</span><span class="token operator">-</span><span class="token number">24</span> <span class="token operator">*</span> <span class="token number">60</span><span class="token punctuation">:</span><span class="token punctuation">]</span>  <span class="token comment"># 每分钟一条</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 计算增长率</span></span>
<span class="line">        ccu_values <span class="token operator">=</span> <span class="token punctuation">[</span>m<span class="token punctuation">[</span><span class="token string">&quot;ccu&quot;</span><span class="token punctuation">]</span> <span class="token keyword">for</span> m <span class="token keyword">in</span> recent<span class="token punctuation">]</span></span>
<span class="line">        growth_rate <span class="token operator">=</span> <span class="token punctuation">(</span>ccu_values<span class="token punctuation">[</span><span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">]</span> <span class="token operator">-</span> ccu_values<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span><span class="token punctuation">)</span> <span class="token operator">/</span> ccu_values<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span> <span class="token keyword">if</span> ccu_values<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span> <span class="token operator">&gt;</span> <span class="token number">0</span> <span class="token keyword">else</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 预测</span></span>
<span class="line">        current_ccu <span class="token operator">=</span> ccu_values<span class="token punctuation">[</span><span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">]</span></span>
<span class="line">        predicted_ccu <span class="token operator">=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>current_ccu <span class="token operator">*</span> <span class="token punctuation">(</span><span class="token number">1</span> <span class="token operator">+</span> growth_rate <span class="token operator">*</span> hours <span class="token operator">/</span> <span class="token number">24</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;current_ccu&quot;</span><span class="token punctuation">:</span> current_ccu<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;growth_rate&quot;</span><span class="token punctuation">:</span> growth_rate<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;predicted_ccu&quot;</span><span class="token punctuation">:</span> predicted_ccu<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;hours_ahead&quot;</span><span class="token punctuation">:</span> hours</span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、容量规划表" tabindex="-1"><a class="header-anchor" href="#六、容量规划表"><span>六、容量规划表</span></a></h2><h3 id="_6-1-不同规模配置" tabindex="-1"><a class="header-anchor" href="#_6-1-不同规模配置"><span>6.1 不同规模配置</span></a></h3><table><thead><tr><th>CCU</th><th>BaseApp</th><th>CellApp</th><th>LoginApp</th><th>DBMgr</th><th>机器配置</th><th>网络带宽</th></tr></thead><tbody><tr><td>500</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1台 8C/16G</td><td>50 Mbps</td></tr><tr><td>1000</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1台 8C/32G</td><td>100 Mbps</td></tr><tr><td>3000</td><td>2</td><td>2</td><td>1</td><td>1</td><td>2台 8C/32G</td><td>200 Mbps</td></tr><tr><td>5000</td><td>2</td><td>3</td><td>1</td><td>1</td><td>2台 16C/64G</td><td>300 Mbps</td></tr><tr><td>10000</td><td>4</td><td>5</td><td>2</td><td>1</td><td>4台 16C/64G</td><td>500 Mbps</td></tr><tr><td>30000</td><td>12</td><td>15</td><td>3</td><td>2</td><td>12台 16C/64G</td><td>1 Gbps</td></tr><tr><td>50000</td><td>20</td><td>25</td><td>5</td><td>3</td><td>20台 16C/64G</td><td>2 Gbps</td></tr></tbody></table><hr><h2 id="七、最佳实践" tabindex="-1"><a class="header-anchor" href="#七、最佳实践"><span>七、最佳实践</span></a></h2><h3 id="_7-1-容量规划建议" tabindex="-1"><a class="header-anchor" href="#_7-1-容量规划建议"><span>7.1 容量规划建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>基准测试</strong></td><td>建立性能基准</td></tr><tr><td><strong>预留空间</strong></td><td>CPU/Memory 预留 20-30%</td></tr><tr><td><strong>弹性扩展</strong></td><td>支持动态增减实例</td></tr><tr><td><strong>监控预测</strong></td><td>基于历史数据预测</td></tr><tr><td><strong>压力测试</strong></td><td>定期进行压测</td></tr><tr><td><strong>分阶段扩容</strong></td><td>避免过度配置</td></tr></tbody></table><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="容量规划核心" tabindex="-1"><a class="header-anchor" href="#容量规划核心"><span>容量规划核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">容量规划 = 性能基准 + 资源计算 + 扩展策略 + 监控预测</span>
<span class="line">- 建立性能基准</span>
<span class="line">- 计算 CPU/内存/网络需求</span>
<span class="line">- 水平扩展策略</span>
<span class="line">- 实时监控和预测</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://kbengine.github.io/docs/en/performance.html" target="_blank" rel="noopener noreferrer">KBEngine Performance</a></li><li><a href="https://www.amazon.com/Capacity-Planning-Fundamentals-Amazon-Web-Services-ebook/dp/B07JHFM2ZV" target="_blank" rel="noopener noreferrer">Capacity Planning Fundamentals</a></li><li><a href="https://www.gamasutra.com/blogs/MichaelKerr/20190813/349290/Scaling_Multiplayer_Games.php" target="_blank" rel="noopener noreferrer">Game Server Scaling</a></li></ul>`,20)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};