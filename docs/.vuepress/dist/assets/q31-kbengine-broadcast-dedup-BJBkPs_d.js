import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q31-kbengine-broadcast-dedup.html","title":"Q31: KBEngine 如何高效广播？如何保证消息不重复？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q31-kbengine-broadcast-dedup.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q31-kbengine-broadcast-dedup.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q31-kbengine-如何高效广播-如何保证消息不重复" tabindex="-1"><a class="header-anchor" href="#q31-kbengine-如何高效广播-如何保证消息不重复"><span>Q31: KBEngine 如何高效广播？如何保证消息不重复？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对 KBEngine <strong>AOI 广播机制</strong> 和 <strong>消息去重策略</strong> 的理解：</p><ul><li>AOI 的实现方式（三轴十字链表）</li><li>ViewEntity 和 Witness 机制</li><li>消息广播的去重策略</li></ul><hr><h2 id="一、kbengine-的-aoi-实现" tabindex="-1"><a class="header-anchor" href="#一、kbengine-的-aoi-实现"><span>一、KBEngine 的 AOI 实现</span></a></h2><h3 id="三轴十字链表" tabindex="-1"><a class="header-anchor" href="#三轴十字链表"><span>三轴十字链表</span></a></h3><p>根据 <a href="https://www.cnblogs.com/coding-my-life/p/14256640.html" target="_blank" rel="noopener noreferrer">KBEngine 源码分析</a>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 的 AOI 实现：三轴十字链表</span>
<span class="line"></span>
<span class="line">传统十字链表（2D）：</span>
<span class="line">     Y</span>
<span class="line">     ↑</span>
<span class="line">     │</span>
<span class="line">  ───┼─── X</span>
<span class="line">     │</span>
<span class="line"></span>
<span class="line">KBEngine 三轴十字链表（3D）：</span>
<span class="line">     Y</span>
<span class="line">     ↑</span>
<span class="line">     │</span>
<span class="line">  ───┼─── X</span>
<span class="line">   ╱</span>
<span class="line">  ↓ Z</span>
<span class="line"></span>
<span class="line">每个 Entity 维护三条链表：</span>
<span class="line">- pXXX_：X 轴方向的邻居</span>
<span class="line">- pYXX_：Y 轴方向的邻居</span>
<span class="line">- pZXX_：Z 轴方向的邻居</span>
<span class="line"></span>
<span class="line">查找附近的 Entity：</span>
<span class="line">1. 遍历三条链表</span>
<span class="line">2. 计算距离</span>
<span class="line">3. 判断是否在视野范围内</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="coordinatesystem-核心" tabindex="-1"><a class="header-anchor" href="#coordinatesystem-核心"><span>CoordinateSystem 核心</span></a></h3><p>根据源码分析，<code>CoordinateSystem</code> 是 AOI 的核心：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// coordinate_system.h 核心结构</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CoordinateSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 三轴链表节点</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">CoordinateNode</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> pEntity<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 三条链表指针</span></span>
<span class="line">        CoordinateNode<span class="token operator">*</span> pXNext_<span class="token punctuation">;</span>   <span class="token comment">// X 轴正向</span></span>
<span class="line">        CoordinateNode<span class="token operator">*</span> pXPrev_<span class="token punctuation">;</span>   <span class="token comment">// X 轴负向</span></span>
<span class="line">        CoordinateNode<span class="token operator">*</span> pYNext_<span class="token punctuation">;</span>   <span class="token comment">// Y 轴正向</span></span>
<span class="line">        CoordinateNode<span class="token operator">*</span> pYPrev_<span class="token punctuation">;</span>   <span class="token comment">// Y 轴负向</span></span>
<span class="line">        CoordinateNode<span class="token operator">*</span> pZNext_<span class="token punctuation">;</span>   <span class="token comment">// Z 轴正向</span></span>
<span class="line">        CoordinateNode<span class="token operator">*</span> pZPrev_<span class="token punctuation">;</span>   <span class="token comment">// Z 轴负向</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">float</span> x<span class="token punctuation">,</span> y<span class="token punctuation">,</span> z<span class="token punctuation">;</span>            <span class="token comment">// 3D 坐标</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 插入节点（Entity 进入世界）</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">insert</span><span class="token punctuation">(</span>CoordinateNode<span class="token operator">*</span> pNode<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新节点位置</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span>CoordinateNode<span class="token operator">*</span> pNode<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、viewentity-和-witness-机制" tabindex="-1"><a class="header-anchor" href="#二、viewentity-和-witness-机制"><span>二、ViewEntity 和 Witness 机制</span></a></h2><h3 id="视野管理架构" tabindex="-1"><a class="header-anchor" href="#视野管理架构"><span>视野管理架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      KBEngine 视野管理                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                 │</span>
<span class="line">│  Entity (观察者)                                                │</span>
<span class="line">│  │                                                              │</span>
<span class="line">│  │  ViewEntity (被观察者列表)                                   │</span>
<span class="line">│  │  │  ┌──────────┐  ┌──────────┐  ┌──────────┐               │</span>
<span class="line">│  │  │  Witness1 │  Witness2 │  Witness3 │  (能看到我的)       │</span>
<span class="line">│  │  │  (玩家A)  │  (玩家B)  │  (NPC)    │                   │</span>
<span class="line">│  │  │  ┌──────┐  │  ┌──────┐  │  ┌──────┐                   │</span>
<span class="line">│  │  │  │ViewEntities│  │ViewEntities│  │ViewEntities│         │</span>
<span class="line">│  │  │  └──────┘  │  └──────┘  │  └──────┘                   │</span>
<span class="line">│  │  └──────────┘  └──────────┘  └──────────┘               │</span>
<span class="line">│  │                                                              │</span>
<span class="line">│  │  当 Entity 状态变化时：                                     │</span>
<span class="line">│  │  1. 遍历 ViewEntity 中的所有 Witness                       │</span>
<span class="line">│  │  2. 向每个 Witness 对应的客户端发送消息                      │</span>
<span class="line">│  │  3. 自动去重（同一个客户端只发一次）                         │</span>
<span class="line">│                                                                 │</span>
<span class="line">└─────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="witness-生命周期" tabindex="-1"><a class="header-anchor" href="#witness-生命周期"><span>Witness 生命周期</span></a></h3><p>根据 <a href="https://imgamer.gitbooks.io/kbengine-overview/content/content/6_1ServerComponents.html" target="_blank" rel="noopener noreferrer">KBEngine 技术概览</a>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ViewTrigger 处理视野进入/离开事件</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ViewTrigger</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">RangeTrigger</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// Entity 进入视野</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onEnter</span><span class="token punctuation">(</span>CoordinateNode<span class="token operator">*</span> pNode<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> pEntity <span class="token operator">=</span> pNode<span class="token operator">-&gt;</span><span class="token function">getEntity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知观察者</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pWitness_ <span class="token operator">!=</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            pWitness_<span class="token operator">-&gt;</span><span class="token function">onEnterWitness</span><span class="token punctuation">(</span>pEntity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Entity 离开视野</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onLeave</span><span class="token punctuation">(</span>CoordinateNode<span class="token operator">*</span> pNode<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> pEntity <span class="token operator">=</span> pNode<span class="token operator">-&gt;</span><span class="token function">getEntity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知观察者</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pWitness_ <span class="token operator">!=</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            pWitness_<span class="token operator">-&gt;</span><span class="token function">onLeaveWitness</span><span class="token punctuation">(</span>pEntity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    Witness<span class="token operator">*</span> pWitness_<span class="token punctuation">;</span>  <span class="token comment">// 关联的 Witness</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="onenterwitness-onleavewitness" tabindex="-1"><a class="header-anchor" href="#onenterwitness-onleavewitness"><span>onEnterWitness / onLeaveWitness</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Python 脚本中定义的回调</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Account</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onEnterWitness</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        当 Entity 进入我的视野时触发</span>
<span class="line">        entity: 进入视野的 Entity</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> entity<span class="token punctuation">.</span>isPlayer<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>addWitness<span class="token punctuation">(</span>entity<span class="token punctuation">)</span>  <span class="token comment"># 添加到 ViewEntity</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onLeaveWitness</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> entity<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        当 Entity 离开我的视野时触发</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> entity<span class="token punctuation">.</span>isPlayer<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>delWitness<span class="token punctuation">(</span>entity<span class="token punctuation">)</span>  <span class="token comment"># 从 ViewEntity 移除</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、广播去重机制" tabindex="-1"><a class="header-anchor" href="#三、广播去重机制"><span>三、广播去重机制</span></a></h2><h3 id="去重策略-1-viewentity-自动去重" tabindex="-1"><a class="header-anchor" href="#去重策略-1-viewentity-自动去重"><span>去重策略 1：ViewEntity 自动去重</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">ViewEntity 的本质：一个客户端对应一个 Witness</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│  Server CellApp                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Entity_Monster                                            │</span>
<span class="line">│  │                                                          │</span>
<span class="line">│  │  ViewEntity (能看到我的客户端列表)                        │</span>
<span class="line">│  │  ┌─────────────────────────────────────────────┐         │</span>
<span class="line">│  │  │ Witness_1 → Client_A (ID=1001)            │         │</span>
<span class="line">│  │  │ Witness_2 → Client_B (ID=1002)            │         │</span>
<span class="line">│  │  │ Witness_3 → Client_C (ID=1003)            │         │</span>
<span class="line">│  │  └─────────────────────────────────────────────┘         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  广播时：                                                    │</span>
<span class="line">│  for witness in ViewEntity:                                 │</span>
<span class="line">│      witness.sendToClient(message)  ← 每个客户端只发一次      │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">关键：同一个客户端只会被添加一次到 ViewEntity</span>
<span class="line">所以自动去重！</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="去重策略-2-坐标系层级" tabindex="-1"><a class="header-anchor" href="#去重策略-2-坐标系层级"><span>去重策略 2：坐标系层级</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">三轴十字链表的层级关系：</span>
<span class="line"></span>
<span class="line">Z 轴分层 (简化为 2D 俯视图)：</span>
<span class="line"></span>
<span class="line">     Z (前)</span>
<span class="line">     ↑</span>
<span class="line">     │</span>
<span class="line">  ┌───┼───┐</span>
<span class="line">  │ 1 │ 2 │</span>
<span class="line">  ├───┼───┤</span>
<span class="line">  │ 3 │ 4 │</span>
<span class="line">  └───┴───┘</span>
<span class="line">     │</span>
<span class="line">     └───→ X (右)</span>
<span class="line"></span>
<span class="line">Entity A 在格子 (1, 1) 附近的 Entity：</span>
<span class="line">- 遍历 X 轴链表：找到 X 方向的邻居</span>
<span class="line">- 遍历 Y 轴链表：找到 Y 方向的邻居</span>
<span class="line">- 遍历 Z 轴链表：找到 Z 方向的邻居</span>
<span class="line">- 取并集后去重（通过 EntityID）</span>
<span class="line"></span>
<span class="line">去重方式：</span>
<span class="line">for entity in x_axis + y_axis + z_axis:</span>
<span class="line">    if entity.id not in seen:</span>
<span class="line">        seen.add(entity.id)</span>
<span class="line">        process(entity)</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="去重策略-3-帧标记" tabindex="-1"><a class="header-anchor" href="#去重策略-3-帧标记"><span>去重策略 3：帧标记</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// entity.h 中的去重标记</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 每帧递增的 ID</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">uint32_t</span> g_viewEntityCacheID<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 每个 Entity 记录最后一次处理的帧 ID</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> viewEntityCacheID_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 检查是否已处理</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isViewEntityCached</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> cacheID<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> viewEntityCacheID_ <span class="token operator">==</span> cacheID<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 标记为已处理</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">markViewEntityCached</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> cacheID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        viewEntityCacheID_ <span class="token operator">=</span> cacheID<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line"><span class="token keyword">uint32_t</span> cacheID <span class="token operator">=</span> <span class="token operator">++</span>g_viewEntityCacheID<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">:</span> nearbyEntities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>entity<span class="token operator">-&gt;</span><span class="token function">isViewEntityCached</span><span class="token punctuation">(</span>cacheID<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">markViewEntityCached</span><span class="token punctuation">(</span>cacheID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 只处理未缓存的 Entity</span></span>
<span class="line">        <span class="token function">addToViewEntity</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、高效广播策略" tabindex="-1"><a class="header-anchor" href="#四、高效广播策略"><span>四、高效广播策略</span></a></h2><h3 id="策略-1-只广播视野内的变化" tabindex="-1"><a class="header-anchor" href="#策略-1-只广播视野内的变化"><span>策略 1：只广播视野内的变化</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">高效广播的核心原则：只发送必要的数据</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│  Entity_Player 的状态变化：                                   │</span>
<span class="line">│  │                                                          │</span>
<span class="line">│  │  位置变化 (每帧)                                         │</span>
<span class="line">│  │  → 只广播给视野内的玩家                                  │</span>
<span class="line">│  │                                                          │</span>
<span class="line">│  │  血量变化 (受伤时)                                       │</span>
<span class="line">│  │  → 只广播给视野内的玩家                                  │</span>
<span class="line">│  │                                                          │</span>
<span class="line">│  │  背包变化 (获得物品)                                     │</span>
<span class="line">│  │  → 不广播（不是视野内关注的数据）                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="策略-2-增量广播" tabindex="-1"><a class="header-anchor" href="#策略-2-增量广播"><span>策略 2：增量广播</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 每个Entity维护脏标记</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 属性脏标记</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> dirtyFlags_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 各属性对应位</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">DirtyFlags</span> <span class="token punctuation">{</span></span>
<span class="line">        DIRTY_POSITION <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">        DIRTY_ROTATION <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">        DIRTY_HP <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">2</span><span class="token punctuation">,</span></span>
<span class="line">        DIRTY_MP <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">3</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token comment">// ...</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setPosition</span><span class="token punctuation">(</span><span class="token keyword">float</span> x<span class="token punctuation">,</span> <span class="token keyword">float</span> y<span class="token punctuation">,</span> <span class="token keyword">float</span> z<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        x_ <span class="token operator">=</span> x<span class="token punctuation">;</span> y_ <span class="token operator">=</span> y<span class="token punctuation">;</span> z_ <span class="token operator">=</span> z<span class="token punctuation">;</span></span>
<span class="line">        dirtyFlags_ <span class="token operator">|=</span> DIRTY_POSITION<span class="token punctuation">;</span>  <span class="token comment">// 标记脏</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播时只发送变化的属性</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcastToWitnesses</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>dirtyFlags_ <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span>  <span class="token comment">// 无变化，不广播</span></span>
<span class="line"></span>
<span class="line">        MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>dirtyFlags_ <span class="token operator">&amp;</span> DIRTY_POSITION<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>x_<span class="token punctuation">)</span><span class="token punctuation">;</span> stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>y_<span class="token punctuation">)</span><span class="token punctuation">;</span> stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>z_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>dirtyFlags_ <span class="token operator">&amp;</span> DIRTY_HP<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>hp_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token comment">// ...</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送给所有 Witness</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> witness <span class="token operator">:</span> viewEntities_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            witness<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        dirtyFlags_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>  <span class="token comment">// 清除脏标记</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="策略-3-频率控制" tabindex="-1"><a class="header-anchor" href="#策略-3-频率控制"><span>策略 3：频率控制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 不同属性使用不同的广播频率</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BroadcastController</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 高频（每帧）：位置</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> POSITION_RATE <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>  <span class="token comment">// 每帧</span></span>
<span class="line">    <span class="token comment">// 中频（每 5 帧）：朝向、速度</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> ROTATION_RATE <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token comment">// 低频（每 10 帧）：血量、状态</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> HP_RATE <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        frameCount_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>frameCount_ <span class="token operator">%</span> POSITION_RATE <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">broadcastPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>frameCount_ <span class="token operator">%</span> ROTATION_RATE <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">broadcastRotation</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>frameCount_ <span class="token operator">%</span> HP_RATE <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">broadcastHP</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、完整的广播流程" tabindex="-1"><a class="header-anchor" href="#五、完整的广播流程"><span>五、完整的广播流程</span></a></h2><h3 id="entity-移动时的广播" tabindex="-1"><a class="header-anchor" href="#entity-移动时的广播"><span>Entity 移动时的广播</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">玩家 A 移动时的完整流程：</span>
<span class="line"></span>
<span class="line">1. 客户端发送移动请求</span>
<span class="line">   Client → Server: &quot;Move to (100, 0, 100)&quot;</span>
<span class="line"></span>
<span class="line">2. 服务端更新位置</span>
<span class="line">   Entity_Player: position_ = (100, 0, 100)</span>
<span class="line">   markDirty(DIRTY_POSITION)</span>
<span class="line"></span>
<span class="line">3. AOI 检测（CoordinateSystem）</span>
<span class="line">   - 更新三轴十字链表位置</span>
<span class="line">   - 检测是否有新的 Entity 进入/离开视野</span>
<span class="line"></span>
<span class="line">4. 视野变化处理（ViewTrigger）</span>
<span class="line">   for newEntity in enteredEntities:</span>
<span class="line">       onEnterWitness(newEntity)</span>
<span class="line">           → 添加到 ViewEntity</span>
<span class="line">           → 发送 &quot;EntityEnter&quot; 消息给客户端</span>
<span class="line"></span>
<span class="line">   for oldEntity in leftEntities:</span>
<span class="line">       onLeaveWitness(oldEntity)</span>
<span class="line">           → 从 ViewEntity 移除</span>
<span class="line">           → 发送 &quot;EntityLeave&quot; 消息给客户端</span>
<span class="line"></span>
<span class="line">5. 状态广播（只给 ViewEntity 中的 Witness）</span>
<span class="line">   for witness in ViewEntity:</span>
<span class="line">       if (dirtyFlags_ != 0):</span>
<span class="line">           witness.sendChanges(dirtyFlags_)</span>
<span class="line"></span>
<span class="line">   dirtyFlags_ = 0</span>
<span class="line"></span>
<span class="line">关键：只给 ViewEntity 中的 Witness 广播，自动避免重复</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="多人广播去重图解" tabindex="-1"><a class="header-anchor" href="#多人广播去重图解"><span>多人广播去重图解</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">场景：玩家 A、B、C 互相在视野内</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   Entity_A          Entity_B          Entity_C              │</span>
<span class="line">│      │                │                │                    │</span>
<span class="line">│      │                │                │                    │</span>
<span class="line">│      └────────────────┼────────────────┘                    │</span>
<span class="line">│                       │                                     │</span>
<span class="line">│                  &quot;Move to (100, 0, 100)&quot;                    │</span>
<span class="line">│                       │                                     │</span>
<span class="line">│                       ▼                                     │</span>
<span class="line">│              Server 处理移动                                 │</span>
<span class="line">│                       │                                     │</span>
<span class="line">│              ┌─────────────────────┐                        │</span>
<span class="line">│              │  AOI 检测          │                        │</span>
<span class="line">│              │  位置更新          │                        │</span>
<span class="line">│              │  脏标记设置        │                        │</span>
<span class="line">│              └─────────────────────┘                        │</span>
<span class="line">│                       │                                     │</span>
<span class="line">│        ┌──────────────┼──────────────┐                      │</span>
<span class="line">│        ▼              ▼              ▼                      │</span>
<span class="line">│  ViewEntity_A    ViewEntity_B    ViewEntity_C               │</span>
<span class="line">│  ├─ Witness_B,C  ├─ Witness_A,C  ├─ Witness_A,B            │</span>
<span class="line">│        │              │              │                      │</span>
<span class="line">│        ▼              ▼              ▼                      │</span>
<span class="line">│    Client_B      Client_A      Client_C                     │</span>
<span class="line">│    (收到A)        (收到B)        (收到A,B)                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  每个客户端只收到一次消息！                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、性能优化技巧" tabindex="-1"><a class="header-anchor" href="#六、性能优化技巧"><span>六、性能优化技巧</span></a></h2><h3 id="_1-视野半径控制" tabindex="-1"><a class="header-anchor" href="#_1-视野半径控制"><span>1. 视野半径控制</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 不同类型的 Entity 使用不同的视野半径</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">NPC</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>view_radius <span class="token operator">=</span> <span class="token number">50</span>  <span class="token comment"># NPC 视野小</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Player</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>view_radius <span class="token operator">=</span> <span class="token number">100</span>  <span class="token comment"># 玩家视野大</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Monster</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>view_radius <span class="token operator">=</span> <span class="token number">30</span>  <span class="token comment"># 怪物视野最小</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-分层广播" tabindex="-1"><a class="header-anchor" href="#_2-分层广播"><span>2. 分层广播</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">根据消息优先级分层：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  高优先级通道（可靠，立即）                                   │</span>
<span class="line">│  ├── 战斗伤害                                               │</span>
<span class="line">│  ├── 血量变化                                               │</span>
<span class="line">│  └── 死亡事件                                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│  中优先级通道（可靠，延迟）                                   │</span>
<span class="line">│  ├── 聊天消息                                               │</span>
<span class="line">│  ├── 技能释放                                               │</span>
<span class="line">│  └── 状态变化                                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│  低优先级通道（可丢，批量）                                   │</span>
<span class="line">│  ├── 位置更新                                               │</span>
<span class="line">│  ├── 动画状态                                               │</span>
<span class="line">│  └── 装饰变化                                               │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-批量打包" tabindex="-1"><a class="header-anchor" href="#_3-批量打包"><span>3. 批量打包</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 将多个小消息打包成一个包发送</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MessageBatcher</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Message<span class="token operator">&gt;</span> pending_<span class="token punctuation">;</span></span>
<span class="line">    size_t batchSize_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> Message<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        pending_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pending_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;=</span> batchSize_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">flush</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">flush</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pending_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        BatchMessage batch<span class="token punctuation">;</span></span>
<span class="line">        batch<span class="token punctuation">.</span>messages <span class="token operator">=</span> pending_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> witness <span class="token operator">:</span> viewEntities_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            witness<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>batch<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        pending_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、常见问题" tabindex="-1"><a class="header-anchor" href="#七、常见问题"><span>七、常见问题</span></a></h2><h3 id="q1-如果两个-entity-同时移动-会重复广播吗" tabindex="-1"><a class="header-anchor" href="#q1-如果两个-entity-同时移动-会重复广播吗"><span>Q1: 如果两个 Entity 同时移动，会重复广播吗？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">不会！</span>
<span class="line"></span>
<span class="line">每个 Entity 独立维护自己的 ViewEntity。</span>
<span class="line">Entity A 的 ViewEntity 只包含能看到 A 的客户端。</span>
<span class="line">Entity B 的 ViewEntity 只包含能看到 B 的客户端。</span>
<span class="line"></span>
<span class="line">广播时：</span>
<span class="line">- Entity A 只向 ViewEntity_A 中的 Witness 发送</span>
<span class="line">- Entity B 只向 ViewEntity_B 中的 Witness 发送</span>
<span class="line"></span>
<span class="line">没有交集，自然没有重复。</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="q2-客户端如何知道哪些-entity-在视野内" tabindex="-1"><a class="header-anchor" href="#q2-客户端如何知道哪些-entity-在视野内"><span>Q2: 客户端如何知道哪些 Entity 在视野内？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">服务端主动通知！</span>
<span class="line"></span>
<span class="line">1. Entity 进入视野</span>
<span class="line">   Server → Client: &quot;onEnterWitness(entityID, entityType, position)&quot;</span>
<span class="line">   → 客户端创建 Entity 显示</span>
<span class="line"></span>
<span class="line">2. Entity 离开视野</span>
<span class="line">   Server → Client: &quot;onLeaveWitness(entityID)&quot;</span>
<span class="line">   → 客户端销毁 Entity</span>
<span class="line"></span>
<span class="line">3. Entity 状态更新</span>
<span class="line">   Server → Client: &quot;onUpdatePosition(entityID, x, y, z)&quot;</span>
<span class="line">   → 客户端更新位置</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="q3-如何避免视野边界频繁进出" tabindex="-1"><a class="header-anchor" href="#q3-如何避免视野边界频繁进出"><span>Q3: 如何避免视野边界频繁进出？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">使用滞后边界：</span>
<span class="line"></span>
<span class="line">实际视野半径: 100m</span>
<span class="line">添加边界:    110m</span>
<span class="line">移除边界:    90m</span>
<span class="line"></span>
<span class="line">Entity 距离 105m → 还在视野内</span>
<span class="line">Entity 距离 95m  → 还在视野内</span>
<span class="line">Entity 距离 89m  → 离开视野</span>
<span class="line"></span>
<span class="line">避免在 100m 边界上频繁进出/退出</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、参考资料" tabindex="-1"><a class="header-anchor" href="#八、参考资料"><span>八、参考资料</span></a></h2><ul><li><a href="https://www.cnblogs.com/coding-my-life/p/14256640.html" target="_blank" rel="noopener noreferrer">游戏服务器 AOI 的实现 - coding my life</a></li><li><a href="https://zhuanlan.zhihu.com/p/697158275" target="_blank" rel="noopener noreferrer">网络游戏同步技术二：状态同步的优化与实现</a></li><li><a href="https://imgamer.gitbooks.io/kbengine-overview/content/content/6_1ServerComponents.html" target="_blank" rel="noopener noreferrer">KBEngine 技术概览 - 服务器组件</a></li><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/view_trigger.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - view_trigger.h</a></li><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/coordinate_system.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - coordinate_system.h</a></li></ul>`,62)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};