import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q91-server-monitoring.html","title":"Q91: 如何监控服务器状态？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q91-server-monitoring.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q91-server-monitoring.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q91-如何监控服务器状态" tabindex="-1"><a class="header-anchor" href="#q91-如何监控服务器状态"><span>Q91: 如何监控服务器状态？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对服务器监控的理解：</p><ul><li>监控指标</li><li>监控工具</li><li>告警机制</li><li>KBEngine 监控</li></ul><hr><h2 id="一、监控指标" tabindex="-1"><a class="header-anchor" href="#一、监控指标"><span>一、监控指标</span></a></h2><h3 id="_1-1-系统指标" tabindex="-1"><a class="header-anchor" href="#_1-1-系统指标"><span>1.1 系统指标</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    监控指标分类                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  系统资源:                                                   │</span>
<span class="line">│  ├── CPU 使用率                                             │</span>
<span class="line">│  ├── 内存使用量                                             │</span>
<span class="line">│  ├── 磁盘 I/O                                               │</span>
<span class="line">│  ├── 网络带宽                                               │</span>
<span class="line">│  └── 连接数                                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  进程状态:                                                   │</span>
<span class="line">│  ├── 进程存活状态                                           │</span>
<span class="line">│  ├── 线程数                                                 │</span>
<span class="line">│  ├── 句柄数                                                 │</span>
<span class="line">│  └── 文件描述符                                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  游戏业务:                                                   │</span>
<span class="line">│  ├── 在线人数                                               │</span>
<span class="line">│  ├── 请求处理速率 (QPS)                                     │</span>
<span class="line">│  ├── 响应时间 (RT)                                           │</span>
<span class="line">│  ├── 错误率                                                 │</span>
<span class="line">│  └── 实体数量                                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  KBEngine 特有:                                              │</span>
<span class="line">│  ├── 各组件状态                                             │</span>
<span class="line">│  ├── 消息队列长度                                           │</span>
<span class="line">│  ├── 空间使用情况                                           │</span>
<span class="line">│  └── 数据库连接池                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、kbengine-监控" tabindex="-1"><a class="header-anchor" href="#二、kbengine-监控"><span>二、KBEngine 监控</span></a></h2><h3 id="_2-1-kbengine-watcher-系统" tabindex="-1"><a class="header-anchor" href="#_2-1-kbengine-watcher-系统"><span>2.1 KBEngine Watcher 系统</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine Watcher 监控系统</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 内置监控机制:</span>
<span class="line"></span>
<span class="line">1. Watcher: 对象属性监控</span>
<span class="line">2. Profile: 性能统计</span>
<span class="line">3. Debug Helper: 调试信息</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">KBEngineMonitor</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;KBEngine 监控包装器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_stats</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取所有统计信息&quot;&quot;&quot;</span></span>
<span class="line">        stats <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 进程信息</span></span>
<span class="line">        stats<span class="token punctuation">[</span><span class="token string">&quot;process&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;stats/*&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 网络统计</span></span>
<span class="line">        stats<span class="token punctuation">[</span><span class="token string">&quot;network&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;network/*&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 内存统计</span></span>
<span class="line">        stats<span class="token punctuation">[</span><span class="token string">&quot;memory&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;mem/*&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 组件状态</span></span>
<span class="line">        stats<span class="token punctuation">[</span><span class="token string">&quot;components&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;components/*&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> stats</span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_entity_count</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取实体数量&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;entities/count&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_space_info</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取空间信息&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;spaces/*&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">profile_snapshot</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;性能快照&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> KBEngine<span class="token punctuation">.</span>profile<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ComponentMonitor</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;组件监控&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>components <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">register_component</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> name<span class="token punctuation">,</span> component<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;注册组件&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>components<span class="token punctuation">[</span>name<span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;start_time&quot;</span><span class="token punctuation">:</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;status&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;running&quot;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_components</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查组件状态&quot;&quot;&quot;</span></span>
<span class="line">        status <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> name<span class="token punctuation">,</span> info <span class="token keyword">in</span> self<span class="token punctuation">.</span>components<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 检查心跳</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>check_heartbeat<span class="token punctuation">(</span>name<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                info<span class="token punctuation">[</span><span class="token string">&quot;status&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token string">&quot;down&quot;</span></span>
<span class="line">                status<span class="token punctuation">[</span>name<span class="token punctuation">]</span> <span class="token operator">=</span> info</span>
<span class="line">            <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">                info<span class="token punctuation">[</span><span class="token string">&quot;status&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token string">&quot;up&quot;</span></span>
<span class="line">                status<span class="token punctuation">[</span>name<span class="token punctuation">]</span> <span class="token operator">=</span> info</span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> status</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_heartbeat</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> component_name<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查组件心跳&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 检查组件是否响应</span></span>
<span class="line">            <span class="token keyword">return</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;components/</span><span class="token interpolation"><span class="token punctuation">{</span>component_name<span class="token punctuation">}</span></span><span class="token string">/heartbeat&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">except</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PerformanceMonitor</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;性能监控&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">collect_metrics</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;收集性能指标&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 1. 消息处理速率</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;messages_per_second&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> self<span class="token punctuation">.</span>calculate_mps<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 2. 实体更新时间</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;entity_update_time&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> self<span class="token punctuation">.</span>calculate_entity_update_time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 3. 网络延迟</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;network_latency&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> self<span class="token punctuation">.</span>calculate_network_latency<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 4. 内存使用</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics<span class="token punctuation">[</span><span class="token string">&quot;memory_usage&quot;</span><span class="token punctuation">]</span> <span class="token operator">=</span> self<span class="token punctuation">.</span>calculate_memory_usage<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> self<span class="token punctuation">.</span>metrics</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_mps</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算每秒消息数&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># KBEngine 内置统计</span></span>
<span class="line">        stats <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;network/messageIn&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> stats<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;count&quot;</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_entity_update_time</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算实体更新时间&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 通过 profile 获取</span></span>
<span class="line">        profile_data <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>profile<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> profile_data<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;entityUpdate&quot;</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_network_latency</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算网络延迟&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 通过 round-trip time 计算</span></span>
<span class="line">        <span class="token comment"># 需要客户端配合</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token number">0</span>  <span class="token comment"># 占位</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">calculate_memory_usage</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;计算内存使用&quot;&quot;&quot;</span></span>
<span class="line">        mem_stats <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getWatcher<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;mem/allocated&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> mem_stats</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、监控工具" tabindex="-1"><a class="header-anchor" href="#三、监控工具"><span>三、监控工具</span></a></h2><h3 id="_3-1-prometheus-集成" tabindex="-1"><a class="header-anchor" href="#_3-1-prometheus-集成"><span>3.1 Prometheus 集成</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Prometheus 监控导出</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">from</span> prometheus_client <span class="token keyword">import</span> Counter<span class="token punctuation">,</span> Gauge<span class="token punctuation">,</span> Histogram<span class="token punctuation">,</span> start_http_server</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PrometheusMetrics</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;Prometheus 监控指标&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> port<span class="token operator">=</span><span class="token number">8000</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 定义指标</span></span>
<span class="line">        self<span class="token punctuation">.</span>online_players <span class="token operator">=</span> Gauge<span class="token punctuation">(</span><span class="token string">&#39;online_players&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;Online players&#39;</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>messages_total <span class="token operator">=</span> Counter<span class="token punctuation">(</span><span class="token string">&#39;messages_total&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;Total messages&#39;</span><span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token string">&#39;direction&#39;</span><span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>message_latency <span class="token operator">=</span> Histogram<span class="token punctuation">(</span><span class="token string">&#39;message_latency_seconds&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;Message latency&#39;</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>entity_count <span class="token operator">=</span> Gauge<span class="token punctuation">(</span><span class="token string">&#39;entity_count&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;Entity count&#39;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 启动 HTTP 服务器</span></span>
<span class="line">        start_http_server<span class="token punctuation">(</span>port<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">update_online_players</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;更新在线人数&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>online_players<span class="token punctuation">.</span><span class="token builtin">set</span><span class="token punctuation">(</span>count<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">inc_messages</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> direction<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;增加消息计数&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>messages_total<span class="token punctuation">.</span>labels<span class="token punctuation">(</span>direction<span class="token operator">=</span>direction<span class="token punctuation">)</span><span class="token punctuation">.</span>inc<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">observe_latency</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> latency<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;观察延迟&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>message_latency<span class="token punctuation">.</span>observe<span class="token punctuation">(</span>latency<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">update_entity_count</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> count<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;更新实体数量&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>entity_count<span class="token punctuation">.</span><span class="token builtin">set</span><span class="token punctuation">(</span>count<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">KBEnginePrometheusExporter</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;KBEngine Prometheus 导出器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics <span class="token operator">=</span> PrometheusMetrics<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>last_update <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">export</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;导出指标到 Prometheus&quot;&quot;&quot;</span></span>
<span class="line">        now <span class="token operator">=</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 每秒更新一次</span></span>
<span class="line">        <span class="token keyword">if</span> now <span class="token operator">-</span> self<span class="token punctuation">.</span>last_update <span class="token operator">&gt;=</span> <span class="token number">1.0</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>update_metrics<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>last_update <span class="token operator">=</span> now</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">update_metrics</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;更新所有指标&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 在线人数</span></span>
<span class="line">        online_count <span class="token operator">=</span> <span class="token builtin">len</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics<span class="token punctuation">.</span>update_online_players<span class="token punctuation">(</span>online_count<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 实体数量</span></span>
<span class="line">        self<span class="token punctuation">.</span>metrics<span class="token punctuation">.</span>update_entity_count<span class="token punctuation">(</span>online_count<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 消息统计 (需要 KBEngine 内部统计)</span></span>
<span class="line">        <span class="token comment"># 这里简化处理</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、告警系统" tabindex="-1"><a class="header-anchor" href="#四、告警系统"><span>四、告警系统</span></a></h2><h3 id="_4-1-告警规则" tabindex="-1"><a class="header-anchor" href="#_4-1-告警规则"><span>4.1 告警规则</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 告警系统</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AlertManager</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;告警管理器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>alert_rules <span class="token operator">=</span> <span class="token punctuation">[</span></span>
<span class="line">            AlertRule<span class="token punctuation">(</span><span class="token string">&quot;high_cpu&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;CPU &gt; 80%&quot;</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>check_high_cpu<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            AlertRule<span class="token punctuation">(</span><span class="token string">&quot;high_memory&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;Memory &gt; 90%&quot;</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>check_high_memory<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            AlertRule<span class="token punctuation">(</span><span class="token string">&quot;entity_overflow&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;Entities &gt; 10000&quot;</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>check_entity_overflow<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            AlertRule<span class="token punctuation">(</span><span class="token string">&quot;process_down&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;Process not responding&quot;</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>check_process_down<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token punctuation">]</span></span>
<span class="line">        self<span class="token punctuation">.</span>alert_handlers <span class="token operator">=</span> <span class="token punctuation">[</span></span>
<span class="line">            EmailAlertHandler<span class="token punctuation">(</span><span class="token string">&quot;admin@example.com&quot;</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            SlackAlertHandler<span class="token punctuation">(</span><span class="token string">&quot;#alerts&quot;</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            SMSAlertHandler<span class="token punctuation">(</span><span class="token string">&quot;+1234567890&quot;</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_all_rules</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查所有告警规则&quot;&quot;&quot;</span></span>
<span class="line">        alerts <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> rule <span class="token keyword">in</span> self<span class="token punctuation">.</span>alert_rules<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> rule<span class="token punctuation">.</span>check<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                alerts<span class="token punctuation">.</span>append<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">                    <span class="token string">&quot;rule&quot;</span><span class="token punctuation">:</span> rule<span class="token punctuation">.</span>name<span class="token punctuation">,</span></span>
<span class="line">                    <span class="token string">&quot;message&quot;</span><span class="token punctuation">:</span> rule<span class="token punctuation">.</span>message<span class="token punctuation">,</span></span>
<span class="line">                    <span class="token string">&quot;severity&quot;</span><span class="token punctuation">:</span> rule<span class="token punctuation">.</span>severity<span class="token punctuation">,</span></span>
<span class="line">                    <span class="token string">&quot;timestamp&quot;</span><span class="token punctuation">:</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 发送告警</span></span>
<span class="line">        <span class="token keyword">for</span> alert <span class="token keyword">in</span> alerts<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>send_alert<span class="token punctuation">(</span>alert<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_high_cpu</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查高 CPU 使用率&quot;&quot;&quot;</span></span>
<span class="line">        cpu_usage <span class="token operator">=</span> self<span class="token punctuation">.</span>get_cpu_usage<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> cpu_usage <span class="token operator">&gt;</span> <span class="token number">80.0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_high_memory</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查高内存使用&quot;&quot;&quot;</span></span>
<span class="line">        mem_usage <span class="token operator">=</span> self<span class="token punctuation">.</span>get_memory_usage<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> mem_usage <span class="token operator">&gt;</span> <span class="token number">90.0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_entity_overflow</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查实体数量溢出&quot;&quot;&quot;</span></span>
<span class="line">        entity_count <span class="token operator">=</span> <span class="token builtin">len</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> entity_count <span class="token operator">&gt;</span> <span class="token number">10000</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_process_down</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查进程是否存活&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 检查关键文件描述符</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>is_process_responding<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">send_alert</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> alert<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;发送告警&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">for</span> handler <span class="token keyword">in</span> self<span class="token punctuation">.</span>alert_handlers<span class="token punctuation">:</span></span>
<span class="line">            handler<span class="token punctuation">.</span>send<span class="token punctuation">(</span>alert<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AlertRule</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;告警规则&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> name<span class="token punctuation">,</span> message<span class="token punctuation">,</span> check_func<span class="token punctuation">,</span> severity<span class="token operator">=</span><span class="token string">&quot;warning&quot;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>name <span class="token operator">=</span> name</span>
<span class="line">        self<span class="token punctuation">.</span>message <span class="token operator">=</span> message</span>
<span class="line">        self<span class="token punctuation">.</span>check_func <span class="token operator">=</span> check_func</span>
<span class="line">        self<span class="token punctuation">.</span>severity <span class="token operator">=</span> severity</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EmailAlertHandler</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;邮件告警处理器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> to_address<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>to_address <span class="token operator">=</span> to_address</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">send</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> alert<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;发送邮件&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">import</span> smtplib</span>
<span class="line">        <span class="token keyword">from</span> email<span class="token punctuation">.</span>mime<span class="token punctuation">.</span>text <span class="token keyword">import</span> MIMEText</span>
<span class="line"></span>
<span class="line">        msg <span class="token operator">=</span> MIMEText<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Alert: </span><span class="token interpolation"><span class="token punctuation">{</span>alert<span class="token punctuation">[</span><span class="token string">&#39;message&#39;</span><span class="token punctuation">]</span><span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        msg<span class="token punctuation">[</span><span class="token string">&#39;Subject&#39;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token string-interpolation"><span class="token string">f&quot;[</span><span class="token interpolation"><span class="token punctuation">{</span>alert<span class="token punctuation">[</span><span class="token string">&#39;severity&#39;</span><span class="token punctuation">]</span><span class="token punctuation">.</span>upper<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">}</span></span><span class="token string">] </span><span class="token interpolation"><span class="token punctuation">{</span>alert<span class="token punctuation">[</span><span class="token string">&#39;rule&#39;</span><span class="token punctuation">]</span><span class="token punctuation">}</span></span><span class="token string">&quot;</span></span></span>
<span class="line">        msg<span class="token punctuation">[</span><span class="token string">&#39;To&#39;</span><span class="token punctuation">]</span> <span class="token operator">=</span> self<span class="token punctuation">.</span>to_address</span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 发送邮件</span></span>
<span class="line">        <span class="token comment"># ...</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、实时监控面板" tabindex="-1"><a class="header-anchor" href="#五、实时监控面板"><span>五、实时监控面板</span></a></h2><h3 id="_5-1-web-监控面板" tabindex="-1"><a class="header-anchor" href="#_5-1-web-监控面板"><span>5.1 Web 监控面板</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Flask 监控面板</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">from</span> flask <span class="token keyword">import</span> Flask<span class="token punctuation">,</span> render_template<span class="token punctuation">,</span> jsonify</span>
<span class="line"></span>
<span class="line">app <span class="token operator">=</span> Flask<span class="token punctuation">(</span>__name__<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MonitorDashboard</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;监控仪表盘&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@app<span class="token punctuation">.</span>route</span><span class="token punctuation">(</span><span class="token string">&#39;/&#39;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">dashboard</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;主面板&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> render_template<span class="token punctuation">(</span><span class="token string">&#39;dashboard.html&#39;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@app<span class="token punctuation">.</span>route</span><span class="token punctuation">(</span><span class="token string">&#39;/api/stats&#39;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_stats</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取统计数据&quot;&quot;&quot;</span></span>
<span class="line">        monitor <span class="token operator">=</span> KBEngineMonitor<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> jsonify<span class="token punctuation">(</span>monitor<span class="token punctuation">.</span>get_stats<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@app<span class="token punctuation">.</span>route</span><span class="token punctuation">(</span><span class="token string">&#39;/api/metrics&#39;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_metrics</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取性能指标&quot;&quot;&quot;</span></span>
<span class="line">        perf_monitor <span class="token operator">=</span> PerformanceMonitor<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> jsonify<span class="token punctuation">(</span>perf_monitor<span class="token punctuation">.</span>collect_metrics<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@app<span class="token punctuation">.</span>route</span><span class="token punctuation">(</span><span class="token string">&#39;/api/components&#39;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_component_status</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取组件状态&quot;&quot;&quot;</span></span>
<span class="line">        comp_monitor <span class="token operator">=</span> ComponentMonitor<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> jsonify<span class="token punctuation">(</span>comp_monitor<span class="token punctuation">.</span>check_components<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@app<span class="token punctuation">.</span>route</span><span class="token punctuation">(</span><span class="token string">&#39;/api/topology&#39;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_topology</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取拓扑结构&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> jsonify<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;loginapp&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token string">&quot;status&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;up&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;address&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;localhost:20013&quot;</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;baseapp1&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token string">&quot;status&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;up&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;address&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;localhost:20011&quot;</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;baseapp2&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token string">&quot;status&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;up&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;address&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;localhost:20012&quot;</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;cellapp1&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token string">&quot;status&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;up&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;address&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;localhost:20021&quot;</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;dbmgr&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token string">&quot;status&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;up&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;address&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;localhost:20004&quot;</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># HTML 模板 (简化)</span></span>
<span class="line">DASHBOARD_HTML <span class="token operator">=</span> <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">&lt;!DOCTYPE html&gt;</span>
<span class="line">&lt;html&gt;</span>
<span class="line">&lt;head&gt;</span>
<span class="line">    &lt;title&gt;KBEngine Monitor&lt;/title&gt;</span>
<span class="line">    &lt;script src=&quot;https://cdn.jsdelivr.net/npm/chart.js&quot;&gt;&lt;/script&gt;</span>
<span class="line">&lt;/head&gt;</span>
<span class="line">&lt;body&gt;</span>
<span class="line">    &lt;h1&gt;KBEngine 服务器监控&lt;/h1&gt;</span>
<span class="line"></span>
<span class="line">    &lt;div id=&quot;stats&quot;&gt;&lt;/div&gt;</span>
<span class="line">    &lt;canvas id=&quot;memoryChart&quot;&gt;&lt;/canvas&gt;</span>
<span class="line">    &lt;canvas id=&quot;cpuChart&quot;&gt;&lt;/canvas&gt;</span>
<span class="line"></span>
<span class="line">    &lt;script&gt;</span>
<span class="line">    // 定期更新</span>
<span class="line">    setInterval(function() {</span>
<span class="line">        fetch(&#39;/api/stats&#39;)</span>
<span class="line">            .then(r =&gt; r.json())</span>
<span class="line">            .then(data =&gt; {</span>
<span class="line">                document.getElementById(&#39;stats&#39;).innerText =</span>
<span class="line">                    JSON.stringify(data, null, 2);</span>
<span class="line">            });</span>
<span class="line">    }, 5000);</span>
<span class="line">    &lt;/script&gt;</span>
<span class="line">&lt;/body&gt;</span>
<span class="line">&lt;/html&gt;</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、日志分析" tabindex="-1"><a class="header-anchor" href="#六、日志分析"><span>六、日志分析</span></a></h2><h3 id="_6-1-日志监控" tabindex="-1"><a class="header-anchor" href="#_6-1-日志监控"><span>6.1 日志监控</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 日志分析监控</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LogMonitor</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;日志监控&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>error_patterns <span class="token operator">=</span> <span class="token punctuation">[</span></span>
<span class="line">            <span class="token string">r&quot;ERROR&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">r&quot;FATAL&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">r&quot;Exception&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">r&quot;Failed to&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">r&quot;Timeout&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">analyze_logs</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;分析日志&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 读取最近日志</span></span>
<span class="line">        logs <span class="token operator">=</span> self<span class="token punctuation">.</span>get_recent_logs<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        stats <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;error_count&quot;</span><span class="token punctuation">:</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;warning_count&quot;</span><span class="token punctuation">:</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;errors_by_type&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> log <span class="token keyword">in</span> logs<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">any</span><span class="token punctuation">(</span>pattern <span class="token keyword">in</span> log <span class="token keyword">for</span> pattern <span class="token keyword">in</span> self<span class="token punctuation">.</span>error_patterns<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                stats<span class="token punctuation">[</span><span class="token string">&quot;error_count&quot;</span><span class="token punctuation">]</span> <span class="token operator">+=</span> <span class="token number">1</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment"># 统计错误类型</span></span>
<span class="line">                <span class="token keyword">for</span> pattern <span class="token keyword">in</span> self<span class="token punctuation">.</span>error_patterns<span class="token punctuation">:</span></span>
<span class="line">                    <span class="token keyword">if</span> pattern <span class="token keyword">in</span> log<span class="token punctuation">:</span></span>
<span class="line">                        stats<span class="token punctuation">[</span><span class="token string">&quot;errors_by_type&quot;</span><span class="token punctuation">]</span><span class="token punctuation">[</span>pattern<span class="token punctuation">]</span> <span class="token operator">=</span> \\</span>
<span class="line">                            stats<span class="token punctuation">[</span><span class="token string">&quot;errors_by_type&quot;</span><span class="token punctuation">]</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span>pattern<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token number">1</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> stats</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_recent_logs</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取最近的日志&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 从日志文件读取最近的日志行</span></span>
<span class="line">        log_file <span class="token operator">=</span> <span class="token string">&quot;kbengine.log&quot;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">with</span> <span class="token builtin">open</span><span class="token punctuation">(</span>log_file<span class="token punctuation">,</span> <span class="token string">&#39;r&#39;</span><span class="token punctuation">)</span> <span class="token keyword">as</span> f<span class="token punctuation">:</span></span>
<span class="line">                <span class="token comment"># 读取最后 100 行</span></span>
<span class="line">                lines <span class="token operator">=</span> f<span class="token punctuation">.</span>readlines<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">[</span><span class="token operator">-</span><span class="token number">100</span><span class="token punctuation">:</span><span class="token punctuation">]</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token punctuation">[</span>line<span class="token punctuation">.</span>strip<span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">for</span> line <span class="token keyword">in</span> lines<span class="token punctuation">]</span></span>
<span class="line">        <span class="token keyword">except</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、最佳实践" tabindex="-1"><a class="header-anchor" href="#七、最佳实践"><span>七、最佳实践</span></a></h2><h3 id="_7-1-监控建议" tabindex="-1"><a class="header-anchor" href="#_7-1-监控建议"><span>7.1 监控建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>分层监控</strong></td><td>系统/进程/业务三层</td></tr><tr><td><strong>可视化</strong></td><td>图表展示趋势</td></tr><tr><td><strong>实时告警</strong></td><td>及时发现问题</td></tr><tr><td><strong>日志分析</strong></td><td>深入分析问题</td></tr><tr><td><strong>容量规划</strong></td><td>基于数据做预测</td></tr></tbody></table><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="服务器监控核心" tabindex="-1"><a class="header-anchor" href="#服务器监控核心"><span>服务器监控核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">服务器监控 = 系统指标 + 业务指标 + 告警机制 + 可视化</span>
<span class="line">- CPU/内存/网络</span>
<span class="line">- 在线人数/QPS/延迟</span>
<span class="line">- KBEngine Watcher</span>
<span class="line">- Prometheus + Grafana</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://prometheus.io/docs/practices/" target="_blank" rel="noopener noreferrer">Prometheus Best Practices</a></li><li><a href="https://kbengine.github.io/docs/" target="_blank" rel="noopener noreferrer">KBEngine Monitoring</a></li><li><a href="https://grafana.com/docs/" target="_blank" rel="noopener noreferrer">Grafana Dashboards</a></li></ul>`,39)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};