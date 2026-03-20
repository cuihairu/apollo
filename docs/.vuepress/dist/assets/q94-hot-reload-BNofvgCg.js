import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q94-hot-reload.html","title":"Q94: 如何实现热更新？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q94-hot-reload.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q94-hot-reload.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q94-如何实现热更新" tabindex="-1"><a class="header-anchor" href="#q94-如何实现热更新"><span>Q94: 如何实现热更新？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对热更新的理解：</p><ul><li>热更新原理</li><li>动态加载</li><li>状态保持</li><li>KBEngine 脚本热更</li></ul><hr><h2 id="一、热更新原理" tabindex="-1"><a class="header-anchor" href="#一、热更新原理"><span>一、热更新原理</span></a></h2><h3 id="_1-1-热更新类型" tabindex="-1"><a class="header-anchor" href="#_1-1-热更新类型"><span>1.1 热更新类型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    热更新类型                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  配置热更新:                                                │</span>
<span class="line">│  ├── 配置文件变化监听                                      │</span>
<span class="line">│  ├── 无缝加载新配置                                        │</span>
<span class="line">│  ├── 不影响运行                                              │</span>
<span class="line">│  └── 示例: 日志级别调整                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  代码热更新:                                                │</span>
<span class="line">│  ├── Python 脚本重载                                          │</span>
<span class="line">│  ├── Lua 脚本热更                                            │</span>
<span class="line">│  ├── C++ 模块加载                                           │</span>
<span class="line">│  └── 示例: Bug 修复、功能增加                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  资源热更新:                                                │</span>
<span class="line">│  ├── 图片/模型                                              │</span>
<span class="line">│  ├── 配置数据                                              │</span>
<span class="line">│  ├── 客户端资源                                              │</span>
<span class="line">│  └── 示例: UI 素材更新                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、python-脚本热更新" tabindex="-1"><a class="header-anchor" href="#二、python-脚本热更新"><span>二、Python 脚本热更新</span></a></h2><h3 id="_2-1-kbengine-脚本热更新" tabindex="-1"><a class="header-anchor" href="#_2-1-kbengine-脚本热更新"><span>2.1 KBEngine 脚本热更新</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine Python 脚本热更新</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 脚本热更新机制:</span>
<span class="line"></span>
<span class="line">1. KBEngine 支持脚本的重新加载</span>
<span class="line">2. 需要配置 reloadScripts</span>
<span class="line">3. 热更新时状态会保持</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># KBEngine 配置</span></span>
<span class="line"><span class="token string">&quot;&quot;</span>&quot;</span>
<span class="line"><span class="token operator">&lt;</span>root<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>reloadScripts<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 是否启用脚本热更新 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>enable<span class="token operator">&gt;</span>true<span class="token operator">&lt;</span><span class="token operator">/</span>enable<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 扫描间隔 <span class="token punctuation">(</span>秒<span class="token punctuation">)</span> <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>scanInterval<span class="token operator">&gt;</span><span class="token number">1.0</span><span class="token operator">&lt;</span><span class="token operator">/</span>scanInterval<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 需要热更的脚本目录 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>scriptsPath<span class="token operator">&gt;</span>scripts<span class="token operator">/</span><span class="token operator">&lt;</span><span class="token operator">/</span>scriptsPath<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 排除的脚本 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>exclude<span class="token operator">&gt;</span></span>
<span class="line">            <span class="token operator">&lt;</span>script<span class="token operator">&gt;</span>scripts<span class="token operator">/</span>common<span class="token operator">/</span>__init__<span class="token punctuation">.</span>py<span class="token operator">&lt;</span><span class="token operator">/</span>script<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span><span class="token operator">/</span>exclude<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span><span class="token operator">/</span>reloadScripts<span class="token operator">&gt;</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>root<span class="token operator">&gt;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># scripts/__init__.py</span></span>
<span class="line"><span class="token comment"># KBEngine 脚本入口</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"><span class="token keyword">import</span> importlib</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 模块缓存</span></span>
<span class="line">_module_cache <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">onInit</span><span class="token punctuation">(</span>isReload<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    初始化函数</span>
<span class="line">    isReload: 是否是热更新调用</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">if</span> isReload<span class="token punctuation">:</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Scripts reloading...&quot;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Scripts initializing...&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 注册热更新回调</span></span>
<span class="line">    KBEngine<span class="token punctuation">.</span>setCallback<span class="token punctuation">(</span><span class="token string">&quot;onReload&quot;</span><span class="token punctuation">,</span> onReloadCallback<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">onReloadCallback</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;热更新回调&quot;&quot;&quot;</span></span>
<span class="line">    INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Script files changed, reloading...&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 清理旧的模块缓存</span></span>
<span class="line">    _cleanup_module_cache<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 重新加载模块</span></span>
<span class="line">    _reload_modules<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Scripts reloaded successfully&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">_cleanup_module_cache</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;清理模块缓存&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">global</span> _module_cache</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> name <span class="token keyword">in</span> <span class="token builtin">list</span><span class="token punctuation">(</span>_module_cache<span class="token punctuation">.</span>keys<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            module <span class="token operator">=</span> _module_cache<span class="token punctuation">[</span>name<span class="token punctuation">]</span></span>
<span class="line">            <span class="token comment"># 调用清理函数 (如果存在)</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>module<span class="token punctuation">,</span> <span class="token string">&#39;onUnload&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                module<span class="token punctuation">.</span>onUnload<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">del</span> sys<span class="token punctuation">.</span>modules<span class="token punctuation">[</span>name<span class="token punctuation">]</span></span>
<span class="line">            <span class="token keyword">del</span> _module_cache<span class="token punctuation">[</span>name<span class="token punctuation">]</span></span>
<span class="line">        <span class="token keyword">except</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">pass</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">_reload_modules</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;重新加载所有模块&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token comment"># 获取需要重新加载的模块列表</span></span>
<span class="line">    modules_to_reload <span class="token operator">=</span> _get_modified_modules<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> module_name <span class="token keyword">in</span> modules_to_reload<span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> module_name <span class="token keyword">in</span> sys<span class="token punctuation">.</span>modules<span class="token punctuation">:</span></span>
<span class="line">                <span class="token comment"># 重新加载模块</span></span>
<span class="line">                importlib<span class="token punctuation">.</span><span class="token builtin">reload</span><span class="token punctuation">(</span>sys<span class="token punctuation">.</span>modules<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Reloaded module: </span><span class="token interpolation"><span class="token punctuation">{</span>module_name<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">            ERROR_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Failed to reload </span><span class="token interpolation"><span class="token punctuation">{</span>module_name<span class="token punctuation">}</span></span><span class="token string">: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">_get_modified_modules</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;获取修改过的模块&quot;&quot;&quot;</span></span>
<span class="line">    modified <span class="token operator">=</span> <span class="token builtin">set</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 检查 scripts 目录下的所有 .py 文件</span></span>
<span class="line">    <span class="token keyword">import</span> os</span>
<span class="line">    scripts_path <span class="token operator">=</span> <span class="token string">&quot;scripts/&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> root<span class="token punctuation">,</span> dirs<span class="token punctuation">,</span> files <span class="token keyword">in</span> os<span class="token punctuation">.</span>walk<span class="token punctuation">(</span>scripts_path<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 排除 __pycache__</span></span>
<span class="line">        dirs<span class="token punctuation">[</span><span class="token punctuation">:</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">[</span>d <span class="token keyword">for</span> d <span class="token keyword">in</span> dirs <span class="token keyword">if</span> d <span class="token keyword">not</span> <span class="token keyword">in</span> <span class="token punctuation">[</span><span class="token string">&#39;__pycache__&#39;</span><span class="token punctuation">]</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token builtin">file</span> <span class="token keyword">in</span> files<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">file</span><span class="token punctuation">.</span>endswith<span class="token punctuation">(</span><span class="token string">&#39;.py&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                module_path <span class="token operator">=</span> os<span class="token punctuation">.</span>path<span class="token punctuation">.</span>join<span class="token punctuation">(</span>root<span class="token punctuation">,</span> <span class="token builtin">file</span><span class="token punctuation">)</span></span>
<span class="line">                module_name <span class="token operator">=</span> _path_to_module<span class="token punctuation">(</span>module_path<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment"># 检查文件修改时间</span></span>
<span class="line">                mtime <span class="token operator">=</span> os<span class="token punctuation">.</span>path<span class="token punctuation">.</span>getmtime<span class="token punctuation">(</span>module_path<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">if</span> module_name <span class="token keyword">in</span> _module_cache<span class="token punctuation">:</span></span>
<span class="line">                    cached_mtime <span class="token operator">=</span> _module_cache<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span><span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&#39;mtime&#39;</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                    <span class="token keyword">if</span> mtime <span class="token operator">&gt;</span> cached_mtime<span class="token punctuation">:</span></span>
<span class="line">                        modified<span class="token punctuation">.</span>add<span class="token punctuation">(</span>module_name<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment"># 缓存模块信息</span></span>
<span class="line">                _module_cache<span class="token punctuation">[</span>module_name<span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token string">&#39;path&#39;</span><span class="token punctuation">:</span> module_path<span class="token punctuation">,</span></span>
<span class="line">                    <span class="token string">&#39;mtime&#39;</span><span class="token punctuation">:</span> mtime</span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> modified</span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">_path_to_module</span><span class="token punctuation">(</span>file_path<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;将文件路径转换为模块名&quot;&quot;&quot;</span></span>
<span class="line">    path <span class="token operator">=</span> file_path<span class="token punctuation">.</span>replace<span class="token punctuation">(</span><span class="token string">&#39;/&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;.&#39;</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">if</span> path<span class="token punctuation">.</span>startswith<span class="token punctuation">(</span><span class="token string">&#39;scripts.&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        path <span class="token operator">=</span> path<span class="token punctuation">[</span><span class="token number">8</span><span class="token punctuation">:</span><span class="token punctuation">]</span>  <span class="token comment"># 移除 &#39;scripts.&#39;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> path<span class="token punctuation">.</span>endswith<span class="token punctuation">(</span><span class="token string">&#39;.py&#39;</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        path <span class="token operator">=</span> path<span class="token punctuation">[</span><span class="token punctuation">:</span><span class="token operator">-</span><span class="token number">3</span><span class="token punctuation">]</span>  <span class="token comment"># 移除 &#39;.py&#39;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> path</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、状态保持" tabindex="-1"><a class="header-anchor" href="#三、状态保持"><span>三、状态保持</span></a></h2><h3 id="_3-1-状态序列化" tabindex="-1"><a class="header-anchor" href="#_3-1-状态序列化"><span>3.1 状态序列化</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 状态序列化保持</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">StatefulEntity</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;有状态的实体&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 需要保持的状态</span></span>
<span class="line">        self<span class="token punctuation">.</span>state <span class="token operator">=</span> <span class="token string">&quot;idle&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>target <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line">        self<span class="token punctuation">.</span>path <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line">        self<span class="token punctuation">.</span>inventory_items <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 状态版本</span></span>
<span class="line">        self<span class="token punctuation">.</span>state_version <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onReload</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;热更新时调用&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 保存当前状态</span></span>
<span class="line">        old_state <span class="token operator">=</span> self<span class="token punctuation">.</span>serializeState<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 执行热更新后</span></span>
<span class="line">        new_state <span class="token operator">=</span> self<span class="token punctuation">.</span>loadState<span class="token punctuation">(</span>old_state<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> state reloaded: </span><span class="token interpolation"><span class="token punctuation">{</span>old_state<span class="token punctuation">}</span></span><span class="token string"> -&gt; </span><span class="token interpolation"><span class="token punctuation">{</span>new_state<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">serializeState</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;序列化状态&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;state&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>state<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;target&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>target<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;path&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>path<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;inventory&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>inventory_items<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;version&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>state_version</span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">loadState</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> state_data<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;加载状态&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>state <span class="token operator">=</span> state_data<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;state&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;idle&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>target <span class="token operator">=</span> state_data<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;target&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>path <span class="token operator">=</span> state_data<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;path&quot;</span><span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>inventory_items <span class="token operator">=</span> state_data<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;inventory&quot;</span><span class="token punctuation">,</span> <span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>state_version <span class="token operator">=</span> state_data<span class="token punctuation">.</span>get<span class="token punctuation">(</span><span class="token string">&quot;version&quot;</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token string-interpolation"><span class="token string">f&quot;</span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>state<span class="token punctuation">}</span></span><span class="token string">|</span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>target<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、c-模块热加载" tabindex="-1"><a class="header-anchor" href="#四、c-模块热加载"><span>四、C++ 模块热加载</span></a></h2><h3 id="_4-1-动态库加载" tabindex="-1"><a class="header-anchor" href="#_4-1-动态库加载"><span>4.1 动态库加载</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 动态库热加载</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">HotReloadManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 加载模块</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">loadModule</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> moduleName<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 加载动态库</span></span>
<span class="line">        HMODULE handle <span class="token operator">=</span> <span class="token function">LoadLibrary</span><span class="token punctuation">(</span><span class="token punctuation">(</span>moduleName <span class="token operator">+</span> <span class="token string">&quot;.dll&quot;</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">c_str</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>handle<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Failed to load module: {}&quot;</span><span class="token punctuation">,</span> moduleName<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 获取初始化函数</span></span>
<span class="line">        <span class="token keyword">using</span> InitFunc <span class="token operator">=</span> <span class="token keyword">void</span><span class="token punctuation">(</span><span class="token operator">*</span><span class="token punctuation">)</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        InitFunc init <span class="token operator">=</span> <span class="token punctuation">(</span>InitFunc<span class="token punctuation">)</span><span class="token function">GetProcAddress</span><span class="token punctuation">(</span>handle<span class="token punctuation">,</span> <span class="token string">&quot;initModule&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>init<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">FreeLibrary</span><span class="token punctuation">(</span>handle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Module missing initModule function: {}&quot;</span><span class="token punctuation">,</span> moduleName<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 初始化模块</span></span>
<span class="line">        <span class="token function">init</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        modules_<span class="token punctuation">[</span>moduleName<span class="token punctuation">]</span> <span class="token operator">=</span> handle<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Module {} loaded successfully&quot;</span><span class="token punctuation">,</span> moduleName<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 热重载模块</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">reloadModule</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> moduleName<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> modules_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>moduleName<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> modules_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Module not loaded: {}&quot;</span><span class="token punctuation">,</span> moduleName<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        HMODULE handle <span class="token operator">=</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 获取清理函数</span></span>
<span class="line">        <span class="token keyword">using</span> CleanupFunc <span class="token operator">=</span> <span class="token keyword">void</span><span class="token punctuation">(</span><span class="token operator">*</span><span class="token punctuation">)</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        CleanupFunc cleanup <span class="token operator">=</span> <span class="token punctuation">(</span>CleanupFunc<span class="token punctuation">)</span><span class="token function">GetProcAddress</span><span class="token punctuation">(</span>handle<span class="token punctuation">,</span> <span class="token string">&quot;cleanupModule&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>cleanup<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">cleanup</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">FreeLibrary</span><span class="token punctuation">(</span>handle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 重新加载</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">loadModule</span><span class="token punctuation">(</span>moduleName<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> HMODULE<span class="token operator">&gt;</span> modules_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、最佳实践" tabindex="-1"><a class="header-anchor" href="#五、最佳实践"><span>五、最佳实践</span></a></h2><h3 id="_5-1-热更新建议" tabindex="-1"><a class="header-anchor" href="#_5-1-热更新建议"><span>5.1 热更新建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>状态保持</strong></td><td>热更后保持状态</td></tr><tr><td><strong>版本兼容</strong></td><td>新版本兼容旧状态</td></tr><tr><td><strong>灰度发布</strong></td><td>先部分服务器热更</td></tr><tr><td><strong>回滚机制</strong></td><td>热更失败可回滚</td></tr><tr><td><strong>日志记录</strong></td><td>记录热更操作</td></tr></tbody></table><hr><h2 id="六、总结" tabindex="-1"><a class="header-anchor" href="#六、总结"><span>六、总结</span></a></h2><h3 id="热更新核心" tabindex="-1"><a class="header-anchor" href="#热更新核心"><span>热更新核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">热更新 = 状态序列化 + 动态加载 + 版本管理 + 灰度发布</span>
<span class="line">- KBEngine 支持脚本热更</span>
<span class="line">- 保持兼容状态</span>
<span class="line">- 失败可回滚</span>
<span class="line">- 充分测试</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://docs.python.org/3/library/importlib.html" target="_blank" rel="noopener noreferrer">Python Module Reloading</a></li><li><a href="https://www.kernel.org/doc/Documentation/process/coding-style.rst" target="_blank" rel="noopener noreferrer">Hot Reload Best Practices</a></li></ul>`,32)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};