import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q92-log-system.html","title":"Q92: 如何设计日志系统？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q92-log-system.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q92-log-system.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q92-如何设计日志系统" tabindex="-1"><a class="header-anchor" href="#q92-如何设计日志系统"><span>Q92: 如何设计日志系统？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对日志系统设计的理解：</p><ul><li>日志分类</li><li>日志格式</li><li>日志存储</li><li>日志检索</li><li>KBEngine 日志</li></ul><hr><h2 id="一、日志系统架构" tabindex="-1"><a class="header-anchor" href="#一、日志系统架构"><span>一、日志系统架构</span></a></h2><h3 id="_1-1-系统组成" tabindex="-1"><a class="header-anchor" href="#_1-1-系统组成"><span>1.1 系统组成</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    日志系统架构                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  应用层 (Application):                                    │</span>
<span class="line">│  ├── 业务日志 (操作、交易、战斗)                             │</span>
<span class="line">│  ├── 错误日志 (异常、失败)                                   │</span>
<span class="line">│  ├── 性能日志 (耗时、瓶颈)                                   │</span>
<span class="line">│  └── 审计日志 (权限、安全)                                   │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│  ▼                                                          │</span>
<span class="line">│  收集层 (Collection):                                      │</span>
<span class="line">│  ├── 日志库 (libfmt, spdlog)                                 │</span>
<span class="line">│  ├── 格式化 (结构化)                                         │</span>
<span class="line">│  ├── 上下文信息 (线程、实体、玩家)                            │</span>
<span class="line">│  └── 元数据 (时间戳、级别、来源)                              │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│  ▼                                                          │</span>
<span class="line">│  缓冲层 (Buffer):                                            │</span>
<span class="line">│  ├── 异步缓冲                                               │</span>
<span class="line">│  ├── 批量写入                                               │</span>
<span class="line">│  └── 内存映射                                               │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│  ▼                                                          │</span>
<span class="line">│  存储层 (Storage):                                          │</span>
<span class="line">│  ├── 本地文件 (按大小/时间轮转)                              │</span>
<span class="line">│  ├── 日志聚合 (Logstash, Fluentd)                             │</span>
<span class="line">│  ├── 搜索引擎 (Elasticsearch)                                │</span>
<span class="line">│  └── 长期存储 (S3, OSS)                                     │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│  ▼                                                          │</span>
<span class="line">│  分析层 (Analysis):                                         │</span>
<span class="line">│  ├── 检索 (Kibana)                                           │</span>
<span class="line">│  ├── 可视化 (Grafana)                                         │</span>
<span class="line">│  └── 告警 (ElastAlert)                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-日志级别" tabindex="-1"><a class="header-anchor" href="#_1-2-日志级别"><span>1.2 日志级别</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    日志级别                                    │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  TRACE (追踪):  详细的执行流程                                │</span>
<span class="line">│  ├── 用途: 开发调试                                         │</span>
<span class="line">│  ├── 示例: 函数进入/退出、变量值                             │</span>
<span class="line">│  └── 生产环境: 关闭                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  DEBUG (调试): 调试信息                                        │</span>
<span class="line">│  ├── 用途: 开发调试                                         │</span>
<span class="line">│  ├── 示例: 状态变化、中间结果                                 │</span>
<span class="line">│  └── 生产环境: 关闭                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  INFO (信息): 一般信息                                        │</span>
<span class="line">│  ├── 用途: 正常运行记录                                       │</span>
<span class="line">│  ├── 示例: 玩家登录、系统启动                                 │</span>
<span class="line">│  └── 生产环境: 开启                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  WARN (警告): 警告信息                                        │</span>
<span class="line">│  ├── 用途: 潜在问题                                           │</span>
<span class="line">│  ├── 示例: 连接慢、重试                                       │</span>
<span class="line">│  └── 生产环境: 开启                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ERROR (错误): 错误信息                                       │</span>
<span class="line">│  ├── 用途: 错误事件                                         │</span>
<span class="line">│  ├── 示例: 请求失败、异常捕获                                 │</span>
<span class="line">│  └── 生产环境: 开启                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  FATAL (致命): 致命错误                                       │</span>
<span class="line">│  ├── 用途: 严重错误导致进程退出                                │</span>
<span class="line">│  ├── 示例: 数据库连接失败、内存溢出                           │</span>
<span class="line">│  └── 生产环境: 开启                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、kbengine-日志系统" tabindex="-1"><a class="header-anchor" href="#二、kbengine-日志系统"><span>二、KBEngine 日志系统</span></a></h2><h3 id="_2-1-kbengine-日志宏" tabindex="-1"><a class="header-anchor" href="#_2-1-kbengine-日志宏"><span>2.1 KBEngine 日志宏</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 日志系统</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 日志宏:</span>
<span class="line"></span>
<span class="line">- DEBUG_MSG(): 调试信息</span>
<span class="line">- INFO_MSG(): 信息日志</span>
<span class="line">- WARNING_MSG(): 警告日志</span>
<span class="line">- ERROR_MSG(): 错误日志</span>
<span class="line">- CRITICAL_MSG(): 严重错误日志</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Account</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Account<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Account<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>login_count <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onLogin</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entityType<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;登录回调&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 记录日志</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Account </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> login as </span><span class="token interpolation"><span class="token punctuation">{</span>entityType<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>login_count <span class="token operator">+=</span> <span class="token number">1</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 警告: 多次登录</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>login_count <span class="token operator">&gt;</span> <span class="token number">3</span><span class="token punctuation">:</span></span>
<span class="line">            WARNING_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Account </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> login </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>login_count<span class="token punctuation">}</span></span><span class="token string"> times&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onLogout</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;登出回调&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Account </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> logout after </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>login_count<span class="token punctuation">}</span></span><span class="token string"> logins&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onAttack</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> attackerId<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;被攻击&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> attacked by </span><span class="token interpolation"><span class="token punctuation">{</span>attackerId<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 计算伤害</span></span>
<span class="line">        damage <span class="token operator">=</span> self<span class="token punctuation">.</span>calculateDamage<span class="token punctuation">(</span>attackerId<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 记录伤害</span></span>
<span class="line">        DEBUG_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Damage calculated: </span><span class="token interpolation"><span class="token punctuation">{</span>damage<span class="token punctuation">}</span></span><span class="token string">, HP: </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>hp<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> damage <span class="token operator">&gt;</span> self<span class="token punctuation">.</span>hp<span class="token punctuation">:</span></span>
<span class="line">            ERROR_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> died from </span><span class="token interpolation"><span class="token punctuation">{</span>attackerId<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-kbengine-日志配置" tabindex="-1"><a class="header-anchor" href="#_2-2-kbengine-日志配置"><span>2.2 KBEngine 日志配置</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 日志配置</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">日志配置文件: kbengine_defs.xml</span>
<span class="line"></span>
<span class="line">&lt;root&gt;</span>
<span class="line">    &lt;logging&gt;</span>
<span class="line">        &lt;!-- 日志级别: DEBUG, INFO, WARNING, ERROR, CRITICAL --&gt;</span>
<span class="line">        &lt;defaultLevel&gt;INFO&lt;/defaultLevel&gt;</span>
<span class="line"></span>
<span class="line">        &lt;!-- 日志路径 --&gt;</span>
<span class="line">        &lt;logPath&gt;logs/&lt;/logPath&gt;</span>
<span class="line"></span>
<span class="line">        &lt;!-- 单个日志文件大小限制 (MB) --&gt;</span>
<span class="line">        &lt;logSize&gt;100&lt;/logSize&gt;</span>
<span class="line"></span>
<span class="line">        &lt;!-- 日志文件数量 --&gt;</span>
<span class="line">        &lt;logCount&gt;10&lt;/logCount&gt;</span>
<span class="line"></span>
<span class="line">        &lt;!-- 组件日志 --&gt;</span>
<span class="line">        &lt;components&gt;</span>
<span class="line">            &lt;!-- 全局日志 --&gt;</span>
<span class="line">            &lt;default&gt;</span>
<span class="line">                &lt;level&gt;INFO&lt;/level&gt;</span>
<span class="line">            &lt;/default&gt;</span>
<span class="line"></span>
<span class="line">            &lt;!-- DBMgr 单独日志 --&gt;</span>
<span class="line">            &lt;DBMgr&gt;</span>
<span class="line">                &lt;level&gt;DEBUG&lt;/level&gt;</span>
<span class="line">            &lt;/DBMgr&gt;</span>
<span class="line"></span>
<span class="line">            &lt;!-- BaseApp 单独日志 --&gt;</span>
<span class="line">            &lt;BaseApp&gt;</span>
<span class="line">                &lt;level&gt;INFO&lt;/level&gt;</span>
<span class="line">            &lt;/BaseApp&gt;</span>
<span class="line"></span>
<span class="line">            &lt;!-- CellApp 单独日志 --&gt;</span>
<span class="line">            &lt;CellApp&gt;</span>
<span class="line">                &lt;level&gt;INFO&lt;/level&gt;</span>
<span class="line">            &lt;/CellApp&gt;</span>
<span class="line">        &lt;/components&gt;</span>
<span class="line">    &lt;/logging&gt;</span>
<span class="line">&lt;/root&gt;</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、结构化日志" tabindex="-1"><a class="header-anchor" href="#三、结构化日志"><span>三、结构化日志</span></a></h2><h3 id="_3-1-json-格式日志" tabindex="-1"><a class="header-anchor" href="#_3-1-json-格式日志"><span>3.1 JSON 格式日志</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 结构化 JSON 日志</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> json</span>
<span class="line"><span class="token keyword">import</span> time</span>
<span class="line"><span class="token keyword">from</span> datetime <span class="token keyword">import</span> datetime</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">StructuredLogger</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;结构化日志记录器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> component<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>component <span class="token operator">=</span> component</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">log</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> level<span class="token punctuation">,</span> message<span class="token punctuation">,</span> <span class="token operator">**</span>context<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;记录结构化日志&quot;&quot;&quot;</span></span>
<span class="line">        log_entry <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;timestamp&quot;</span><span class="token punctuation">:</span> datetime<span class="token punctuation">.</span>utcnow<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>isoformat<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;level&quot;</span><span class="token punctuation">:</span> level<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;component&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>component<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;message&quot;</span><span class="token punctuation">:</span> message<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;context&quot;</span><span class="token punctuation">:</span> context</span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 输出到文件</span></span>
<span class="line">        self<span class="token punctuation">.</span>write_log<span class="token punctuation">(</span>log_entry<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">write_log</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> log_entry<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;写入日志文件&quot;&quot;&quot;</span></span>
<span class="line">        log_file <span class="token operator">=</span> <span class="token string-interpolation"><span class="token string">f&quot;logs/</span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>component<span class="token punctuation">}</span></span><span class="token string">.log&quot;</span></span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">with</span> <span class="token builtin">open</span><span class="token punctuation">(</span>log_file<span class="token punctuation">,</span> <span class="token string">&#39;a&#39;</span><span class="token punctuation">)</span> <span class="token keyword">as</span> f<span class="token punctuation">:</span></span>
<span class="line">            f<span class="token punctuation">.</span>write<span class="token punctuation">(</span>json<span class="token punctuation">.</span>dumps<span class="token punctuation">(</span>log_entry<span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token string">&#39;\\n&#39;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">GameLogger</span><span class="token punctuation">(</span>StructuredLogger<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;游戏日志记录器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token builtin">super</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>__init__<span class="token punctuation">(</span><span class="token string">&quot;game&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">log_trade</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> player_a<span class="token punctuation">,</span> player_b<span class="token punctuation">,</span> items<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;记录交易日志&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>log<span class="token punctuation">(</span><span class="token string">&quot;INFO&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;Trade completed&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                 player_a<span class="token operator">=</span>player_a<span class="token punctuation">,</span></span>
<span class="line">                 player_b<span class="token operator">=</span>player_b<span class="token punctuation">,</span></span>
<span class="line">                 items<span class="token operator">=</span>items<span class="token punctuation">,</span></span>
<span class="line">                 trade_id<span class="token operator">=</span>self<span class="token punctuation">.</span>generate_trade_id<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">log_combat</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity_id<span class="token punctuation">,</span> damage<span class="token punctuation">,</span> attacker_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;记录战斗日志&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>log<span class="token punctuation">(</span><span class="token string">&quot;INFO&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;Entity took damage&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                 entity_id<span class="token operator">=</span>entity_id<span class="token punctuation">,</span></span>
<span class="line">                 damage<span class="token operator">=</span>damage<span class="token punctuation">,</span></span>
<span class="line">                 attacker_id<span class="token operator">=</span>attacker_id<span class="token punctuation">,</span></span>
<span class="line">                 remaining_hp<span class="token operator">=</span>self<span class="token punctuation">.</span>get_entity_hp<span class="token punctuation">(</span>entity_id<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">log_chat</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> channel<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;记录聊天日志&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>log<span class="token punctuation">(</span><span class="token string">&quot;INFO&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;Chat message&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                 player_id<span class="token operator">=</span>player_id<span class="token punctuation">,</span></span>
<span class="line">                 channel<span class="token operator">=</span>channel<span class="token punctuation">,</span></span>
<span class="line">                 message<span class="token operator">=</span>message<span class="token punctuation">,</span></span>
<span class="line">                 timestamp<span class="token operator">=</span><span class="token builtin">int</span><span class="token punctuation">(</span>time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、日志收集" tabindex="-1"><a class="header-anchor" href="#四、日志收集"><span>四、日志收集</span></a></h2><h3 id="_4-1-filebeat-收集" tabindex="-1"><a class="header-anchor" href="#_4-1-filebeat-收集"><span>4.1 Filebeat 收集</span></a></h3><div class="language-yaml line-numbers-mode" data-highlighter="prismjs" data-ext="yml"><pre><code class="language-yaml"><span class="line"><span class="token comment"># filebeat 配置</span></span>
<span class="line"></span>
<span class="line"><span class="token key atrule">filebeat.inputs</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token comment"># KBEngine 日志文件</span></span>
<span class="line">  <span class="token punctuation">-</span> <span class="token key atrule">type</span><span class="token punctuation">:</span> log</span>
<span class="line">    <span class="token key atrule">enabled</span><span class="token punctuation">:</span> <span class="token boolean important">true</span></span>
<span class="line">    <span class="token key atrule">paths</span><span class="token punctuation">:</span></span>
<span class="line">      <span class="token punctuation">-</span> /path/to/kbengine/logs/<span class="token important">*.log</span></span>
<span class="line">    <span class="token key atrule">fields</span><span class="token punctuation">:</span></span>
<span class="line">      <span class="token key atrule">service</span><span class="token punctuation">:</span> kbengine</span>
<span class="line">      <span class="token key atrule">env</span><span class="token punctuation">:</span> production</span>
<span class="line">    <span class="token key atrule">fields_under_root</span><span class="token punctuation">:</span> <span class="token boolean important">true</span></span>
<span class="line">    <span class="token key atrule">multiline</span><span class="token punctuation">:</span></span>
<span class="line">      <span class="token key atrule">pattern</span><span class="token punctuation">:</span> <span class="token string">&#39;^[&#39;</span></span>
<span class="line">      <span class="token key atrule">negate</span><span class="token punctuation">:</span> <span class="token boolean important">true</span></span>
<span class="line">      <span class="token key atrule">match</span><span class="token punctuation">:</span> after</span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 解析 JSON 日志</span></span>
<span class="line">    <span class="token key atrule">json.keys_under_root</span><span class="token punctuation">:</span> <span class="token boolean important">true</span></span>
<span class="line"></span>
<span class="line"><span class="token key atrule">output.elasticsearch</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token key atrule">hosts</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token string">&quot;localhost:9200&quot;</span><span class="token punctuation">]</span></span>
<span class="line">  <span class="token key atrule">indices</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token punctuation">-</span> <span class="token string">&quot;kbengine-%{+yyyy.MM.dd}&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、日志检索" tabindex="-1"><a class="header-anchor" href="#五、日志检索"><span>五、日志检索</span></a></h2><h3 id="_5-1-kibana-查询" tabindex="-1"><a class="header-anchor" href="#_5-1-kibana-查询"><span>5.1 Kibana 查询</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 日志查询示例</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">Elasticsearch/Kibana 查询示例:</span>
<span class="line"></span>
<span class="line">1. 查询所有错误日志:</span>
<span class="line">   level: &quot;ERROR&quot;</span>
<span class="line"></span>
<span class="line">2. 查询特定玩家的操作:</span>
<span class="line">   context.player_id: &quot;12345&quot;</span>
<span class="line"></span>
<span class="line">3. 查询特定时间范围:</span>
<span class="line">   timestamp: [2024-01-01 TO 2024-01-31]</span>
<span class="line"></span>
<span class="line">4. 查询包含特定关键词:</span>
<span class="line">   message: *login*</span>
<span class="line"></span>
<span class="line">5. 聚合查询:</span>
<span class="line">   level: &quot;ERROR&quot; AND message: *database*</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-日志设计建议" tabindex="-1"><a class="header-anchor" href="#_6-1-日志设计建议"><span>6.1 日志设计建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>结构化</strong></td><td>JSON 格式便于解析</td></tr><tr><td><strong>上下文</strong></td><td>包含关键信息</td></tr><tr><td><strong>分级记录</strong></td><td>合理设置日志级别</td></tr><tr><td><strong>定期轮转</strong></td><td>避免单个文件过大</td></tr><tr><td><strong>异步写入</strong></td><td>不阻塞主线程</td></tr></tbody></table><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="日志系统核心" tabindex="-1"><a class="header-anchor" href="#日志系统核心"><span>日志系统核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">日志系统 = 分级分类 + 结构化格式 + 异步写入 + 集中存储</span>
<span class="line">- 合理的日志级别</span>
<span class="line">- JSON 结构化</span>
<span class="line">- 文件轮转</span>
<span class="line">- Elasticsearch 存储</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://kbengine.github.io/docs/" target="_blank" rel="noopener noreferrer">KBEngine Logging</a></li><li><a href="https://www.elastic.co/what-is/elk" target="_blank" rel="noopener noreferrer">ELK Stack</a></li><li><a href="https://docs.python.org/3/library/logging.html" target="_blank" rel="noopener noreferrer">Python Logging</a></li></ul>`,39)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};