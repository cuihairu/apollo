import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q101-script-language.html","title":"Q101: 为什么要嵌入脚本语言？Lua vs Python 如何选择？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q101-script-language.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q101-script-language.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q101-为什么要嵌入脚本语言-lua-vs-python-如何选择" tabindex="-1"><a class="header-anchor" href="#q101-为什么要嵌入脚本语言-lua-vs-python-如何选择"><span>Q101: 为什么要嵌入脚本语言？Lua vs Python 如何选择？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对脚本语言的理解：</p><ul><li>脚本语言优势</li><li>Lua vs Python 对比</li><li>KBEngine Python 集成</li><li>游戏服务器应用</li></ul><hr><h2 id="一、脚本语言优势" tabindex="-1"><a class="header-anchor" href="#一、脚本语言优势"><span>一、脚本语言优势</span></a></h2><h3 id="_1-1-为什么使用脚本" tabindex="-1"><a class="header-anchor" href="#_1-1-为什么使用脚本"><span>1.1 为什么使用脚本</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    脚本语言在游戏中的优势                       │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  开发效率:                                                   │</span>
<span class="line">│  ├── 无需编译，快速迭代                                       │</span>
<span class="line">│  ├── 动态类型，代码简洁                                       │</span>
<span class="line">│  ├── 丰富的标准库                                            │</span>
<span class="line">│  └── 示例: 策划可修改游戏参数                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  热更新:                                                     │</span>
<span class="line">│  ├── 运行时加载新代码                                        │</span>
<span class="line">│  ├── 无需停服更新                                            │</span>
<span class="line">│  ├── 快速修复 Bug                                           │</span>
<span class="line">│  └── 示例: 紧急 Bug 修复                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  安全性:                                                     │</span>
<span class="line">│  ├── 沙箱执行，隔离风险                                       │</span>
<span class="line">│  ├── 限制访问系统资源                                        │</span>
<span class="line">│  ├── 异常捕获                                               │</span>
<span class="line">│  └── 示例: 玩家自定义脚本                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  分工协作:                                                   │</span>
<span class="line">│  ├── 程序写 C++ 核心                                         │</span>
<span class="line">│  ├── 策划写游戏逻辑                                          │</span>
<span class="line">│  ├── 降低耦合                                               │</span>
<span class="line">│  └── 示例: 任务系统、技能系统                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、lua-vs-python-对比" tabindex="-1"><a class="header-anchor" href="#二、lua-vs-python-对比"><span>二、Lua vs Python 对比</span></a></h2><h3 id="_2-1-特性对比" tabindex="-1"><a class="header-anchor" href="#_2-1-特性对比"><span>2.1 特性对比</span></a></h3><table><thead><tr><th>特性</th><th>Lua</th><th>Python</th></tr></thead><tbody><tr><td><strong>性能</strong></td><td>极快，轻量级</td><td>较快，但比 Lua 慢</td></tr><tr><td><strong>内存占用</strong></td><td>非常小 (几百 KB)</td><td>较大 (几 MB)</td></tr><tr><td><strong>学习曲线</strong></td><td>简单，语法精简</td><td>简单，语法自然</td></tr><tr><td><strong>标准库</strong></td><td>最小化</td><td>非常丰富</td></tr><tr><td><strong>第三方库</strong></td><td>较少</td><td>极其丰富</td></tr><tr><td><strong>集成难度</strong></td><td>容易，C API 设计优秀</td><td>中等，需要 CPython</td></tr><tr><td><strong>多线程</strong></td><td>协程 (原生)</td><td>线程 + 协程</td></tr><tr><td><strong>面向对象</strong></td><td>基于 Table (模拟)</td><td>原生支持</td></tr><tr><td><strong>热门游戏</strong></td><td>魔兽世界、Roblox</td><td>EVE Online、KBEngine</td></tr></tbody></table><h3 id="_2-2-性能对比" tabindex="-1"><a class="header-anchor" href="#_2-2-性能对比"><span>2.2 性能对比</span></a></h3><div class="language-lua line-numbers-mode" data-highlighter="prismjs" data-ext="lua"><pre><code class="language-lua"><span class="line"><span class="token comment">-- Lua 斐波那契示例</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">function</span> <span class="token function">fibonacci</span><span class="token punctuation">(</span>n<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">if</span> n <span class="token operator">&lt;=</span> <span class="token number">1</span> <span class="token keyword">then</span></span>
<span class="line">        <span class="token keyword">return</span> n</span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line">    <span class="token keyword">return</span> <span class="token function">fibonacci</span><span class="token punctuation">(</span>n <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">+</span> <span class="token function">fibonacci</span><span class="token punctuation">(</span>n <span class="token operator">-</span> <span class="token number">2</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- LuaJIT 可以达到接近 C 的性能</span></span>
<span class="line"><span class="token comment">-- 标准Lua 比 Python 快约 2-3 倍</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Python 斐波那契示例</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">fibonacci</span><span class="token punctuation">(</span>n<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">if</span> n <span class="token operator">&lt;=</span> <span class="token number">1</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">return</span> n</span>
<span class="line">    <span class="token keyword">return</span> fibonacci<span class="token punctuation">(</span>n <span class="token operator">-</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">+</span> fibonacci<span class="token punctuation">(</span>n <span class="token operator">-</span> <span class="token number">2</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># CPython 性能约为 Lua 的 1/2 - 1/3</span></span>
<span class="line"><span class="token comment"># 但可以使用 PyPy 提升 3-5 倍</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、lua-在游戏中" tabindex="-1"><a class="header-anchor" href="#三、lua-在游戏中"><span>三、Lua 在游戏中</span></a></h2><h3 id="_3-1-lua-特性" tabindex="-1"><a class="header-anchor" href="#_3-1-lua-特性"><span>3.1 Lua 特性</span></a></h3><div class="language-lua line-numbers-mode" data-highlighter="prismjs" data-ext="lua"><pre><code class="language-lua"><span class="line"><span class="token comment">-- Lua 基础语法</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 表 (Table) 是核心数据结构</span></span>
<span class="line">player <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">    name <span class="token operator">=</span> <span class="token string">&quot;Player1&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    level <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">,</span></span>
<span class="line">    hp <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">-- 方法</span></span>
<span class="line">    attack <span class="token operator">=</span> <span class="token keyword">function</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> target<span class="token punctuation">)</span></span>
<span class="line">        <span class="token function">print</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>name <span class="token operator">..</span> <span class="token string">&quot; attacks &quot;</span> <span class="token operator">..</span> target<span class="token punctuation">.</span>name<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 元表 (Metatable) 实现 OOP</span></span>
<span class="line">Player <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">Player<span class="token punctuation">.</span>__index <span class="token operator">=</span> Player</span>
<span class="line"></span>
<span class="line"><span class="token keyword">function</span> Player<span class="token punctuation">.</span><span class="token function">new</span><span class="token punctuation">(</span>name<span class="token punctuation">,</span> level<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">local</span> self <span class="token operator">=</span> <span class="token function">setmetatable</span><span class="token punctuation">(</span><span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">,</span> Player<span class="token punctuation">)</span></span>
<span class="line">    self<span class="token punctuation">.</span>name <span class="token operator">=</span> name</span>
<span class="line">    self<span class="token punctuation">.</span>level <span class="token operator">=</span> level</span>
<span class="line">    self<span class="token punctuation">.</span>hp <span class="token operator">=</span> <span class="token number">100</span></span>
<span class="line">    <span class="token keyword">return</span> self</span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">function</span> Player<span class="token punctuation">:</span><span class="token function">attack</span><span class="token punctuation">(</span>target<span class="token punctuation">)</span></span>
<span class="line">    damage <span class="token operator">=</span> self<span class="token punctuation">.</span>level <span class="token operator">*</span> <span class="token number">10</span></span>
<span class="line">    target<span class="token punctuation">.</span>hp <span class="token operator">=</span> target<span class="token punctuation">.</span>hp <span class="token operator">-</span> damage</span>
<span class="line">    <span class="token function">print</span><span class="token punctuation">(</span>string<span class="token punctuation">.</span><span class="token function">format</span><span class="token punctuation">(</span><span class="token string">&quot;%s attacks %s for %d damage&quot;</span><span class="token punctuation">,</span></span>
<span class="line">        self<span class="token punctuation">.</span>name<span class="token punctuation">,</span> target<span class="token punctuation">.</span>name<span class="token punctuation">,</span> damage<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token keyword">end</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 使用</span></span>
<span class="line"><span class="token keyword">local</span> p1 <span class="token operator">=</span> Player<span class="token punctuation">.</span><span class="token function">new</span><span class="token punctuation">(</span><span class="token string">&quot;Hero&quot;</span><span class="token punctuation">,</span> <span class="token number">10</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token keyword">local</span> p2 <span class="token operator">=</span> Player<span class="token punctuation">.</span><span class="token function">new</span><span class="token punctuation">(</span><span class="token string">&quot;Monster&quot;</span><span class="token punctuation">,</span> <span class="token number">5</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">p1<span class="token punctuation">:</span><span class="token function">attack</span><span class="token punctuation">(</span>p2<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">-- 协程 (原生)</span></span>
<span class="line">co <span class="token operator">=</span> coroutine<span class="token punctuation">.</span><span class="token function">create</span><span class="token punctuation">(</span><span class="token keyword">function</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">for</span> i <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span> <span class="token number">3</span> <span class="token keyword">do</span></span>
<span class="line">        <span class="token function">print</span><span class="token punctuation">(</span><span class="token string">&quot;Step &quot;</span> <span class="token operator">..</span> i<span class="token punctuation">)</span></span>
<span class="line">        coroutine<span class="token punctuation">.</span><span class="token function">yield</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">end</span></span>
<span class="line"><span class="token keyword">end</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">coroutine<span class="token punctuation">.</span><span class="token function">resume</span><span class="token punctuation">(</span>co<span class="token punctuation">)</span>  <span class="token comment">-- Step 1</span></span>
<span class="line">coroutine<span class="token punctuation">.</span><span class="token function">resume</span><span class="token punctuation">(</span>co<span class="token punctuation">)</span>  <span class="token comment">-- Step 2</span></span>
<span class="line">coroutine<span class="token punctuation">.</span><span class="token function">resume</span><span class="token punctuation">(</span>co<span class="token punctuation">)</span>  <span class="token comment">-- Step 3</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-lua-c-绑定" tabindex="-1"><a class="header-anchor" href="#_3-2-lua-c-绑定"><span>3.2 Lua C 绑定</span></a></h3><div class="language-c line-numbers-mode" data-highlighter="prismjs" data-ext="c"><pre><code class="language-c"><span class="line"><span class="token comment">// Lua C API 示例</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;lua.h&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;lauxlib.h&gt;</span></span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;lualib.h&gt;</span></span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// C 函数供 Lua 调用</span></span>
<span class="line"><span class="token keyword">static</span> <span class="token keyword">int</span> <span class="token function">lua_get_player_level</span><span class="token punctuation">(</span>lua_State <span class="token operator">*</span>L<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 获取参数 (玩家 ID)</span></span>
<span class="line">    <span class="token keyword">int</span> player_id <span class="token operator">=</span> <span class="token function">luaL_checkinteger</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取玩家等级 (从 C++ 数据结构)</span></span>
<span class="line">    <span class="token keyword">int</span> level <span class="token operator">=</span> <span class="token function">get_player_level_cpp</span><span class="token punctuation">(</span>player_id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 返回结果</span></span>
<span class="line">    <span class="token function">lua_pushinteger</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> level<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">return</span> <span class="token number">1</span><span class="token punctuation">;</span>  <span class="token comment">// 返回值个数</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 注册函数到 Lua</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">register_lua_functions</span><span class="token punctuation">(</span>lua_State <span class="token operator">*</span>L<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token function">lua_register</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token string">&quot;getPlayerLevel&quot;</span><span class="token punctuation">,</span> lua_get_player_level<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 从 C 调用 Lua</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">call_lua_function</span><span class="token punctuation">(</span>lua_State <span class="token operator">*</span>L<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 加载 Lua 文件</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">luaL_dofile</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token string">&quot;script/game_logic.lua&quot;</span><span class="token punctuation">)</span> <span class="token operator">!=</span> LUA_OK<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">printf</span><span class="token punctuation">(</span><span class="token string">&quot;Error: %s\\n&quot;</span><span class="token punctuation">,</span> <span class="token function">lua_tostring</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 调用 Lua 函数</span></span>
<span class="line">    <span class="token function">lua_getglobal</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token string">&quot;onPlayerLogin&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token function">lua_pushinteger</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token number">12345</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 玩家 ID</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 执行调用 (1 个参数，0 个返回值)</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">lua_pcall</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token operator">!=</span> LUA_OK<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">printf</span><span class="token punctuation">(</span><span class="token string">&quot;Error: %s\\n&quot;</span><span class="token punctuation">,</span> <span class="token function">lua_tostring</span><span class="token punctuation">(</span>L<span class="token punctuation">,</span> <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、python-在游戏中" tabindex="-1"><a class="header-anchor" href="#四、python-在游戏中"><span>四、Python 在游戏中</span></a></h2><h3 id="_4-1-kbengine-python-集成" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-python-集成"><span>4.1 KBEngine Python 集成</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 使用 Python 作为脚本语言</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine Python 架构:</span>
<span class="line"></span>
<span class="line">1. C++ 引擎核心</span>
<span class="line">2. Python 脚本层</span>
<span class="line">3. 实体定义使用 Python</span>
<span class="line">4. 回调函数在 Python 中实现</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Account</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Account<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;账号类 - 继承自 KBEngine.Account&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Account<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 自定义属性</span></span>
<span class="line">        self<span class="token punctuation">.</span>playerName <span class="token operator">=</span> <span class="token string">&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>level <span class="token operator">=</span> <span class="token number">1</span></span>
<span class="line">        self<span class="token punctuation">.</span>gold <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onLogin</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entityType<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;登录回调 (C++ 调用 Python)&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Account </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> login as </span><span class="token interpolation"><span class="token punctuation">{</span>entityType<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 创建角色</span></span>
<span class="line">        <span class="token keyword">if</span> entityType <span class="token operator">==</span> <span class="token string">&quot;Avatar&quot;</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>createAvatar<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onLogout</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;登出回调&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Account </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> logout&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">createAvatar</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;创建角色&quot;&quot;&quot;</span></span>
<span class="line">        avatar <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>createEntityLocally<span class="token punctuation">(</span></span>
<span class="line">            <span class="token string">&quot;Avatar&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token string">&quot;position&quot;</span><span class="token punctuation">:</span> <span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">)</span><span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Avatar</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;角色类&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 属性</span></span>
<span class="line">        self<span class="token punctuation">.</span>hp <span class="token operator">=</span> <span class="token number">100</span></span>
<span class="line">        self<span class="token punctuation">.</span>maxHp <span class="token operator">=</span> <span class="token number">100</span></span>
<span class="line">        self<span class="token punctuation">.</span>mp <span class="token operator">=</span> <span class="token number">50</span></span>
<span class="line">        self<span class="token punctuation">.</span>maxMp <span class="token operator">=</span> <span class="token number">50</span></span>
<span class="line">        self<span class="token punctuation">.</span>level <span class="token operator">=</span> <span class="token number">1</span></span>
<span class="line">        self<span class="token punctuation">.</span>exp <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">receiveDamage</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> attackerID<span class="token punctuation">,</span> damage<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;受到伤害 (C++ 调用)&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>hp <span class="token operator">-=</span> damage</span>
<span class="line"></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Avatar </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> takes </span><span class="token interpolation"><span class="token punctuation">{</span>damage<span class="token punctuation">}</span></span><span class="token string"> damage, HP: </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>hp<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>hp <span class="token operator">&lt;=</span> <span class="token number">0</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>onDeath<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onDeath</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;死亡处理&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Avatar </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> died&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 复活逻辑</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>addTimer<span class="token punctuation">(</span><span class="token number">5.0</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>resurrect<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">resurrect</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;复活&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>hp <span class="token operator">=</span> self<span class="token punctuation">.</span>maxHp</span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Avatar </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> resurrected&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-python-c-扩展" tabindex="-1"><a class="header-anchor" href="#_4-2-python-c-扩展"><span>4.2 Python C 扩展</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Python C 扩展示例</span></span>
<span class="line"></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">include</span> <span class="token string">&lt;Python.h&gt;</span></span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// C 函数供 Python 调用</span></span>
<span class="line"><span class="token keyword">static</span> PyObject<span class="token operator">*</span> <span class="token function">py_get_player_level</span><span class="token punctuation">(</span>PyObject <span class="token operator">*</span>self<span class="token punctuation">,</span> PyObject <span class="token operator">*</span>args<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">int</span> player_id<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 解析参数</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">PyArg_ParseTuple</span><span class="token punctuation">(</span>args<span class="token punctuation">,</span> <span class="token string">&quot;i&quot;</span><span class="token punctuation">,</span> <span class="token operator">&amp;</span>player_id<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token constant">NULL</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取玩家等级 (从 C++ 数据结构)</span></span>
<span class="line">    <span class="token keyword">int</span> level <span class="token operator">=</span> <span class="token function">get_player_level_cpp</span><span class="token punctuation">(</span>player_id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 返回结果</span></span>
<span class="line">    <span class="token keyword">return</span> <span class="token function">PyLong_FromLong</span><span class="token punctuation">(</span>level<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 方法定义</span></span>
<span class="line"><span class="token keyword">static</span> PyMethodDef game_methods<span class="token punctuation">[</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token punctuation">{</span><span class="token string">&quot;getPlayerLevel&quot;</span><span class="token punctuation">,</span> py_get_player_level<span class="token punctuation">,</span> METH_VARARGS<span class="token punctuation">,</span></span>
<span class="line">     <span class="token string">&quot;Get player level by ID&quot;</span><span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">{</span><span class="token constant">NULL</span><span class="token punctuation">,</span> <span class="token constant">NULL</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> <span class="token constant">NULL</span><span class="token punctuation">}</span>  <span class="token comment">// 结束标记</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 模块定义</span></span>
<span class="line"><span class="token keyword">static</span> <span class="token keyword">struct</span> <span class="token class-name">PyModuleDef</span> game_module <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">    PyModuleDef_HEAD_INIT<span class="token punctuation">,</span></span>
<span class="line">    <span class="token string">&quot;game&quot;</span><span class="token punctuation">,</span>        <span class="token comment">// 模块名</span></span>
<span class="line">    <span class="token string">&quot;Game module&quot;</span><span class="token punctuation">,</span> <span class="token comment">// 模块文档</span></span>
<span class="line">    <span class="token operator">-</span><span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">    game_methods</span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 模块初始化</span></span>
<span class="line">PyMODINIT_FUNC <span class="token function">PyInit_game</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">return</span> <span class="token function">PyModule_Create</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>game_module<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、选择建议" tabindex="-1"><a class="header-anchor" href="#五、选择建议"><span>五、选择建议</span></a></h2><h3 id="_5-1-选择-lua-的情况" tabindex="-1"><a class="header-anchor" href="#_5-1-选择-lua-的情况"><span>5.1 选择 Lua 的情况</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">适用场景:</span>
<span class="line">├── 嵌入式环境 (内存受限)</span>
<span class="line">├── 性能要求极高</span>
<span class="line">├── 客户端 UI 脚本</span>
<span class="line">├── 需要轻量级解决方案</span>
<span class="line">└── 示例: 手游、独立游戏</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>推荐理由:</strong></p><ul><li>LuaJIT 性能接近 C</li><li>内存占用极小</li><li>容易集成到任何 C/C++ 项目</li><li>丰富的游戏框架 (Love2D, Corona SDK)</li></ul><h3 id="_5-2-选择-python-的情况" tabindex="-1"><a class="header-anchor" href="#_5-2-选择-python-的情况"><span>5.2 选择 Python 的情况</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">适用场景:</span>
<span class="line">├── 复杂游戏逻辑</span>
<span class="line">├── 需要丰富的库支持</span>
<span class="line">├── 服务端开发</span>
<span class="line">├── 快速开发迭代</span>
<span class="line">└── 示例: KBEngine, MMO 服务器</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>推荐理由:</strong></p><ul><li>标准库极其丰富</li><li>面向对象原生支持</li><li>易于学习和维护</li><li>大量现成框架</li></ul><hr><h2 id="六、kbengine-脚本架构" tabindex="-1"><a class="header-anchor" href="#六、kbengine-脚本架构"><span>六、KBEngine 脚本架构</span></a></h2><h3 id="_6-1-实体定义" tabindex="-1"><a class="header-anchor" href="#_6-1-实体定义"><span>6.1 实体定义</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 实体定义</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 实体文件结构:</span>
<span class="line"></span>
<span class="line">scripts/</span>
<span class="line">├── __init__.py           # 入口</span>
<span class="line">├── account.py            # Account 实体</span>
<span class="line">├── avatar.py             # Avatar 实体</span>
<span class="line">├── monster.py            # Monster 实体</span>
<span class="line">├── npc.py                # NPC 实体</span>
<span class="line">└── data/</span>
<span class="line">    ├── skills.xml        # 技能配置</span>
<span class="line">    └── items.xml         # 物品配置</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># data/entities/Avatar.def</span></span>
<span class="line"><span class="token comment"># 实体定义文件 (KBEngine 特有)</span></span>
<span class="line"></span>
<span class="line"><span class="token operator">&lt;</span>root<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>Avatar<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 继承 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>Base<span class="token operator">&gt;</span>Entity<span class="token operator">&lt;</span><span class="token operator">/</span>Base<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 属性 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>Properties<span class="token operator">&gt;</span></span>
<span class="line">            <span class="token operator">&lt;</span>hp<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Type<span class="token operator">&gt;</span>UINT32<span class="token operator">&lt;</span><span class="token operator">/</span>Type<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Flags<span class="token operator">&gt;</span>CELL_PUBLIC<span class="token operator">&lt;</span><span class="token operator">/</span>Flags<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Default<span class="token operator">&gt;</span><span class="token number">100</span><span class="token operator">&lt;</span><span class="token operator">/</span>Default<span class="token operator">&gt;</span></span>
<span class="line">            <span class="token operator">&lt;</span><span class="token operator">/</span>hp<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">            <span class="token operator">&lt;</span>level<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Type<span class="token operator">&gt;</span>UINT8<span class="token operator">&lt;</span><span class="token operator">/</span>Type<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Flags<span class="token operator">&gt;</span>BASE_AND_CLIENT<span class="token operator">&lt;</span><span class="token operator">/</span>Flags<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Default<span class="token operator">&gt;</span><span class="token number">1</span><span class="token operator">&lt;</span><span class="token operator">/</span>Default<span class="token operator">&gt;</span></span>
<span class="line">            <span class="token operator">&lt;</span><span class="token operator">/</span>level<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">            <span class="token operator">&lt;</span>position<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Type<span class="token operator">&gt;</span>VECTOR3<span class="token operator">&lt;</span><span class="token operator">/</span>Type<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Flags<span class="token operator">&gt;</span>CELL_PUBLIC<span class="token operator">&lt;</span><span class="token operator">/</span>Flags<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Default<span class="token operator">&gt;</span><span class="token number">0</span><span class="token punctuation">,</span><span class="token number">0</span><span class="token punctuation">,</span><span class="token number">0</span><span class="token operator">&lt;</span><span class="token operator">/</span>Default<span class="token operator">&gt;</span></span>
<span class="line">            <span class="token operator">&lt;</span><span class="token operator">/</span>position<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span><span class="token operator">/</span>Properties<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 方法 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>Methods<span class="token operator">&gt;</span></span>
<span class="line">            <span class="token operator">&lt;</span>receiveDamage<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Arg<span class="token operator">&gt;</span>UINT32<span class="token operator">&lt;</span><span class="token operator">/</span>Arg<span class="token operator">&gt;</span>  <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> attackerID <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Arg<span class="token operator">&gt;</span>UINT16<span class="token operator">&lt;</span><span class="token operator">/</span>Arg<span class="token operator">&gt;</span>  <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> damage <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">            <span class="token operator">&lt;</span><span class="token operator">/</span>receiveDamage<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span><span class="token operator">/</span>Methods<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">        <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 客户端方法 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>ClientMethods<span class="token operator">&gt;</span></span>
<span class="line">            <span class="token operator">&lt;</span>onHpChanged<span class="token operator">&gt;</span></span>
<span class="line">                <span class="token operator">&lt;</span>Arg<span class="token operator">&gt;</span>UINT32<span class="token operator">&lt;</span><span class="token operator">/</span>Arg<span class="token operator">&gt;</span>  <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> newHp <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">            <span class="token operator">&lt;</span><span class="token operator">/</span>onHpChanged<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span><span class="token operator">/</span>ClientMethods<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span><span class="token operator">/</span>Avatar<span class="token operator">&gt;</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>root<span class="token operator">&gt;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、最佳实践" tabindex="-1"><a class="header-anchor" href="#七、最佳实践"><span>七、最佳实践</span></a></h2><h3 id="_7-1-脚本语言选择建议" tabindex="-1"><a class="header-anchor" href="#_7-1-脚本语言选择建议"><span>7.1 脚本语言选择建议</span></a></h3><table><thead><tr><th>场景</th><th>推荐</th><th>理由</th></tr></thead><tbody><tr><td>客户端 UI</td><td>Lua</td><td>轻量、快速</td></tr><tr><td>服务端逻辑</td><td>Python</td><td>库丰富、易维护</td></tr><tr><td>性能关键</td><td>Lua/C++</td><td>LuaJIT 性能好</td></tr><tr><td>快速原型</td><td>Python</td><td>开发效率高</td></tr><tr><td>嵌入系统</td><td>Lua</td><td>集成简单</td></tr></tbody></table><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="脚本语言选择核心" tabindex="-1"><a class="header-anchor" href="#脚本语言选择核心"><span>脚本语言选择核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">脚本选择 = 性能需求 + 生态 + 团队技能 + 项目规模</span>
<span class="line">- Lua: 轻量高性能，适合嵌入</span>
<span class="line">- Python: 生态丰富，适合服务端</span>
<span class="line">- KBEngine 选择 Python 是为了开发效率</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.lua.org/manual/5.4/" target="_blank" rel="noopener noreferrer">Lua Reference Manual</a></li><li><a href="https://docs.python.org/3/c-api/index.html" target="_blank" rel="noopener noreferrer">Python C API</a></li><li><a href="https://kbengine.github.io/docs/en/programming-guide/script-entity.html" target="_blank" rel="noopener noreferrer">KBEngine Python Programming</a></li><li><a href="https://www.gamedeveloper.com/business/lua-vs.-python-in-game-development" target="_blank" rel="noopener noreferrer">Lua vs Python in Game Development</a></li></ul>`,52)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};