import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q30-kbengine-space-concept.html","title":"Q30: KBEngine 的 Space 是什么？与物理空间划分有什么区别？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q30-kbengine-space-concept.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q30-kbengine-space-concept.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q30-kbengine-的-space-是什么-与物理空间划分有什么区别" tabindex="-1"><a class="header-anchor" href="#q30-kbengine-的-space-是什么-与物理空间划分有什么区别"><span>Q30: KBEngine 的 Space 是什么？与物理空间划分有什么区别？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对 <strong>KBEngine Space 概念</strong> 的准确理解：</p><ul><li>Space 的真实定义和作用</li><li>Space 与物理坐标的关系</li><li>Space 与 CoordinateSystem 的区别</li><li>多 Space 管理机制</li></ul><hr><h2 id="一、space-的真实定义" tabindex="-1"><a class="header-anchor" href="#一、space-的真实定义"><span>一、Space 的真实定义</span></a></h2><h3 id="官方定义" tabindex="-1"><a class="header-anchor" href="#官方定义"><span>官方定义</span></a></h3><p>根据 <a href="https://www.kbelab.com/guide/space/" target="_blank" rel="noopener noreferrer">KBEngine Lab 官方文档</a>：</p><blockquote><p><strong>Space 是一个抽象概念，只存在于 CellApp 的内存中</strong></p><p>Space 的具体含义由用户定义，可以是：</p><ul><li>一个游戏场景（如新手村、主城）</li><li>一个副本实例（如地下城）</li><li>一个房间（如战场房间）</li><li>任何逻辑上的空间分组</li></ul></blockquote><h3 id="关键特性" tabindex="-1"><a class="header-anchor" href="#关键特性"><span>关键特性</span></a></h3><table><thead><tr><th>特性</th><th>说明</th></tr></thead><tbody><tr><td><strong>抽象概念</strong></td><td>Space 不是物理空间划分，是逻辑分组</td></tr><tr><td><strong>内存存在</strong></td><td>只存在于 CellApp 内存，不持久化</td></tr><tr><td><strong>用户定义</strong></td><td>开发者决定 Space 的含义</td></tr><tr><td><strong>独立管理</strong></td><td>每个 Space 独立管理其中的 Entity</td></tr><tr><td><strong>多 Space</strong></td><td>一个 CellApp 可以有多个 Space</td></tr></tbody></table><h3 id="源码证据" tabindex="-1"><a class="header-anchor" href="#源码证据"><span>源码证据</span></a></h3><p>根据 <a href="https://blog.csdn.net/kbengine/article/details/78327185" target="_blank" rel="noopener noreferrer">KBEngine 源码分析</a>：</p><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># spaces.py - KBEngine 默认脚本</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">initAlloc</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    创建 Space 实体的示例</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token comment"># 创建不同类型的 Space</span></span>
<span class="line">    createSpace<span class="token punctuation">(</span><span class="token string">&quot;Newbie&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;SpaceNewbie&quot;</span><span class="token punctuation">)</span>     <span class="token comment"># 新手村</span></span>
<span class="line">    createSpace<span class="token punctuation">(</span><span class="token string">&quot;MainCity&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;SpaceCity&quot;</span><span class="token punctuation">)</span>      <span class="token comment"># 主城</span></span>
<span class="line">    createSpace<span class="token punctuation">(</span><span class="token string">&quot;Dungeon&quot;</span><span class="token punctuation">,</span> <span class="token string">&quot;SpaceDungeon&quot;</span><span class="token punctuation">)</span>    <span class="token comment"># 副本</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// space.h 核心定义</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Space</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Entity</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// Space 是一种特殊的 Entity</span></span>
<span class="line">    <span class="token comment">// 由 CellApp 管理，不直接暴露给客户端</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 几何映射（用于导航、寻路）</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">addSpaceGeometryMapping</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> path<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Space 边界（用于物理检测，不是空间划分）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setBounds</span><span class="token punctuation">(</span><span class="token keyword">const</span> AABB<span class="token operator">&amp;</span> bounds<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、space-vs-物理空间划分" tabindex="-1"><a class="header-anchor" href="#二、space-vs-物理空间划分"><span>二、Space vs 物理空间划分</span></a></h2><h3 id="常见误解" tabindex="-1"><a class="header-anchor" href="#常见误解"><span>常见误解</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">误解：CellApp 按物理坐标划分空间</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    错误的理解                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│   CellApp1                    CellApp2                       │</span>
<span class="line">│   ┌────────────┐              ┌────────────┐                 │</span>
<span class="line">│   │ X: [0,256)  │              │ X: [256,512) │                │</span>
<span class="line">│   │ Z: [0,256)  │              │ Z: [0,256)  │                 │</span>
<span class="line">│   │            │              │            │                 │</span>
<span class="line">│   │ 固定物理区域│              │ 固定物理区域│                 │</span>
<span class="line">│   └────────────┘              └────────────┘                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│   ✗ 这不是 KBEngine 的设计                                  │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="kbengine-的实际设计" tabindex="-1"><a class="header-anchor" href="#kbengine-的实际设计"><span>KBEngine 的实际设计</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">实际情况：CellApp 包含多个逻辑 Space</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      CellApp                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │</span>
<span class="line">│  │  Space A    │  │  Space B    │  │  Space C    │        │</span>
<span class="line">│  │  (新手村)    │  │  (副本1)    │  │  (副本2)    │        │</span>
<span class="line">│  │             │  │             │  │             │        │</span>
<span class="line">│  │ Entity1-100 │  │ Entity101-150│  │ Entity151-200│       │</span>
<span class="line">│  │             │  │             │  │             │        │</span>
<span class="line">│  │ 独立 AOI    │  │ 独立 AOI    │  │ 独立 AOI    │        │</span>
<span class="line">│  └─────────────┘  └─────────────┘  └─────────────┘        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │              CoordinateSystem                        │   │</span>
<span class="line">│  │        (用于所有 Space 的 AOI 计算)                  │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">✓ Space 是逻辑分组，不是物理区域划分</span>
<span class="line">✓ 一个 CellApp 可以有多个 Space</span>
<span class="line">✓ 每个 Space 独立管理 Entity 和 AOI</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、space-与-coordinatesystem-的区别" tabindex="-1"><a class="header-anchor" href="#三、space-与-coordinatesystem-的区别"><span>三、Space 与 CoordinateSystem 的区别</span></a></h2><p>这是两个完全不同的概念：</p><h3 id="对比表" tabindex="-1"><a class="header-anchor" href="#对比表"><span>对比表</span></a></h3><table><thead><tr><th>维度</th><th>Space</th><th>CoordinateSystem</th></tr></thead><tbody><tr><td><strong>用途</strong></td><td>逻辑分组（副本/场景）</td><td>AOI 计算</td></tr><tr><td><strong>数量</strong></td><td>一个 CellApp 多个</td><td>一个 CellApp 一个</td></tr><tr><td><strong>源码</strong></td><td>space.cpp/h</td><td>coordinate_system.cpp/h</td></tr><tr><td><strong>坐标</strong></td><td>可以有独立坐标系</td><td>3D 世界坐标系统</td></tr><tr><td><strong>持久化</strong></td><td>不持久化，重启消失</td><td>不持久化</td></tr><tr><td><strong>客户端</strong></td><td>客户端不知道 Space</td><td>客户端感知 AOI</td></tr></tbody></table><h3 id="coordinatesystem-的实际作用" tabindex="-1"><a class="header-anchor" href="#coordinatesystem-的实际作用"><span>CoordinateSystem 的实际作用</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/coordinate_system.cpp" target="_blank" rel="noopener noreferrer">coordinate_system.cpp 源码</a>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// CoordinateSystem 用于 AOI (感兴趣区域) 管理</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CoordinateSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 插入节点（Entity 进入世界）</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">insert</span><span class="token punctuation">(</span>CoordinateNode<span class="token operator">*</span> pNode<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新节点位置</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span>CoordinateNode<span class="token operator">*</span> pNode<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// XYZ 轴独立移动</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">moveNodeX</span><span class="token punctuation">(</span>CoordinateNode<span class="token operator">*</span> pNode<span class="token punctuation">,</span> <span class="token keyword">float</span> px<span class="token punctuation">,</span> CoordinateNode<span class="token operator">*</span> pCurrNode<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">moveNodeY</span><span class="token punctuation">(</span>CoordinateNode<span class="token operator">*</span> pNode<span class="token punctuation">,</span> <span class="token keyword">float</span> py<span class="token punctuation">,</span> CoordinateNode<span class="token operator">*</span> pCurrNode<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">moveNodeZ</span><span class="token punctuation">(</span>CoordinateNode<span class="token operator">*</span> pNode<span class="token punctuation">,</span> <span class="token keyword">float</span> pz<span class="token punctuation">,</span> CoordinateNode<span class="token operator">*</span> pCurrNode<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 使用分层空间划分算法（类似八叉树）</span></span>
<span class="line">    <span class="token comment">// 用于快速查找附近的 Entity</span></span>
<span class="line">    SpaceNodes _spaceNodes<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// CoordinateNode 是每个 Entity 在 AOI 系统中的节点</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CoordinateNode</span> <span class="token punctuation">{</span></span>
<span class="line">    Position3D pos<span class="token punctuation">;</span>      <span class="token comment">// 3D 坐标</span></span>
<span class="line">    Entity<span class="token operator">*</span> pEntity<span class="token punctuation">;</span>     <span class="token comment">// 关联的 Entity</span></span>
<span class="line">    <span class="token keyword">float</span> viewRadius<span class="token punctuation">;</span>    <span class="token comment">// 感知半径</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>CoordinateSystem 不负责 Space 划分</strong>，它负责：</p><ul><li>Entity 在 3D 世界中的位置管理</li><li>AOI 计算（查找附近的 Entity）</li><li>进入/离开视野事件</li></ul><hr><h2 id="四、space-的创建和管理" tabindex="-1"><a class="header-anchor" href="#四、space-的创建和管理"><span>四、Space 的创建和管理</span></a></h2><h3 id="space-创建流程" tabindex="-1"><a class="header-anchor" href="#space-创建流程"><span>Space 创建流程</span></a></h3>`,34),i(d,{code:`eJxt0MtKw0AYBeB9n2KWuugLZFHIRbJJRfAJhnYIgTYZ0/HSnYqLoJXWhaX1QkFpFW9RESKWvo3/NL6Fk5m2FJqswvlyZjhpkJ1d4leI5WE3xPUCEg/FIfMqHsU+QybCDQRvdzxKps/xCtsZ25iRfdxcQSdDJ3A9X6d0RY1MDdwgeWjqmZqkVstTS3Yto+yGBYlmsVSyNTTtj2FymcYJ/ziWuS1yR0Pp5AXaF0pl7ojcMjT099RK46P0awTtRIJlFFVFCY86cDpYVEQDomsY/6CtMDhQgw2Rm7q4Q946422KK0TyZsAICvZIKCZpiN8f8sGQ92LojCD5hNce3Dym8XCpYOqzA3+/z5VD1FfH8u779Ook71uIbuHhDFpdhWgNV6vyzSZBnbCwWcaUer67Pq8tj1kaOR8zm5H9f7ThM481C/9TJdvP`}),o[1]||=e(`<h3 id="space-负载均衡" tabindex="-1"><a class="header-anchor" href="#space-负载均衡"><span>Space 负载均衡</span></a></h3><p>根据 <a href="https://www.kbelab.com/guide/space/howtos/balance.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 负载均衡文档</a>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Space 创建的负载均衡策略：</span>
<span class="line"></span>
<span class="line">1. 默认：CellAppMgr 自动选择负载最低的 CellApp</span>
<span class="line">   ┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│   CellAppMgr 选择逻辑：                                          │</span>
<span class="line">│   ┌─────────────────────────────────────────────────────────┐  │</span>
<span class="line">│   │ 遍历所有 CellApp，选择：                                   │  │</span>
<span class="line">│   │ 1. 负载最低的（Entity 数量最少）                           │  │</span>
<span class="line">│   │ 2. 或者已经存在该 Space 的（复用）                         │  │</span>
<span class="line">│   └─────────────────────────────────────────────────────────┘  │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">2. 指定：创建到特定 CellApp</span>
<span class="line">   // 脚本配置</span>
<span class="line">   KBEngine.setAppFlags(KBEngine.APP_FLAGS_NOT_PARTicipate_BALANCE)</span>
<span class="line"></span>
<span class="line">   // 这样配置的 CellApp 不会参与自动负载均衡</span>
<span class="line">   // 只能显式指定 Space 创建到上面</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="指定-space-到特定-cellapp" tabindex="-1"><a class="header-anchor" href="#指定-space-到特定-cellapp"><span>指定 Space 到特定 CellApp</span></a></h3><p>根据 <a href="https://www.kbelab.com/guide/space/howtos/target.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 指定 Space 文档</a>：</p><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 配置不参与负载均衡</span></span>
<span class="line"><span class="token comment"># kbengine_defs.xml</span></span>
<span class="line"><span class="token operator">&lt;</span>cellapp<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>address<span class="token operator">&gt;</span>                            <span class="token comment"># 内网地址</span></span>
<span class="line">        <span class="token operator">&lt;</span>internalHost<span class="token operator">&gt;</span><span class="token number">192.168</span><span class="token number">.1</span><span class="token number">.10</span><span class="token operator">&lt;</span><span class="token operator">/</span>internalHost<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>externalAddress<span class="token operator">&gt;</span>                <span class="token comment"># 外网地址(如果有)</span></span>
<span class="line">            <span class="token operator">&lt;</span>externalHost<span class="token operator">&gt;</span><span class="token number">1.2</span><span class="token number">.3</span><span class="token number">.4</span><span class="token operator">&lt;</span><span class="token operator">/</span>externalHost<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span><span class="token operator">/</span>externalAddress<span class="token operator">&gt;</span></span>
<span class="line">        <span class="token operator">&lt;</span>port<span class="token operator">&gt;</span><span class="token number">20013</span><span class="token operator">&lt;</span><span class="token operator">/</span>port<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span><span class="token operator">/</span>address<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token comment"># 不参与负载均衡</span></span>
<span class="line">    <span class="token operator">&lt;</span>flags<span class="token operator">&gt;</span><span class="token number">0x00000010</span><span class="token operator">&lt;</span><span class="token operator">/</span>flags<span class="token operator">&gt;</span>            <span class="token comment"># APP_FLAGS_NOT_PARTicipate_BALANCE</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>cellapp<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 脚本中显式指定</span></span>
<span class="line">KBEngine<span class="token punctuation">.</span>createEntityAnywhere<span class="token punctuation">(</span></span>
<span class="line">    <span class="token string">&quot;Space&quot;</span><span class="token punctuation">,</span></span>
<span class="line">    spaceID<span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">{</span><span class="token string">&quot;cellapp&quot;</span><span class="token punctuation">:</span> specifiedCellAppID<span class="token punctuation">}</span>  <span class="token comment"># 指定 CellApp</span></span>
<span class="line"><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、space-的边界管理" tabindex="-1"><a class="header-anchor" href="#五、space-的边界管理"><span>五、Space 的边界管理</span></a></h2><h3 id="space-边界-vs-物理边界" tabindex="-1"><a class="header-anchor" href="#space-边界-vs-物理边界"><span>Space 边界 vs 物理边界</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// space.h 中的边界定义</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Space</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 设置几何边界（用于物理检测）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setBounds</span><span class="token punctuation">(</span><span class="token keyword">const</span> AABB<span class="token operator">&amp;</span> bounds<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        bounds_ <span class="token operator">=</span> bounds<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加几何映射（导航网格）</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">addSpaceGeometryMapping</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> path<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 加载导航网格文件</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">loadNavigationMesh</span><span class="token punctuation">(</span>path<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    AABB bounds_<span class="token punctuation">;</span>  <span class="token comment">// 轴对齐包围盒</span></span>
<span class="line">    <span class="token comment">// 用于：</span></span>
<span class="line">    <span class="token comment">// 1. 物理碰撞检测</span></span>
<span class="line">    <span class="token comment">// 2. 寻路限制</span></span>
<span class="line">    <span class="token comment">// 3. 不是用于 Space 划分！</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="边界的用途" tabindex="-1"><a class="header-anchor" href="#边界的用途"><span>边界的用途</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Space 边界的实际用途：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│  AABB (Axis-Aligned Bounding Box)                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────┐    │</span>
<span class="line">│  │                                                     │    │</span>
<span class="line">│  │    Space Bounds (用于物理检测)                      │    │</span>
<span class="line">│  │    ┌───────────────────────────────────┐            │    │</span>
<span class="line">│  │    │  导航网格 (Navigation Mesh)       │            │    │</span>
<span class="line">│  │    │                                   │            │    │</span>
<span class="line">│  │    │  [可行走区域]                     │            │    │</span>
<span class="line">│  │    │                                   │            │    │</span>
<span class="line">│  │    └───────────────────────────────────┘            │    │</span>
<span class="line">│  │                                                     │    │</span>
<span class="line">│  └─────────────────────────────────────────────────────┘    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  用途：                                                      │</span>
<span class="line">│  1. 物理碰撞检测 - Entity 是否超出边界                       │</span>
<span class="line">│  2. 寻路限制 - AI 只在边界内移动                              │</span>
<span class="line">│  3. 不用于 Space 划分！                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、多-space-管理" tabindex="-1"><a class="header-anchor" href="#六、多-space-管理"><span>六、多 Space 管理</span></a></h2><h3 id="一个-cellapp-多-space-示例" tabindex="-1"><a class="header-anchor" href="#一个-cellapp-多-space-示例"><span>一个 CellApp 多 Space 示例</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">CellApp 上的实际 Space 分布：</span>
<span class="line"></span>
<span class="line">CellApp (进程 ID: 1234)</span>
<span class="line">│</span>
<span class="line">├── Space 1: &quot;Newbie_Village_001&quot;</span>
<span class="line">│   ├── Entity: Player_1 (玩家)</span>
<span class="line">│   ├── Entity: Player_2 (玩家)</span>
<span class="line">│   ├── Entity: NPC_Guard_1 (NPC)</span>
<span class="line">│   ├── Entity: NPC_Guard_2 (NPC)</span>
<span class="line">│   └── CoordinateNode 链表 (AOI)</span>
<span class="line">│</span>
<span class="line">├── Space 2: &quot;Dungeon_Fire_001&quot; (副本实例)</span>
<span class="line">│   ├── Entity: Player_3 (玩家)</span>
<span class="line">│   ├── Entity: Player_4 (玩家)</span>
<span class="line">│   ├── Entity: Boss_Fire_Dragon (Boss)</span>
<span class="line">│   └── CoordinateNode 链表 (AOI)</span>
<span class="line">│</span>
<span class="line">├── Space 3: &quot;BattleGround_Arena_001&quot;</span>
<span class="line">│   ├── Entity: Player_5 (队伍A)</span>
<span class="line">│   ├── Entity: Player_6 (队伍A)</span>
<span class="line">│   ├── Entity: Player_7 (队伍B)</span>
<span class="line">│   └── CoordinateNode 链表 (AOI)</span>
<span class="line">│</span>
<span class="line">└── CoordinateSystem (全局，管理所有 Entity 的 AOI)</span>
<span class="line"></span>
<span class="line">每个 Space 的 Entity 是隔离的，不同 Space 之间不会产生 AOI 事件</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="space-之间-entity-迁移" tabindex="-1"><a class="header-anchor" href="#space-之间-entity-迁移"><span>Space 之间 Entity 迁移</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">玩家从一个 Space 移动到另一个 Space：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│  Space A (新手村)                    Space B (主城)           │</span>
<span class="line">│  ┌─────────────────┐                ┌─────────────────┐     │</span>
<span class="line">│  │ Player_1        │     传送       │ Player_1        │     │</span>
<span class="line">│  │                 │ ─────────────► │                 │     │</span>
<span class="line">│  │ x=100, y=0, z=50│                 │ x=500, y=0, z=500│     │</span>
<span class="line">│  └─────────────────┘                └─────────────────┘     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  迁移方式：                                                  │</span>
<span class="line">│  1. 客户端请求切换 Space（如进入副本入口）                   │</span>
<span class="line">│  2. 旧 Space 销毁玩家的 Entity                               │</span>
<span class="line">│  3. 新 Space 创建玩家的 Entity                                │</span>
<span class="line">│  4. 位置重置到新 Space 的入口点                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  注意：不是自动的物理边界迁移！需要脚本控制                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、与物理空间划分的对比" tabindex="-1"><a class="header-anchor" href="#七、与物理空间划分的对比"><span>七、与物理空间划分的对比</span></a></h2><h3 id="不同引擎的-space-概念" tabindex="-1"><a class="header-anchor" href="#不同引擎的-space-概念"><span>不同引擎的 Space 概念</span></a></h3><table><thead><tr><th>引擎</th><th>Space 概念</th><th>物理空间划分</th></tr></thead><tbody><tr><td><strong>KBEngine</strong></td><td>逻辑分组（副本/场景）</td><td>不划分，全 3D 世界</td></tr><tr><td><strong>BigWorld</strong></td><td>逻辑分组</td><td>CellApp 按物理空间划分</td></tr><tr><td><strong>Unreal Server</strong></td><td>Level/World</td><td>不划分</td></tr><tr><td><strong>自定义</strong></td><td>可自定义</td><td>可自定义</td></tr></tbody></table><h3 id="kbengine-的选择" tabindex="-1"><a class="header-anchor" href="#kbengine-的选择"><span>KBEngine 的选择</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 为什么不用物理空间划分？</span>
<span class="line"></span>
<span class="line">优点：</span>
<span class="line">✓ 简化架构 - 不需要复杂的边界管理</span>
<span class="line">✓ 灵活的副本系统 - Space 可以动态创建/销毁</span>
<span class="line">✓ 负载均衡简单 - 按 Space 数量而非位置分配</span>
<span class="line"></span>
<span class="line">缺点：</span>
<span class="line">✗ 单 CellApp 内存上限 - 所有 Space 在同一进程</span>
<span class="line">✗ 不支持超大世界 - 需要手动分割 Space</span>
<span class="line">✗ 跨 Space 交互复杂 - 需要特殊处理</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、实际应用示例" tabindex="-1"><a class="header-anchor" href="#八、实际应用示例"><span>八、实际应用示例</span></a></h2><h3 id="场景-mmo-的多-space-管理" tabindex="-1"><a class="header-anchor" href="#场景-mmo-的多-space-管理"><span>场景：MMO 的多 Space 管理</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 场景脚本示例</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 1. 主城 Space（固定，持久）</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SpaceCity</span><span class="token punctuation">(</span>Space<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        Space<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 设置城市边界</span></span>
<span class="line">        self<span class="token punctuation">.</span>setBounds<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&quot;minX&quot;</span><span class="token punctuation">:</span> <span class="token operator">-</span><span class="token number">500</span><span class="token punctuation">,</span> <span class="token string">&quot;maxX&quot;</span><span class="token punctuation">:</span> <span class="token number">500</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;minY&quot;</span><span class="token punctuation">:</span> <span class="token operator">-</span><span class="token number">100</span><span class="token punctuation">,</span> <span class="token string">&quot;maxY&quot;</span><span class="token punctuation">:</span> <span class="token number">100</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&quot;minZ&quot;</span><span class="token punctuation">:</span> <span class="token operator">-</span><span class="token number">500</span><span class="token punctuation">,</span> <span class="token string">&quot;maxZ&quot;</span><span class="token punctuation">:</span> <span class="token number">500</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 加载导航网格</span></span>
<span class="line">        self<span class="token punctuation">.</span>addSpaceGeometryMapping<span class="token punctuation">(</span><span class="token string">&quot;spaces/city/navmesh&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. 副本 Space（动态创建/销毁）</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SpaceDungeon</span><span class="token punctuation">(</span>Space<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> dungeonID<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        Space<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>dungeonID <span class="token operator">=</span> dungeonID</span>
<span class="line">        self<span class="token punctuation">.</span>addSpaceGeometryMapping<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;spaces/dungeon</span><span class="token interpolation"><span class="token punctuation">{</span>dungeonID<span class="token punctuation">}</span></span><span class="token string">/navmesh&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 副本结束条件</span></span>
<span class="line">        self<span class="token punctuation">.</span>registerTimePeriod<span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>checkTimeout<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">checkTimeout</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>players<span class="token punctuation">)</span> <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 无玩家时销毁副本</span></span>
<span class="line">            self<span class="token punctuation">.</span>destroy<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 3. 战场 Space（限时）</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SpaceBattle</span><span class="token punctuation">(</span>Space<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> battleID<span class="token punctuation">,</span> duration<span class="token operator">=</span><span class="token number">1800</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        Space<span class="token punctuation">.</span>__init__<span class="token punctuation">(</span>self<span class="token punctuation">)</span></span>
<span class="line">        self<span class="token punctuation">.</span>battleID <span class="token operator">=</span> battleID</span>
<span class="line">        self<span class="token punctuation">.</span>addSpaceGeometryMapping<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;spaces/battle/navmesh&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 倒计时销毁</span></span>
<span class="line">        self<span class="token punctuation">.</span>registerTimePeriod<span class="token punctuation">(</span>duration<span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> self<span class="token punctuation">.</span>endBattle<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">endBattle</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 战场结束，统计结果</span></span>
<span class="line">        self<span class="token punctuation">.</span>calculateScore<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 传送所有玩家回主城</span></span>
<span class="line">        <span class="token keyword">for</span> player <span class="token keyword">in</span> self<span class="token punctuation">.</span>players<span class="token punctuation">:</span></span>
<span class="line">            player<span class="token punctuation">.</span>teleport<span class="token punctuation">(</span><span class="token string">&quot;SpaceCity&quot;</span><span class="token punctuation">,</span> <span class="token number">100</span><span class="token punctuation">,</span> <span class="token number">0</span><span class="token punctuation">,</span> <span class="token number">100</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token comment"># 销毁战场</span></span>
<span class="line">        self<span class="token punctuation">.</span>destroy<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="九、常见问题" tabindex="-1"><a class="header-anchor" href="#九、常见问题"><span>九、常见问题</span></a></h2><h3 id="q1-entity-如何跨-space-移动" tabindex="-1"><a class="header-anchor" href="#q1-entity-如何跨-space-移动"><span>Q1: Entity 如何跨 Space 移动？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">不是自动的！需要脚本控制：</span>
<span class="line"></span>
<span class="line">方法 1: 传送门</span>
<span class="line">def onEnter(self, entity):</span>
<span class="line">    # 从主城传送到副本</span>
<span class="line">    entity.teleport(</span>
<span class="line">        &quot;SpaceDungeon&quot;,    # 目标 Space</span>
<span class="line">        100, 0, 100,       # 目标位置</span>
<span class="line">        {}                 # 额外数据</span>
<span class="line">    )</span>
<span class="line"></span>
<span class="line">方法 2: 脚本直接调用</span>
<span class="line">entity.moveToSpace(spaceID, position)</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="q2-不同-space-的-entity-能交互吗" tabindex="-1"><a class="header-anchor" href="#q2-不同-space-的-entity-能交互吗"><span>Q2: 不同 Space 的 Entity 能交互吗？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">默认不能！</span>
<span class="line"></span>
<span class="line">不同 Space 的 Entity 互不可见，不会产生 AOI 事件。</span>
<span class="line"></span>
<span class="line">如果需要跨 Space 交互：</span>
<span class="line">1. 使用聊天系统（BaseApp 层）</span>
<span class="line">2. 使用全局广播（BaseApp 转发）</span>
<span class="line">3. 使用特殊机制（如跨服战场）</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="q3-coordinatesystem-是全局的还是每个-space-独立" tabindex="-1"><a class="header-anchor" href="#q3-coordinatesystem-是全局的还是每个-space-独立"><span>Q3: CoordinateSystem 是全局的还是每个 Space 独立？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">CoordinateSystem 是 CellApp 全局的！</span>
<span class="line"></span>
<span class="line">但 AOI 计算会考虑 Space 隔离：</span>
<span class="line">- 不同 Space 的 Entity 不会互相触发 AOI</span>
<span class="line">- 同一 Space 的 Entity 才会触发 AOI</span>
<span class="line"></span>
<span class="line">这通过 SpaceID 过滤实现。</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="十、参考资料" tabindex="-1"><a class="header-anchor" href="#十、参考资料"><span>十、参考资料</span></a></h2><ul><li><a href="https://www.kbelab.com/guide/space/" target="_blank" rel="noopener noreferrer">KBEngine Lab - Space 空间</a></li><li><a href="https://www.kbelab.com/guide/space/howtos/balance.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 负载均衡</a></li><li><a href="https://www.kbelab.com/guide/space/howtos/target.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 指定 Space</a></li><li><a href="https://blog.csdn.net/kbengine/article/details/78327185" target="_blank" rel="noopener noreferrer">KBEngine 源码分析笔记</a></li><li><a href="https://www.cnblogs.com/losophy/p/9575792.html" target="_blank" rel="noopener noreferrer">KBEngine 源码：Space 空间</a></li><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/space.cpp" target="_blank" rel="noopener noreferrer">KBEngine GitHub - space.cpp</a></li><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/coordinate_system.cpp" target="_blank" rel="noopener noreferrer">KBEngine GitHub - coordinate_system.cpp</a></li></ul>`,39)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};