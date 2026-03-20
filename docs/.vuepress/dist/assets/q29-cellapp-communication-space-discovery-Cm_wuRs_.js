import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q29-cellapp-communication-space-discovery.html","title":"Q29: KBEngine CellApp 之间如何通信？如何发现自己的空间位置？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q29-cellapp-communication-space-discovery.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q29-cellapp-communication-space-discovery.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q29-kbengine-cellapp-之间如何通信-如何发现自己的空间位置" tabindex="-1"><a class="header-anchor" href="#q29-kbengine-cellapp-之间如何通信-如何发现自己的空间位置"><span>Q29: KBEngine CellApp 之间如何通信？如何发现自己的空间位置？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对 KBEngine/Apollo 中 <strong>CellApp 间通信机制</strong> 和 <strong>空间划分策略</strong> 的深入理解：</p><ul><li>CellApp 之间的通信方式</li><li>CellApp 如何知道自己在空间中的位置</li><li>空间边界的动态调整机制</li></ul><hr><h2 id="一、cellapp-通信架构" tabindex="-1"><a class="header-anchor" href="#一、cellapp-通信架构"><span>一、CellApp 通信架构</span></a></h2><h3 id="通信拓扑" tabindex="-1"><a class="header-anchor" href="#通信拓扑"><span>通信拓扑</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                        CellApp 通信网络                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                     │</span>
<span class="line">│   ┌─────────────┐                                                  │</span>
<span class="line">│   │ CellAppMgr  │ ◄─────── 心跳/注册/空间查询                       │</span>
<span class="line">│   └──────┬──────┘                                                  │</span>
<span class="line">│          │                                                         │</span>
<span class="line">│          │ 注册/发现                                                │</span>
<span class="line">│          ▼                                                         │</span>
<span class="line">│   ┌──────────────────────────────────────────────────────────────┐ │</span>
<span class="line">│   │                    CellApp 集群                              │ │</span>
<span class="line">│   │                                                              │ │</span>
<span class="line">│   │   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐  │ │</span>
<span class="line">│   │   │CellApp1 │◄──►│CellApp2 │◄──►│CellApp3 │◄──►│CellApp4 │  │ │</span>
<span class="line">│   │   │ [0,0)   │    │ [256,0) │    │ [0,256) │    │ [256,256)│ │ │</span>
<span class="line">│   │   └─────────┘    └─────────┘    └─────────┘    └─────────┘  │ │</span>
<span class="line">│   │        │              │              │              │        │ │</span>
<span class="line">│   │        └──────────────┴──────────────┴──────────────┘        │ │</span>
<span class="line">│   │                       全连接网状拓扑                            │ │</span>
<span class="line">│   └──────────────────────────────────────────────────────────────┘ │</span>
<span class="line">│                                                                     │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="通信方式" tabindex="-1"><a class="header-anchor" href="#通信方式"><span>通信方式</span></a></h3><table><thead><tr><th>方式</th><th>协议</th><th>用途</th><th>连接建立</th></tr></thead><tbody><tr><td><strong>CellApp → CellAppMgr</strong></td><td>TCP</td><td>注册、心跳、空间查询</td><td>启动时连接</td></tr><tr><td><strong>CellApp ↔ CellApp</strong></td><td>TCP</td><td>Entity 迁移、Ghost 同步、边界通信</td><td>通过 Mgr 发现后直连</td></tr></tbody></table><hr><h2 id="二、cellapp-启动与空间分配流程" tabindex="-1"><a class="header-anchor" href="#二、cellapp-启动与空间分配流程"><span>二、CellApp 启动与空间分配流程</span></a></h2><h3 id="启动流程" tabindex="-1"><a class="header-anchor" href="#启动流程"><span>启动流程</span></a></h3>`,13),i(d,{code:`eJxlkt1y0lAUhe99ivMAMR3/HcZhxklvvEA7oz5AxAxkprYxxL87qiZDacC0VqqEljJjR2wl4BStBCgPI/uccNVX6E4OLS3kdn97rbPWTkZ5+UpZSirzqpzS5RdXCH6arBtqUtXkJYNIRM4QSVlcvK9pM8PEhWEipc/MHxlpRQ8ZMP8Ou6VznQiUrsbjiRi5JhJwmpCvn/Rs+LQe/Dmih3Ww7KB5RH+/j8iHy4ZCll+jlCTgBtgmOAcnvTJz18H5BZUWbGf/Z1ekhadzYJnQ+Doc1OhKk9skuM11kQRejXlb7Kc/2mpDzhqZhSl1xOhuhxY8VmzRymrQrgb9PmzYSLK+d++ZPhcH6xC8Mt+Gz/vDf/tg+1CtTrykGLmBXoNNcHc4x8ofuWlgfwC3PW0q4AbHMVJGk5PKg3mBvBXIO4G8UZ8baYGkFTWVNia14cZNrC23DT/WwC5xdYbxHAu+1ac7Q3nvGAMExx32xQ5rzrnQ9cnj0OvSLW6JhJcPnk1zToj2suhBKwXI1yYZo7vGyG2RjLJlVt2jpdbZbQnkd8Hcmwk5XoHOgG40LvLYDr8gFn2ppkgiWuOJ72Dirs8O1qDZYY3VYLBDi9M+ER4W+kRaIMxtI4Qh2GZ96BfBKbJuAx+Mf8ck9fhdd0XCal7gfR/LngIXMHDD`}),o[1]||=e(`<h3 id="空间分配策略" tabindex="-1"><a class="header-anchor" href="#空间分配策略"><span>空间分配策略</span></a></h3><h4 id="坐标系统说明" tabindex="-1"><a class="header-anchor" href="#坐标系统说明"><span>坐标系统说明</span></a></h4><p>MMORPG 通常使用 <strong>3D 坐标系</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">     Y (上/高度)</span>
<span class="line">     ↑</span>
<span class="line">     │</span>
<span class="line">     │</span>
<span class="line">     │─────→ X (右)</span>
<span class="line">    ╱</span>
<span class="line">   ╱</span>
<span class="line">  ↓ Z (前/深度)</span>
<span class="line"></span>
<span class="line">不同引擎的约定：</span>
<span class="line">- KBEngine/BigWorld: Y 轴向上</span>
<span class="line">- Unity: Y 轴向上</span>
<span class="line">- Unreal Engine: Z 轴向上</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>策略 1: 静态 2D 划分（最常用）</strong></p><p>大多数 MMO 只在 <strong>XZ 平面</strong> 划分空间，Y 轴（高度）不划分：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">假设地图 512x512，4 个 CellApp（俯视图）：</span>
<span class="line"></span>
<span class="line">           Z (前)</span>
<span class="line">           ↑</span>
<span class="line">           │</span>
<span class="line">           │</span>
<span class="line">     ┌─────┼─────┐</span>
<span class="line">     │     │     │</span>
<span class="line">     │  1  │  2  │</span>
<span class="line">     │─────┼─────│</span>
<span class="line">     │  3  │  4  │</span>
<span class="line">     └─────┴─────┘</span>
<span class="line">           │</span>
<span class="line">           └─────────→ X (右)</span>
<span class="line"></span>
<span class="line">每个 CellApp 的空间：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│ CellApp1:                                                   │</span>
<span class="line">│   X: [0, 256),    Z: [256, 512),    Y: [-∞, +∞]            │</span>
<span class="line">│                                                             │</span>
<span class="line">│   Y 轴不划分 → 整个垂直空间都由这个 CellApp 管理           │</span>
<span class="line">│   适用于：玩家主要在地面上活动                               │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">为什么只划分 XZ 平面？</span>
<span class="line">- 玩家大部分时间在地面上</span>
<span class="line">- 飞行坐骑/跳跃不需要跨 CellApp</span>
<span class="line">- 简化边界计算和 Entity 迁移</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>策略 2: 静态 3D 划分（有飞行系统的游戏）</strong></p><p>如果游戏有大量飞行内容，可以按 <strong>3D 立方体</strong> 划分：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">3D 空间划分（8 个 CellApp）：</span>
<span class="line"></span>
<span class="line">        Y (上)</span>
<span class="line">        ↑</span>
<span class="line">        │</span>
<span class="line">   ┌────┼────┐</span>
<span class="line">   │ 4  │ 5  │  高层</span>
<span class="line">   ├────┼────┤</span>
<span class="line">   │ 6  │ 7  │</span>
<span class="line">   └────┴────┘</span>
<span class="line">        │</span>
<span class="line">        └──────────→ X</span>
<span class="line">       ╱</span>
<span class="line">      ↓ Z</span>
<span class="line"></span>
<span class="line">     (俯视图)</span>
<span class="line"></span>
<span class="line">层次划分：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   Y ≥ 100m (高层):                                          │</span>
<span class="line">│   ┌──────────┬──────────┐                                  │</span>
<span class="line">│   │CellApp4  │CellApp5  │  飞行区域                        │</span>
<span class="line">│   │XZ: 1-2象限│XZ: 3-4象限│                                  │</span>
<span class="line">│   ├──────────┼──────────┤                                  │</span>
<span class="line">│   │CellApp6  │CellApp7  │                                  │</span>
<span class="line">│   └──────────┴──────────┘                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│   Y &lt; 100m (低层):                                          │</span>
<span class="line">│   ┌──────────┬──────────┐                                  │</span>
<span class="line">│   │CellApp1  │CellApp2  │  地面活动                        │</span>
<span class="line">│   ├──────────┼──────────┤                                  │</span>
<span class="line">│   │CellApp3  │CellApp4  │                                  │</span>
<span class="line">│   └──────────┴──────────┘                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">问题：玩家从地面飞到空中 → 需要跨 CellApp 迁移</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>策略 2: 动态负载均衡</strong></p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">根据实际负载动态调整边界：</span>
<span class="line"></span>
<span class="line">初始状态：</span>
<span class="line">┌──────────────┬──────────────┐</span>
<span class="line">│  CellApp1    │  CellApp2    │</span>
<span class="line">│  负载: 200   │  负载: 2000  │ ← 过载</span>
<span class="line">│  空间: 50%   │  空间: 50%   │</span>
<span class="line">└──────────────┴──────────────┘</span>
<span class="line"></span>
<span class="line">调整后：</span>
<span class="line">┌────────────────┬──────────────┐</span>
<span class="line">│  CellApp1      │  CellApp2    │</span>
<span class="line">│  负载: 800     │  负载: 1400  │ ← 平衡</span>
<span class="line">│  空间: 75% ▲   │  空间: 25% ▼ │</span>
<span class="line">└────────────────┴──────────────┘</span>
<span class="line">     边界移动 →</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、空间位置发现机制" tabindex="-1"><a class="header-anchor" href="#三、空间位置发现机制"><span>三、空间位置发现机制</span></a></h2><h3 id="cellapp-的空间元数据" tabindex="-1"><a class="header-anchor" href="#cellapp-的空间元数据"><span>CellApp 的空间元数据</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 3D 空间边界</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">SpaceBounds3D</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> spaceID<span class="token punctuation">;</span>         <span class="token comment">// 空间 ID</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 3D 边界</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Vec3</span> <span class="token punctuation">{</span> <span class="token keyword">float</span> x<span class="token punctuation">,</span> y<span class="token punctuation">,</span> z<span class="token punctuation">;</span> <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">    Vec3 min<span class="token punctuation">;</span>                 <span class="token comment">// 最小坐标</span></span>
<span class="line">    Vec3 max<span class="token punctuation">;</span>                 <span class="token comment">// 最大坐标</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 尺寸</span></span>
<span class="line">    <span class="token keyword">float</span> width<span class="token punctuation">,</span> height<span class="token punctuation">,</span> depth<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 2D 划分时，Y 轴不限制</span></span>
<span class="line">    <span class="token keyword">bool</span> unboundedY<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 相邻 CellApp 信息</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Neighbor</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> cellAppID<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string address<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> port<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3D 空间中的方向</span></span>
<span class="line">        <span class="token keyword">enum</span> <span class="token class-name">Direction</span> <span class="token punctuation">{</span></span>
<span class="line">            LEFT<span class="token punctuation">,</span> RIGHT<span class="token punctuation">,</span>      <span class="token comment">// -X, +X</span></span>
<span class="line">            FRONT<span class="token punctuation">,</span> BACK<span class="token punctuation">,</span>      <span class="token comment">// -Z, +Z (或 BOTTOM, TOP 取决于坐标系)</span></span>
<span class="line">            BELOW<span class="token punctuation">,</span> ABOVE<span class="token punctuation">,</span>     <span class="token comment">// -Y, +Y (仅 3D 划分时使用)</span></span>
<span class="line">            LEFT_FRONT<span class="token punctuation">,</span> LEFT_BACK<span class="token punctuation">,</span> RIGHT_FRONT<span class="token punctuation">,</span> RIGHT_BACK<span class="token punctuation">,</span>  <span class="token comment">// 组合方向</span></span>
<span class="line">            <span class="token comment">// ... 更多组合</span></span>
<span class="line">        <span class="token punctuation">}</span> direction<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Neighbor<span class="token operator">&gt;</span> neighbors<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellApp</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    SpaceBounds3D bounds_<span class="token punctuation">;</span>           <span class="token comment">// 自己的空间范围</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> Neighbor<span class="token operator">&gt;</span> neighborMap_<span class="token punctuation">;</span>  <span class="token comment">// 邻居映射</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 判断坐标是否在自己的空间内</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">contains</span><span class="token punctuation">(</span><span class="token keyword">float</span> x<span class="token punctuation">,</span> <span class="token keyword">float</span> y<span class="token punctuation">,</span> <span class="token keyword">float</span> z<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// X 和 Z 必须在范围内</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>x <span class="token operator">&lt;</span> bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>x <span class="token operator">||</span> x <span class="token operator">&gt;=</span> bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>x<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>z <span class="token operator">&lt;</span> bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>z <span class="token operator">||</span> z <span class="token operator">&gt;=</span> bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>z<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// Y 轴检查（如果划分了）</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>bounds_<span class="token punctuation">.</span>unboundedY<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>y <span class="token operator">&lt;</span> bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>y <span class="token operator">||</span> y <span class="token operator">&gt;=</span> bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>y<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 判断坐标是否在边界区域</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isNearBoundary</span><span class="token punctuation">(</span><span class="token keyword">float</span> x<span class="token punctuation">,</span> <span class="token keyword">float</span> y<span class="token punctuation">,</span> <span class="token keyword">float</span> z<span class="token punctuation">,</span> <span class="token keyword">float</span> threshold <span class="token operator">=</span> <span class="token number">50.0f</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">bool</span> nearX <span class="token operator">=</span> <span class="token punctuation">(</span>x <span class="token operator">-</span> bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>x <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span> <span class="token operator">||</span></span>
<span class="line">                     <span class="token punctuation">(</span>bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>x <span class="token operator">-</span> x <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">bool</span> nearZ <span class="token operator">=</span> <span class="token punctuation">(</span>z <span class="token operator">-</span> bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>z <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span> <span class="token operator">||</span></span>
<span class="line">                     <span class="token punctuation">(</span>bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>z <span class="token operator">-</span> z <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// Y 轴边界检查（如果划分了）</span></span>
<span class="line">        <span class="token keyword">bool</span> nearY <span class="token operator">=</span> bounds_<span class="token punctuation">.</span>unboundedY <span class="token operator">?</span> <span class="token boolean">false</span> <span class="token operator">:</span></span>
<span class="line">                     <span class="token punctuation">(</span>y <span class="token operator">-</span> bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>y <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span> <span class="token operator">||</span></span>
<span class="line">                     <span class="token punctuation">(</span>bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>y <span class="token operator">-</span> y <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> nearX <span class="token operator">||</span> nearZ <span class="token operator">||</span> nearY<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 根据坐标找到目标 CellApp</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> <span class="token function">findTargetCellApp</span><span class="token punctuation">(</span><span class="token keyword">float</span> x<span class="token punctuation">,</span> <span class="token keyword">float</span> y<span class="token punctuation">,</span> <span class="token keyword">float</span> z<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">contains</span><span class="token punctuation">(</span>x<span class="token punctuation">,</span> y<span class="token punctuation">,</span> z<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token function">getCellAppID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 自己</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 判断移动方向，找到对应的邻居</span></span>
<span class="line">        Direction dir <span class="token operator">=</span> <span class="token function">calculateDirection</span><span class="token punctuation">(</span>x<span class="token punctuation">,</span> y<span class="token punctuation">,</span> z<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>id<span class="token punctuation">,</span> neighbor<span class="token punctuation">]</span> <span class="token operator">:</span> neighborMap_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>neighbor<span class="token punctuation">.</span>direction <span class="token operator">==</span> dir<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> id<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 不在相邻区域，询问 CellAppMgr</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">queryCellAppMgr</span><span class="token punctuation">(</span>x<span class="token punctuation">,</span> y<span class="token punctuation">,</span> z<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">Direction</span> <span class="token punctuation">{</span></span>
<span class="line">        LEFT<span class="token punctuation">,</span> RIGHT<span class="token punctuation">,</span> FRONT<span class="token punctuation">,</span> BACK<span class="token punctuation">,</span> BELOW<span class="token punctuation">,</span> ABOVE<span class="token punctuation">,</span></span>
<span class="line">        LEFT_FRONT<span class="token punctuation">,</span> LEFT_BACK<span class="token punctuation">,</span> RIGHT_FRONT<span class="token punctuation">,</span> RIGHT_BACK<span class="token punctuation">,</span></span>
<span class="line">        <span class="token comment">// ... 更多组合</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    Direction <span class="token function">calculateDirection</span><span class="token punctuation">(</span><span class="token keyword">float</span> x<span class="token punctuation">,</span> <span class="token keyword">float</span> y<span class="token punctuation">,</span> <span class="token keyword">float</span> z<span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 计算相对于中心的位置</span></span>
<span class="line">        <span class="token keyword">float</span> centerX <span class="token operator">=</span> <span class="token punctuation">(</span>bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>x <span class="token operator">+</span> bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>x<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> centerZ <span class="token operator">=</span> <span class="token punctuation">(</span>bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>z <span class="token operator">+</span> bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>z<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        Direction dir <span class="token operator">=</span> NONE<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>x <span class="token operator">&lt;</span> bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>x<span class="token punctuation">)</span> dir <span class="token operator">|=</span> LEFT<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>x <span class="token operator">&gt;=</span> bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>x<span class="token punctuation">)</span> dir <span class="token operator">|=</span> RIGHT<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>z <span class="token operator">&lt;</span> bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>z<span class="token punctuation">)</span> dir <span class="token operator">|=</span> BACK<span class="token punctuation">;</span>  <span class="token comment">// 或 FRONT</span></span>
<span class="line">        <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>z <span class="token operator">&gt;=</span> bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>z<span class="token punctuation">)</span> dir <span class="token operator">|=</span> FRONT<span class="token punctuation">;</span>  <span class="token comment">// 或 BACK</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>bounds_<span class="token punctuation">.</span>unboundedY<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">float</span> centerY <span class="token operator">=</span> <span class="token punctuation">(</span>bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>y <span class="token operator">+</span> bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>y<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>y <span class="token operator">&lt;</span> bounds_<span class="token punctuation">.</span>min<span class="token punctuation">.</span>y<span class="token punctuation">)</span> dir <span class="token operator">|=</span> BELOW<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span>y <span class="token operator">&gt;=</span> bounds_<span class="token punctuation">.</span>max<span class="token punctuation">.</span>y<span class="token punctuation">)</span> dir <span class="token operator">|=</span> ABOVE<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> dir<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="边界感知" tabindex="-1"><a class="header-anchor" href="#边界感知"><span>边界感知</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">边界区域定义（俯视图，XZ 平面）：</span>
<span class="line"></span>
<span class="line">           Z (前)</span>
<span class="line">           ↑</span>
<span class="line">           │</span>
<span class="line">     ┌─────┼─────┐</span>
<span class="line">     │  1  │  2  │</span>
<span class="line">     │ ┌───┼───┐ │</span>
<span class="line">     │ │ █ │ █ │ │  █ = 边界区 (50m)</span>
<span class="line">     │ ├───┼───┤ │  └─ 双方边界重叠</span>
<span class="line">     │ │ █ │ █ │ │</span>
<span class="line">     │ └───┴───┘ │</span>
<span class="line">     │  3  │  4  │</span>
<span class="line">     └─────┴─────┘</span>
<span class="line">           │</span>
<span class="line">           └─────────→ X (右)</span>
<span class="line"></span>
<span class="line">3D 视图（Y 轴不划分的情况）：</span>
<span class="line"></span>
<span class="line">         Y (上)</span>
<span class="line">         ↑</span>
<span class="line">         │</span>
<span class="line">         │   ┌─────────────────────┐</span>
<span class="line">         │   │                     │  整个 Y 轴</span>
<span class="line">         │   │    CellApp1         │  都属于</span>
<span class="line">         │   │   (XZ: 象限1)       │  CellApp1</span>
<span class="line">         │   │                     │</span>
<span class="line">         │   │   █ 边界区 █        │</span>
<span class="line">         │   ├─────────────────────┤</span>
<span class="line">         │   │   正常区域          │</span>
<span class="line">         │   │                     │</span>
<span class="line">         │   └─────────────────────┘</span>
<span class="line">         │</span>
<span class="line">         └─────────────────→ X</span>
<span class="line">        ╱</span>
<span class="line">       ╱</span>
<span class="line">      ↓ Z</span>
<span class="line"></span>
<span class="line">Y 轴（高度）处理：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│  方案 A: Y 轴不划分（推荐）                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────┐    │</span>
<span class="line">│  │                 Y → 无限高度                        │    │</span>
<span class="line">│  │  天空 (∞) ────────────────────────────────────      │    │</span>
<span class="line">│  │                                                      │    │</span>
<span class="line">│  │  飞行区 (100m) ────────────────                    │    │</span>
<span class="line">│  │                                                      │    │</span>
<span class="line">│  │  地面 (0m) ────────────────────────                │    │</span>
<span class="line">│  │                                                      │    │</span>
<span class="line">│  │  地下 (-50m) ───────────────────────                │    │</span>
<span class="line">│  │                                                      │    │</span>
<span class="line">│  │  深层 (-∞) ─────────────────────────────            │    │</span>
<span class="line">│  │                                                      │    │</span>
<span class="line">│  │  → 所有高度的 Entity 都在同一个 CellApp            │    │</span>
<span class="line">│  └─────────────────────────────────────────────────────┘    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  方案 B: Y 轴划分（有大量飞行内容）                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────┐    │</span>
<span class="line">│  │                 Y → 分层管理                        │    │</span>
<span class="line">│  │  天空层 (200m+) ──→ CellApp 高层组                  │    │</span>
<span class="line">│  │  飞行层 (50-200m) ──→ CellApp 中层组                │    │</span>
<span class="line">│  │  地面层 (-50~50m) ──→ CellApp 地面组                │    │</span>
<span class="line">│  │  地下层 (&lt;-50m) ──→ CellApp 地下组                  │    │</span>
<span class="line">│  │                                                      │    │</span>
<span class="line">│  │  → Entity 上升/下降需要跨 CellApp 迁移              │    │</span>
<span class="line">│  └─────────────────────────────────────────────────────┘    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  大多数 MMO 使用方案 A：                                      │</span>
<span class="line">│  - Y 轴不划分，简化逻辑                                      │</span>
<span class="line">│  - AOI 在 XZ 平面计算                                        │</span>
<span class="line">│  - Y 轴只用于视线遮挡、碰撞检测                              │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">当 Entity 进入边界区域时：</span>
<span class="line">1. 在 XZ 平面进入边界 → 创建 Ghost 给相邻 CellApp</span>
<span class="line">2. Y 轴移动 → 不触发 Ghost（Y 轴不划分）</span>
<span class="line">3. 完全离开 XZ 区域 → 迁移 Entity</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="邻居方向定义-3d" tabindex="-1"><a class="header-anchor" href="#邻居方向定义-3d"><span>邻居方向定义（3D）</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 8 个基本方向（2D 划分）</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">Direction2D</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint8_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">    NONE <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">    LEFT <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">0</span><span class="token punctuation">,</span>      <span class="token comment">// -X</span></span>
<span class="line">    RIGHT <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">1</span><span class="token punctuation">,</span>     <span class="token comment">// +X</span></span>
<span class="line">    FRONT <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">2</span><span class="token punctuation">,</span>     <span class="token comment">// -Z 或 +Z</span></span>
<span class="line">    BACK <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">3</span><span class="token punctuation">,</span>      <span class="token comment">// +Z 或 -Z</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 组合方向（对角线）</span></span>
<span class="line"><span class="token keyword">constexpr</span> Direction2D LEFT_FRONT <span class="token operator">=</span> LEFT <span class="token operator">|</span> FRONT<span class="token punctuation">;</span></span>
<span class="line"><span class="token keyword">constexpr</span> Direction2D LEFT_BACK <span class="token operator">=</span> LEFT <span class="token operator">|</span> BACK<span class="token punctuation">;</span></span>
<span class="line"><span class="token keyword">constexpr</span> Direction2D RIGHT_FRONT <span class="token operator">=</span> RIGHT <span class="token operator">|</span> FRONT<span class="token punctuation">;</span></span>
<span class="line"><span class="token keyword">constexpr</span> Direction2D RIGHT_BACK <span class="token operator">=</span> RIGHT <span class="token operator">|</span> BACK<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 3D 划分时增加 Y 轴方向</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">Direction3D</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint8_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// ... 2D 方向 ...</span></span>
<span class="line">    BELOW <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">4</span><span class="token punctuation">,</span>      <span class="token comment">// -Y</span></span>
<span class="line">    ABOVE <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">5</span><span class="token punctuation">,</span>      <span class="token comment">// +Y</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 邻居表结构</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">Neighbor3D</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> cellAppID<span class="token punctuation">;</span></span>
<span class="line">    Direction3D direction<span class="token punctuation">;</span></span>
<span class="line">    SpaceBounds3D bounds<span class="token punctuation">;</span></span>
<span class="line">    TCPConnection<span class="token operator">*</span> connection<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 示例：4 个 CellApp 的邻居关系</span></span>
<span class="line"><span class="token comment">// CellApp1 (X: [0,256), Z: [256,512)) 的邻居：</span></span>
<span class="line">neighbors <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token punctuation">{</span> <span class="token number">2</span><span class="token punctuation">,</span> RIGHT<span class="token punctuation">,</span>   <span class="token punctuation">{</span>X<span class="token operator">:</span> <span class="token punctuation">[</span><span class="token number">256</span><span class="token punctuation">,</span><span class="token number">512</span><span class="token punctuation">)</span><span class="token punctuation">,</span> Z<span class="token operator">:</span> <span class="token punctuation">[</span><span class="token number">256</span><span class="token punctuation">,</span><span class="token number">512</span><span class="token punctuation">)</span><span class="token punctuation">}</span> <span class="token punctuation">}</span><span class="token punctuation">,</span>   <span class="token comment">// 右侧</span></span>
<span class="line">    <span class="token punctuation">{</span> <span class="token number">3</span><span class="token punctuation">,</span> BACK<span class="token punctuation">,</span>    <span class="token punctuation">{</span>X<span class="token operator">:</span> <span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">,</span><span class="token number">256</span><span class="token punctuation">)</span><span class="token punctuation">,</span>   Z<span class="token operator">:</span> <span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">,</span><span class="token number">256</span><span class="token punctuation">)</span><span class="token punctuation">}</span> <span class="token punctuation">}</span><span class="token punctuation">,</span>     <span class="token comment">// 后方</span></span>
<span class="line">    <span class="token punctuation">{</span> <span class="token number">4</span><span class="token punctuation">,</span> RIGHT_BACK<span class="token punctuation">,</span> <span class="token punctuation">{</span>X<span class="token operator">:</span> <span class="token punctuation">[</span><span class="token number">256</span><span class="token punctuation">,</span><span class="token number">512</span><span class="token punctuation">)</span><span class="token punctuation">,</span> Z<span class="token operator">:</span> <span class="token punctuation">[</span><span class="token number">0</span><span class="token punctuation">,</span><span class="token number">256</span><span class="token punctuation">)</span><span class="token punctuation">}</span> <span class="token punctuation">}</span><span class="token punctuation">,</span>  <span class="token comment">// 右后对角</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、cellapp-间通信协议" tabindex="-1"><a class="header-anchor" href="#四、cellapp-间通信协议"><span>四、CellApp 间通信协议</span></a></h2><h3 id="通信类型" tabindex="-1"><a class="header-anchor" href="#通信类型"><span>通信类型</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// CellApp 间消息类型</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">CellAppMessageType</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint16_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// Entity 迁移</span></span>
<span class="line">    ENTITY_MIGRATE_REQUEST<span class="token punctuation">,</span>      <span class="token comment">// 迁移请求</span></span>
<span class="line">    ENTITY_MIGRATE_RESPONSE<span class="token punctuation">,</span>      <span class="token comment">// 迁移响应</span></span>
<span class="line">    ENTITY_MIGRATE_DATA<span class="token punctuation">,</span>          <span class="token comment">// 迁移数据</span></span>
<span class="line">    ENTITY_MIGRATE_COMPLETE<span class="token punctuation">,</span>      <span class="token comment">// 迁移完成</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Ghost 同步</span></span>
<span class="line">    GHOST_CREATE_REQUEST<span class="token punctuation">,</span>         <span class="token comment">// 创建 Ghost 请求</span></span>
<span class="line">    GHOST_CREATE_RESPONSE<span class="token punctuation">,</span>        <span class="token comment">// 创建 Ghost 响应</span></span>
<span class="line">    GHOST_UPDATE<span class="token punctuation">,</span>                 <span class="token comment">// Ghost 状态更新</span></span>
<span class="line">    GHOST_DESTROY<span class="token punctuation">,</span>                <span class="token comment">// 销毁 Ghost</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 边界交互</span></span>
<span class="line">    BOUNDARY_CROSS_REQUEST<span class="token punctuation">,</span>       <span class="token comment">// 跨边界请求</span></span>
<span class="line">    BOUNDARY_CROSS_APPROVE<span class="token punctuation">,</span>       <span class="token comment">// 跨边界批准</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播转发</span></span>
<span class="line">    BROADCAST_FORWARD<span class="token punctuation">,</span>            <span class="token comment">// 广播转发</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 负载均衡</span></span>
<span class="line">    LOAD_BALANCE_QUERY<span class="token punctuation">,</span>           <span class="token comment">// 负载查询</span></span>
<span class="line">    LOAD_BALANCE_REPORT<span class="token punctuation">,</span>          <span class="token comment">// 负载报告</span></span>
<span class="line">    BOUNDARY_ADJUST<span class="token punctuation">,</span>              <span class="token comment">// 边界调整</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="entity-迁移协议" tabindex="-1"><a class="header-anchor" href="#entity-迁移协议"><span>Entity 迁移协议</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">迁移流程：</span>
<span class="line"></span>
<span class="line">┌──────────────────┐                    ┌──────────────────┐</span>
<span class="line">│   CellApp A      │                    │   CellApp B      │</span>
<span class="line">│   (源)           │                    │   (目标)         │</span>
<span class="line">└─────────┬────────┘                    └────────┬─────────┘</span>
<span class="line">          │                                      │</span>
<span class="line">          │ 1. ENTITY_MIGRATE_REQUEST           │</span>
<span class="line">          │    (entityID, entityData)           │</span>
<span class="line">          ├─────────────────────────────────────&gt;│</span>
<span class="line">          │                                      │</span>
<span class="line">          │           2. 创建 Real Entity        │</span>
<span class="line">          │                                      │</span>
<span class="line">          │ 3. ENTITY_MIGRATE_RESPONSE          │</span>
<span class="line">          │    (success, newEntityID)            │</span>
<span class="line">          │&lt;─────────────────────────────────────┤</span>
<span class="line">          │                                      │</span>
<span class="line">          │ 4. 通知相关方 Entity 已迁移          │</span>
<span class="line">          │    (更新路由表)                      │</span>
<span class="line">          │                                      │</span>
<span class="line">          │ 5. ENTITY_MIGRATE_COMPLETE           │</span>
<span class="line">          ├─────────────────────────────────────&gt;│</span>
<span class="line">          │                                      │</span>
<span class="line">          │           6. 销毁旧 Entity            │</span>
<span class="line">          │                                      │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="ghost-同步协议" tabindex="-1"><a class="header-anchor" href="#ghost-同步协议"><span>Ghost 同步协议</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Ghost 创建请求</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">GhostCreateRequest</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> entityID<span class="token punctuation">;</span>          <span class="token comment">// Entity ID</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> ownerCellAppID<span class="token punctuation">;</span>    <span class="token comment">// 所属 CellApp</span></span>
<span class="line">    Position position<span class="token punctuation">;</span>          <span class="token comment">// 初始位置</span></span>
<span class="line">    EntityType type<span class="token punctuation">;</span>            <span class="token comment">// Entity 类型</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> initialState<span class="token punctuation">;</span>  <span class="token comment">// 初始状态</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Ghost 更新（增量）</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">GhostUpdate</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> entityID<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> dirtyFlags<span class="token punctuation">;</span>        <span class="token comment">// 变化标记</span></span>
<span class="line">    Position position<span class="token punctuation">;</span>          <span class="token comment">// 如果位置脏</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> hp<span class="token punctuation">;</span>                <span class="token comment">// 如果血量脏</span></span>
<span class="line">    <span class="token comment">// ... 其他属性</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、边界协调机制" tabindex="-1"><a class="header-anchor" href="#五、边界协调机制"><span>五、边界协调机制</span></a></h2><h3 id="边界宽度设计" tabindex="-1"><a class="header-anchor" href="#边界宽度设计"><span>边界宽度设计</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">边界宽度权衡：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│  窄边界 (10m)                    宽边界 (100m)             │</span>
<span class="line">│  ┌─────┬─────┐                 ┌─────────────┬───────────┐ │</span>
<span class="line">│  │ CA1 │ CA2 │                 │    CA1      │   CA2     │ │</span>
<span class="line">│  │ ────┼─────│                 │ ────────────┼───────────│ │</span>
<span class="line">│  │     │     │                 │             │           │ │</span>
<span class="line">│  └─────┴─────┘                 └─────────────┴───────────┘ │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优点: Ghost 少                  优点: 减少频繁迁移           │</span>
<span class="line">│  缺点: 频繁迁移                  缺点: Ghost 多，同步开销大  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  推荐值: 30-50 米（根据 AOI 半径调整）                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="跨边界判断" tabindex="-1"><a class="header-anchor" href="#跨边界判断"><span>跨边界判断</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">class</span> <span class="token class-name">BoundaryManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 检查 Entity 是否需要迁移</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">shouldMigrate</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> SpaceBounds<span class="token operator">&amp;</span> targetBounds<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">float</span> x <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>x<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> y <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>y<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 已离开当前空间</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>bounds_<span class="token punctuation">.</span><span class="token function">contains</span><span class="token punctuation">(</span>x<span class="token punctuation">,</span> y<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 找到目标 CellApp</span></span>
<span class="line">            targetBounds <span class="token operator">=</span> <span class="token function">findTargetBounds</span><span class="token punctuation">(</span>x<span class="token punctuation">,</span> y<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 在边界区域，判断移动趋势</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isInBoundaryZone</span><span class="token punctuation">(</span>x<span class="token punctuation">,</span> y<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span> velocity <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getVelocity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">auto</span> futurePos <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">+</span> velocity <span class="token operator">*</span> <span class="token number">2.0f</span><span class="token punctuation">;</span>  <span class="token comment">// 2秒后位置</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>bounds_<span class="token punctuation">.</span><span class="token function">contains</span><span class="token punctuation">(</span>futurePos<span class="token punctuation">.</span>x<span class="token punctuation">,</span> futurePos<span class="token punctuation">.</span>y<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                targetBounds <span class="token operator">=</span> <span class="token function">findTargetBounds</span><span class="token punctuation">(</span>futurePos<span class="token punctuation">.</span>x<span class="token punctuation">,</span> futurePos<span class="token punctuation">.</span>y<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、邻居发现与维护" tabindex="-1"><a class="header-anchor" href="#六、邻居发现与维护"><span>六、邻居发现与维护</span></a></h2><h3 id="邻居表维护" tabindex="-1"><a class="header-anchor" href="#邻居表维护"><span>邻居表维护</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">class</span> <span class="token class-name">NeighborTable</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">NeighborInfo</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> cellAppID<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string host<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> port<span class="token punctuation">;</span></span>
<span class="line">        SpaceBounds bounds<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// TCP 连接</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>shared_ptr<span class="token operator">&lt;</span>TCPConnection<span class="token operator">&gt;</span> connection<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 心跳</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>steady_clock<span class="token double-colon punctuation">::</span>time_point lastHeartbeat<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 统计</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> messagesSent<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> messagesReceived<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">double</span> avgLatency<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> NeighborInfo<span class="token operator">&gt;</span> neighbors_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 添加邻居（由 CellAppMgr 通知）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addNeighbor</span><span class="token punctuation">(</span><span class="token keyword">const</span> NeighborInfo<span class="token operator">&amp;</span> info<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        neighbors_<span class="token punctuation">[</span>info<span class="token punctuation">.</span>cellAppID<span class="token punctuation">]</span> <span class="token operator">=</span> info<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">connectToNeighbor</span><span class="token punctuation">(</span>info<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 移除邻居</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">removeNeighbor</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> cellAppID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> neighbors_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>cellAppID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> neighbors_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            it<span class="token operator">-&gt;</span>second<span class="token punctuation">.</span>connection<span class="token operator">-&gt;</span><span class="token function">close</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            neighbors_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取目标方向的邻居</span></span>
<span class="line">    NeighborInfo<span class="token operator">*</span> <span class="token function">getNeighborInDirection</span><span class="token punctuation">(</span><span class="token keyword">float</span> dx<span class="token punctuation">,</span> <span class="token keyword">float</span> dy<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>id<span class="token punctuation">,</span> info<span class="token punctuation">]</span> <span class="token operator">:</span> neighbors_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>dx <span class="token operator">&gt;</span> <span class="token number">0</span> <span class="token operator">&amp;&amp;</span> info<span class="token punctuation">.</span>bounds<span class="token punctuation">.</span>minX <span class="token operator">&gt;</span> bounds_<span class="token punctuation">.</span>maxX<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token operator">&amp;</span>info<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>dx <span class="token operator">&lt;</span> <span class="token number">0</span> <span class="token operator">&amp;&amp;</span> info<span class="token punctuation">.</span>bounds<span class="token punctuation">.</span>maxX <span class="token operator">&lt;</span> bounds_<span class="token punctuation">.</span>minX<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token operator">&amp;</span>info<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>dy <span class="token operator">&gt;</span> <span class="token number">0</span> <span class="token operator">&amp;&amp;</span> info<span class="token punctuation">.</span>bounds<span class="token punctuation">.</span>minY <span class="token operator">&gt;</span> bounds_<span class="token punctuation">.</span>maxY<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token operator">&amp;</span>info<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>dy <span class="token operator">&lt;</span> <span class="token number">0</span> <span class="token operator">&amp;&amp;</span> info<span class="token punctuation">.</span>bounds<span class="token punctuation">.</span>maxY <span class="token operator">&lt;</span> bounds_<span class="token punctuation">.</span>minY<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token operator">&amp;</span>info<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 心跳检测</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">checkHeartbeats</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> now <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>steady_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>id<span class="token punctuation">,</span> info<span class="token punctuation">]</span> <span class="token operator">:</span> neighbors_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span> elapsed <span class="token operator">=</span> now <span class="token operator">-</span> info<span class="token punctuation">.</span>lastHeartbeat<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>elapsed <span class="token operator">&gt;</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">seconds</span><span class="token punctuation">(</span><span class="token number">10</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 心跳超时，重新连接或上报</span></span>
<span class="line">                <span class="token function">reconnectNeighbor</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="邻居变化通知" tabindex="-1"><a class="header-anchor" href="#邻居变化通知"><span>邻居变化通知</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">CellApp 加入/退出时的广播：</span>
<span class="line"></span>
<span class="line">CellAppMgr → 所有 CellApp:</span>
<span class="line">{</span>
<span class="line">    &quot;type&quot;: &quot;NEIGHBOR_UPDATE&quot;,</span>
<span class="line">    &quot;action&quot;: &quot;ADD&quot;,  // 或 &quot;REMOVE&quot;</span>
<span class="line">    &quot;cellApp&quot;: {</span>
<span class="line">        &quot;id&quot;: 5,</span>
<span class="line">        &quot;host&quot;: &quot;10.0.0.5&quot;,</span>
<span class="line">        &quot;port&quot;: 6005,</span>
<span class="line">        &quot;bounds&quot;: {</span>
<span class="line">            &quot;spaceID&quot;: 1,</span>
<span class="line">            &quot;minX&quot;: 256,</span>
<span class="line">            &quot;minY&quot;: 0,</span>
<span class="line">            &quot;maxX&quot;: 512,</span>
<span class="line">            &quot;maxY&quot;: 256</span>
<span class="line">        }</span>
<span class="line">    }</span>
<span class="line">}</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、动态空间调整" tabindex="-1"><a class="header-anchor" href="#七、动态空间调整"><span>七、动态空间调整</span></a></h2><h3 id="负载触发的边界调整" tabindex="-1"><a class="header-anchor" href="#负载触发的边界调整"><span>负载触发的边界调整</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">场景：CellApp2 负载过高（俯视图，XZ 平面）</span>
<span class="line"></span>
<span class="line">调整前：                         调整后：</span>
<span class="line">┌──────────────┬──────────────┐  ┌──────────────┬─────────┐</span>
<span class="line">│CellApp1      │CellApp2      │  │CellApp1      │CellApp2 │</span>
<span class="line">│负载: 500     │负载: 2000 ★  │  │负载: 1000    │负载: 1500│</span>
<span class="line">│50%           │50%           │  │60% ▲         │40% ▼    │</span>
<span class="line">└──────────────┴──────────────┘  └──────────────┴─────────┘</span>
<span class="line"></span>
<span class="line">           Z                           Z</span>
<span class="line">           ↑                           ↑</span>
<span class="line">     ┌─────┼─────┐              ┌─────┼─────┐</span>
<span class="line">     │  1  │  2  │              │  1  │  2  │</span>
<span class="line">     │500  │2000 │              │1000 │1500 │</span>
<span class="line">     │─────│─────│  → 调整 →    │─────│─────│</span>
<span class="line">     │  3  │  4  │              │  3  │  4  │</span>
<span class="line">     └─────┴─────┘              └─────┴─────┘</span>
<span class="line">           │                           │</span>
<span class="line">           └─────────→ X               └─────────→ X</span>
<span class="line"></span>
<span class="line">边界移动（虚线是新边界）：</span>
<span class="line">     ┌───────────┬───────────┐</span>
<span class="line">     │           │       │   │</span>
<span class="line">     │     1     │   2   │   │  ← 边界右移</span>
<span class="line">     │           │       │   │</span>
<span class="line">     ├───────────┼───────┴───┤</span>
<span class="line">     │           │           │</span>
<span class="line">     │     3     │     4     │</span>
<span class="line">     │           │           │</span>
<span class="line">     └───────────┴───────────┘</span>
<span class="line"></span>
<span class="line">Y 轴负载均衡（如果使用 3D 划分）：</span>
<span class="line"></span>
<span class="line">调整前（按高度分层）：        调整后：</span>
<span class="line">┌─────────────────────┐      ┌─────────────────────┐</span>
<span class="line">│ 高层: 负载 1500 ★   │      │ 高层: 负载 800      │</span>
<span class="line">│ ← 瓶颈               │      │ ← 降低分界线         │</span>
<span class="line">├─────────────────────┤      ├─────────────────────┤</span>
<span class="line">│ 低层: 负载 500       │      │ 低层: 负载 1200     │</span>
<span class="line">└─────────────────────┘      └─────────────────────┘</span>
<span class="line"></span>
<span class="line">     Y →                    Y →</span>
<span class="line">    200m ─────────        200m ─────────</span>
<span class="line">          高层组                 高层组</span>
<span class="line">    100m ─════════        150m ─════════  ← 分界线下移</span>
<span class="line">          地面组                 地面组</span>
<span class="line">      0m ─────────         0m ─────────</span>
<span class="line"></span>
<span class="line">流程：</span>
<span class="line">1. CellApp 定期上报负载（Entity 数量、CPU、内存）</span>
<span class="line">2. CellAppMgr 检测到负载不均衡</span>
<span class="line">3. CellAppMgr 计算新的 XZ 边界（或 Y 分界线）</span>
<span class="line">4. CellAppMgr 通知相关 CellApp 边界变化</span>
<span class="line">5. Entity 根据新边界迁移</span>
<span class="line">6. 稳定后继续监控</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="边界调整协议" tabindex="-1"><a class="header-anchor" href="#边界调整协议"><span>边界调整协议</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">struct</span> <span class="token class-name">BoundaryAdjustMessage</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> fromCellApp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> toCellApp<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 新边界</span></span>
<span class="line">    <span class="token keyword">float</span> newBoundaryX<span class="token punctuation">;</span>  <span class="token comment">// 或 newBoundaryY</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 迁移的 Entity 列表</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint64_t</span><span class="token operator">&gt;</span> entityIDs<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 时间戳</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、通信优化" tabindex="-1"><a class="header-anchor" href="#八、通信优化"><span>八、通信优化</span></a></h2><h3 id="批量-ghost-更新" tabindex="-1"><a class="header-anchor" href="#批量-ghost-更新"><span>批量 Ghost 更新</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 批量发送 Ghost 更新</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">GhostSyncManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">PendingUpdate</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> entityID<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> data<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 每个 CellApp 一个批量队列</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>PendingUpdate<span class="token operator">&gt;&gt;</span> pendingUpdates_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 添加更新（暂存）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">queueUpdate</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> targetCellApp<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> entityID<span class="token punctuation">,</span></span>
<span class="line">                     <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> data<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        pendingUpdates_<span class="token punctuation">[</span>targetCellApp<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">{</span>entityID<span class="token punctuation">,</span> data<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 定时批量发送（每 50ms）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">flush</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>targetCellApp<span class="token punctuation">,</span> updates<span class="token punctuation">]</span> <span class="token operator">:</span> pendingUpdates_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>updates<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                BatchGhostUpdate msg<span class="token punctuation">;</span></span>
<span class="line">                msg<span class="token punctuation">.</span>updates <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>updates<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">sendToCellApp</span><span class="token punctuation">(</span>targetCellApp<span class="token punctuation">,</span> msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        pendingUpdates_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="消息优先级" tabindex="-1"><a class="header-anchor" href="#消息优先级"><span>消息优先级</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">MessagePriority</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint8_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">    CRITICAL <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span>    <span class="token comment">// Entity 迁移、边界变化</span></span>
<span class="line">    HIGH <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span>        <span class="token comment">// 战斗相关、血量变化</span></span>
<span class="line">    NORMAL <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span>      <span class="token comment">// 位置更新</span></span>
<span class="line">    LOW <span class="token operator">=</span> <span class="token number">3</span>          <span class="token comment">// 非关键状态同步</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PriorityMessageQueue</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">enqueue</span><span class="token punctuation">(</span><span class="token keyword">const</span> Message<span class="token operator">&amp;</span> msg<span class="token punctuation">,</span> MessagePriority priority<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        queues_<span class="token punctuation">[</span>priority<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    Message <span class="token function">dequeue</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 按优先级出队</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> <span class="token number">4</span><span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>queues_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">auto</span> msg <span class="token operator">=</span> queues_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                queues_<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">return</span> msg<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">;</span>  <span class="token comment">// 空</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="九、故障处理" tabindex="-1"><a class="header-anchor" href="#九、故障处理"><span>九、故障处理</span></a></h2><h3 id="cellapp-故障检测" tabindex="-1"><a class="header-anchor" href="#cellapp-故障检测"><span>CellApp 故障检测</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">class</span> <span class="token class-name">FailureDetector</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>steady_clock<span class="token double-colon punctuation">::</span>time_point<span class="token operator">&gt;</span> lastSeen_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateHeartbeat</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> cellAppID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        lastSeen_<span class="token punctuation">[</span>cellAppID<span class="token punctuation">]</span> <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>steady_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> <span class="token function">detectFailures</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> failed<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> now <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span>steady_clock<span class="token double-colon punctuation">::</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>id<span class="token punctuation">,</span> lastTime<span class="token punctuation">]</span> <span class="token operator">:</span> lastSeen_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span> elapsed <span class="token operator">=</span> now <span class="token operator">-</span> lastTime<span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>elapsed <span class="token operator">&gt;</span> std<span class="token double-colon punctuation">::</span>chrono<span class="token double-colon punctuation">::</span><span class="token function">seconds</span><span class="token punctuation">(</span><span class="token number">15</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                failed<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> failed<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="故障恢复策略" tabindex="-1"><a class="header-anchor" href="#故障恢复策略"><span>故障恢复策略</span></a></h3><table><thead><tr><th>场景</th><th>策略</th></tr></thead><tbody><tr><td><strong>相邻 CellApp 故障</strong></td><td>冻结边界区域，等待恢复</td></tr><tr><td><strong>CellAppMgr 故障</strong></td><td>使用本地缓存，选举新 Mgr</td></tr><tr><td><strong>网络分区</strong></td><td>多数派继续服务，标记少数派为不可用</td></tr></tbody></table><hr><h2 id="十、参考资料" tabindex="-1"><a class="header-anchor" href="#十、参考资料"><span>十、参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine CellApp 通信机制</a></li><li><a href="https://www.bigworldtech.com/" target="_blank" rel="noopener noreferrer">BigWorld 空间分区算法</a></li><li><a href="https://dl.acm.org/doi/10.1145/3190508.3190521" target="_blank" rel="noopener noreferrer">动态负载均衡策略</a></li><li><a href="https://www.cs.cornell.edu/home/rvr/CS614/Files/fd.pdf" target="_blank" rel="noopener noreferrer">分布式系统故障检测</a></li></ul>`,61)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};