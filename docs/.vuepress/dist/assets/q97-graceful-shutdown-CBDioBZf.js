import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q97-graceful-shutdown.html","title":"Q97: 如何实现优雅关机？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q97-graceful-shutdown.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q97-graceful-shutdown.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q97-如何实现优雅关机" tabindex="-1"><a class="header-anchor" href="#q97-如何实现优雅关机"><span>Q97: 如何实现优雅关机？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对优雅关机的理解：</p><ul><li>关机流程</li><li>连接清理</li><li>数据保存</li><li>KBEngine 关机机制</li></ul><hr><h2 id="一、优雅关机原理" tabindex="-1"><a class="header-anchor" href="#一、优雅关机原理"><span>一、优雅关机原理</span></a></h2><h3 id="_1-1-关机类型" tabindex="-1"><a class="header-anchor" href="#_1-1-关机类型"><span>1.1 关机类型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    关机类型                                    │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  立即关机 (Immediate):                                     │</span>
<span class="line">│  ├── SIGKILL 信号                                          │</span>
<span class="line">│  ├── 强制终止                                               │</span>
<span class="line">│  ├── 可能丢失数据                                           │</span>
<span class="line">│  └── 示例: kill -9, 服务器断电                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优雅关机 (Graceful):                                      │</span>
<span class="line">│  ├── SIGTERM 信号                                          │</span>
<span class="line">│  ├── 完成当前请求                                           │</span>
<span class="line">│  ├── 保存状态                                               │</span>
<span class="line">│  └── 示例: 正常关闭脚本                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  延迟关机 (Delayed):                                       │</span>
<span class="line">│  ├── 等待玩家退出                                           │</span>
<span class="line">│  ├── 定时触发                                               │</span>
<span class="line">│  ├── 维护通知                                               │</span>
<span class="line">│  └── 示例: 维护模式                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、关机流程" tabindex="-1"><a class="header-anchor" href="#二、关机流程"><span>二、关机流程</span></a></h2><h3 id="_2-1-关机流程图" tabindex="-1"><a class="header-anchor" href="#_2-1-关机流程图"><span>2.1 关机流程图</span></a></h3>`,11),i(d,{code:`eJxNj81Kw0AUhfd9inmBvoJg/1u37oYuRBAXgiCCCyMkYmhNUmtprMXGFEVBkMSUaopJ27xM7p3kLUxnKmRWM/N93HvO0cnpxeHxwdk52a8USHZ2KZo+dD1Q52gFcfQC/UWbFIs7pHQp/tgsBFu/4nZpQyT2qUNvLpEyjaNncMaZl5ouPnjYc9s5L16O04kqkQoFxULnFe/es2U48pLIzu55FUI/iaYSqVLoD1JZYeE3am+p/MSmmcfFCk9Vo8y5hbWafC1wdg2ugd17MagquJBr/FH/D+ja8WrINB9lRch1zhsUDSX+VcEYseUwE/MdGlxp0k27R4etBiy08sGbnLdo2tHQXCc/Nxhsk7Q42aOpLEMnSKIJ+9C3JcoCFf4AQsu41A==`}),o[1]||=e(`<h3 id="_2-2-kbengine-关机流程" tabindex="-1"><a class="header-anchor" href="#_2-2-kbengine-关机流程"><span>2.2 KBEngine 关机流程</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 优雅关机</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">KBEngine 关机机制:</span>
<span class="line"></span>
<span class="line">1. 收到 SIGTERM 信号</span>
<span class="line">2. 调用 onKBEngineShutDown 闭包</span>
<span class="line">3. 停止接收新连接</span>
<span class="line">4. 等待实体保存</span>
<span class="line">5. 关闭网络</span>
<span class="line">6. 退出进程</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"><span class="token keyword">import</span> signal</span>
<span class="line"><span class="token keyword">import</span> sys</span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ShutdownManager</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;关机管理器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>shutdown initiated <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line">        self<span class="token punctuation">.</span>shutdown_timeout <span class="token operator">=</span> <span class="token number">60</span>  <span class="token comment"># 秒</span></span>
<span class="line">        self<span class="token punctuation">.</span>players_to_save <span class="token operator">=</span> <span class="token builtin">set</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 注册信号处理</span></span>
<span class="line">        signal<span class="token punctuation">.</span>signal<span class="token punctuation">(</span>signal<span class="token punctuation">.</span>SIGTERM<span class="token punctuation">,</span> self<span class="token punctuation">.</span>on_signal<span class="token punctuation">)</span></span>
<span class="line">        signal<span class="token punctuation">.</span>signal<span class="token punctuation">(</span>signal<span class="token punctuation">.</span>SIGINT<span class="token punctuation">,</span> self<span class="token punctuation">.</span>on_signal<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 注册关机回调</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>registerCallback<span class="token punctuation">(</span><span class="token string">&quot;onShutdown&quot;</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>on_shutdown<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">on_signal</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> signum<span class="token punctuation">,</span> frame<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;信号处理&quot;&quot;&quot;</span></span>
<span class="line">        signal_name <span class="token operator">=</span> signal<span class="token punctuation">.</span>Signals<span class="token punctuation">(</span>signum<span class="token punctuation">)</span><span class="token punctuation">.</span>name</span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Received signal: </span><span class="token interpolation"><span class="token punctuation">{</span>signal_name<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>shutdown_initiated<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>initiate_shutdown<span class="token punctuation">(</span><span class="token string">&quot;normal&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">            WARNING_MSG<span class="token punctuation">(</span><span class="token string">&quot;Shutdown already in progress, forcing...&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>force_shutdown<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">initiate_shutdown</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> reason<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;发起关机&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>shutdown_initiated<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span></span>
<span class="line"></span>
<span class="line">        self<span class="token punctuation">.</span>shutdown_initiated <span class="token operator">=</span> <span class="token boolean">True</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Initiating graceful shutdown: </span><span class="token interpolation"><span class="token punctuation">{</span>reason<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 1. 停止接收新连接</span></span>
<span class="line">        self<span class="token punctuation">.</span>stop_accepting_connections<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 2. 通知所有玩家</span></span>
<span class="line">        self<span class="token punctuation">.</span>notify_players_shutdown<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 3. 开始保存实体</span></span>
<span class="line">        self<span class="token punctuation">.</span>save_all_entities<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">stop_accepting_connections</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;停止接收新连接&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># KBEngine 内置方法</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>setAcceptNewConnections<span class="token punctuation">(</span><span class="token boolean">False</span><span class="token punctuation">)</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Stopped accepting new connections&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">notify_players_shutdown</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;通知玩家关机&quot;&quot;&quot;</span></span>
<span class="line">        shutdown_msg <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;type&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;server_shutdown&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;reason&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;maintenance&quot;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;timeout&quot;</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>shutdown_timeout</span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 广播给所有在线玩家</span></span>
<span class="line">        <span class="token keyword">for</span> entity_id<span class="token punctuation">,</span> entity <span class="token keyword">in</span> KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> <span class="token string">&#39;client&#39;</span><span class="token punctuation">)</span> <span class="token keyword">and</span> entity<span class="token punctuation">.</span>client<span class="token punctuation">:</span></span>
<span class="line">                entity<span class="token punctuation">.</span>client<span class="token punctuation">.</span>onServerShutdown<span class="token punctuation">(</span>shutdown_msg<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">save_all_entities</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;保存所有实体&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Saving all entities...&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 获取需要保存的实体</span></span>
<span class="line">        self<span class="token punctuation">.</span>players_to_save <span class="token operator">=</span> <span class="token builtin">set</span><span class="token punctuation">(</span></span>
<span class="line">            eid <span class="token keyword">for</span> eid<span class="token punctuation">,</span> e <span class="token keyword">in</span> KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>e<span class="token punctuation">,</span> <span class="token string">&#39;accountEntity&#39;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entities to save: </span><span class="token interpolation"><span class="token punctuation">{</span><span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>players_to_save<span class="token punctuation">)</span><span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># KBEngine 会自动触发实体写入</span></span>
<span class="line">        <span class="token comment"># 我们需要等待写入完成</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>setShutdownTimer<span class="token punctuation">(</span>self<span class="token punctuation">.</span>shutdown_timeout<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">on_entity_saved</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity_id<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;实体保存回调&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> entity_id <span class="token keyword">in</span> self<span class="token punctuation">.</span>players_to_save<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>players_to_save<span class="token punctuation">.</span>remove<span class="token punctuation">(</span>entity_id<span class="token punctuation">)</span></span>
<span class="line">            INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>entity_id<span class="token punctuation">}</span></span><span class="token string"> saved, &quot;</span></span></span>
<span class="line">                    <span class="token string-interpolation"><span class="token string">f&quot;remaining: </span><span class="token interpolation"><span class="token punctuation">{</span><span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>players_to_save<span class="token punctuation">)</span><span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>players_to_save<span class="token punctuation">:</span></span>
<span class="line">            INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;All entities saved, shutting down...&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>final_shutdown<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">final_shutdown</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;最终关机&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Performing final shutdown...&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 关闭数据库连接</span></span>
<span class="line">        self<span class="token punctuation">.</span>close_database<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 退出</span></span>
<span class="line">        sys<span class="token punctuation">.</span>exit<span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">force_shutdown</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;强制关机&quot;&quot;&quot;</span></span>
<span class="line">        WARNING_MSG<span class="token punctuation">(</span><span class="token string">&quot;Forcing shutdown...&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        sys<span class="token punctuation">.</span>exit<span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">close_database</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;关闭数据库连接&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># KBEngine DBMgr 会自动关闭</span></span>
<span class="line">        <span class="token keyword">pass</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token comment"># KBEngine 脚本中的关机处理</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">onKBEngineShutDown</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    KBEngine 关机回调</span>
<span class="line"></span>
<span class="line">    这个函数在 KBEngine 准备关闭时被调用</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;KBEngine is shutting down...&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 执行清理工作</span></span>
<span class="line">    cleanup_resources<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 保存全局状态</span></span>
<span class="line">    save_global_state<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">cleanup_resources</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;清理资源&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token comment"># 清理缓存</span></span>
<span class="line">    clear_caches<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 关闭外部连接</span></span>
<span class="line">    close_external_connections<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 取消定时器</span></span>
<span class="line">    cancel_all_timers<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">save_global_state</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;保存全局状态&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token comment"># 保存游戏全局状态</span></span>
<span class="line">    <span class="token keyword">pass</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、数据保存保证" tabindex="-1"><a class="header-anchor" href="#三、数据保存保证"><span>三、数据保存保证</span></a></h2><h3 id="_3-1-实体保存" tabindex="-1"><a class="header-anchor" href="#_3-1-实体保存"><span>3.1 实体保存</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 实体保存保证</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SaveOnShutdown</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;关机时保存&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>save_callbacks <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">register_save_callback</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> callback<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;注册保存回调&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>save_callbacks<span class="token punctuation">.</span>append<span class="token punctuation">(</span>callback<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">execute_saves</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;执行所有保存&quot;&quot;&quot;</span></span>
<span class="line">        results <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> callback <span class="token keyword">in</span> self<span class="token punctuation">.</span>save_callbacks<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                result <span class="token operator">=</span> callback<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">                results<span class="token punctuation">.</span>append<span class="token punctuation">(</span><span class="token punctuation">(</span>callback<span class="token punctuation">.</span>__name__<span class="token punctuation">,</span> <span class="token boolean">True</span><span class="token punctuation">,</span> result<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">                ERROR_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Save callback failed: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">                results<span class="token punctuation">.</span>append<span class="token punctuation">(</span><span class="token punctuation">(</span>callback<span class="token punctuation">.</span>__name__<span class="token punctuation">,</span> <span class="token boolean">False</span><span class="token punctuation">,</span> <span class="token builtin">str</span><span class="token punctuation">(</span>e<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> results</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Account</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Account<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;账号实体 - 支持关机保存&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Account<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>pending_save <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onShutdown</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;关机时调用&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Account </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> onShutdown&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 标记需要保存</span></span>
<span class="line">        self<span class="token punctuation">.</span>pending_save <span class="token operator">=</span> <span class="token boolean">True</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 立即写入数据库</span></span>
<span class="line">        self<span class="token punctuation">.</span>writeToDB<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onWriteToDBCallback</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> success<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;数据库写入回调&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> success<span class="token punctuation">:</span></span>
<span class="line">            INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Account </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> saved successfully&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">            ERROR_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Account </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> save failed!&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 通知关机管理器</span></span>
<span class="line">        shutdown_mgr<span class="token punctuation">.</span>on_entity_saved<span class="token punctuation">(</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、连接清理" tabindex="-1"><a class="header-anchor" href="#四、连接清理"><span>四、连接清理</span></a></h2><h3 id="_4-1-连接关闭" tabindex="-1"><a class="header-anchor" href="#_4-1-连接关闭"><span>4.1 连接关闭</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 连接清理</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ConnectionManager</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;连接管理器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>connections <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">register_connection</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> conn_id<span class="token punctuation">,</span> conn<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;注册连接&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>connections<span class="token punctuation">[</span>conn_id<span class="token punctuation">]</span> <span class="token operator">=</span> conn</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">close_all_connections</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;关闭所有连接&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Closing </span><span class="token interpolation"><span class="token punctuation">{</span><span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>connections<span class="token punctuation">)</span><span class="token punctuation">}</span></span><span class="token string"> connections...&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> conn_id<span class="token punctuation">,</span> conn <span class="token keyword">in</span> <span class="token builtin">list</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>connections<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">                <span class="token comment"># 发送关机消息</span></span>
<span class="line">                conn<span class="token punctuation">.</span>send_shutdown_notification<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment"># 关闭连接</span></span>
<span class="line">                conn<span class="token punctuation">.</span>close<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">del</span> self<span class="token punctuation">.</span>connections<span class="token punctuation">[</span>conn_id<span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">                ERROR_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Error closing connection </span><span class="token interpolation"><span class="token punctuation">{</span>conn_id<span class="token punctuation">}</span></span><span class="token string">: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">wait_connections_empty</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> timeout<span class="token operator">=</span><span class="token number">30</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;等待所有连接关闭&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">import</span> time</span>
<span class="line"></span>
<span class="line">        start <span class="token operator">=</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">while</span> self<span class="token punctuation">.</span>connections <span class="token keyword">and</span> <span class="token punctuation">(</span>time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> start<span class="token punctuation">)</span> <span class="token operator">&lt;</span> timeout<span class="token punctuation">:</span></span>
<span class="line">            INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Waiting for </span><span class="token interpolation"><span class="token punctuation">{</span><span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>connections<span class="token punctuation">)</span><span class="token punctuation">}</span></span><span class="token string"> connections...&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            time<span class="token punctuation">.</span>sleep<span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>connections<span class="token punctuation">:</span></span>
<span class="line">            WARNING_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Timeout with </span><span class="token interpolation"><span class="token punctuation">{</span><span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>connections<span class="token punctuation">)</span><span class="token punctuation">}</span></span><span class="token string"> connections remaining&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">KBEngineConnection</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;KBEngine 连接处理&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onClientDeath</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;客户端断开&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Client </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> disconnected&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onShutdownNotify</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;关机通知&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>client<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>client<span class="token punctuation">.</span>onServerShutdown<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&quot;message&quot;</span><span class="token punctuation">:</span> <span class="token string">&quot;Server is shutting down&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;timeout&quot;</span><span class="token punctuation">:</span> <span class="token number">30</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、维护模式" tabindex="-1"><a class="header-anchor" href="#五、维护模式"><span>五、维护模式</span></a></h2><h3 id="_5-1-延迟关机" tabindex="-1"><a class="header-anchor" href="#_5-1-延迟关机"><span>5.1 延迟关机</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 维护模式</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MaintenanceMode</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;维护模式&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> shutdown_delay<span class="token operator">=</span><span class="token number">300</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>shutdown_delay <span class="token operator">=</span> shutdown_delay  <span class="token comment"># 5分钟</span></span>
<span class="line">        self<span class="token punctuation">.</span>shutdown_scheduled <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line">        self<span class="token punctuation">.</span>shutdown_time <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">schedule_maintenance</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> delay<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;安排维护&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>shutdown_scheduled <span class="token operator">=</span> <span class="token boolean">True</span></span>
<span class="line">        self<span class="token punctuation">.</span>shutdown_time <span class="token operator">=</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">+</span> delay</span>
<span class="line"></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Maintenance scheduled in </span><span class="token interpolation"><span class="token punctuation">{</span>delay<span class="token punctuation">}</span></span><span class="token string"> seconds&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 通知所有玩家</span></span>
<span class="line">        self<span class="token punctuation">.</span>broadcast_maintenance_notice<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 设置定时器</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>addTimer<span class="token punctuation">(</span>delay<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>execute_shutdown<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">broadcast_maintenance_notice</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;广播维护通知&quot;&quot;&quot;</span></span>
<span class="line">        remaining <span class="token operator">=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>shutdown_time <span class="token operator">-</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> entity_id<span class="token punctuation">,</span> entity <span class="token keyword">in</span> KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> <span class="token string">&#39;client&#39;</span><span class="token punctuation">)</span> <span class="token keyword">and</span> entity<span class="token punctuation">.</span>client<span class="token punctuation">:</span></span>
<span class="line">                <span class="token comment"># 每分钟通知一次</span></span>
<span class="line">                <span class="token keyword">for</span> t <span class="token keyword">in</span> <span class="token builtin">range</span><span class="token punctuation">(</span>remaining<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> <span class="token operator">-</span><span class="token number">60</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                    KBEngine<span class="token punctuation">.</span>addTimer<span class="token punctuation">(</span>t<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> <span class="token keyword">lambda</span><span class="token punctuation">:</span> self<span class="token punctuation">.</span>send_notice<span class="token punctuation">(</span>entity<span class="token punctuation">,</span> remaining <span class="token operator">-</span> t<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">send_notice</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity<span class="token punctuation">,</span> remaining<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;发送通知&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> entity<span class="token punctuation">.</span>client<span class="token punctuation">:</span></span>
<span class="line">            entity<span class="token punctuation">.</span>client<span class="token punctuation">.</span>onMaintenanceNotice<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&quot;remaining&quot;</span><span class="token punctuation">:</span> remaining<span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&quot;message&quot;</span><span class="token punctuation">:</span> <span class="token string-interpolation"><span class="token string">f&quot;Server will shutdown in </span><span class="token interpolation"><span class="token punctuation">{</span>remaining<span class="token punctuation">}</span></span><span class="token string"> seconds&quot;</span></span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">execute_shutdown</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;执行关机&quot;&quot;&quot;</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string">&quot;Maintenance shutdown executing...&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        shutdown_mgr<span class="token punctuation">.</span>initiate_shutdown<span class="token punctuation">(</span><span class="token string">&quot;scheduled_maintenance&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-优雅关机建议" tabindex="-1"><a class="header-anchor" href="#_6-1-优雅关机建议"><span>6.1 优雅关机建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>信号处理</strong></td><td>捕获 SIGTERM/SIGINT</td></tr><tr><td><strong>数据保存</strong></td><td>确保所有实体写入数据库</td></tr><tr><td><strong>连接通知</strong></td><td>提前通知玩家</td></tr><tr><td><strong>超时保护</strong></td><td>设置关机超时</td></tr><tr><td><strong>状态检查</strong></td><td>关机前检查服务健康</td></tr><tr><td><strong>日志记录</strong></td><td>记录关机过程</td></tr></tbody></table><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="优雅关机核心" tabindex="-1"><a class="header-anchor" href="#优雅关机核心"><span>优雅关机核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">优雅关机 = 信号捕获 + 连接通知 + 数据保存 + 资源清理</span>
<span class="line">- KBEngine onKBEngineShutDown</span>
<span class="line">- 停止接收新连接</span>
<span class="line">- 等待实体保存完成</span>
<span class="line">- 超时保护机制</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li>[Graceful Shutdown Patterns](https://medium.com/@haxnus/graceful-shutdown-in-go-and-linux- signals-cb4db09e6a1a)</li><li><a href="https://kbengine.github.io/docs/" target="_blank" rel="noopener noreferrer">KBEngine Lifecycle</a></li><li><a href="https://man7.org/linux/man-pages/man7/signal.7.html" target="_blank" rel="noopener noreferrer">Linux Signal Handling</a></li></ul>`,25)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};