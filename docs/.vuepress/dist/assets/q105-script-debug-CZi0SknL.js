import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q105-script-debug.html","title":"Q105: 如何调试脚本？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q105-script-debug.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q105-script-debug.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q105-如何调试脚本" tabindex="-1"><a class="header-anchor" href="#q105-如何调试脚本"><span>Q105: 如何调试脚本？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对脚本调试的理解：</p><ul><li>日志调试</li><li>断点调试</li><li>远程调试</li><li>KBEngine 调试</li></ul><hr><h2 id="一、调试方法概述" tabindex="-1"><a class="header-anchor" href="#一、调试方法概述"><span>一、调试方法概述</span></a></h2><h3 id="_1-1-调试技术" tabindex="-1"><a class="header-anchor" href="#_1-1-调试技术"><span>1.1 调试技术</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    脚本调试方法                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  日志调试:                                                   │</span>
<span class="line">│  ├── print 输出                                             │</span>
<span class="line">│  ├── 结构化日志                                             │</span>
<span class="line">│  ├── 日志级别                                               │</span>
<span class="line">│  └── 示例: print(f&quot;x={x}&quot;)                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  断点调试:                                                   │</span>
<span class="line">│  ├── IDE 断点                                               │</span>
<span class="line">│  ├── 条件断点                                               │</span>
<span class="line">│  ├── 单步执行                                               │</span>
<span class="line">│  └── 示例: pdb, VSCode debugger                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  远程调试:                                                   │</span>
<span class="line">│  ├── 远程断点                                               │</span>
<span class="line">│  ├── 远程控制台                                             │</span>
<span class="line">│  ├── 性能分析                                               │</span>
<span class="line">│  └── 示例: rpdb, pydevd                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  性能分析:                                                   │</span>
<span class="line">│  ├── cProfile                                               │</span>
<span class="line">│  ├── 内存分析                                               │</span>
<span class="line">│  ├── 调用追踪                                               │</span>
<span class="line">│  └── 示例: python -m cProfile                               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、python-调试" tabindex="-1"><a class="header-anchor" href="#二、python-调试"><span>二、Python 调试</span></a></h2><h3 id="_2-1-pdb-基础" tabindex="-1"><a class="header-anchor" href="#_2-1-pdb-基础"><span>2.1 pdb 基础</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Python pdb 调试</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> pdb</span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">complex_calculation</span><span class="token punctuation">(</span>a<span class="token punctuation">,</span> b<span class="token punctuation">,</span> c<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;复杂计算&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 设置断点</span></span>
<span class="line">    pdb<span class="token punctuation">.</span>set_trace<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    result <span class="token operator">=</span> a <span class="token operator">*</span> b <span class="token operator">+</span> c</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> i <span class="token keyword">in</span> <span class="token builtin">range</span><span class="token punctuation">(</span><span class="token number">10</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        result <span class="token operator">+=</span> i</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> result</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># pdb 常用命令</span></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">pdb 命令:</span>
<span class="line"></span>
<span class="line">n (next)        # 执行下一行</span>
<span class="line">s (step)        # 进入函数</span>
<span class="line">c (continue)    # 继续执行</span>
<span class="line">l (list)        # 显示代码</span>
<span class="line">p variable      # 打印变量</span>
<span class="line">pp variable     # 美化打印</span>
<span class="line">w (where)       # 显示堆栈</span>
<span class="line">b 10            # 在第10行设置断点</span>
<span class="line">b func_name     # 在函数设置断点</span>
<span class="line">cl 1            # 清除断点1</span>
<span class="line">                # 清除所有断点</span>
<span class="line">!expression     # 执行表达式</span>
<span class="line">q (quit)        # 退出</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line"><span class="token keyword">if</span> __name__ <span class="token operator">==</span> <span class="token string">&quot;__main__&quot;</span><span class="token punctuation">:</span></span>
<span class="line">    x <span class="token operator">=</span> <span class="token number">5</span></span>
<span class="line">    y <span class="token operator">=</span> <span class="token number">10</span></span>
<span class="line">    z <span class="token operator">=</span> <span class="token number">3</span></span>
<span class="line"></span>
<span class="line">    result <span class="token operator">=</span> complex_calculation<span class="token punctuation">(</span>x<span class="token punctuation">,</span> y<span class="token punctuation">,</span> z<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Result: </span><span class="token interpolation"><span class="token punctuation">{</span>result<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-pdb-高级用法" tabindex="-1"><a class="header-anchor" href="#_2-2-pdb-高级用法"><span>2.2 pdb 高级用法</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 高级 pdb 技巧</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> pdb</span>
<span class="line"><span class="token keyword">import</span> sys</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CustomPdb</span><span class="token punctuation">(</span>pdb<span class="token punctuation">.</span>Pdb<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;自定义 pdb&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token builtin">super</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>__init__<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>prompt <span class="token operator">=</span> <span class="token string">&quot;(DEBUG) &quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">do_stack</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> arg<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;自定义命令: 显示完整堆栈&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">import</span> traceback</span>
<span class="line">        traceback<span class="token punctuation">.</span>print_stack<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">do_vars</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> arg<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;自定义命令: 显示所有局部变量&quot;&quot;&quot;</span></span>
<span class="line">        frame <span class="token operator">=</span> self<span class="token punctuation">.</span>curframe</span>
<span class="line">        <span class="token keyword">if</span> frame<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">for</span> name<span class="token punctuation">,</span> value <span class="token keyword">in</span> frame<span class="token punctuation">.</span>f_locals<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;</span><span class="token interpolation"><span class="token punctuation">{</span>name<span class="token punctuation">}</span></span><span class="token string"> = </span><span class="token interpolation"><span class="token punctuation">{</span>value<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">debug_on_error</span><span class="token punctuation">(</span>func<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;错误时自动进入调试&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">wrapper</span><span class="token punctuation">(</span><span class="token operator">*</span>args<span class="token punctuation">,</span> <span class="token operator">**</span>kwargs<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> func<span class="token punctuation">(</span><span class="token operator">*</span>args<span class="token punctuation">,</span> <span class="token operator">**</span>kwargs<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Error occurred: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;Entering debugger...&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            debugger <span class="token operator">=</span> CustomPdb<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            debugger<span class="token punctuation">.</span>set_trace<span class="token punctuation">(</span>sys<span class="token punctuation">.</span>_getframe<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>f_back<span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">raise</span></span>
<span class="line">    <span class="token keyword">return</span> wrapper</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token decorator annotation punctuation">@debug_on_error</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">buggy_function</span><span class="token punctuation">(</span>data<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;有问题的函数&quot;&quot;&quot;</span></span>
<span class="line">    result <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line">    <span class="token keyword">for</span> item <span class="token keyword">in</span> data<span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 这里可能会有 bug</span></span>
<span class="line">        value <span class="token operator">=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>item<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token builtin">len</span><span class="token punctuation">(</span>item<span class="token punctuation">)</span></span>
<span class="line">        result<span class="token punctuation">.</span>append<span class="token punctuation">(</span>value<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">return</span> result</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 条件断点</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">conditional_break</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;条件断点示例&quot;&quot;&quot;</span></span>
<span class="line">    items <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token number">1</span><span class="token punctuation">,</span> <span class="token number">2</span><span class="token punctuation">,</span> <span class="token number">3</span><span class="token punctuation">,</span> <span class="token number">4</span><span class="token punctuation">,</span> <span class="token number">5</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> i<span class="token punctuation">,</span> item <span class="token keyword">in</span> <span class="token builtin">enumerate</span><span class="token punctuation">(</span>items<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 只在特定条件下断点</span></span>
<span class="line">        <span class="token keyword">if</span> item <span class="token operator">==</span> <span class="token number">3</span><span class="token punctuation">:</span></span>
<span class="line">            pdb<span class="token punctuation">.</span>set_trace<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Processing: </span><span class="token interpolation"><span class="token punctuation">{</span>item<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-远程调试" tabindex="-1"><a class="header-anchor" href="#_2-3-远程调试"><span>2.3 远程调试</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 远程调试 (rpdb)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> rpdb</span>
<span class="line"><span class="token keyword">import</span> socket</span>
<span class="line"><span class="token keyword">import</span> time</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RemoteDebugger</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;远程调试器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> port<span class="token operator">=</span><span class="token number">4444</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>port <span class="token operator">=</span> port</span>
<span class="line">        self<span class="token punctuation">.</span>debugger <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">start</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;启动远程调试服务器&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>debugger <span class="token operator">=</span> rpdb<span class="token punctuation">.</span>Rpdb<span class="token punctuation">(</span>port<span class="token operator">=</span>self<span class="token punctuation">.</span>port<span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Remote debugger started on port </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>port<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Connect with: gdb -ex &#39;target remote localhost:</span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>port<span class="token punctuation">}</span></span><span class="token string">&#39;&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">True</span></span>
<span class="line">        <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Failed to start remote debugger: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">set_trace</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;设置断点&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>debugger<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>debugger<span class="token punctuation">.</span>set_trace<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">remote_debug_example</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;远程调试示例&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    debugger <span class="token operator">=</span> RemoteDebugger<span class="token punctuation">(</span>port<span class="token operator">=</span><span class="token number">4444</span><span class="token punctuation">)</span></span>
<span class="line">    debugger<span class="token punctuation">.</span>start<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 代码执行</span></span>
<span class="line">    data <span class="token operator">=</span> <span class="token builtin">list</span><span class="token punctuation">(</span><span class="token builtin">range</span><span class="token punctuation">(</span><span class="token number">100</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> i<span class="token punctuation">,</span> value <span class="token keyword">in</span> <span class="token builtin">enumerate</span><span class="token punctuation">(</span>data<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 在这里设置断点</span></span>
<span class="line">        <span class="token keyword">if</span> i <span class="token operator">==</span> <span class="token number">50</span><span class="token punctuation">:</span></span>
<span class="line">            debugger<span class="token punctuation">.</span>set_trace<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        result <span class="token operator">=</span> value <span class="token operator">*</span> <span class="token number">2</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> data</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用 pydevd (PyCharm 兼容)</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">pydevd_debug</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;使用 pydevd 调试&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">import</span> pydevd</span>
<span class="line">        <span class="token comment"># 启用远程调试</span></span>
<span class="line">        pydevd<span class="token punctuation">.</span>settrace<span class="token punctuation">(</span><span class="token string">&#39;localhost&#39;</span><span class="token punctuation">,</span> port<span class="token operator">=</span><span class="token number">5678</span><span class="token punctuation">,</span> stdoutToServer<span class="token operator">=</span><span class="token boolean">True</span><span class="token punctuation">,</span></span>
<span class="line">                       stderrToServer<span class="token operator">=</span><span class="token boolean">True</span><span class="token punctuation">,</span> trace_only_current_thread<span class="token operator">=</span><span class="token boolean">False</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">except</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">pass</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 代码在这里可以调试</span></span>
<span class="line">    x <span class="token operator">=</span> <span class="token number">10</span></span>
<span class="line">    y <span class="token operator">=</span> <span class="token number">20</span></span>
<span class="line">    z <span class="token operator">=</span> x <span class="token operator">+</span> y</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> z</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、lua-调试" tabindex="-1"><a class="header-anchor" href="#三、lua-调试"><span>三、Lua 调试</span></a></h2><h3 id="_3-1-lua-调试库" tabindex="-1"><a class="header-anchor" href="#_3-1-lua-调试库"><span>3.1 Lua 调试库</span></a></h3><div class="language-lua line-numbers-mode" data-highlighter="prismjs" data-ext="lua"><pre><code class="language-lua"><span class="line"><span class="token comment">-- Lua 调试</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 调试辅助函数</span></span>
<span class="line"><span class="token keyword">local</span> debug <span class="token operator">=</span> require <span class="token string">&quot;debug&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">local</span> <span class="token keyword">function</span> <span class="token function">print_traceback</span><span class="token punctuation">(</span>level<span class="token punctuation">)</span></span>
<span class="line">    level <span class="token operator">=</span> level <span class="token keyword">or</span> <span class="token number">1</span></span>
<span class="line">    <span class="token function">print</span><span class="token punctuation">(</span><span class="token string">&quot;=== Traceback ===&quot;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">for</span> i <span class="token operator">=</span> level<span class="token punctuation">,</span> math<span class="token punctuation">.</span>huge <span class="token keyword">do</span></span>
<span class="line">        <span class="token keyword">local</span> info <span class="token operator">=</span> debug<span class="token punctuation">.</span><span class="token function">getinfo</span><span class="token punctuation">(</span>i<span class="token punctuation">,</span> <span class="token string">&quot;Slfn&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> info <span class="token keyword">then</span> <span class="token keyword">break</span> <span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">print</span><span class="token punctuation">(</span>string<span class="token punctuation">.</span><span class="token function">format</span><span class="token punctuation">(</span><span class="token string">&quot;[%d] %s:%d in %s&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            i <span class="token operator">-</span> level<span class="token punctuation">,</span></span>
<span class="line">            info<span class="token punctuation">.</span>short_src<span class="token punctuation">,</span></span>
<span class="line">            info<span class="token punctuation">.</span>currentline<span class="token punctuation">,</span></span>
<span class="line">            info<span class="token punctuation">.</span>name <span class="token keyword">or</span> <span class="token string">&quot;(anonymous)&quot;</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">local</span> <span class="token keyword">function</span> <span class="token function">print_locals</span><span class="token punctuation">(</span>level<span class="token punctuation">)</span></span>
<span class="line">    level <span class="token operator">=</span> level <span class="token keyword">or</span> <span class="token number">2</span></span>
<span class="line">    <span class="token function">print</span><span class="token punctuation">(</span><span class="token string">&quot;=== Locals ===&quot;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">local</span> i <span class="token operator">=</span> <span class="token number">1</span></span>
<span class="line">    <span class="token keyword">while</span> <span class="token keyword">true</span> <span class="token keyword">do</span></span>
<span class="line">        <span class="token keyword">local</span> name<span class="token punctuation">,</span> value <span class="token operator">=</span> debug<span class="token punctuation">.</span><span class="token function">getlocal</span><span class="token punctuation">(</span>level<span class="token punctuation">,</span> i<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> name <span class="token keyword">then</span> <span class="token keyword">break</span> <span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">print</span><span class="token punctuation">(</span>string<span class="token punctuation">.</span><span class="token function">format</span><span class="token punctuation">(</span><span class="token string">&quot;%s = %s&quot;</span><span class="token punctuation">,</span> name<span class="token punctuation">,</span> <span class="token function">tostring</span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        i <span class="token operator">=</span> i <span class="token operator">+</span> <span class="token number">1</span></span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">local</span> <span class="token keyword">function</span> <span class="token function">debug_hook</span><span class="token punctuation">(</span>event<span class="token punctuation">,</span> line<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">if</span> event <span class="token operator">==</span> <span class="token string">&quot;line&quot;</span> <span class="token keyword">then</span></span>
<span class="line">        <span class="token comment">-- 可以在这里设置断点条件</span></span>
<span class="line">        <span class="token keyword">local</span> info <span class="token operator">=</span> debug<span class="token punctuation">.</span><span class="token function">getinfo</span><span class="token punctuation">(</span><span class="token number">2</span><span class="token punctuation">,</span> <span class="token string">&quot;Sn&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> info<span class="token punctuation">.</span>currentline <span class="token operator">==</span> <span class="token number">10</span> <span class="token keyword">then</span>  <span class="token comment">-- 在第10行断点</span></span>
<span class="line">            <span class="token function">print</span><span class="token punctuation">(</span><span class="token string">&quot;Breakpoint at line 10&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token function">print_locals</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            debug<span class="token punctuation">.</span><span class="token function">sethook</span><span class="token punctuation">(</span><span class="token punctuation">)</span>  <span class="token comment">-- 清除钩子</span></span>
<span class="line">        <span class="token keyword">end</span></span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 使用示例</span></span>
<span class="line"><span class="token keyword">local</span> <span class="token keyword">function</span> <span class="token function">complex_function</span><span class="token punctuation">(</span>a<span class="token punctuation">,</span> b<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">local</span> result <span class="token operator">=</span> a <span class="token operator">+</span> b</span>
<span class="line"></span>
<span class="line">    <span class="token comment">-- 设置断点</span></span>
<span class="line">    debug<span class="token punctuation">.</span><span class="token function">sethook</span><span class="token punctuation">(</span>debug_hook<span class="token punctuation">,</span> <span class="token string">&quot;l&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> i <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span> <span class="token number">10</span> <span class="token keyword">do</span></span>
<span class="line">        result <span class="token operator">=</span> result <span class="token operator">+</span> i</span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line">    debug<span class="token punctuation">.</span><span class="token function">sethook</span><span class="token punctuation">(</span><span class="token punctuation">)</span>  <span class="token comment">-- 清除钩子</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> result</span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 错误处理</span></span>
<span class="line"><span class="token keyword">local</span> <span class="token keyword">function</span> <span class="token function">safe_call</span><span class="token punctuation">(</span>func<span class="token punctuation">,</span> <span class="token punctuation">...</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">local</span> ok<span class="token punctuation">,</span> result <span class="token operator">=</span> <span class="token function">pcall</span><span class="token punctuation">(</span>func<span class="token punctuation">,</span> <span class="token punctuation">...</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> <span class="token keyword">not</span> ok <span class="token keyword">then</span></span>
<span class="line">        <span class="token function">print</span><span class="token punctuation">(</span><span class="token string">&quot;Error occurred:&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token function">print</span><span class="token punctuation">(</span>result<span class="token punctuation">)</span></span>
<span class="line">        <span class="token function">print_traceback</span><span class="token punctuation">(</span><span class="token number">2</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> ok<span class="token punctuation">,</span> result</span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 测试</span></span>
<span class="line"><span class="token function">safe_call</span><span class="token punctuation">(</span>complex_function<span class="token punctuation">,</span> <span class="token number">10</span><span class="token punctuation">,</span> <span class="token number">20</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-调试" tabindex="-1"><a class="header-anchor" href="#四、kbengine-调试"><span>四、KBEngine 调试</span></a></h2><h3 id="_4-1-kbengine-内置调试" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-内置调试"><span>4.1 KBEngine 内置调试</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 调试工具</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">KBEngineDebugger</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;KBEngine 调试器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">list_all_entities</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;列出所有实体&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;=== All Entities ===&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">for</span> entity_id<span class="token punctuation">,</span> entity <span class="token keyword">in</span> KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;ID: </span><span class="token interpolation"><span class="token punctuation">{</span>entity_id<span class="token punctuation">}</span></span><span class="token string">, Type: </span><span class="token interpolation"><span class="token punctuation">{</span><span class="token builtin">type</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">.</span>__name__<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> <span class="token string">&#39;position&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;  Position: </span><span class="token interpolation"><span class="token punctuation">{</span>entity<span class="token punctuation">.</span>position<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> <span class="token string">&#39;playerName&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;  Name: </span><span class="token interpolation"><span class="token punctuation">{</span>entity<span class="token punctuation">.</span>playerName<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">find_entity_by_name</span><span class="token punctuation">(</span>name<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;按名称查找实体&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">for</span> entity_id<span class="token punctuation">,</span> entity <span class="token keyword">in</span> KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> <span class="token string">&#39;playerName&#39;</span><span class="token punctuation">)</span> <span class="token keyword">and</span> entity<span class="token punctuation">.</span>playerName <span class="token operator">==</span> name<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">return</span> entity</span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">dump_entity</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;转储实体详情&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;=== Entity </span><span class="token interpolation"><span class="token punctuation">{</span>entity<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> ===&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Type: </span><span class="token interpolation"><span class="token punctuation">{</span><span class="token builtin">type</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">.</span>__name__<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 打印所有属性</span></span>
<span class="line">        <span class="token keyword">for</span> attr <span class="token keyword">in</span> <span class="token builtin">dir</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> attr<span class="token punctuation">.</span>startswith<span class="token punctuation">(</span><span class="token string">&#39;_&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                    value <span class="token operator">=</span> <span class="token builtin">getattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> attr<span class="token punctuation">)</span></span>
<span class="line">                    <span class="token keyword">if</span> <span class="token keyword">not</span> <span class="token builtin">callable</span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;  </span><span class="token interpolation"><span class="token punctuation">{</span>attr<span class="token punctuation">}</span></span><span class="token string">: </span><span class="token interpolation"><span class="token punctuation">{</span>value<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">except</span><span class="token punctuation">:</span></span>
<span class="line">                    <span class="token keyword">pass</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">watch_variable</span><span class="token punctuation">(</span>entity_id<span class="token punctuation">,</span> attr_name<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;监控实体属性&quot;&quot;&quot;</span></span>
<span class="line">        entity <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getEntity<span class="token punctuation">(</span>entity_id<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> entity<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>entity_id<span class="token punctuation">}</span></span><span class="token string"> not found&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> attr_name<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity has no attribute &#39;</span><span class="token interpolation"><span class="token punctuation">{</span>attr_name<span class="token punctuation">}</span></span><span class="token string">&#39;&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        value <span class="token operator">=</span> <span class="token builtin">getattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> attr_name<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>entity_id<span class="token punctuation">}</span></span><span class="token string">.</span><span class="token interpolation"><span class="token punctuation">{</span>attr_name<span class="token punctuation">}</span></span><span class="token string"> = </span><span class="token interpolation"><span class="token punctuation">{</span>value<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">profile_entity_update</span><span class="token punctuation">(</span>duration<span class="token operator">=</span><span class="token number">10</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;分析实体更新性能&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">import</span> time</span>
<span class="line">        <span class="token keyword">import</span> sys</span>
<span class="line"></span>
<span class="line">        entity_times <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">def</span> <span class="token function">trace_calls</span><span class="token punctuation">(</span>frame<span class="token punctuation">,</span> event<span class="token punctuation">,</span> arg<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> event <span class="token operator">==</span> <span class="token string">&#39;call&#39;</span><span class="token punctuation">:</span></span>
<span class="line">                func_name <span class="token operator">=</span> frame<span class="token punctuation">.</span>f_code<span class="token punctuation">.</span>co_name</span>
<span class="line">                <span class="token keyword">if</span> <span class="token string">&#39;update&#39;</span> <span class="token keyword">in</span> func_name<span class="token punctuation">.</span>lower<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                    entity_times<span class="token punctuation">[</span>func_name<span class="token punctuation">]</span> <span class="token operator">=</span> entity_times<span class="token punctuation">.</span>get<span class="token punctuation">(</span>func_name<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token number">1</span></span>
<span class="line">            <span class="token keyword">return</span> trace_calls</span>
<span class="line"></span>
<span class="line">        old_trace <span class="token operator">=</span> sys<span class="token punctuation">.</span>gettrace<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        sys<span class="token punctuation">.</span>settrace<span class="token punctuation">(</span>trace_calls<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        start_time <span class="token operator">=</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">while</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> start_time <span class="token operator">&lt;</span> duration<span class="token punctuation">:</span></span>
<span class="line">            time<span class="token punctuation">.</span>sleep<span class="token punctuation">(</span><span class="token number">0.1</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        sys<span class="token punctuation">.</span>settrace<span class="token punctuation">(</span>old_trace<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;=== Update Profile ===&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">for</span> func<span class="token punctuation">,</span> count <span class="token keyword">in</span> <span class="token builtin">sorted</span><span class="token punctuation">(</span>entity_times<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                                   key<span class="token operator">=</span><span class="token keyword">lambda</span> x<span class="token punctuation">:</span> x<span class="token punctuation">[</span><span class="token number">1</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">                                   reverse<span class="token operator">=</span><span class="token boolean">True</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;</span><span class="token interpolation"><span class="token punctuation">{</span>func<span class="token punctuation">}</span></span><span class="token string">: </span><span class="token interpolation"><span class="token punctuation">{</span>count<span class="token punctuation">}</span></span><span class="token string"> calls&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 实体中添加调试方法</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">DebuggableEntity</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;可调试的实体&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>_debug_mode <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line">        self<span class="token punctuation">.</span>_debug_log <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">enable_debug</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;启用调试模式&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>_debug_mode <span class="token operator">=</span> <span class="token boolean">True</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> debug mode enabled&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">disable_debug</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;禁用调试模式&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>_debug_mode <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> debug mode disabled&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">debug_log</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> message<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;调试日志&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>_debug_mode<span class="token punctuation">:</span></span>
<span class="line">            log_entry <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&#39;time&#39;</span><span class="token punctuation">:</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;message&#39;</span><span class="token punctuation">:</span> message</span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            self<span class="token punctuation">.</span>_debug_log<span class="token punctuation">.</span>append<span class="token punctuation">(</span>log_entry<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 限制日志长度</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>_debug_log<span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">1000</span><span class="token punctuation">:</span></span>
<span class="line">                self<span class="token punctuation">.</span>_debug_log <span class="token operator">=</span> self<span class="token punctuation">.</span>_debug_log<span class="token punctuation">[</span><span class="token operator">-</span><span class="token number">1000</span><span class="token punctuation">:</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;[DEBUG] Entity </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string">: </span><span class="token interpolation"><span class="token punctuation">{</span>message<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_debug_log</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取调试日志&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> self<span class="token punctuation">.</span>_debug_log</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onDebugCommand</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> command<span class="token punctuation">,</span> <span class="token operator">*</span>args<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;调试命令处理&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> command <span class="token operator">==</span> <span class="token string">&quot;dump&quot;</span><span class="token punctuation">:</span></span>
<span class="line">            KBEngineDebugger<span class="token punctuation">.</span>dump_entity<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">elif</span> command <span class="token operator">==</span> <span class="token string">&quot;watch&quot;</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> args<span class="token punctuation">:</span></span>
<span class="line">                KBEngineDebugger<span class="token punctuation">.</span>watch_variable<span class="token punctuation">(</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">,</span> args<span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">elif</span> command <span class="token operator">==</span> <span class="token string">&quot;profile&quot;</span><span class="token punctuation">:</span></span>
<span class="line">            KBEngineDebugger<span class="token punctuation">.</span>profile_entity_update<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">elif</span> command <span class="token operator">==</span> <span class="token string">&quot;help&quot;</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;Debug commands: dump, watch &lt;attr&gt;, profile&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、性能分析" tabindex="-1"><a class="header-anchor" href="#五、性能分析"><span>五、性能分析</span></a></h2><h3 id="_5-1-cprofile" tabindex="-1"><a class="header-anchor" href="#_5-1-cprofile"><span>5.1 cProfile</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 性能分析</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> cProfile</span>
<span class="line"><span class="token keyword">import</span> pstats</span>
<span class="line"><span class="token keyword">import</span> io</span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">profile_function</span><span class="token punctuation">(</span>func<span class="token punctuation">,</span> <span class="token operator">*</span>args<span class="token punctuation">,</span> <span class="token operator">**</span>kwargs<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;分析函数性能&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 创建分析器</span></span>
<span class="line">    profiler <span class="token operator">=</span> cProfile<span class="token punctuation">.</span>Profile<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    profiler<span class="token punctuation">.</span>enable<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 执行函数</span></span>
<span class="line">    result <span class="token operator">=</span> func<span class="token punctuation">(</span><span class="token operator">*</span>args<span class="token punctuation">,</span> <span class="token operator">**</span>kwargs<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    profiler<span class="token punctuation">.</span>disable<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 输出统计</span></span>
<span class="line">    s <span class="token operator">=</span> io<span class="token punctuation">.</span>StringIO<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    stats <span class="token operator">=</span> pstats<span class="token punctuation">.</span>Stats<span class="token punctuation">(</span>profiler<span class="token punctuation">,</span> stream<span class="token operator">=</span>s<span class="token punctuation">)</span></span>
<span class="line">    stats<span class="token punctuation">.</span>strip_dirs<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    stats<span class="token punctuation">.</span>sort_stats<span class="token punctuation">(</span><span class="token string">&#39;cumulative&#39;</span><span class="token punctuation">)</span></span>
<span class="line">    stats<span class="token punctuation">.</span>print_stats<span class="token punctuation">(</span><span class="token number">20</span><span class="token punctuation">)</span>  <span class="token comment"># 打印前 20 个</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">print</span><span class="token punctuation">(</span>s<span class="token punctuation">.</span>getvalue<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> result</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">game_logic_update</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;游戏逻辑更新&quot;&quot;&quot;</span></span>
<span class="line">    players <span class="token operator">=</span> <span class="token builtin">list</span><span class="token punctuation">(</span><span class="token builtin">range</span><span class="token punctuation">(</span><span class="token number">1000</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> player_id <span class="token keyword">in</span> players<span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 模拟玩家更新</span></span>
<span class="line">        x <span class="token operator">=</span> player_id <span class="token operator">*</span> <span class="token number">2</span></span>
<span class="line">        y <span class="token operator">=</span> player_id <span class="token operator">*</span> <span class="token number">3</span></span>
<span class="line">        z <span class="token operator">=</span> x <span class="token operator">+</span> y</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> <span class="token builtin">len</span><span class="token punctuation">(</span>players<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 分析</span></span>
<span class="line">profile_function<span class="token punctuation">(</span>game_logic_update<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、调试工具推荐" tabindex="-1"><a class="header-anchor" href="#六、调试工具推荐"><span>六、调试工具推荐</span></a></h2><h3 id="_6-1-工具对比" tabindex="-1"><a class="header-anchor" href="#_6-1-工具对比"><span>6.1 工具对比</span></a></h3><table><thead><tr><th>工具</th><th>语言</th><th>类型</th><th>特点</th></tr></thead><tbody><tr><td>pdb</td><td>Python</td><td>命令行</td><td>内置，无需安装</td></tr><tr><td>ipdb</td><td>Python</td><td>命令行</td><td>增强 pdb，支持 tab 补全</td></tr><tr><td>pudb</td><td>Python</td><td>TUI</td><td>类似 IDE 的界面</td></tr><tr><td>VSCode</td><td>Python</td><td>GUI</td><td>集成调试</td></tr><tr><td>PyCharm</td><td>Python</td><td>GUI</td><td>强大的 Python IDE</td></tr><tr><td>lua-debugger</td><td>Lua</td><td>命令行</td><td>内置调试库</td></tr><tr><td>ZeroBrane</td><td>Lua</td><td>GUI</td><td>Lua IDE</td></tr><tr><td>Decoda</td><td>Lua</td><td>GUI</td><td>商业 Lua 调试器</td></tr></tbody></table><hr><h2 id="七、最佳实践" tabindex="-1"><a class="header-anchor" href="#七、最佳实践"><span>七、最佳实践</span></a></h2><h3 id="_7-1-调试建议" tabindex="-1"><a class="header-anchor" href="#_7-1-调试建议"><span>7.1 调试建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>日志优先</strong></td><td>先用日志定位问题</td></tr><tr><td><strong>单元测试</strong></td><td>隔离测试函数</td></tr><tr><td><strong>最小复现</strong></td><td>简化问题场景</td></tr><tr><td><strong>二分法</strong></td><td>通过二分定位问题代码</td></tr><tr><td><strong>版本控制</strong></td><td>对比正常/异常版本</td></tr><tr><td><strong>远程调试</strong></td><td>生产环境谨慎使用</td></tr></tbody></table><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="脚本调试核心" tabindex="-1"><a class="header-anchor" href="#脚本调试核心"><span>脚本调试核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">脚本调试 = 日志 + 断点 + 远程 + 性能分析</span>
<span class="line">- pdb/Lua debugger 内置支持</span>
<span class="line">- VSCode/PyCharm 强大易用</span>
<span class="line">- 远程调试用于生产环境</span>
<span class="line">- 性能分析找瓶颈</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://docs.python.org/3/library/pdb.html" target="_blank" rel="noopener noreferrer">Python pdb Documentation</a></li><li><a href="https://www.lua.org/manual/5.4/manual.html#pdf-debug" target="_blank" rel="noopener noreferrer">Lua Debug Library</a></li><li><a href="https://code.visualstudio.com/docs/python/debugging" target="_blank" rel="noopener noreferrer">VSCode Python Debugging</a></li></ul>`,43)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};