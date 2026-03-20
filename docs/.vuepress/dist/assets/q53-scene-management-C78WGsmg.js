import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q53-scene-management.html","title":"Q53: 如何设计场景管理？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q53-scene-management.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q53-scene-management.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q53-如何设计场景管理" tabindex="-1"><a class="header-anchor" href="#q53-如何设计场景管理"><span>Q53: 如何设计场景管理？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对场景管理系统的理解：</p><ul><li>场景分层结构</li><li>场景加载与卸载</li><li>场景对象管理</li><li>场景切换与同步</li></ul><hr><h2 id="一、场景管理架构" tabindex="-1"><a class="header-anchor" href="#一、场景管理架构"><span>一、场景管理架构</span></a></h2><h3 id="_1-1-系统组成" tabindex="-1"><a class="header-anchor" href="#_1-1-系统组成"><span>1.1 系统组成</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    场景管理架构                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景层 (Scene Layer):                                      │</span>
<span class="line">│  ├── 场景加载/卸载                                          │</span>
<span class="line">│  ├── 场景资源管理                                          │</span>
<span class="line">│  ├── 场景生命周期                                          │</span>
<span class="line">│  └── 场景切换                                              │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼                                  │</span>
<span class="line">│  空间层 (Space Layer):                                      │</span>
<span class="line">│  ├── 空间划分管理                                          │</span>
<span class="line">│  ├── 场景分区管理                                          │</span>
<span class="line">│  ├── AOI 管理                                              │</span>
<span class="line">│  └── 跨场景移动                                            │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼                                  │</span>
<span class="line">│  对象层 (Entity Layer):                                    │</span>
<span class="line">│  ├── 实体创建/销毁                                          │</span>
<span class="line">│  ├── 实体状态同步                                          │</span>
<span class="line">│  ├── 实体位置更新                                          │</span>
<span class="line">│  └── 实体属性管理                                          │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼                                  │</span>
<span class="line">│  渲染层 (Render Layer):                                    │</span>
<span class="line">│  ├── 场景渲染                                              │</span>
<span class="line">│  ├── LOD 管理                                              │</span>
<span class="line">│  ├── 视锥剔除                                              │</span>
<span class="line">│  └── 场景特效                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-场景类型" tabindex="-1"><a class="header-anchor" href="#_1-2-场景类型"><span>1.2 场景类型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    场景类型                                    │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 游戏场景 (Game Scene)                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 主游戏区域                                       │       │</span>
<span class="line">│  │  - 玩家主要活动区域                                 │       │</span>
<span class="line">│  │  - 需要完整加载                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 副本场景 (Instance Scene)                               │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 独立的副本空间                                    │       │</span>
<span class="line">│  │  - 每个队伍独立实例                                  │       │</span>
<span class="line">│  │  - 动态创建和销毁                                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 房间隔间 (Room Scene)                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 房间划分的场景                                    │       │</span>
<span class="line">│  │  - 用于室内场景                                     │       │</span>
<span class="line">│  │  - 门作为连接点                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 过渡场景 (Transition Scene)                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  - 加载界面                                         │       │</span>
<span class="line">│  │  - 场景切换过渡                                     │       │</span>
<span class="line">│  │  - 资源预加载                                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、场景管理实现" tabindex="-1"><a class="header-anchor" href="#二、场景管理实现"><span>二、场景管理实现</span></a></h2><h3 id="_2-1-场景管理器" tabindex="-1"><a class="header-anchor" href="#_2-1-场景管理器"><span>2.1 场景管理器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 场景管理器</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SceneManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 初始化场景管理器</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">initialize</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 创建默认场景</span></span>
<span class="line">        <span class="token function">createScene</span><span class="token punctuation">(</span><span class="token string">&quot;default&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 创建场景</span></span>
<span class="line">    Scene<span class="token operator">*</span> <span class="token function">createScene</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> name<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> scene <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">make_unique</span><span class="token generic class-name"><span class="token operator">&lt;</span>Scene<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        scene<span class="token operator">-&gt;</span>name <span class="token operator">=</span> name<span class="token punctuation">;</span></span>
<span class="line">        scene<span class="token operator">-&gt;</span>sceneId <span class="token operator">=</span> <span class="token function">generateSceneId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        scene<span class="token operator">-&gt;</span>state <span class="token operator">=</span> SceneState<span class="token double-colon punctuation">::</span>INITIALIZING<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        scenes_<span class="token punctuation">[</span>scene<span class="token operator">-&gt;</span>sceneId<span class="token punctuation">]</span> <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Scene created: {} (id: {})&quot;</span><span class="token punctuation">,</span> name<span class="token punctuation">,</span> scene<span class="token operator">-&gt;</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> scenes_<span class="token punctuation">[</span>scene<span class="token operator">-&gt;</span>sceneId<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">get</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 加载场景</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">loadScene</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> sceneId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> scene <span class="token operator">=</span> <span class="token function">getScene</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>scene<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">ERROR</span><span class="token punctuation">(</span><span class="token string">&quot;Scene not found: {}&quot;</span><span class="token punctuation">,</span> sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 如果场景已加载，直接返回</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>scene<span class="token operator">-&gt;</span>state <span class="token operator">==</span> SceneState<span class="token double-colon punctuation">::</span>LOADED<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Loading scene: {}&quot;</span><span class="token punctuation">,</span> scene<span class="token operator">-&gt;</span>name<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 设置为加载中</span></span>
<span class="line">        scene<span class="token operator">-&gt;</span>state <span class="token operator">=</span> SceneState<span class="token double-colon punctuation">::</span>LOADING<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 加载场景资源</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">loadSceneResources</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            scene<span class="token operator">-&gt;</span>state <span class="token operator">=</span> SceneState<span class="token double-colon punctuation">::</span>UNLOADED<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 初始化场景</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">initializeScene</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            scene<span class="token operator">-&gt;</span>state <span class="token operator">=</span> SceneState<span class="token double-colon punctuation">::</span>UNLOADED<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        scene<span class="token operator">-&gt;</span>state <span class="token operator">=</span> SceneState<span class="token double-colon punctuation">::</span>LOADED<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Scene loaded: {}&quot;</span><span class="token punctuation">,</span> scene<span class="token operator">-&gt;</span>name<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 卸载场景</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">unloadScene</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> sceneId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> scene <span class="token operator">=</span> <span class="token function">getScene</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>scene<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Unloading scene: {}&quot;</span><span class="token punctuation">,</span> scene<span class="token operator">-&gt;</span>name<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知场景中的实体</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">:</span> scene<span class="token operator">-&gt;</span>entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">onSceneUnload</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 清理场景</span></span>
<span class="line">        <span class="token function">cleanupScene</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        scene<span class="token operator">-&gt;</span>state <span class="token operator">=</span> SceneState<span class="token double-colon punctuation">::</span>UNLOADED<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 销毁场景</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">destroyScene</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> sceneId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> scenes_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> scenes_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Destroying scene: {}&quot;</span><span class="token punctuation">,</span> it<span class="token operator">-&gt;</span>second<span class="token operator">-&gt;</span>name<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 先卸载</span></span>
<span class="line">        <span class="token function">unloadScene</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 移除场景</span></span>
<span class="line">        scenes_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取场景</span></span>
<span class="line">    Scene<span class="token operator">*</span> <span class="token function">getScene</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> sceneId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> scenes_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> it <span class="token operator">!=</span> scenes_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">?</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span><span class="token function">get</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">:</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加实体到场景</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">addEntityToScene</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> entityId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> sceneId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> scene <span class="token operator">=</span> <span class="token function">getScene</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>scene<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查实体是否已在场景中</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">getEntityScene</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span> <span class="token operator">!=</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 添加到场景</span></span>
<span class="line">        scene<span class="token operator">-&gt;</span>entities<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        entityToScene_<span class="token punctuation">[</span>entityId<span class="token punctuation">]</span> <span class="token operator">=</span> sceneId<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知实体</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">onSceneEnter</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 从场景移除实体</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">removeEntityFromScene</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> entityId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> entityToScene_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> entityToScene_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">uint64_t</span> sceneId <span class="token operator">=</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> scene <span class="token operator">=</span> <span class="token function">getScene</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>scene<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 从场景实体列表移除</span></span>
<span class="line">        <span class="token keyword">auto</span> entityIt <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">find</span><span class="token punctuation">(</span>scene<span class="token operator">-&gt;</span>entities<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> scene<span class="token operator">-&gt;</span>entities<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entityIt <span class="token operator">!=</span> scene<span class="token operator">-&gt;</span>entities<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            scene<span class="token operator">-&gt;</span>entities<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>entityIt<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知实体</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">onSceneLeave</span><span class="token punctuation">(</span>sceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        entityToScene_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新场景 (每帧调用)</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span>uint32 deltaTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>sceneId<span class="token punctuation">,</span> scene<span class="token punctuation">]</span> <span class="token operator">:</span> scenes_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>scene<span class="token operator">-&gt;</span>state <span class="token operator">==</span> SceneState<span class="token double-colon punctuation">::</span>LOADED<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">updateScene</span><span class="token punctuation">(</span>scene<span class="token punctuation">.</span><span class="token function">get</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> deltaTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取实体所在场景</span></span>
<span class="line">    Scene<span class="token operator">*</span> <span class="token function">getEntityScene</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> entityId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> entityToScene_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> entityToScene_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">getScene</span><span class="token punctuation">(</span>it<span class="token operator">-&gt;</span>second<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">loadSceneResources</span><span class="token punctuation">(</span>Scene<span class="token operator">*</span> scene<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 加载地形</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">loadTerrain</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 加载静态对象</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">loadStaticObjects</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 加载光源</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">loadLights</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 加载特效</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">loadEffects</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">initializeScene</span><span class="token punctuation">(</span>Scene<span class="token operator">*</span> scene<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 初始化物理</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">initPhysics</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 初始化导航网格</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">initNavMesh</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 初始化AOI</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">initAOI</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateScene</span><span class="token punctuation">(</span>Scene<span class="token operator">*</span> scene<span class="token punctuation">,</span> uint32 deltaTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 更新场景中的实体</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint64_t</span> entityId <span class="token operator">:</span> scene<span class="token operator">-&gt;</span>entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                entity<span class="token operator">-&gt;</span><span class="token function">update</span><span class="token punctuation">(</span>deltaTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新场景特效</span></span>
<span class="line">        <span class="token function">updateEffects</span><span class="token punctuation">(</span>scene<span class="token punctuation">,</span> deltaTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新物理</span></span>
<span class="line">        <span class="token function">updatePhysics</span><span class="token punctuation">(</span>scene<span class="token punctuation">,</span> deltaTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新AOI</span></span>
<span class="line">        <span class="token function">updateAOI</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">cleanupScene</span><span class="token punctuation">(</span>Scene<span class="token operator">*</span> scene<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 清理实体</span></span>
<span class="line">        scene<span class="token operator">-&gt;</span>entities<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 清理资源</span></span>
<span class="line">        scene<span class="token operator">-&gt;</span>resources<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 清理物理</span></span>
<span class="line">        <span class="token function">cleanupPhysics</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 清理导航网格</span></span>
<span class="line">        <span class="token function">cleanupNavMesh</span><span class="token punctuation">(</span>scene<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint64_t</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>Scene<span class="token operator">&gt;&gt;</span> scenes_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint64_t</span><span class="token punctuation">,</span> <span class="token keyword">uint64_t</span><span class="token operator">&gt;</span> entityToScene_<span class="token punctuation">;</span>  <span class="token comment">// entityId -&gt; sceneId</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 场景数据结构</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">SceneState</span> <span class="token punctuation">{</span></span>
<span class="line">    UNLOADED <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span>       <span class="token comment">// 未加载</span></span>
<span class="line">    INITIALIZING <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span>   <span class="token comment">// 初始化中</span></span>
<span class="line">    LOADING <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span>        <span class="token comment">// 加载中</span></span>
<span class="line">    LOADED <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">,</span>         <span class="token comment">// 已加载</span></span>
<span class="line">    UNLOADING <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">,</span>      <span class="token comment">// 卸载中</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">Scene</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> sceneId<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string name<span class="token punctuation">;</span></span>
<span class="line">    SceneState state<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 场景内容</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint64_t</span><span class="token operator">&gt;</span> entities<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>SceneObject<span class="token operator">&gt;&gt;</span> staticObjects<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>Light<span class="token operator">&gt;&gt;</span> lights<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>Effect<span class="token operator">&gt;&gt;</span> effects<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 场景数据</span></span>
<span class="line">    Terrain<span class="token operator">*</span> terrain<span class="token punctuation">;</span></span>
<span class="line">    NavMesh<span class="token operator">*</span> navMesh<span class="token punctuation">;</span></span>
<span class="line">    PhysicsWorld<span class="token operator">*</span> physicsWorld<span class="token punctuation">;</span></span>
<span class="line">    AOIManager<span class="token operator">*</span> aoiManager<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 场景配置</span></span>
<span class="line">    SceneConfig config<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、场景切换" tabindex="-1"><a class="header-anchor" href="#三、场景切换"><span>三、场景切换</span></a></h2><h3 id="_3-1-切换流程" tabindex="-1"><a class="header-anchor" href="#_3-1-切换流程"><span>3.1 切换流程</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    场景切换流程                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 准备切换                                                │</span>
<span class="line">│     ├── 保存当前场景状态                                    │</span>
<span class="line">│     ├── 通知实体即将离开场景                                │</span>
<span class="line">│     └── 显示加载界面                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 卸载当前场景                                            │</span>
<span class="line">│     ├── 停止场景更新                                        │</span>
<span class="line">│     ├── 卸载场景资源                                        │</span>
<span class="line">│     └── 清理场景数据                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 加载目标场景                                            │</span>
<span class="line">│     ├── 加载场景资源                                        │</span>
<span class="line">│     ├── 初始化场景数据                                        │</span>
<span class="line">│     └── 预加载周边区域                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 切换完成                                                │</span>
<span class="line">│     ├── 传送玩家到新场景                                    │</span>
<span class="line">│     ├── 恢复玩家状态                                        │</span>
<span class="line">│     ├── 隐藏加载界面                                        │</span>
<span class="line">│     └── 开始场景更新                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-场景切换实现" tabindex="-1"><a class="header-anchor" href="#_3-2-场景切换实现"><span>3.2 场景切换实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 场景切换</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SceneTransition</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 切换场景</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">transition</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> fromSceneId<span class="token punctuation">,</span></span>
<span class="line">                   <span class="token keyword">uint64_t</span> toSceneId<span class="token punctuation">,</span> <span class="token keyword">const</span> Vector3<span class="token operator">&amp;</span> position<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 保存玩家状态</span></span>
<span class="line">        PlayerState state <span class="token operator">=</span> <span class="token function">savePlayerState</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 显示加载界面</span></span>
<span class="line">        <span class="token function">showLoadingScreen</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 从旧场景移除</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> fromScene <span class="token operator">=</span> sceneManager_<span class="token operator">-&gt;</span><span class="token function">getScene</span><span class="token punctuation">(</span>fromSceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>fromScene<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            sceneManager_<span class="token operator">-&gt;</span><span class="token function">removeEntityFromScene</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 加载目标场景 (如果未加载)</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> toScene <span class="token operator">=</span> sceneManager_<span class="token operator">-&gt;</span><span class="token function">getScene</span><span class="token punctuation">(</span>toSceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>toScene <span class="token operator">||</span> toScene<span class="token operator">-&gt;</span>state <span class="token operator">!=</span> SceneState<span class="token double-colon punctuation">::</span>LOADED<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>sceneManager_<span class="token operator">-&gt;</span><span class="token function">loadScene</span><span class="token punctuation">(</span>toSceneId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 加载失败，返回原场景</span></span>
<span class="line">                <span class="token function">hideLoadingScreen</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            toScene <span class="token operator">=</span> sceneManager_<span class="token operator">-&gt;</span><span class="token function">getScene</span><span class="token punctuation">(</span>toSceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 添加到新场景</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>sceneManager_<span class="token operator">-&gt;</span><span class="token function">addEntityToScene</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> toSceneId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">hideLoadingScreen</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 6. 设置玩家位置</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">setPosition</span><span class="token punctuation">(</span>position<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">onSceneEnter</span><span class="token punctuation">(</span>toSceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 7. 恢复玩家状态</span></span>
<span class="line">        <span class="token function">restorePlayerState</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> state<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 8. 隐藏加载界面</span></span>
<span class="line">        <span class="token function">hideLoadingScreen</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 9. 通知客户端</span></span>
<span class="line">        <span class="token function">sendToClient</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> <span class="token string">&quot;onSceneChanged&quot;</span><span class="token punctuation">,</span> toSceneId<span class="token punctuation">,</span> position<span class="token punctuation">.</span>x<span class="token punctuation">,</span> position<span class="token punctuation">.</span>y<span class="token punctuation">,</span> position<span class="token punctuation">.</span>z<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token function">INFO</span><span class="token punctuation">(</span><span class="token string">&quot;Player {} transitioned from scene {} to scene {}&quot;</span><span class="token punctuation">,</span></span>
<span class="line">             playerId<span class="token punctuation">,</span> fromSceneId<span class="token punctuation">,</span> toSceneId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 异步切换场景 (用于跨服务器场景)</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">asyncTransition</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> targetServer<span class="token punctuation">,</span></span>
<span class="line">                        <span class="token keyword">uint64_t</span> toSceneId<span class="token punctuation">,</span> <span class="token keyword">const</span> Vector3<span class="token operator">&amp;</span> position<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 保存玩家数据到数据库</span></span>
<span class="line">        <span class="token function">savePlayerData</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 通知目标服务器</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">notifyTargetServer</span><span class="token punctuation">(</span>targetServer<span class="token punctuation">,</span> playerId<span class="token punctuation">,</span> toSceneId<span class="token punctuation">,</span> position<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 断开当前连接</span></span>
<span class="line">        <span class="token function">disconnectPlayer</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    PlayerState <span class="token function">savePlayerState</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        PlayerState state<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            state<span class="token punctuation">.</span>position <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            state<span class="token punctuation">.</span>rotation <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getRotation</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            state<span class="token punctuation">.</span>hp <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getHP</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            state<span class="token punctuation">.</span>mp <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getMP</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> state<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">restorePlayerState</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">,</span> <span class="token keyword">const</span> PlayerState<span class="token operator">&amp;</span> state<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">setPosition</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>position<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">setRotation</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>rotation<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">setHP</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>hp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">setMP</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>mp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">showLoadingScreen</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">sendToClient</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> <span class="token string">&quot;onShowLoadingScreen&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">hideLoadingScreen</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">sendToClient</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> <span class="token string">&quot;onHideLoadingScreen&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-场景管理" tabindex="-1"><a class="header-anchor" href="#四、kbengine-场景管理"><span>四、KBEngine 场景管理</span></a></h2><h3 id="_4-1-kbengine-space-管理" tabindex="-1"><a class="header-anchor" href="#_4-1-kbengine-space-管理"><span>4.1 KBEngine Space 管理</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine Space 管理</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># scripts/spaces/space_base.py</span></span>
<span class="line"><span class="token keyword">import</span> KBEngine</span>
<span class="line"><span class="token keyword">from</span> KBEDedef <span class="token keyword">import</span> <span class="token operator">*</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SpaceBase</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Space<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>Space<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># Space 数据</span></span>
<span class="line">        self<span class="token punctuation">.</span>spaceID <span class="token operator">=</span> <span class="token builtin">id</span></span>
<span class="line">        self<span class="token punctuation">.</span>spaceName <span class="token operator">=</span> <span class="token string">&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>entityCount <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line">        self<span class="token punctuation">.</span>maxEntities <span class="token operator">=</span> <span class="token number">100</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># Space 配置</span></span>
<span class="line">        self<span class="token punctuation">.</span>isPVP <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line">        self<span class="token punctuation">.</span>isInstance <span class="token operator">=</span> <span class="token boolean">False</span></span>
<span class="line">        self<span class="token punctuation">.</span>cell <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onEnter</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;实体进入 Space&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>entityCount <span class="token operator">+=</span> <span class="token number">1</span></span>
<span class="line">        INFO<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>entity<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> entered space </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>spaceID<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        entity<span class="token punctuation">.</span>onEnterSpace<span class="token punctuation">(</span>self<span class="token punctuation">.</span>spaceID<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onLeave</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;实体离开 Space&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>entityCount <span class="token operator">-=</span> <span class="token number">1</span></span>
<span class="line">        INFO<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Entity </span><span class="token interpolation"><span class="token punctuation">{</span>entity<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">}</span></span><span class="token string"> left space </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>spaceID<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        entity<span class="token punctuation">.</span>onLeaveSpace<span class="token punctuation">(</span>self<span class="token punctuation">.</span>spaceID<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">isFull</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查 Space 是否已满&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">return</span> self<span class="token punctuation">.</span>entityCount <span class="token operator">&gt;=</span> self<span class="token punctuation">.</span>maxEntities</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">getEntitiesInRange</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> position<span class="token punctuation">,</span> <span class="token builtin">range</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;获取范围内的实体&quot;&quot;&quot;</span></span>
<span class="line">        entities <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line">        <span class="token keyword">for</span> entityID<span class="token punctuation">,</span> entity <span class="token keyword">in</span> KBEngine<span class="token punctuation">.</span>entities<span class="token punctuation">.</span>items<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token builtin">hasattr</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> <span class="token string">&#39;spaceID&#39;</span><span class="token punctuation">)</span> <span class="token keyword">and</span> entity<span class="token punctuation">.</span>spaceID <span class="token operator">==</span> self<span class="token punctuation">.</span>spaceID<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">if</span> entity<span class="token punctuation">.</span>position<span class="token punctuation">.</span>distanceTo<span class="token punctuation">(</span>position<span class="token punctuation">)</span> <span class="token operator">&lt;=</span> <span class="token builtin">range</span><span class="token punctuation">:</span></span>
<span class="line">                    entities<span class="token punctuation">.</span>append<span class="token punctuation">(</span>entity<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> entities</span>
<span class="line"></span>
<span class="line"><span class="token comment"># scripts/spaces/space_instance.py</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SpaceInstance</span><span class="token punctuation">(</span>SpaceBase<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        SpaceBase<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        self<span class="token punctuation">.</span>isInstance <span class="token operator">=</span> <span class="token boolean">True</span></span>
<span class="line">        self<span class="token punctuation">.</span>teamID <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line">        self<span class="token punctuation">.</span>ownerID <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line">        self<span class="token punctuation">.</span>createTime <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">initialize</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> teamID<span class="token punctuation">,</span> mapID<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;初始化副本&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>teamID <span class="token operator">=</span> teamID</span>
<span class="line">        self<span class="token punctuation">.</span>createTime <span class="token operator">=</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 创建 CellApp</span></span>
<span class="line">        self<span class="token punctuation">.</span>createCell<span class="token punctuation">(</span>mapID<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">createCell</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> mapID<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;创建 Cell&quot;&quot;&quot;</span></span>
<span class="line">        self<span class="token punctuation">.</span>cell <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>createEntityAnywhere<span class="token punctuation">(</span>Cell<span class="token punctuation">,</span> <span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>cell<span class="token punctuation">.</span>spaceID <span class="token operator">=</span> self<span class="token punctuation">.</span>spaceID</span>
<span class="line">        self<span class="token punctuation">.</span>cell<span class="token punctuation">.</span>mapID <span class="token operator">=</span> mapID</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onAllPlayersLeft</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;所有玩家离开副本&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 延迟销毁副本</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>addTimer<span class="token punctuation">(</span><span class="token number">30</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>destroy<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">destroy</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;销毁副本&quot;&quot;&quot;</span></span>
<span class="line">        INFO<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Destroying instance space </span><span class="token interpolation"><span class="token punctuation">{</span>self<span class="token punctuation">.</span>spaceID<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 销毁所有实体</span></span>
<span class="line">        <span class="token keyword">for</span> entityID <span class="token keyword">in</span> self<span class="token punctuation">.</span>getEntities<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            KBEngine<span class="token punctuation">.</span>destroyEntity<span class="token punctuation">(</span>entityID<span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 销毁自己</span></span>
<span class="line">        KBEngine<span class="token punctuation">.</span>destroyEntity<span class="token punctuation">(</span>self<span class="token punctuation">.</span>spaceID<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、最佳实践" tabindex="-1"><a class="header-anchor" href="#五、最佳实践"><span>五、最佳实践</span></a></h2><h3 id="_5-1-场景管理设计建议" tabindex="-1"><a class="header-anchor" href="#_5-1-场景管理设计建议"><span>5.1 场景管理设计建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>异步加载</strong></td><td>后台加载场景资源</td></tr><tr><td><strong>资源池化</strong></td><td>复用场景资源</td></tr><tr><td><strong>分层卸载</strong></td><td>优先卸载远处场景</td></tr><tr><td><strong>状态保存</strong></td><td>场景切换保存状态</td></tr><tr><td><strong>预加载</strong></td><td>预加载可能进入的场景</td></tr></tbody></table><h3 id="_5-2-性能优化" tabindex="-1"><a class="header-anchor" href="#_5-2-性能优化"><span>5.2 性能优化</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">优化策略:</span>
<span class="line">1. 场景资源池化共享</span>
<span class="line">2. LOD 分级加载</span>
<span class="line">3. 异步场景切换</span>
<span class="line">4. 增量场景更新</span>
<span class="line">5. 对象池管理实体</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、总结" tabindex="-1"><a class="header-anchor" href="#六、总结"><span>六、总结</span></a></h2><h3 id="场景管理核心" tabindex="-1"><a class="header-anchor" href="#场景管理核心"><span>场景管理核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">场景管理 = 场景加载 + 实体管理 + 场景切换</span>
<span class="line">- 按需加载场景资源</span>
<span class="line">- 管理场景中的实体</span>
<span class="line">- 平滑的场景切换</span>
<span class="line">- AOI 优化同步范围</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://docs.unity3d.com/" target="_blank" rel="noopener noreferrer">Unity 场景管理</a></li><li><a href="https://docs.unrealengine.com/" target="_blank" rel="noopener noreferrer">Unreal World Partition</a></li><li><a href="https://kbengine.github.io/docs/" target="_blank" rel="noopener noreferrer">KBEngine Space 文档</a></li></ul>`,37)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};