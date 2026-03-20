import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q103-script-hot-reload.html","title":"Q103: 如何实现脚本热更新？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q103-script-hot-reload.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q103-script-hot-reload.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q103-如何实现脚本热更新" tabindex="-1"><a class="header-anchor" href="#q103-如何实现脚本热更新"><span>Q103: 如何实现脚本热更新？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对脚本热更新的理解：</p><ul><li>热更新原理</li><li>状态保持</li><li>模块重载</li><li>KBEngine 热更</li></ul><hr><h2 id="一、热更新原理" tabindex="-1"><a class="header-anchor" href="#一、热更新原理"><span>一、热更新原理</span></a></h2><h3 id="_1-1-热更类型" tabindex="-1"><a class="header-anchor" href="#_1-1-热更类型"><span>1.1 热更类型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    脚本热更新类型                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  配置热更新:                                                 │</span>
<span class="line">│  ├── 配置文件变化                                            │</span>
<span class="line">│  ├── 重新加载配置                                            │</span>
<span class="line">│  ├── 无需重启                                              │</span>
<span class="line">│  └── 示例: 伤害系数、掉落表                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  代码热更新:                                                 │</span>
<span class="line">│  ├── 函数实现变化                                            │</span>
<span class="line">│  ├── 新增函数                                               │</span>
<span class="line">│  ├── 模块重载                                               │</span>
<span class="line">│  └── 示例: Bug 修复、功能增强                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  结构热更新 (复杂):                                          │</span>
<span class="line">│  ├── 类定义变化                                              │</span>
<span class="line">│  ├── 继承关系变化                                            │</span>
<span class="line">│  ├── 需要重启或特殊处理                                       │</span>
<span class="line">│  └── 示例: 新增实体属性                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、python-热更新" tabindex="-1"><a class="header-anchor" href="#二、python-热更新"><span>二、Python 热更新</span></a></h2><h3 id="_2-1-importlib-reload" tabindex="-1"><a class="header-anchor" href="#_2-1-importlib-reload"><span>2.1 importlib.reload</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Python 热更新实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> sys</span>
<span class="line"><span class="token keyword">import</span> importlib</span>
<span class="line"><span class="token keyword">import</span> os</span>
<span class="line"><span class="token keyword">import</span> time</span>
<span class="line"><span class="token keyword">from</span> watchdog<span class="token punctuation">.</span>observers <span class="token keyword">import</span> Observer</span>
<span class="line"><span class="token keyword">from</span> watchdog<span class="token punctuation">.</span>events <span class="token keyword">import</span> FileSystemEventHandler</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ScriptReloader</span><span class="token punctuation">(</span>FileSystemEventHandler<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;脚本文件变化处理器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> script_dir<span class="token operator">=</span><span class="token string">&quot;scripts&quot;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>script_dir <span class="token operator">=</span> script_dir</span>
<span class="line">        self<span class="token punctuation">.</span>loaded_modules <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">        self<span class="token punctuation">.</span>reload_callbacks <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">on_modified</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> event<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;文件修改时调用&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> event<span class="token punctuation">.</span>is_directory<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        file_path <span class="token operator">=</span> event<span class="token punctuation">.</span>src_path</span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> file_path<span class="token punctuation">.</span>endswith<span class="token punctuation">(</span><span class="token string">&#39;.py&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 转换为模块名</span></span>
<span class="line">        module_name <span class="token operator">=</span> self<span class="token punctuation">.</span>path_to_module<span class="token punctuation">(</span>file_path<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> module_name <span class="token keyword">in</span> sys<span class="token punctuation">.</span>modules<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;[HotReload] Reloading: </span><span class="token interpolation"><span class="token punctuation">{</span>module_name<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 保存旧模块状态</span></span>
<span class="line">            old_module <span class="token operator">=</span> sys<span class="token punctuation">.</span>modules<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 调用清理函数</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>old_module<span class="token punctuation">,</span> <span class="token string">&#39;on_unload&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                old_module<span class="token punctuation">.</span>on_unload<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 重新加载</span></span>
<span class="line">            <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                new_module <span class="token operator">=</span> importlib<span class="token punctuation">.</span><span class="token builtin">reload</span><span class="token punctuation">(</span>old_module<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment"># 调用加载函数</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>new_module<span class="token punctuation">,</span> <span class="token string">&#39;on_load&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                    new_module<span class="token punctuation">.</span>on_load<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment"># 通知回调</span></span>
<span class="line">                <span class="token keyword">for</span> callback <span class="token keyword">in</span> self<span class="token punctuation">.</span>reload_callbacks<span class="token punctuation">:</span></span>
<span class="line">                    callback<span class="token punctuation">(</span>module_name<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;[HotReload] Success: </span><span class="token interpolation"><span class="token punctuation">{</span>module_name<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;[HotReload] Error reloading </span><span class="token interpolation"><span class="token punctuation">{</span>module_name<span class="token punctuation">}</span></span><span class="token string">: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">path_to_module</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> file_path<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;文件路径转模块名&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 移除 .py 后缀</span></span>
<span class="line">        module_path <span class="token operator">=</span> file_path<span class="token punctuation">.</span>replace<span class="token punctuation">(</span><span class="token string">&#39;.py&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;&#39;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 转换路径分隔符</span></span>
<span class="line">        module_path <span class="token operator">=</span> module_path<span class="token punctuation">.</span>replace<span class="token punctuation">(</span><span class="token string">&#39;/&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;.&#39;</span><span class="token punctuation">)</span><span class="token punctuation">.</span>replace<span class="token punctuation">(</span><span class="token string">&#39;\\\\&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;.&#39;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 移除 scripts 前缀</span></span>
<span class="line">        <span class="token keyword">if</span> module_path<span class="token punctuation">.</span>startswith<span class="token punctuation">(</span><span class="token string">&#39;scripts.&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            module_path <span class="token operator">=</span> module_path<span class="token punctuation">[</span><span class="token number">8</span><span class="token punctuation">:</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> module_path</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">HotReloadManager</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;热更新管理器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> script_dir<span class="token operator">=</span><span class="token string">&quot;scripts&quot;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>script_dir <span class="token operator">=</span> script_dir</span>
<span class="line">        self<span class="token punctuation">.</span>observer <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line">        self<span class="token punctuation">.</span>handlers <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">start</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;启动热更新监控&quot;&quot;&quot;</span></span>
<span class="line">        event_handler <span class="token operator">=</span> ScriptReloader<span class="token punctuation">(</span>self<span class="token punctuation">.</span>script_dir<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>observer <span class="token operator">=</span> Observer<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>observer<span class="token punctuation">.</span>schedule<span class="token punctuation">(</span>event_handler<span class="token punctuation">,</span> self<span class="token punctuation">.</span>script_dir<span class="token punctuation">,</span> recursive<span class="token operator">=</span><span class="token boolean">True</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>observer<span class="token punctuation">.</span>start<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;[HotReload] Watching: </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>script_dir<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">stop</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;停止监控&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>observer<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>observer<span class="token punctuation">.</span>stop<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>observer<span class="token punctuation">.</span>join<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line"><span class="token keyword">if</span> __name__ <span class="token operator">==</span> <span class="token string">&quot;__main__&quot;</span><span class="token punctuation">:</span></span>
<span class="line">    reloader <span class="token operator">=</span> HotReloadManager<span class="token punctuation">(</span><span class="token string">&quot;scripts&quot;</span><span class="token punctuation">)</span></span>
<span class="line">    reloader<span class="token punctuation">.</span>start<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token boolean">True</span><span class="token punctuation">:</span></span>
<span class="line">            time<span class="token punctuation">.</span>sleep<span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">except</span> KeyboardInterrupt<span class="token punctuation">:</span></span>
<span class="line">        reloader<span class="token punctuation">.</span>stop<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-带状态保持的热更新" tabindex="-1"><a class="header-anchor" href="#_2-2-带状态保持的热更新"><span>2.2 带状态保持的热更新</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 状态保持的热更新</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> types</span>
<span class="line"><span class="token keyword">import</span> copy</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">StatefulReloader</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;带状态保持的重新加载器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>module_states <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">reload_with_state</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> module_name<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;重新加载模块并保持状态&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> module_name <span class="token keyword">not</span> <span class="token keyword">in</span> sys<span class="token punctuation">.</span>modules<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">        old_module <span class="token operator">=</span> sys<span class="token punctuation">.</span>modules<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 保存状态</span></span>
<span class="line">        state <span class="token operator">=</span> self<span class="token punctuation">.</span>save_state<span class="token punctuation">(</span>old_module<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>module_states<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span> <span class="token operator">=</span> state</span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 重新加载</span></span>
<span class="line">        new_module <span class="token operator">=</span> importlib<span class="token punctuation">.</span><span class="token builtin">reload</span><span class="token punctuation">(</span>old_module<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 恢复状态</span></span>
<span class="line">        self<span class="token punctuation">.</span>restore_state<span class="token punctuation">(</span>new_module<span class="token punctuation">,</span> state<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> new_module</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">save_state</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> module<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;保存模块状态&quot;&quot;&quot;</span></span>
<span class="line">        state <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> name <span class="token keyword">in</span> <span class="token builtin">dir</span><span class="token punctuation">(</span>module<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> name<span class="token punctuation">.</span>startswith<span class="token punctuation">(</span><span class="token string">&#39;_&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">continue</span></span>
<span class="line"></span>
<span class="line">            value <span class="token operator">=</span> <span class="token builtin">getattr</span><span class="token punctuation">(</span>module<span class="token punctuation">,</span> name<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 保存数据属性，跳过函数/类</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> <span class="token builtin">isinstance</span><span class="token punctuation">(</span>value<span class="token punctuation">,</span> <span class="token punctuation">(</span>types<span class="token punctuation">.</span>FunctionType<span class="token punctuation">,</span> types<span class="token punctuation">.</span>ModuleType<span class="token punctuation">,</span> <span class="token builtin">type</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                    <span class="token comment"># 尝试深拷贝</span></span>
<span class="line">                    state<span class="token punctuation">[</span>name<span class="token punctuation">]</span> <span class="token operator">=</span> copy<span class="token punctuation">.</span>deepcopy<span class="token punctuation">(</span>value<span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">except</span><span class="token punctuation">:</span></span>
<span class="line">                    <span class="token comment"># 无法拷贝的跳过</span></span>
<span class="line">                    <span class="token keyword">pass</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> state</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">restore_state</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> module<span class="token punctuation">,</span> state<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;恢复模块状态&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">for</span> name<span class="token punctuation">,</span> value <span class="token keyword">in</span> state<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>module<span class="token punctuation">,</span> name<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                    <span class="token builtin">setattr</span><span class="token punctuation">(</span>module<span class="token punctuation">,</span> name<span class="token punctuation">,</span> value<span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">except</span><span class="token punctuation">:</span></span>
<span class="line">                    <span class="token keyword">pass</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 脚本中定义保存/恢复函数</span></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line"># game_logic.py</span>
<span class="line"></span>
<span class="line">class GameState:</span>
<span class="line">    def __init__(self):</span>
<span class="line">        self.players = {}</span>
<span class="line">        self.counter = 0</span>
<span class="line"></span>
<span class="line"># 全局实例</span>
<span class="line">game_state = GameState()</span>
<span class="line"></span>
<span class="line">def on_load():</span>
<span class="line">    print(&quot;Module loaded&quot;)</span>
<span class="line"></span>
<span class="line">def on_unload():</span>
<span class="line">    print(&quot;Module unloading&quot;)</span>
<span class="line">    # 保存必要数据</span>
<span class="line">    return save_game_state()</span>
<span class="line"></span>
<span class="line">def save_game_state():</span>
<span class="line">    return {</span>
<span class="line">        &#39;players&#39;: game_state.players.copy(),</span>
<span class="line">        &#39;counter&#39;: game_state.counter</span>
<span class="line">    }</span>
<span class="line"></span>
<span class="line">def load_game_state(state):</span>
<span class="line">    game_state.players.update(state.get(&#39;players&#39;, {}))</span>
<span class="line">    game_state.counter = state.get(&#39;counter&#39;, 0)</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、lua-热更新" tabindex="-1"><a class="header-anchor" href="#三、lua-热更新"><span>三、Lua 热更新</span></a></h2><h3 id="_3-1-lua-模块重载" tabindex="-1"><a class="header-anchor" href="#_3-1-lua-模块重载"><span>3.1 Lua 模块重载</span></a></h3><div class="language-lua line-numbers-mode" data-highlighter="prismjs" data-ext="lua"><pre><code class="language-lua"><span class="line"><span class="token comment">-- Lua 热更新实现</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 模块缓存</span></span>
<span class="line"><span class="token keyword">local</span> module_cache <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"><span class="token keyword">local</span> module_states <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 保存模块状态</span></span>
<span class="line"><span class="token keyword">local</span> <span class="token keyword">function</span> <span class="token function">save_module_state</span><span class="token punctuation">(</span>module_name<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">local</span> module <span class="token operator">=</span> package<span class="token punctuation">.</span>loaded<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token keyword">not</span> module <span class="token keyword">then</span> <span class="token keyword">return</span> <span class="token keyword">nil</span> <span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">local</span> state <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">for</span> k<span class="token punctuation">,</span> v <span class="token keyword">in</span> <span class="token function">pairs</span><span class="token punctuation">(</span>module<span class="token punctuation">)</span> <span class="token keyword">do</span></span>
<span class="line">        <span class="token comment">-- 只保存数据，不保存函数</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token function">type</span><span class="token punctuation">(</span>v<span class="token punctuation">)</span> <span class="token operator">~=</span> <span class="token string">&quot;function&quot;</span> <span class="token keyword">and</span> <span class="token function">type</span><span class="token punctuation">(</span>k<span class="token punctuation">)</span> <span class="token operator">~=</span> <span class="token string">&quot;string&quot;</span> <span class="token keyword">or</span></span>
<span class="line">           <span class="token keyword">not</span> string<span class="token punctuation">.</span><span class="token function">match</span><span class="token punctuation">(</span>k<span class="token punctuation">,</span> <span class="token string">&quot;^_&quot;</span><span class="token punctuation">)</span> <span class="token keyword">then</span></span>
<span class="line">            state<span class="token punctuation">[</span>k<span class="token punctuation">]</span> <span class="token operator">=</span> v</span>
<span class="line">        <span class="token keyword">end</span></span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> state</span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 恢复模块状态</span></span>
<span class="line"><span class="token keyword">local</span> <span class="token keyword">function</span> <span class="token function">restore_module_state</span><span class="token punctuation">(</span>module_name<span class="token punctuation">,</span> state<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">local</span> module <span class="token operator">=</span> package<span class="token punctuation">.</span>loaded<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token keyword">not</span> module <span class="token keyword">then</span> <span class="token keyword">return</span> <span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> state <span class="token keyword">then</span></span>
<span class="line">        <span class="token keyword">for</span> k<span class="token punctuation">,</span> v <span class="token keyword">in</span> <span class="token function">pairs</span><span class="token punctuation">(</span>state<span class="token punctuation">)</span> <span class="token keyword">do</span></span>
<span class="line">            module<span class="token punctuation">[</span>k<span class="token punctuation">]</span> <span class="token operator">=</span> v</span>
<span class="line">        <span class="token keyword">end</span></span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 重新加载模块</span></span>
<span class="line"><span class="token keyword">function</span> <span class="token function">reload_module</span><span class="token punctuation">(</span>module_name<span class="token punctuation">)</span></span>
<span class="line">    <span class="token function">print</span><span class="token punctuation">(</span><span class="token string">&quot;Reloading module:&quot;</span><span class="token punctuation">,</span> module_name<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">-- 保存状态</span></span>
<span class="line">    <span class="token keyword">local</span> state <span class="token operator">=</span> <span class="token function">save_module_state</span><span class="token punctuation">(</span>module_name<span class="token punctuation">)</span></span>
<span class="line">    module_states<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span> <span class="token operator">=</span> state</span>
<span class="line"></span>
<span class="line">    <span class="token comment">-- 清除模块缓存</span></span>
<span class="line">    package<span class="token punctuation">.</span>loaded<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token keyword">nil</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">-- 重新加载</span></span>
<span class="line">    <span class="token keyword">local</span> ok<span class="token punctuation">,</span> new_module <span class="token operator">=</span> <span class="token function">pcall</span><span class="token punctuation">(</span>require<span class="token punctuation">,</span> module_name<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> <span class="token keyword">not</span> ok <span class="token keyword">then</span></span>
<span class="line">        <span class="token function">print</span><span class="token punctuation">(</span><span class="token string">&quot;Error reloading:&quot;</span><span class="token punctuation">,</span> new_module<span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment">-- 恢复旧模块</span></span>
<span class="line">        <span class="token keyword">if</span> state <span class="token keyword">then</span></span>
<span class="line">            package<span class="token punctuation">.</span>loaded<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span> <span class="token operator">=</span> state</span>
<span class="line">        <span class="token keyword">end</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token keyword">false</span><span class="token punctuation">,</span> new_module</span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">-- 恢复状态</span></span>
<span class="line">    <span class="token function">restore_module_state</span><span class="token punctuation">(</span>module_name<span class="token punctuation">,</span> state<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">-- 调用重载回调</span></span>
<span class="line">    <span class="token keyword">if</span> new_module<span class="token punctuation">.</span>on_reload <span class="token keyword">then</span></span>
<span class="line">        new_module<span class="token punctuation">.</span><span class="token function">on_reload</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line">    <span class="token function">print</span><span class="token punctuation">(</span><span class="token string">&quot;Module reloaded:&quot;</span><span class="token punctuation">,</span> module_name<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">return</span> <span class="token keyword">true</span><span class="token punctuation">,</span> new_module</span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 示例模块</span></span>
<span class="line"><span class="token comment">--[[</span>
<span class="line">-- player.lua</span>
<span class="line">local player = {}</span>
<span class="line"></span>
<span class="line">player.data = {</span>
<span class="line">    count = 0,</span>
<span class="line">    items = {}</span>
<span class="line">}</span>
<span class="line"></span>
<span class="line">function player.on_reload()</span>
<span class="line">    print(&quot;Player module reloaded!&quot;)</span>
<span class="line">    -- 保持 data 不变</span>
<span class="line">end</span>
<span class="line"></span>
<span class="line">return player</span>
<span class="line">--]]</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 使用</span></span>
<span class="line"><span class="token keyword">local</span> player <span class="token operator">=</span> <span class="token function">require</span><span class="token punctuation">(</span><span class="token string">&quot;player&quot;</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token comment">-- ... 修改文件 ...</span></span>
<span class="line"><span class="token function">reload_module</span><span class="token punctuation">(</span><span class="token string">&quot;player&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-脚本热更新" tabindex="-1"><a class="header-anchor" href="#四、kbengine-脚本热更新"><span>四、KBEngine 脚本热更新</span></a></h2><h3 id="_4-1-kbengine-热更机制" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-热更机制"><span>4.1 KBEngine 热更机制</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 脚本热更新</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 脚本热更新配置:</span>
<span class="line"></span>
<span class="line">在 kbengine_defaults.xml 中配置:</span>
<span class="line"></span>
<span class="line">&lt;root&gt;</span>
<span class="line">    &lt;reloadScripts&gt;</span>
<span class="line">        &lt;enable&gt;true&lt;/enable&gt;</span>
<span class="line">        &lt;scanInterval&gt;1.0&lt;/scanInterval&gt;</span>
<span class="line">        &lt;scriptsPath&gt;scripts/&lt;/scriptsPath&gt;</span>
<span class="line">    &lt;/reloadScripts&gt;</span>
<span class="line">&lt;/root&gt;</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"><span class="token keyword">import</span> sys</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">HotReloadHandler</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;KBEngine 热更新处理器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>entity_types <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">        self<span class="token punctuation">.</span>callbacks <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onInit</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> isReload<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;初始化回调&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> isReload<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>handle_reload<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;Scripts initializing...&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>register_entities<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">handle_reload</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;处理热更新&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;Scripts reloading...&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 通知所有实体</span></span>
<span class="line">        <span class="token keyword">for</span> entity_id<span class="token punctuation">,</span> entity <span class="token keyword">in</span> KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> <span class="token string">&#39;onScriptReload&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                    entity<span class="token punctuation">.</span>onScriptReload<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">                    <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Error in entity </span><span class="token interpolation"><span class="token punctuation">{</span>entity_id<span class="token punctuation">}</span></span><span class="token string"> onScriptReload: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 执行注册回调</span></span>
<span class="line">        <span class="token keyword">for</span> callback <span class="token keyword">in</span> self<span class="token punctuation">.</span>callbacks<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                callback<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Error in reload callback: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;Scripts reloaded successfully&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">register_callback</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> callback<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;注册重载回调&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>callbacks<span class="token punctuation">.</span>append<span class="token punctuation">(</span>callback<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">register_entities</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;注册实体类型&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 注册所有实体类</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>registerEntityType<span class="token punctuation">(</span><span class="token string">&quot;Account&quot;</span><span class="token punctuation">,</span> Account<span class="token punctuation">)</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>registerEntityType<span class="token punctuation">(</span><span class="token string">&quot;Avatar&quot;</span><span class="token punctuation">,</span> Avatar<span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># ...</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 实体中处理热更新</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Account</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Account<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Account<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>_state_version <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onScriptReload</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;脚本热更新时调用&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Account </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string">: Script reloaded&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 保存需要保持的状态</span></span>
<span class="line">        self<span class="token punctuation">.</span>_save_reload_state<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        self<span class="token punctuation">.</span>_state_version <span class="token operator">+=</span> <span class="token number">1</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">_save_reload_state</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;保存重载状态&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>_reload_state <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;playerName&#39;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>playerName<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;level&#39;</span><span class="token punctuation">:</span> <span class="token builtin">getattr</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> <span class="token string">&#39;level&#39;</span><span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;gold&#39;</span><span class="token punctuation">:</span> <span class="token builtin">getattr</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> <span class="token string">&#39;gold&#39;</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">_restore_reload_state</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;恢复重载状态&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> <span class="token string">&#39;_reload_state&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            state <span class="token operator">=</span> self<span class="token punctuation">.</span>_reload_state</span>
<span class="line">            self<span class="token punctuation">.</span>playerName <span class="token operator">=</span> state<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&#39;playerName&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;&#39;</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>level <span class="token operator">=</span> state<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&#39;level&#39;</span><span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>gold <span class="token operator">=</span> state<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&#39;gold&#39;</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token builtin">delattr</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> <span class="token string">&#39;_reload_state&#39;</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、热更新策略" tabindex="-1"><a class="header-anchor" href="#五、热更新策略"><span>五、热更新策略</span></a></h2><h3 id="_5-1-渐进式热更新" tabindex="-1"><a class="header-anchor" href="#_5-1-渐进式热更新"><span>5.1 渐进式热更新</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 渐进式热更新策略</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ProgressiveReloader</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;渐进式热更新器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>phases <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line">        self<span class="token punctuation">.</span>current_phase <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">add_phase</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> description<span class="token punctuation">,</span> action<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;添加热更新阶段&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>phases<span class="token punctuation">.</span>append<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;description&#39;</span><span class="token punctuation">:</span> description<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;action&#39;</span><span class="token punctuation">:</span> action</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">execute</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;执行渐进式热更新&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">for</span> i<span class="token punctuation">,</span> phase <span class="token keyword">in</span> <span class="token builtin">enumerate</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>phases<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;[Phase </span><span class="token interpolation"><span class="token punctuation">{</span>i<span class="token operator">+</span><span class="token number">1</span><span class="token punctuation">}</span></span><span class="token string">/</span><span class="token interpolation"><span class="token punctuation">{</span><span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>phases<span class="token punctuation">)</span><span class="token punctuation">}</span></span><span class="token string">] </span><span class="token interpolation"><span class="token punctuation">{</span>phase<span class="token punctuation">[</span><span class="token string">&#39;description&#39;</span><span class="token punctuation">]</span><span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                result <span class="token operator">=</span> phase<span class="token punctuation">[</span><span class="token string">&#39;action&#39;</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> result <span class="token keyword">is</span> <span class="token boolean">False</span><span class="token punctuation">:</span></span>
<span class="line">                    <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Phase </span><span class="token interpolation"><span class="token punctuation">{</span>i<span class="token operator">+</span><span class="token number">1</span><span class="token punctuation">}</span></span><span class="token string"> failed, aborting&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Phase </span><span class="token interpolation"><span class="token punctuation">{</span>i<span class="token operator">+</span><span class="token number">1</span><span class="token punctuation">}</span></span><span class="token string"> completed&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Phase </span><span class="token interpolation"><span class="token punctuation">{</span>i<span class="token operator">+</span><span class="token number">1</span><span class="token punctuation">}</span></span><span class="token string"> error: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string">&quot;All phases completed successfully&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">True</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 使用示例</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">hot_update_game_logic</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    reloader <span class="token operator">=</span> ProgressiveReloader<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># Phase 1: 保存关键状态</span></span>
<span class="line">    reloader<span class="token punctuation">.</span>add_phase<span class="token punctuation">(</span><span class="token string">&quot;Saving critical state&quot;</span><span class="token punctuation">,</span> <span class="token keyword">lambda</span><span class="token punctuation">:</span> save_critical_state<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># Phase 2: 重新加载核心模块</span></span>
<span class="line">    reloader<span class="token punctuation">.</span>add_phase<span class="token punctuation">(</span><span class="token string">&quot;Reloading core modules&quot;</span><span class="token punctuation">,</span> <span class="token keyword">lambda</span><span class="token punctuation">:</span> reload_core_modules<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># Phase 3: 更新配置</span></span>
<span class="line">    reloader<span class="token punctuation">.</span>add_phase<span class="token punctuation">(</span><span class="token string">&quot;Updating configuration&quot;</span><span class="token punctuation">,</span> <span class="token keyword">lambda</span><span class="token punctuation">:</span> update_configuration<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># Phase 4: 通知玩家</span></span>
<span class="line">    reloader<span class="token punctuation">.</span>add_phase<span class="token punctuation">(</span><span class="token string">&quot;Notifying players&quot;</span><span class="token punctuation">,</span> <span class="token keyword">lambda</span><span class="token punctuation">:</span> notify_players_reload<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># Phase 5: 验证状态</span></span>
<span class="line">    reloader<span class="token punctuation">.</span>add_phase<span class="token punctuation">(</span><span class="token string">&quot;Validating state&quot;</span><span class="token punctuation">,</span> <span class="token keyword">lambda</span><span class="token punctuation">:</span> validate_reload_state<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> reloader<span class="token punctuation">.</span>execute<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-热更新建议" tabindex="-1"><a class="header-anchor" href="#_6-1-热更新建议"><span>6.1 热更新建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>状态保持</strong></td><td>热更前保存关键状态</td></tr><tr><td><strong>版本兼容</strong></td><td>新版本兼容旧状态</td></tr><tr><td><strong>回滚机制</strong></td><td>热更失败可回滚</td></tr><tr><td><strong>灰度发布</strong></td><td>部分服务器先热更</td></tr><tr><td><strong>日志记录</strong></td><td>记录所有热更操作</td></tr><tr><td><strong>测试验证</strong></td><td>热更前充分测试</td></tr></tbody></table><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="脚本热更新核心" tabindex="-1"><a class="header-anchor" href="#脚本热更新核心"><span>脚本热更新核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">脚本热更新 = 状态保存 + 模块重载 + 状态恢复 + 验证</span>
<span class="line">- Python: importlib.reload</span>
<span class="line">- Lua: package.loaded 清除</span>
<span class="line">- KBEngine 内置支持</span>
<span class="line">- 保持兼容性是关键</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://docs.python.org/3/library/importlib.html" target="_blank" rel="noopener noreferrer">Python Module Reloading</a></li><li><a href="https://www.lua.org/manual/5.4/manual.html#pdf-require" target="_blank" rel="noopener noreferrer">Lua Package Loading</a></li><li><a href="https://kbengine.github.io/docs/en/configuration.html#reloadscripts" target="_blank" rel="noopener noreferrer">KBEngine Script Reload</a></li></ul>`,37)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};