import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q78-race-condition.html","title":"Q78: 如何处理竞态条件？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q78-race-condition.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q78-race-condition.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q78-如何处理竞态条件" tabindex="-1"><a class="header-anchor" href="#q78-如何处理竞态条件"><span>Q78: 如何处理竞态条件？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对竞态条件的理解：</p><ul><li>竞态条件产生原因</li><li>检测方法</li><li>防护策略</li><li>线程安全设计</li></ul><hr><h2 id="一、竞态条件基础" tabindex="-1"><a class="header-anchor" href="#一、竞态条件基础"><span>一、竞态条件基础</span></a></h2><h3 id="_1-1-定义" tabindex="-1"><a class="header-anchor" href="#_1-1-定义"><span>1.1 定义</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    竞态条件定义                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  竞态条件 (Race Condition):                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  系统的输出依赖于事件发生的相对时序，                        │</span>
<span class="line">│  当多个线程/进程同时访问共享资源时，                         │</span>
<span class="line">│  执行顺序的不确定性导致错误结果。                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  示例:                                                      │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  int counter = 0;                                │       │</span>
<span class="line">│  │                                                  │       │</span>
<span class="line">│  │  // Thread 1:                                     │       │</span>
<span class="line">│  │  counter = counter + 1;  // 读取、加1、写回        │       │</span>
<span class="line">│  │                                                  │       │</span>
<span class="line">│  │  // Thread 2:                                     │       │</span>
<span class="line">│  │  counter = counter + 1;  // 可能被中断！           │       │</span>
<span class="line">│  │                                                  │       │</span>
<span class="line">│  │  结果: counter 可能是 1 而不是 2                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-常见类型" tabindex="-1"><a class="header-anchor" href="#_1-2-常见类型"><span>1.2 常见类型</span></a></h3><table><thead><tr><th>类型</th><th>描述</th><th>示例</th></tr></thead><tbody><tr><td><strong>数据竞争</strong></td><td>多线程同时读写共享数据</td><td>全局变量未加锁</td></tr><tr><td><strong>检查-时-竞争</strong></td><td>检查和使用之间状态改变</td><td>单例模式</td></tr><tr><td><strong>死锁</strong></td><td>相互等待对方持有的锁</td><td>A 等待 B，B 等待 A</td></tr><tr><td><strong>活锁</strong></td><td>相互让步导致无法进展</td><td>礼让式重试</td></tr><tr><td><strong>饥饿</strong></td><td>某线程长时间得不到资源</td><td>低优先级线程</td></tr></tbody></table><hr><h2 id="二、竞态条件示例" tabindex="-1"><a class="header-anchor" href="#二、竞态条件示例"><span>二、竞态条件示例</span></a></h2><h3 id="_2-1-检查-时-竞争" tabindex="-1"><a class="header-anchor" href="#_2-1-检查-时-竞争"><span>2.1 检查-时-竞争</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ❌ 检查-时-竞争示例</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Singleton</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> Singleton<span class="token operator">*</span> <span class="token function">getInstance</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 线程 A 检查</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>instance_ <span class="token operator">==</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 线程 A 被中断...</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 线程 B 也检查，发现为空</span></span>
<span class="line">            <span class="token comment">// 线程 B 创建实例</span></span>
<span class="line">            <span class="token comment">// 线程 B 完成赋值</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 线程 A 恢复，又创建一个实例！</span></span>
<span class="line">            instance_ <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">Singleton</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> instance_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> Singleton<span class="token operator">*</span> instance_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ✅ 解决方案 1: 静态局部变量</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Singleton</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> Singleton<span class="token operator">&amp;</span> <span class="token function">getInstance</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// C++11 保证线程安全</span></span>
<span class="line">        <span class="token keyword">static</span> Singleton instance<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> instance<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ✅ 解决方案 2: 双重检查锁定</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Singleton</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> Singleton<span class="token operator">*</span> <span class="token function">getInstance</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>instance_ <span class="token operator">==</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span>  <span class="token comment">// 第一次检查</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>instance_ <span class="token operator">==</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span>  <span class="token comment">// 第二次检查</span></span>
<span class="line">                instance_ <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">Singleton</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> instance_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> std<span class="token double-colon punctuation">::</span>mutex mutex_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> Singleton<span class="token operator">*</span> instance_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-延迟初始化竞争" tabindex="-1"><a class="header-anchor" href="#_2-2-延迟初始化竞争"><span>2.2 延迟初始化竞争</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ❌ 延迟初始化竞争</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Cache</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    Value<span class="token operator">*</span> <span class="token function">get</span><span class="token punctuation">(</span><span class="token keyword">const</span> Key<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> cache_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> cache_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 竞态！多个线程可能同时创建</span></span>
<span class="line">            Value<span class="token operator">*</span> v <span class="token operator">=</span> <span class="token function">loadValue</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            cache_<span class="token punctuation">[</span>key<span class="token punctuation">]</span> <span class="token operator">=</span> v<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> v<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>Key<span class="token punctuation">,</span> Value<span class="token operator">*</span><span class="token operator">&gt;</span> cache_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ✅ 解决方案: 使用 std::call_once</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Cache</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    Value<span class="token operator">*</span> <span class="token function">get</span><span class="token punctuation">(</span><span class="token keyword">const</span> Key<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 先检查是否已存在（无锁快速路径）</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>shared_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">auto</span> it <span class="token operator">=</span> cache_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> cache_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 需要初始化</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 再次检查（可能其他线程已初始化）</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> cache_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> cache_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 初始化</span></span>
<span class="line">        Value<span class="token operator">*</span> v <span class="token operator">=</span> <span class="token function">loadValue</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        cache_<span class="token punctuation">[</span>key<span class="token punctuation">]</span> <span class="token operator">=</span> v<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> v<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>Key<span class="token punctuation">,</span> Value<span class="token operator">*</span><span class="token operator">&gt;</span> cache_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>shared_mutex mutex_<span class="token punctuation">;</span>  <span class="token comment">// 读写锁</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-迭代器失效" tabindex="-1"><a class="header-anchor" href="#_2-3-迭代器失效"><span>2.3 迭代器失效</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ❌ 迭代器失效竞态</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PlayerManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 线程 A: 遍历</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> player <span class="token operator">:</span> players_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            player<span class="token operator">-&gt;</span><span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addPlayer</span><span class="token punctuation">(</span>Player<span class="token operator">*</span> player<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 线程 B: 添加</span></span>
<span class="line">        players_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>player<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token comment">// 可能导致线程 A 的迭代器失效！</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Player<span class="token operator">*</span><span class="token operator">&gt;</span> players_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ✅ 解决方案: 使用版本控制或快照</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PlayerManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 获取当前版本</span></span>
<span class="line">        size_t version <span class="token operator">=</span> <span class="token function">getVersion</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> snapshot <span class="token operator">=</span> <span class="token function">getPlayersSnapshot</span><span class="token punctuation">(</span>version<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> player <span class="token operator">:</span> snapshot<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            player<span class="token operator">-&gt;</span><span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addPlayer</span><span class="token punctuation">(</span>Player<span class="token operator">*</span> player<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        players_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>player<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        version_<span class="token operator">++</span><span class="token punctuation">;</span>  <span class="token comment">// 增加版本号</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Player<span class="token operator">*</span><span class="token operator">&gt;</span> <span class="token function">getPlayersSnapshot</span><span class="token punctuation">(</span>size_t version<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>shared_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>version <span class="token operator">!=</span> version_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 版本变了，重新获取</span></span>
<span class="line">            <span class="token comment">// 实际可以使用读写拷贝技巧</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> players_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    size_t <span class="token function">getVersion</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> version_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Player<span class="token operator">*</span><span class="token operator">&gt;</span> players_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>shared_mutex mutex_<span class="token punctuation">;</span></span>
<span class="line">    size_t version_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、检测工具" tabindex="-1"><a class="header-anchor" href="#三、检测工具"><span>三、检测工具</span></a></h2><h3 id="_3-1-threadsanitizer" tabindex="-1"><a class="header-anchor" href="#_3-1-threadsanitizer"><span>3.1 ThreadSanitizer</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 使用 ThreadSanitizer 检测竞态</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 编译时加上 -fsanitize=thread 标志</span></span>
<span class="line">g++ <span class="token parameter variable">-fsanitize</span><span class="token operator">=</span>thread <span class="token parameter variable">-g</span> <span class="token parameter variable">-O2</span> your_program.cpp</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 运行程序</span></span>
<span class="line">./a.out</span>
<span class="line"></span>
<span class="line"><span class="token comment"># ThreadSanitizer 会报告潜在的竞态条件</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ThreadSanitizer 检测示例</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">int</span> global <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">thread1</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    global <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>  <span class="token comment">// 数据竞争</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">thread2</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    global <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>  <span class="token comment">// 数据竞争</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ThreadSanitizer 输出:</span></span>
<span class="line"><span class="token comment">// WARNING: ThreadSanitizer: data race on vptr</span></span>
<span class="line"><span class="token comment">//   Write of size 4 at 0x... by thread T1:</span></span>
<span class="line"><span class="token comment">//     #0 thread1()</span></span>
<span class="line"><span class="token comment">//   Previous write of size 4 at 0x... by thread T2:</span></span>
<span class="line"><span class="token comment">//     #0 thread2()</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-静态分析" tabindex="-1"><a class="header-anchor" href="#_3-2-静态分析"><span>3.2 静态分析</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 使用静态分析工具检测</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Clang Thread Safety Analysis</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Mutex</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">lock</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token function">__attribute__</span><span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token function">exclusive_lock_function</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">unlock</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token function">__attribute__</span><span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token function">unlock_function</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Data</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">process</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        mu_<span class="token punctuation">.</span><span class="token function">lock</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 获取锁</span></span>
<span class="line">        data_<span class="token operator">++</span><span class="token punctuation">;</span>     <span class="token comment">// 操作数据</span></span>
<span class="line">        mu_<span class="token punctuation">.</span><span class="token function">unlock</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token comment">// 释放锁</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    Mutex mu_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> data_ <span class="token function">__attribute__</span><span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token function">guarded_by</span><span class="token punctuation">(</span>mu_<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、防护策略" tabindex="-1"><a class="header-anchor" href="#四、防护策略"><span>四、防护策略</span></a></h2><h3 id="_4-1-互斥锁" tabindex="-1"><a class="header-anchor" href="#_4-1-互斥锁"><span>4.1 互斥锁</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 使用互斥锁保护共享数据</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ThreadSafeCounter</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">increment</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token operator">++</span>value_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">get</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> value_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">mutable</span> std<span class="token double-colon punctuation">::</span>mutex mutex_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> value_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-原子操作" tabindex="-1"><a class="header-anchor" href="#_4-2-原子操作"><span>4.2 原子操作</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 使用原子操作</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AtomicCounter</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">increment</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 原子递增，无需锁</span></span>
<span class="line">        value_<span class="token punctuation">.</span><span class="token function">fetch_add</span><span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">int</span> <span class="token function">get</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> value_<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span>memory_order_relaxed<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>atomic<span class="token operator">&lt;</span><span class="token keyword">int</span><span class="token operator">&gt;</span> value_<span class="token punctuation">{</span><span class="token number">0</span><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-读写锁" tabindex="-1"><a class="header-anchor" href="#_4-3-读写锁"><span>4.3 读写锁</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 读写锁分离读写操作</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ThreadSafeCache</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    Value<span class="token operator">*</span> <span class="token function">get</span><span class="token punctuation">(</span><span class="token keyword">const</span> Key<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 读操作可以并发</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>shared_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> cache_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> it <span class="token operator">!=</span> cache_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">?</span> it<span class="token operator">-&gt;</span>second <span class="token operator">:</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">put</span><span class="token punctuation">(</span><span class="token keyword">const</span> Key<span class="token operator">&amp;</span> key<span class="token punctuation">,</span> Value<span class="token operator">*</span> value<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 写操作独占访问</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>unique_lock<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>shared_mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        cache_<span class="token punctuation">[</span>key<span class="token punctuation">]</span> <span class="token operator">=</span> value<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">mutable</span> std<span class="token double-colon punctuation">::</span>shared_mutex mutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>Key<span class="token punctuation">,</span> Value<span class="token operator">*</span><span class="token operator">&gt;</span> cache_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-4-线程局部存储" tabindex="-1"><a class="header-anchor" href="#_4-4-线程局部存储"><span>4.4 线程局部存储</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 使用 thread_local 避免共享</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PerThreadCache</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    Value<span class="token operator">*</span> <span class="token function">get</span><span class="token punctuation">(</span><span class="token keyword">const</span> Key<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 每个线程独立的缓存，无需加锁</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> cache <span class="token operator">=</span> <span class="token function">getThreadLocalCache</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> cache<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> cache<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        Value<span class="token operator">*</span> v <span class="token operator">=</span> <span class="token function">loadValue</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        cache<span class="token punctuation">[</span>key<span class="token punctuation">]</span> <span class="token operator">=</span> v<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> v<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>Key<span class="token punctuation">,</span> Value<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> <span class="token function">getThreadLocalCache</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">static</span> <span class="token keyword">thread_local</span> std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>Key<span class="token punctuation">,</span> Value<span class="token operator">*</span><span class="token operator">&gt;</span> cache<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> cache<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、kbengine-避免竞态" tabindex="-1"><a class="header-anchor" href="#五、kbengine-避免竞态"><span>五、KBEngine 避免竞态</span></a></h2><h3 id="_5-1-单线程模型" tabindex="-1"><a class="header-anchor" href="#_5-1-单线程模型"><span>5.1 单线程模型</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 使用单线程事件循环避免竞态</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 的竞态避免策略:</span>
<span class="line"></span>
<span class="line">1. 每个进程内单线程事件循环</span>
<span class="line">   - 串行处理所有消息</span>
<span class="line">   - 避免进程内竞态</span>
<span class="line"></span>
<span class="line">2. 实体单线程绑定</span>
<span class="line">   - 每个实体只在一个线程处理</span>
<span class="line">   - 状态变更串行化</span>
<span class="line"></span>
<span class="line">3. 邮箱消息传递</span>
<span class="line">   - 进程间异步通信</span>
<span class="line">   - 避免共享状态</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># KBEngine 风格实现</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>counter <span class="token operator">=</span> <span class="token number">0</span>  <span class="token comment"># 单线程访问，无需锁</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onAttack</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> attackerId<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 串行执行，无竞态</span></span>
<span class="line">        self<span class="token punctuation">.</span>counter <span class="token operator">+=</span> <span class="token number">1</span></span>
<span class="line">        self<span class="token punctuation">.</span>processDamage<span class="token punctuation">(</span>attackerId<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">processDamage</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> attackerId<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        attacker <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>getEntity<span class="token punctuation">(</span>attackerId<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> attacker<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>hp <span class="token operator">-=</span> attacker<span class="token punctuation">.</span>attack</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-actor-模型" tabindex="-1"><a class="header-anchor" href="#_5-2-actor-模型"><span>5.2 Actor 模型</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Actor 模型避免竞态</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Actor</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 发送消息 (异步)</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">send</span><span class="token punctuation">(</span>Message msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        mailbox_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理消息 (单线程执行)</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">process</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>Message<span class="token operator">&gt;</span> messages<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>lock_guard<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>mutex<span class="token operator">&gt;</span> <span class="token function">lock</span><span class="token punctuation">(</span>mutex_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            messages<span class="token punctuation">.</span><span class="token function">swap</span><span class="token punctuation">(</span>mailbox_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>messages<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">handleMessage</span><span class="token punctuation">(</span>messages<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            messages<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">handleMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> Message<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>mutex mutex_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>Message<span class="token operator">&gt;</span> mailbox_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-避免竞态建议" tabindex="-1"><a class="header-anchor" href="#_6-1-避免竞态建议"><span>6.1 避免竞态建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>最小化共享</strong></td><td>尽量减少共享数据</td></tr><tr><td><strong>使用原子操作</strong></td><td>简单计数等用原子类型</td></tr><tr><td><strong>锁保护</strong></td><td>复杂操作用锁</td></tr><tr><td><strong>单线程模型</strong></td><td>KBEngine 方式</td></tr><tr><td><strong>消息传递</strong></td><td>Actor 模型</td></tr></tbody></table><h3 id="_6-2-编码检查清单" tabindex="-1"><a class="header-anchor" href="#_6-2-编码检查清单"><span>6.2 编码检查清单</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">竞态检查清单:</span>
<span class="line"></span>
<span class="line">□ 共享变量是否有保护?</span>
<span class="line">□ 检查-时-操作是否原子?</span>
<span class="line">□ 迭代器是否可能失效?</span>
<span class="line">□ 延迟初始化是否安全?</span>
<span class="line">□ 是否使用了 ThreadSanitizer?</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="竞态条件防护" tabindex="-1"><a class="header-anchor" href="#竞态条件防护"><span>竞态条件防护</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">竞态防护 = 最小共享 + 同步机制 + 单线程模型 + 检测工具</span>
<span class="line">- 能不共享就不共享</span>
<span class="line">- 共享就用锁/原子操作</span>
<span class="line">- KBEngine 选择单线程事件循环</span>
<span class="line">- Actor 模型消息传递</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual" target="_blank" rel="noopener noreferrer">ThreadSanitizer</a></li><li><a href="https://www.manning.com/books/c-plus-plus-concurrency-in-action" target="_blank" rel="noopener noreferrer">C++ Concurrency in Action</a></li><li><a href="https://kbengine.github.io/docs/" target="_blank" rel="noopener noreferrer">KBEngine Thread Safety</a></li></ul>`,54)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};