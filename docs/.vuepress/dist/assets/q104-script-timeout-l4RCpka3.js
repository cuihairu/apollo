import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q104-script-timeout.html","title":"Q104: 如何限制脚本的执行时间？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q104-script-timeout.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q104-script-timeout.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q104-如何限制脚本的执行时间" tabindex="-1"><a class="header-anchor" href="#q104-如何限制脚本的执行时间"><span>Q104: 如何限制脚本的执行时间？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对脚本执行控制的理解：</p><ul><li>执行超时检测</li><li>中断执行</li><li>沙箱隔离</li><li>KBEngine 机制</li></ul><hr><h2 id="一、执行超时问题" tabindex="-1"><a class="header-anchor" href="#一、执行超时问题"><span>一、执行超时问题</span></a></h2><h3 id="_1-1-为什么需要限制" tabindex="-1"><a class="header-anchor" href="#_1-1-为什么需要限制"><span>1.1 为什么需要限制</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    脚本执行风险                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  无限循环:                                                   │</span>
<span class="line">│  ├── while True: pass                                       │</span>
<span class="line">│  ├── 占用 CPU                                              │</span>
<span class="line">│  ├── 阻塞其他请求                                           │</span>
<span class="line">│  └── 示例: 死循环搜索                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  长时间运行:                                                 │</span>
<span class="line">│  ├── 复杂计算                                               │</span>
<span class="line">│  ├── 网络请求                                               │</span>
<span class="line">│  ├── 超时响应                                               │</span>
<span class="line">│  └── 示例: 大量数据处理                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  恶意代码:                                                   │</span>
<span class="line">│  ├── 故意延迟                                               │</span>
<span class="line">│  ├── 资源耗尽                                               │</span>
<span class="line">│  └── 示例: 外挂脚本                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、python-超时控制" tabindex="-1"><a class="header-anchor" href="#二、python-超时控制"><span>二、Python 超时控制</span></a></h2><h3 id="_2-1-信号超时" tabindex="-1"><a class="header-anchor" href="#_2-1-信号超时"><span>2.1 信号超时</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Python 执行超时控制</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> signal</span>
<span class="line"><span class="token keyword">import</span> time</span>
<span class="line"><span class="token keyword">from</span> contextlib <span class="token keyword">import</span> contextmanager</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TimeoutError</span><span class="token punctuation">(</span>Exception<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;超时异常&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">pass</span></span>
<span class="line"></span>
<span class="line"><span class="token decorator annotation punctuation">@contextmanager</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">time_limit</span><span class="token punctuation">(</span>seconds<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;时间限制上下文管理器&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">signal_handler</span><span class="token punctuation">(</span>signum<span class="token punctuation">,</span> frame<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">raise</span> TimeoutError<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Timed out after </span><span class="token interpolation"><span class="token punctuation">{</span>seconds<span class="token punctuation">}</span></span><span class="token string"> seconds&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 注册信号处理</span></span>
<span class="line">    old_handler <span class="token operator">=</span> signal<span class="token punctuation">.</span>signal<span class="token punctuation">(</span>signal<span class="token punctuation">.</span>SIGALRM<span class="token punctuation">,</span> signal_handler<span class="token punctuation">)</span></span>
<span class="line">    signal<span class="token punctuation">.</span>alarm<span class="token punctuation">(</span>seconds<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">yield</span></span>
<span class="line">    <span class="token keyword">finally</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 恢复旧处理</span></span>
<span class="line">        signal<span class="token punctuation">.</span>alarm<span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">        signal<span class="token punctuation">.</span>signal<span class="token punctuation">(</span>signal<span class="token punctuation">.</span>SIGALRM<span class="token punctuation">,</span> old_handler<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">execute_script_with_timeout</span><span class="token punctuation">(</span>script_func<span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">5</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;执行脚本并限制时间&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">with</span> time_limit<span class="token punctuation">(</span>timeout<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> script_func<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">except</span> TimeoutError <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Script execution timeout: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line">    <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Script execution error: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 测试</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">long_running_task</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;长时间运行的任务&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;Task started...&quot;</span><span class="token punctuation">)</span></span>
<span class="line">    time<span class="token punctuation">.</span>sleep<span class="token punctuation">(</span><span class="token number">10</span><span class="token punctuation">)</span>  <span class="token comment"># 模拟长时间运行</span></span>
<span class="line">    <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;Task completed&quot;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">return</span> <span class="token string">&quot;Done&quot;</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 正常情况</span></span>
<span class="line">result <span class="token operator">=</span> execute_script_with_timeout<span class="token punctuation">(</span><span class="token keyword">lambda</span><span class="token punctuation">:</span> time<span class="token punctuation">.</span>sleep<span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">5</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Result: </span><span class="token interpolation"><span class="token punctuation">{</span>result<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span>  <span class="token comment"># Result: None (但执行完成)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 超时情况</span></span>
<span class="line">result <span class="token operator">=</span> execute_script_with_timeout<span class="token punctuation">(</span>long_running_task<span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">3</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Result: </span><span class="token interpolation"><span class="token punctuation">{</span>result<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span>  <span class="token comment"># Result: None (超时)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-线程超时" tabindex="-1"><a class="header-anchor" href="#_2-2-线程超时"><span>2.2 线程超时</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 使用线程控制超时</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> threading</span>
<span class="line"><span class="token keyword">import</span> queue</span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">execute_with_timeout</span><span class="token punctuation">(</span>func<span class="token punctuation">,</span> args<span class="token operator">=</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> kwargs<span class="token operator">=</span><span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">5</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;使用线程执行并超时控制&quot;&quot;&quot;</span></span>
<span class="line">    result_queue <span class="token operator">=</span> queue<span class="token punctuation">.</span>Queue<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">worker</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            result <span class="token operator">=</span> func<span class="token punctuation">(</span><span class="token operator">*</span>args<span class="token punctuation">,</span> <span class="token operator">**</span>kwargs<span class="token punctuation">)</span></span>
<span class="line">            result_queue<span class="token punctuation">.</span>put<span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token boolean">True</span><span class="token punctuation">,</span> result<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">            result_queue<span class="token punctuation">.</span>put<span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token boolean">False</span><span class="token punctuation">,</span> e<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    thread <span class="token operator">=</span> threading<span class="token punctuation">.</span>Thread<span class="token punctuation">(</span>target<span class="token operator">=</span>worker<span class="token punctuation">)</span></span>
<span class="line">    thread<span class="token punctuation">.</span>daemon <span class="token operator">=</span> <span class="token boolean">True</span></span>
<span class="line">    thread<span class="token punctuation">.</span>start<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    thread<span class="token punctuation">.</span>join<span class="token punctuation">(</span>timeout<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> thread<span class="token punctuation">.</span>is_alive<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 线程仍在运行，超时</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Function execution timeout after </span><span class="token interpolation"><span class="token punctuation">{</span>timeout<span class="token punctuation">}</span></span><span class="token string">s&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">        success<span class="token punctuation">,</span> result <span class="token operator">=</span> result_queue<span class="token punctuation">.</span>get_nowait<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> success<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> result</span>
<span class="line">        <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Function execution error: </span><span class="token interpolation"><span class="token punctuation">{</span>result<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line">    <span class="token keyword">except</span> queue<span class="token punctuation">.</span>Empty<span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">complex_calculation</span><span class="token punctuation">(</span>n<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;复杂计算&quot;&quot;&quot;</span></span>
<span class="line">    total <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line">    <span class="token keyword">for</span> i <span class="token keyword">in</span> <span class="token builtin">range</span><span class="token punctuation">(</span>n<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        total <span class="token operator">+=</span> i <span class="token operator">**</span> <span class="token number">2</span></span>
<span class="line">        time<span class="token punctuation">.</span>sleep<span class="token punctuation">(</span><span class="token number">0.001</span><span class="token punctuation">)</span>  <span class="token comment"># 模拟耗时</span></span>
<span class="line">    <span class="token keyword">return</span> total</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 超时控制</span></span>
<span class="line">result <span class="token operator">=</span> execute_with_timeout<span class="token punctuation">(</span>complex_calculation<span class="token punctuation">,</span> args<span class="token operator">=</span><span class="token punctuation">(</span><span class="token number">1000000</span><span class="token punctuation">,</span><span class="token punctuation">)</span><span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">2</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Result: </span><span class="token interpolation"><span class="token punctuation">{</span>result<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-多进程隔离" tabindex="-1"><a class="header-anchor" href="#_2-3-多进程隔离"><span>2.3 多进程隔离</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 使用多进程完全隔离</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> multiprocessing</span>
<span class="line"><span class="token keyword">import</span> time</span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">execute_in_process</span><span class="token punctuation">(</span>func<span class="token punctuation">,</span> args<span class="token operator">=</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> kwargs<span class="token operator">=</span><span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">5</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;在独立进程中执行函数&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token comment"># 创建进程</span></span>
<span class="line">    process <span class="token operator">=</span> multiprocessing<span class="token punctuation">.</span>Process<span class="token punctuation">(</span></span>
<span class="line">        target<span class="token operator">=</span>process_wrapper<span class="token punctuation">,</span></span>
<span class="line">        args<span class="token operator">=</span><span class="token punctuation">(</span>func<span class="token punctuation">,</span> args<span class="token punctuation">,</span> kwargs<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    process<span class="token punctuation">.</span>start<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    process<span class="token punctuation">.</span>join<span class="token punctuation">(</span>timeout<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> process<span class="token punctuation">.</span>is_alive<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 超时，终止进程</span></span>
<span class="line">        process<span class="token punctuation">.</span>terminate<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        process<span class="token punctuation">.</span>join<span class="token punctuation">(</span>timeout<span class="token operator">=</span><span class="token number">1</span><span class="token punctuation">)</span>  <span class="token comment"># 等待 1 秒清理</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> process<span class="token punctuation">.</span>is_alive<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            process<span class="token punctuation">.</span>kill<span class="token punctuation">(</span><span class="token punctuation">)</span>  <span class="token comment"># 强制杀死</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Process execution timeout after </span><span class="token interpolation"><span class="token punctuation">{</span>timeout<span class="token punctuation">}</span></span><span class="token string">s&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> process<span class="token punctuation">.</span>exitcode</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">process_wrapper</span><span class="token punctuation">(</span>func<span class="token punctuation">,</span> args<span class="token punctuation">,</span> kwargs<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;进程包装器&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">        result <span class="token operator">=</span> func<span class="token punctuation">(</span><span class="token operator">*</span>args<span class="token punctuation">,</span> <span class="token operator">**</span>kwargs<span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 通过队列返回结果</span></span>
<span class="line">    <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Process error: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 安全的脚本执行</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SafeScriptExecutor</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;安全的脚本执行器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">10</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>timeout <span class="token operator">=</span> timeout</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">execute</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> script_code<span class="token punctuation">,</span> context<span class="token operator">=</span><span class="token boolean">None</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;执行脚本代码&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> context <span class="token keyword">is</span> <span class="token boolean">None</span><span class="token punctuation">:</span></span>
<span class="line">            context <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">def</span> <span class="token function">script_runner</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 在独立进程中执行</span></span>
<span class="line">            <span class="token keyword">exec</span><span class="token punctuation">(</span>script_code<span class="token punctuation">,</span> context<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 使用多进程执行</span></span>
<span class="line">        result <span class="token operator">=</span> execute_in_process<span class="token punctuation">(</span>script_runner<span class="token punctuation">,</span> timeout<span class="token operator">=</span>self<span class="token punctuation">.</span>timeout<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> result</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line">executor <span class="token operator">=</span> SafeScriptExecutor<span class="token punctuation">(</span>timeout<span class="token operator">=</span><span class="token number">3</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">script <span class="token operator">=</span> <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">import time</span>
<span class="line">def slow_function():</span>
<span class="line">    time.sleep(10)</span>
<span class="line">    return &quot;Done&quot;</span>
<span class="line">slow_function()</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">executor<span class="token punctuation">.</span>execute<span class="token punctuation">(</span>script<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、lua-超时控制" tabindex="-1"><a class="header-anchor" href="#三、lua-超时控制"><span>三、Lua 超时控制</span></a></h2><h3 id="_3-1-钩子超时" tabindex="-1"><a class="header-anchor" href="#_3-1-钩子超时"><span>3.1 钩子超时</span></a></h3><div class="language-c line-numbers-mode" data-highlighter="prismjs" data-ext="c"><pre><code class="language-c"><span class="line"><span class="token comment">// Lua 执行超时控制</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;lua.hpp&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;chrono&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;thread&gt;</span></span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 超时检测结构</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">TimeoutState</span> <span class="token punctuation">{</span></span>
<span class="line">    bool timeout<span class="token punctuation">;</span></span>
<span class="line">    <span class="token class-name">time_t</span> deadline<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 钩子函数</span></span>
<span class="line"><span class="token keyword">static</span> <span class="token keyword">void</span> <span class="token function">timeout_hook</span><span class="token punctuation">(</span>lua_State <span class="token operator">*</span>L<span class="token punctuation">,</span> lua_Debug <span class="token operator">*</span>ar<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    TimeoutState <span class="token operator">*</span>state <span class="token operator">=</span> <span class="token punctuation">(</span>TimeoutState <span class="token operator">*</span><span class="token punctuation">)</span><span class="token function">lua_getextraspace</span><span class="token punctuation">(</span>L<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">time</span><span class="token punctuation">(</span><span class="token constant">NULL</span><span class="token punctuation">)</span> <span class="token operator">&gt;=</span> state<span class="token operator">-&gt;</span>deadline<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        state<span class="token operator">-&gt;</span>timeout <span class="token operator">=</span> true<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">luaL_error</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token string">&quot;script execution timeout&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 带超时的执行</span></span>
<span class="line">bool <span class="token function">execute_with_timeout</span><span class="token punctuation">(</span>lua_State <span class="token operator">*</span>L<span class="token punctuation">,</span> <span class="token keyword">const</span> <span class="token keyword">char</span> <span class="token operator">*</span>script<span class="token punctuation">,</span> <span class="token keyword">int</span> timeout_ms<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 设置超时</span></span>
<span class="line">    TimeoutState <span class="token operator">*</span>state <span class="token operator">=</span> <span class="token punctuation">(</span>TimeoutState <span class="token operator">*</span><span class="token punctuation">)</span><span class="token function">lua_getextraspace</span><span class="token punctuation">(</span>L<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    state<span class="token operator">-&gt;</span>timeout <span class="token operator">=</span> false<span class="token punctuation">;</span></span>
<span class="line">    state<span class="token operator">-&gt;</span>deadline <span class="token operator">=</span> <span class="token function">time</span><span class="token punctuation">(</span><span class="token constant">NULL</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token punctuation">(</span>timeout_ms <span class="token operator">/</span> <span class="token number">1000</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 设置钩子 (每条指令检查)</span></span>
<span class="line">    <span class="token function">lua_sethook</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> timeout_hook<span class="token punctuation">,</span> LUA_MASKCOUNT<span class="token punctuation">,</span> <span class="token number">100</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 加载并执行</span></span>
<span class="line">    bool success <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token function">luaL_dostring</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> script<span class="token punctuation">)</span> <span class="token operator">==</span> LUA_OK<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 清除钩子</span></span>
<span class="line">    <span class="token function">lua_sethook</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token constant">NULL</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> success <span class="token operator">&amp;&amp;</span> <span class="token operator">!</span>state<span class="token operator">-&gt;</span>timeout<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">example</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    lua_State <span class="token operator">*</span>L <span class="token operator">=</span> <span class="token function">luaL_newstate</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token function">luaL_openlibs</span><span class="token punctuation">(</span>L<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 正常执行</span></span>
<span class="line">    <span class="token keyword">const</span> <span class="token keyword">char</span> <span class="token operator">*</span>normal_script <span class="token operator">=</span> <span class="token string">&quot;print(&#39;Hello, World!&#39;)&quot;</span><span class="token punctuation">;</span></span>
<span class="line">    bool ok <span class="token operator">=</span> <span class="token function">execute_with_timeout</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> normal_script<span class="token punctuation">,</span> <span class="token number">5000</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token function">printf</span><span class="token punctuation">(</span><span class="token string">&quot;Normal script: %s\\n&quot;</span><span class="token punctuation">,</span> ok <span class="token operator">?</span> <span class="token string">&quot;OK&quot;</span> <span class="token operator">:</span> <span class="token string">&quot;FAILED&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 超时执行</span></span>
<span class="line">    <span class="token keyword">const</span> <span class="token keyword">char</span> <span class="token operator">*</span>timeout_script <span class="token operator">=</span> <span class="token string">&quot;while true do end&quot;</span><span class="token punctuation">;</span></span>
<span class="line">    ok <span class="token operator">=</span> <span class="token function">execute_with_timeout</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> timeout_script<span class="token punctuation">,</span> <span class="token number">1000</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token function">printf</span><span class="token punctuation">(</span><span class="token string">&quot;Timeout script: %s\\n&quot;</span><span class="token punctuation">,</span> ok <span class="token operator">?</span> <span class="token string">&quot;OK&quot;</span> <span class="token operator">:</span> <span class="token string">&quot;TIMEOUT&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token function">lua_close</span><span class="token punctuation">(</span>L<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-lua-线程隔离" tabindex="-1"><a class="header-anchor" href="#_3-2-lua-线程隔离"><span>3.2 Lua 线程隔离</span></a></h3><div class="language-c line-numbers-mode" data-highlighter="prismjs" data-ext="c"><pre><code class="language-c"><span class="line"><span class="token comment">// 使用独立线程执行 Lua</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;lua.hpp&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;thread&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;atomic&gt;</span></span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">LuaExecution</span> <span class="token punctuation">{</span></span>
<span class="line">    lua_State <span class="token operator">*</span>L<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">const</span> <span class="token keyword">char</span> <span class="token operator">*</span>script<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token operator">::</span>atomic<span class="token operator">&lt;</span>bool<span class="token operator">&gt;</span> completed<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token operator">::</span>atomic<span class="token operator">&lt;</span>bool<span class="token operator">&gt;</span> timeout<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">lua_thread_main</span><span class="token punctuation">(</span>LuaExecution <span class="token operator">*</span>exec<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    lua_State <span class="token operator">*</span>L <span class="token operator">=</span> <span class="token function">luaL_newstate</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token function">luaL_openlibs</span><span class="token punctuation">(</span>L<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">luaL_dostring</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> exec<span class="token operator">-&gt;</span>script<span class="token punctuation">)</span> <span class="token operator">==</span> LUA_OK<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        exec<span class="token operator">-&gt;</span>completed <span class="token operator">=</span> true<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 错误</span></span>
<span class="line">        <span class="token keyword">const</span> <span class="token keyword">char</span> <span class="token operator">*</span>error <span class="token operator">=</span> <span class="token function">lua_tostring</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">fprintf</span><span class="token punctuation">(</span><span class="token constant">stderr</span><span class="token punctuation">,</span> <span class="token string">&quot;Lua error: %s\\n&quot;</span><span class="token punctuation">,</span> error<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token function">lua_close</span><span class="token punctuation">(</span>L<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">bool <span class="token function">execute_in_thread</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">char</span> <span class="token operator">*</span>script<span class="token punctuation">,</span> <span class="token keyword">int</span> timeout_ms<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    LuaExecution exec <span class="token operator">=</span> <span class="token punctuation">{</span>nullptr<span class="token punctuation">,</span> script<span class="token punctuation">,</span> false<span class="token punctuation">,</span> false<span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 启动线程</span></span>
<span class="line">    std<span class="token operator">::</span>thread <span class="token function">thread</span><span class="token punctuation">(</span>lua_thread_main<span class="token punctuation">,</span> <span class="token operator">&amp;</span>thread<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 等待完成或超时</span></span>
<span class="line">    <span class="token keyword">auto</span> start <span class="token operator">=</span> std<span class="token operator">::</span>chrono<span class="token operator">::</span>steady_clock<span class="token operator">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>exec<span class="token punctuation">.</span>completed<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> elapsed <span class="token operator">=</span> std<span class="token operator">::</span>chrono<span class="token operator">::</span>duration_cast<span class="token operator">&lt;</span>std<span class="token operator">::</span>chrono<span class="token operator">::</span>milliseconds<span class="token operator">&gt;</span><span class="token punctuation">(</span></span>
<span class="line">            std<span class="token operator">::</span>chrono<span class="token operator">::</span>steady_clock<span class="token operator">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> start</span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">count</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>elapsed <span class="token operator">&gt;=</span> timeout_ms<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            exec<span class="token punctuation">.</span>timeout <span class="token operator">=</span> true<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        std<span class="token operator">::</span>this_thread<span class="token operator">::</span><span class="token function">sleep_for</span><span class="token punctuation">(</span>std<span class="token operator">::</span>chrono<span class="token operator">::</span><span class="token function">milliseconds</span><span class="token punctuation">(</span><span class="token number">10</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    thread<span class="token punctuation">.</span><span class="token function">detach</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 分离线程 (让 Lua 自己清理)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> exec<span class="token punctuation">.</span>completed <span class="token operator">&amp;&amp;</span> <span class="token operator">!</span>exec<span class="token punctuation">.</span>timeout<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、沙箱隔离" tabindex="-1"><a class="header-anchor" href="#四、沙箱隔离"><span>四、沙箱隔离</span></a></h2><h3 id="_4-1-python-沙箱" tabindex="-1"><a class="header-anchor" href="#_4-1-python-沙箱"><span>4.1 Python 沙箱</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Python 沙箱执行</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> sys</span>
<span class="line"><span class="token keyword">import</span> types</span>
<span class="line"><span class="token keyword">import</span> time</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Sandbox</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;Python 沙箱&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 允许的内置函数</span></span>
<span class="line">    ALLOWED_BUILTINS <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&#39;print&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;len&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;range&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;str&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;int&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;float&#39;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;list&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;dict&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;tuple&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;set&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;bool&#39;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;min&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;max&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;sum&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;abs&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;round&#39;</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token string">&#39;enumerate&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;zip&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;map&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;filter&#39;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">10</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>timeout <span class="token operator">=</span> timeout</span>
<span class="line">        self<span class="token punctuation">.</span><span class="token builtin">globals</span> <span class="token operator">=</span> self<span class="token punctuation">.</span>_create_globals<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">_create_globals</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;创建安全的全局命名空间&quot;&quot;&quot;</span></span>
<span class="line">        safe_globals <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;__builtins__&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">                name<span class="token punctuation">:</span> __builtins__<span class="token punctuation">[</span>name<span class="token punctuation">]</span></span>
<span class="line">                <span class="token keyword">for</span> name <span class="token keyword">in</span> self<span class="token punctuation">.</span>ALLOWED_BUILTINS</span>
<span class="line">                <span class="token keyword">if</span> name <span class="token keyword">in</span> __builtins__</span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 添加允许的模块</span></span>
<span class="line">        safe_globals<span class="token punctuation">[</span><span class="token string">&#39;math&#39;</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token builtin">__import__</span><span class="token punctuation">(</span><span class="token string">&#39;math&#39;</span><span class="token punctuation">)</span></span>
<span class="line">        safe_globals<span class="token punctuation">[</span><span class="token string">&#39;time&#39;</span><span class="token punctuation">]</span> <span class="token operator">=</span> time</span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> safe_globals</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">execute</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> code<span class="token punctuation">,</span> context<span class="token operator">=</span><span class="token boolean">None</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;执行沙箱代码&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> context <span class="token keyword">is</span> <span class="token boolean">None</span><span class="token punctuation">:</span></span>
<span class="line">            context <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 合并上下文</span></span>
<span class="line">        exec_globals <span class="token operator">=</span> self<span class="token punctuation">.</span><span class="token builtin">globals</span><span class="token punctuation">.</span>copy<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        exec_globals<span class="token punctuation">.</span>update<span class="token punctuation">(</span>context<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 使用线程执行</span></span>
<span class="line">        <span class="token keyword">def</span> <span class="token function">run</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">exec</span><span class="token punctuation">(</span>code<span class="token punctuation">,</span> exec_globals<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        thread <span class="token operator">=</span> threading<span class="token punctuation">.</span>Thread<span class="token punctuation">(</span>target<span class="token operator">=</span>run<span class="token punctuation">,</span> daemon<span class="token operator">=</span><span class="token boolean">True</span><span class="token punctuation">)</span></span>
<span class="line">        thread<span class="token punctuation">.</span>start<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        thread<span class="token punctuation">.</span>join<span class="token punctuation">(</span>self<span class="token punctuation">.</span>timeout<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> thread<span class="token punctuation">.</span>is_alive<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">raise</span> TimeoutError<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Script execution timeout (</span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>timeout<span class="token punctuation">}</span></span><span class="token string">s)&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> exec_globals</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line">sandbox <span class="token operator">=</span> Sandbox<span class="token punctuation">(</span>timeout<span class="token operator">=</span><span class="token number">3</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 正常代码</span></span>
<span class="line">code1 <span class="token operator">=</span> <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">result = 0</span>
<span class="line">for i in range(100):</span>
<span class="line">    result += i</span>
<span class="line">print(&quot;Sum:&quot;, result)</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line">sandbox<span class="token punctuation">.</span>execute<span class="token punctuation">(</span>code1<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 超时代码</span></span>
<span class="line">code2 <span class="token operator">=</span> <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">import time</span>
<span class="line">time.sleep(10)</span>
<span class="line">print(&quot;Done&quot;)</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"><span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">    sandbox<span class="token punctuation">.</span>execute<span class="token punctuation">(</span>code2<span class="token punctuation">)</span></span>
<span class="line"><span class="token keyword">except</span> TimeoutError <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Error: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、kbengine-执行控制" tabindex="-1"><a class="header-anchor" href="#五、kbengine-执行控制"><span>五、KBEngine 执行控制</span></a></h2><h3 id="_5-1-kbengine-机制" tabindex="-1"><a class="header-anchor" href="#_5-1-kbengine-机制"><span>5.1 KBEngine 机制</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 脚本执行控制</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 内置机制:</span>
<span class="line"></span>
<span class="line">1. 单线程执行: Python 脚本在单线程中执行</span>
<span class="line">2. 消息分片: 大任务分片处理</span>
<span class="line">3. 定时器限制: 单次处理不超过一定时间</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TaskExecutor</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;任务执行器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    MAX_EXECUTION_TIME <span class="token operator">=</span> <span class="token number">0.1</span>  <span class="token comment"># 单次最多执行 100ms</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>task_queue <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line">        self<span class="token punctuation">.</span>current_task <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line">        self<span class="token punctuation">.</span>task_progress <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">submit_task</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity_id<span class="token punctuation">,</span> task_func<span class="token punctuation">,</span> total_steps<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;提交任务&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>task_queue<span class="token punctuation">.</span>append<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;entity_id&#39;</span><span class="token punctuation">:</span> entity_id<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;func&#39;</span><span class="token punctuation">:</span> task_func<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;total_steps&#39;</span><span class="token punctuation">:</span> total_steps<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;current_step&#39;</span><span class="token punctuation">:</span> <span class="token number">0</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">process_tasks</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;处理任务队列&quot;&quot;&quot;</span></span>
<span class="line">        start_time <span class="token operator">=</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">while</span> self<span class="token punctuation">.</span>task_queue<span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 检查执行时间</span></span>
<span class="line">            <span class="token keyword">if</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> start_time <span class="token operator">&gt;</span> self<span class="token punctuation">.</span>MAX_EXECUTION_TIME<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">break</span></span>
<span class="line"></span>
<span class="line">            task <span class="token operator">=</span> self<span class="token punctuation">.</span>task_queue<span class="token punctuation">.</span>pop<span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 执行一步</span></span>
<span class="line">            self<span class="token punctuation">.</span>execute_step<span class="token punctuation">(</span>task<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 如果未完成，放回队列</span></span>
<span class="line">            <span class="token keyword">if</span> task<span class="token punctuation">[</span><span class="token string">&#39;current_step&#39;</span><span class="token punctuation">]</span> <span class="token operator">&lt;</span> task<span class="token punctuation">[</span><span class="token string">&#39;total_steps&#39;</span><span class="token punctuation">]</span><span class="token punctuation">:</span></span>
<span class="line">                self<span class="token punctuation">.</span>task_queue<span class="token punctuation">.</span>append<span class="token punctuation">(</span>task<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">execute_step</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> task<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;执行任务的一步&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 调用任务函数，传入进度</span></span>
<span class="line">            task<span class="token punctuation">[</span><span class="token string">&#39;func&#39;</span><span class="token punctuation">]</span><span class="token punctuation">(</span></span>
<span class="line">                task<span class="token punctuation">[</span><span class="token string">&#39;current_step&#39;</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">                task<span class="token punctuation">[</span><span class="token string">&#39;total_steps&#39;</span><span class="token punctuation">]</span></span>
<span class="line">            <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            task<span class="token punctuation">[</span><span class="token string">&#39;current_step&#39;</span><span class="token punctuation">]</span> <span class="token operator">+=</span> <span class="token number">1</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">            ERROR_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Task execution error: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 玩家任务示例</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Avatar</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;角色实体&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">start_long_task</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> total_steps<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;启动长时间任务&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 使用任务执行器</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>task_executor<span class="token punctuation">.</span>submit_task<span class="token punctuation">(</span></span>
<span class="line">            self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">,</span></span>
<span class="line">            self<span class="token punctuation">.</span>process_task_step<span class="token punctuation">,</span></span>
<span class="line">            total_steps</span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">process_task_step</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> current<span class="token punctuation">,</span> total<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;处理任务的一步&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Processing </span><span class="token interpolation"><span class="token punctuation">{</span>current<span class="token punctuation">}</span></span><span class="token string">/</span><span class="token interpolation"><span class="token punctuation">{</span>total<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 执行一小部分工作</span></span>
<span class="line">        <span class="token comment"># 这里保证每次执行很快</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> current <span class="token operator">&gt;=</span> total<span class="token punctuation">:</span></span>
<span class="line">            INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Task completed!&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>on_task_complete<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">on_task_complete</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;任务完成&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 发送结果给客户端</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>client<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>client<span class="token punctuation">.</span>onTaskComplete<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-超时控制建议" tabindex="-1"><a class="header-anchor" href="#_6-1-超时控制建议"><span>6.1 超时控制建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>进程隔离</strong></td><td>最安全的方式</td></tr><tr><td><strong>信号控制</strong></td><td>Unix 系统推荐</td></tr><tr><td><strong>线程超时</strong></td><td>跨平台但资源开销大</td></tr><tr><td><strong>分片处理</strong></td><td>大任务分成小步骤</td></tr><tr><td><strong>超时检测</strong></td><td>定期检查执行时间</td></tr><tr><td><strong>资源限制</strong></td><td>限制内存/CPU</td></tr></tbody></table><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="脚本超时控制核心" tabindex="-1"><a class="header-anchor" href="#脚本超时控制核心"><span>脚本超时控制核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">超时控制 = 进程隔离 + 信号中断 + 定期检测 + 分片处理</span>
<span class="line">- 多进程最安全</span>
<span class="line">- 信号轻量但 Unix only</span>
<span class="line">- 线程通用但开销大</span>
<span class="line">- 分片处理是游戏服务器推荐方案</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://docs.python.org/3/library/signal.html" target="_blank" rel="noopener noreferrer">Python Signal Module</a></li><li><a href="https://www.lua.org/manual/5.4/manual.html#lua_sethook" target="_blank" rel="noopener noreferrer">Lua Hooks</a></li><li><a href="https://docs.python.org/3/library/multiprocessing.html" target="_blank" rel="noopener noreferrer">Process Isolation</a></li></ul>`,41)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};