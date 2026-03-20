import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q4-seamless-world.html","title":"Q4: 如何实现大地图的无缝切换？如何处理跨服务器的玩家移动？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q4-seamless-world.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q4-seamless-world.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q4-如何实现大地图的无缝切换-如何处理跨服务器的玩家移动" tabindex="-1"><a class="header-anchor" href="#q4-如何实现大地图的无缝切换-如何处理跨服务器的玩家移动"><span>Q4: 如何实现大地图的无缝切换？如何处理跨服务器的玩家移动？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察大世界 MMORPG 的 <strong>无缝世界设计</strong>：</p><ul><li>大地图的无缝切换机制</li><li>跨 CellApp 的玩家移动处理</li><li>边界过渡的用户体验优化</li></ul><hr><h2 id="一、大地图无缝切换的概念" tabindex="-1"><a class="header-anchor" href="#一、大地图无缝切换的概念"><span>一、大地图无缝切换的概念</span></a></h2><h3 id="什么是无缝切换" tabindex="-1"><a class="header-anchor" href="#什么是无缝切换"><span>什么是无缝切换</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">非无缝 vs 无缝：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  非无缝切换（传统方式）                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────┐        ┌─────────┐                                 │</span>
<span class="line">│  │ 地图 A   │        │  地图 B   │                                 │</span>
<span class="line">│  │         │        │         │                                 │</span>
<span class="line">│  │  玩家   │        │  野外   │                                 │</span>
<span class="line">│  └────┬────┘        └────┬────┘                                 │</span>
<span class="line">│       │                  │                                     │</span>
<span class="line">│   传送门 ◄───────────────►                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  体验问题：                                                   │</span>
<span class="line">│  - 需要点击传送门/读取进入                                     │</span>
<span class="line">│  - 有加载画面（黑屏几秒）                                     │</span>
<span class="line">│  - 世界不连贯                                                │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  无缝切换（大世界方式）                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│    ┌─────────────────────────────────────────┐              │</span>
<span class="line">│    │         一个连续的大世界地图                  │              │</span>
<span class="line">│    │                                             │              │</span>
<span class="line">│    │  新手村    主城    野外    副本            │              │</span>
<span class="line">│    │    │       │       │       │                │              │</span>
<span class="line">│    │    └───────┴───────┴───────┴────────►        │              │</span>
<span class="line">│    │              玩家可以无缝走到任何地方          │              │</span>
<span class="line">│    │              没有加载画面                    │              │</span>
<span class="line">│    │              世界是连续的                      │              │</span>
<span class="line">│    └─────────────────────────────────────────┘              │</span>
<span class="line">│                                                             │</span>
<span class="line">│  体验优势：                                                   │</span>
<span class="line">│  - 自由探索，无需传送                                            │</span>
<span class="line">│  - 世界连贯，沉浸感强                                          │</span>
<span class="line">│  - 支持动态扩容                                                │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="技术挑战" tabindex="-1"><a class="header-anchor" href="#技术挑战"><span>技术挑战</span></a></h3><table><thead><tr><th>挑战</th><th>说明</th></tr></thead><tbody><tr><td><strong>空间划分</strong></td><td>如何将大地图划分到多个 CellApp</td></tr><tr><td><strong>边界处理</strong></td><td>玩家跨越 CellApp 边界时的过渡</td></tr><tr><td><strong>Entity 迁移</strong></td><td>如何无缝迁移 Entity 到新 CellApp</td></tr><tr><td><strong>状态同步</strong></td><td>如何保证迁移期间状态一致性</td></tr><tr><td><strong>客户端感知</strong></td><td>如何让客户端无感知</td></tr></tbody></table><hr><h2 id="二、空间划分策略" tabindex="-1"><a class="header-anchor" href="#二、空间划分策略"><span>二、空间划分策略</span></a></h2><h3 id="单一大地图-vs-多地图" tabindex="-1"><a class="header-anchor" href="#单一大地图-vs-多地图"><span>单一大地图 vs 多地图</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">传统多地图（魔兽世界早期设计）：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   LoginScreen → 选择角色 → 进入主城 → 传送副本 → 传送回城    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  每个地图是独立的实例：                                       │</span>
<span class="line">│  - 主城地图 = 一个或多个 CellApp（固定）                        │</span>
<span class="line">│  - 副本地图 = 动态创建的 Space，可以是任意 CellApp                │</span>
<span class="line">│  - 野外地图 = 一个或多个 CellApp（固定）                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  缺点：世界不连贯，需要传送                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">无缝大世界（Black Desert / Ark 等）：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   一个连续的大世界地图                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                                                         │   │</span>
<span class="line">│  │   新手村 ─────► 主城 ─────► 野外 ─────► 副本               │   │</span>
<span class="line">│  │     │           │          │          │                   │   │</span>
<span class="line">│  │     │           │          │          │                   │   │</span>
<span class="line">│  │     └───────────┴──────────┴──────────┘                   │   │</span>
<span class="line">│  │                                                         │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  地图被划分为多个 CellApp 管理：                               │</span>
<span class="line">│  - CellApp1: 新手村 + 主城                                     │</span>
<span class="line">│  - CellApp2: 野外西区                                           │</span>
<span class="line">│  - CellApp3: 野外东区                                           │</span>
<span class="line">│  - CellApp4: 副本区                                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  玩家可以无缝在这些区域间移动                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="cellapp-空间划分方案" tabindex="-1"><a class="header-anchor" href="#cellapp-空间划分方案"><span>CellApp 空间划分方案</span></a></h3><p><strong>方案 1：固定区域划分</strong></p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">大地图按区域划分给 CellApp：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      大地图 (5120x5120)                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌────────────┬────────────┬────────────┬────────────┐            │</span>
<span class="line">│   │CellApp1    │CellApp2    │CellApp3    │CellApp4    │            │</span>
<span class="line">│   │新手村+主城  │野外西区    │野外东区    │副本区      │            │</span>
<span class="line">│   │1280x1280  │1280x1280  │1280x1280  │1280x1280  │            │</span>
<span class="line">│   └────────────┴────────────┴────────────┴────────────┘            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优点：区域固定，配置简单                                      │</span>
<span class="line">│  缺点：某些区域过载，其他区域空闲（负载不均）                   │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>方案 2：动态负载均衡</strong></p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">根据负载动态调整边界：</span>
<span class="line"></span>
<span class="line">初始状态：</span>
<span class="line">┌───────────────┬───────────────┐</span>
<span class="line">│ CellApp1     │ CellApp2     │</span>
<span class="line">│ 负载: 60%    │ 负载: 40%    │</span>
<span class="line">│ 60% 地图     │ 40% 地图     │</span>
<span class="line">└───────────────┴───────────────┘</span>
<span class="line"></span>
<span class="line">主城玩家增多：</span>
<span class="line">┌───────────────┬───────────────┐</span>
<span class="line">│ CellApp1     │ CellApp2     │</span>
<span class="line">│ 负载: 95% ★   │ 负载: 30%    │</span>
<span class="line">│ 80% 地图     │ 20% 地图     │</span>
<span class="line">└───────────────┴───────────────┘</span>
<span class="line">        │ 调整</span>
<span class="line">        ▼</span>
<span class="line">┌───────────────┬───────────────┐</span>
<span class="line">│ CellApp1     │ CellApp2     │</span>
<span class="line">│ 负载: 75%    │ �载: 50%    │</span>
<span class="line">│ 70% 地板 ▲    │ 30% 地板 ▲    │</span>
<span class="line">└───────────────┴───────────────┘</span>
<span class="line"></span>
<span class="line">边界移动 → Entity 迁移</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、跨-cellapp-玩家移动流程" tabindex="-1"><a class="header-anchor" href="#三、跨-cellapp-玩家移动流程"><span>三、跨 CellApp 玩家移动流程</span></a></h2><h3 id="核心机制" tabindex="-1"><a class="header-anchor" href="#核心机制"><span>核心机制</span></a></h3>`,22),i(d,{code:`eJyVVFFv0lAYfedXfI8jQRzotoSHJQWN8UWN/6BZGiVBQKjGvRUzWWWMMUWNsjjdnMUodEsU2AD5L2b39vLEX/C7vYW0BUzsS5Oe0/Odc77b5pXHT5T0hnIjKT/IyY8CgFdWzqnJjWRWTquQADkPpHVE9Y713ZyB4xLH43JekbLZq/dymWebsxJShJMSSiqFpAgs0XdGcA4r6mJFkfX2NBiwaXcyqgKZp0oOEiGkxSASBto8Jt2uZfRIqTHu6+SgQarly642EQBSfDHuv7TfT1xZX49LMRBsZnbo2XMbiEuIoL0YsMEPsrc/Q0BswqD1n+jocrBrDVoujOuyYY3UPwpI0FzqMSDnQ/qqKWCr954ZxdF2Be1ZlW+k1fbPke7eBnqs0V87435ZqJE9kxkFqhkL6ohiHZUTNtxnv8+tN2WsY3S0RUqf2GAwvwKin8JSdGUlBMshwLvYxvw2ZnqwvaGCe6SHhI6IXie9C7j1MJNX4WZaTaqbc135KqR6lZQOPeWJPZPXu+SitiD+tTCwToO1S9P4YiKKFjDDPytYcypYC8KfYhW4yHDblek/OxkdaOxrQcwVbt2l9DVi7AjQc8aivtruK3LKac3D4C7st0mrjFW5B/BgI+2DdXjCOqZVO/MfQ46LZwJnn0UIJx+fXa1YvSZt67Rgiqy8IoS8OdDDqKZRs4AfsdfjNKfAxfIXrOx6GP8pPINT1aL9rOJy+H5Wlz1HFBWmFjHS5Jv31+mcnS9bVrUY+At9qhgl`}),o[1]||=e(`<h3 id="边界检测算法" tabindex="-1"><a class="header-anchor" href="#边界检测算法"><span>边界检测算法</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">class</span> <span class="token class-name">CellApp</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    SpaceBounds bounds_<span class="token punctuation">;</span>  <span class="token comment">// 自己的空间边界</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 检查是否需要迁移</span></span>
<span class="line">    MigrationCheck <span class="token function">checkMigration</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Position pos <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 已离开当前空间</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>bounds_<span class="token punctuation">.</span><span class="token function">contains</span><span class="token punctuation">(</span>pos<span class="token punctuation">.</span>x<span class="token punctuation">,</span> pos<span class="token punctuation">.</span>z<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 找到目标 CellApp</span></span>
<span class="line">            CellApp<span class="token operator">*</span> target <span class="token operator">=</span> <span class="token function">findTargetCellApp</span><span class="token punctuation">(</span>pos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token function">Migrate</span><span class="token punctuation">(</span>target<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 在边界区域，检查移动趋势</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isNearBoundary</span><span class="token punctuation">(</span>pos<span class="token punctuation">,</span> <span class="token number">50.0f</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Velocity vel <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getVelocity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            Position futurePos <span class="token operator">=</span> pos <span class="token operator">+</span> vel <span class="token operator">*</span> <span class="token number">2.0f</span><span class="token punctuation">;</span>  <span class="token comment">// 2秒后位置</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>bounds_<span class="token punctuation">.</span><span class="token function">contains</span><span class="token punctuation">(</span>futurePos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                CellApp<span class="token operator">*</span> target <span class="token operator">=</span> <span class="token function">findTargetCellApp</span><span class="token punctuation">(</span>futurePos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">return</span> <span class="token function">PreloadGhost</span><span class="token punctuation">(</span>target<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 预加载 Ghost</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> None<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isNearBoundary</span><span class="token punctuation">(</span>Position pos<span class="token punctuation">,</span> <span class="token keyword">float</span> threshold<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span>pos<span class="token punctuation">.</span>x <span class="token operator">-</span> bounds_<span class="token punctuation">.</span>minX <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span> <span class="token operator">||</span></span>
<span class="line">               <span class="token punctuation">(</span>bounds_<span class="token punctuation">.</span>maxX <span class="token operator">-</span> pos<span class="token punctuation">.</span>x <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span> <span class="token operator">||</span></span>
<span class="line">               <span class="token punctuation">(</span>pos<span class="token punctuation">.</span>z <span class="token operator">-</span> bounds_<span class="token punctuation">.</span>minZ <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span> <span class="token operator">||</span></span>
<span class="line">               <span class="token punctuation">(</span>bounds_<span class="token punctuation">.</span>maxZ <span class="token operator">-</span> pos<span class="token punctuation">.</span>z <span class="token operator">&lt;</span> threshold<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、entity-迁移机制" tabindex="-1"><a class="header-anchor" href="#四、entity-迁移机制"><span>四、Entity 迁移机制</span></a></h2><h3 id="迁移触发条件" tabindex="-1"><a class="header-anchor" href="#迁移触发条件"><span>迁移触发条件</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">迁移触发的三种情况：</span>
<span class="line"></span>
<span class="line">1. 主动迁移（玩家移动跨边界）</span>
<span class="line">   玩家从 CellApp1 移动到 CellApp2</span>
<span class="line">   ↓</span>
<span class="line">   触发 Entity 迁移</span>
<span class="line"></span>
<span class="line">2. 负载均衡迁移</span>
<span class="line">   CellApp1 负载过高</span>
<span class="line">   ↓</span>
<span class="line">   CellAppMgr 决定迁移部分玩家到 CellApp2</span>
<span class="line"></span>
<span class="line">3. 故障迁移</span>
<span class="line">   CellApp1 即将宕机</span>
<span class="line">   ↓</span>
<span class="line">   迁移玩家到其他 CellApp</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="entity-迁移详细流程" tabindex="-1"><a class="header-anchor" href="#entity-迁移详细流程"><span>Entity 迁移详细流程</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 迁移请求</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EntityMigration</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 源 CellApp 发起迁移</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">migrateTo</span><span class="token punctuation">(</span>CellApp<span class="token operator">*</span> targetCellApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 冻结 Entity 状态</span></span>
<span class="line">        <span class="token function">freeze</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 序列化状态</span></span>
<span class="line">        MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">serializeTo</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 发送到目标 CellApp</span></span>
<span class="line">        targetCellApp<span class="token operator">-&gt;</span><span class="token function">receiveEntity</span><span class="token punctuation">(</span></span>
<span class="line">            <span class="token function">getEntityID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            stream<span class="token punctuation">.</span><span class="token function">getData</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            stream<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 等待确认</span></span>
<span class="line">        <span class="token comment">// ... 等待目标 CellApp 完成</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 目标 CellApp 接收</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">receiveEntity</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">,</span> <span class="token keyword">const</span> <span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 反序列化创建 Entity</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">deserializeEntity</span><span class="token punctuation">(</span>id<span class="token punctuation">,</span> data<span class="token punctuation">,</span> size<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 添加到 AOI 系统</span></span>
<span class="line">        coordinateSystem_<span class="token operator">-&gt;</span><span class="token function">insert</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 恢复状态</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">unfreeze</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 通知源 CellApp</span></span>
<span class="line">        sourceCellApp<span class="token operator">-&gt;</span><span class="token function">onMigrationComplete</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="迁移过程中的状态处理" tabindex="-1"><a class="header-anchor" href="#迁移过程中的状态处理"><span>迁移过程中的状态处理</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">迁移期间的状态处理：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   迁移时间轴：                                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│   T0: CellApp1                                               │</span>
<span class="line">│   ├─ Entity (Real) ──────► Ghost 创建到 CellApp2               │</span>
<span class="line">│   │                   ──► 客户端开始接收 CellApp2 的消息         │</span>
<span class="line">│   │                                                            │</span>
<span class="line">│   T1: 迁移中                                                  │</span>
<span class="line">│   ├─ CellApp1 Entity: 冻结，不接受新操作                         │</span>
<span class="line">│   ├─ CellApp2 Entity: 激活，开始处理操作                          │</span>
<span class="line">│   ├─ 客户端: 同时接收两个 CellApp 的消息（平滑过渡）                 │</span>
<span class="line">│   │                                                            │</span>
<span class="line">│   T2: 迁移完成                                                │</span>
<span class="line">│   ├─ CellApp1: 销毁 Entity                                   │</span>
<span class="line">│   ├─ CellApp2: Entity 成为唯一 Real                             │</span>
<span class="line">│   ├─ 客户端: 只接收 CellApp2 的消息                             │</span>
<span class="line">│   │                                                            │</span>
<span class="line">│   └────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  关键点：平滑过渡，避免卡顿                                    │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、客户端无感知优化" tabindex="-1"><a class="header-anchor" href="#五、客户端无感知优化"><span>五、客户端无感知优化</span></a></h2><h3 id="_1-边界预加载" tabindex="-1"><a class="header-anchor" href="#_1-边界预加载"><span>1. 边界预加载</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">玩家接近边界时，提前准备：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│  CellApp1                   CellApp2                         │</span>
<span class="line">│     │                           │                          │</span>
<span class="line">│     │  Ghost                    │                          │</span>
<span class="line">│     ├──────────────────────►│  预创建边界附近 Entity 的   │</span>
<span class="line">│     │                         │  Ghost，让客户端提前加载     │</span>
<span class="line">│     │                         │                          │</span>
<span class="line">│     │    玩家 ─────────────►│                          │</span>
<span class="line">│     │    移动到边界          │                          │</span>
<span class="line">│     │                         │                          │</span>
<span class="line">│     ▼                         ▼                          │</span>
<span class="line">│   正在迁移                  接收新玩家                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">客户端感受：</span>
<span class="line">- 没有&quot;正在连接到服务器...&quot;的等待</span>
<span class="line">- 没有明显的卡顿</span>
<span class="line">- 流畅的过渡</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-双向缓冲" tabindex="-1"><a class="header-anchor" href="#_2-双向缓冲"><span>2. 双向缓冲</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 迁移期间的双向缓冲</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MigrationBuffer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    EntityState state_<span class="token punctuation">;</span>        <span class="token comment">// 当前状态</span></span>
<span class="line">    EntityState shadowState_<span class="token punctuation">;</span> <span class="token comment">// 影子状态</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 迁移开始</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">beginMigration</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 保存快照</span></span>
<span class="line">        shadowState_ <span class="token operator">=</span> state_<span class="token punctuation">.</span><span class="token function">snapshot</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 双写期间</span></span>
<span class="line">        doubleWrite_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收输入</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onReceiveInput</span><span class="token punctuation">(</span>Input input<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>doubleWrite_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 写入两个状态</span></span>
<span class="line">            state_<span class="token punctuation">.</span><span class="token function">apply</span><span class="token punctuation">(</span>input<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            shadowState_<span class="token punctuation">.</span><span class="token function">apply</span><span class="token punctuation">(</span>input<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            state_<span class="token punctuation">.</span><span class="token function">apply</span><span class="token punctuation">(</span>input<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 迁移完成</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">endMigration</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        doubleWrite_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        shadowState_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-预测补偿" tabindex="-1"><a class="header-anchor" href="#_3-预测补偿"><span>3. 预测补偿</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">客户端预测迁移：</span>
<span class="line"></span>
<span class="line">// 客户端逻辑</span>
<span class="line">class ClientMovement {</span>
<span class="line">public:</span>
<span class="line">    void move(const Position&amp; target) {</span>
<span class="line">        // 检测是否接近边界</span>
<span class="line">        if (isNearBoundary(target)) {</span>
<span class="line">            // 预测可能需要迁移</span>
<span class="line">            CellApp* targetApp = predictTargetCellApp(target);</span>
<span class="line"></span>
<span class="line">            // 提前连接到目标 CellApp</span>
<span class="line">            connectToCellApp(targetApp);</span>
<span class="line">        }</span>
<span class="line"></span>
<span class="line">        // 发送移动请求</span>
<span class="line">        sendMoveRequest(target);</span>
<span class="line"></span>
<span class="line">        // 客户端预测移动</span>
<span class="line">        localPosition_ = target;</span>
<span class="line">    }</span>
<span class="line">};</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、ghost-机制在边界的作用" tabindex="-1"><a class="header-anchor" href="#六、ghost-机制在边界的作用"><span>六、Ghost 机制在边界的作用</span></a></h2><h3 id="ghost-的作用" tabindex="-1"><a class="header-anchor" href="#ghost-的作用"><span>Ghost 的作用</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Ghost 在边界的三种状态：</span>
<span class="line"></span>
<span class="line">状态 1：正常（远离边界）</span>
<span class="line">┌─────────────────┐              ┌─────────────────┐</span>
<span class="line">│   CellApp1     │              │   CellApp2     │</span>
<span class="line">│                 │              │                 │</span>
<span class="line">│   Real Entity   │              │     无          │</span>
<span class="line">│                 │              │                 │</span>
<span class="line">└─────────────────┘              └─────────────────┘</span>
<span class="line"></span>
<span class="line">状态 2：边界区域（预加载 Ghost）</span>
<span class="line">┌─────────────────┐              ┌─────────────────┐</span>
<span class="line">│   CellApp1     │              │   CellApp2     │</span>
<span class="line">│                 │              │                 │</span>
<span class="line">│   Real Entity   │◄────────────►│   Ghost Entity   │</span>
<span class="line">│                 │  预加载        │                 │</span>
<span class="line">└─────────────────┘              └─────────────────┘</span>
<span class="line"></span>
<span class="line">状态 3：迁移中（双向可见）</span>
<span class="line">┌─────────────────┐              ┌─────────────────┐</span>
<span class="line">│   CellApp1     │              │   CellApp2     │</span>
<span class="line">│                 │              │                 │</span>
<span class="line">│   Ghost Entity   │◄────────────►│   Real Entity   │</span>
<span class="line">│   (只读)        │  迁移中        │   (可写)        │</span>
<span class="line">└─────────────────┘              └─────────────────┘</span>
<span class="line"></span>
<span class="line">状态 4：迁移完成</span>
<span class="line">┌─────────────────┐              ┌─────────────────┐</span>
<span class="line">│   CellApp1     │              │   CellApp2     │</span>
<span class="line">│                 │              │                 │</span>
<span class="line">│     无          │              │   Real Entity   │</span>
<span class="line">│                 │              │                 │</span>
<span class="line">└─────────────────┘              └─────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="ghost-的创建和销毁" tabindex="-1"><a class="header-anchor" href="#ghost-的创建和销毁"><span>Ghost 的创建和销毁</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Ghost 管理</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">GhostManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 创建 Ghost（预加载）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">createGhost</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> CellApp<span class="token operator">*</span> targetApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 创建只读副本</span></span>
<span class="line">        GhostEntity<span class="token operator">*</span> ghost <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">GhostEntity</span><span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">getData</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 发送到目标 CellApp</span></span>
<span class="line">        targetApp<span class="token operator">-&gt;</span><span class="token function">addGhost</span><span class="token punctuation">(</span>ghost<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 更新 AOI</span></span>
<span class="line">        ghost<span class="token operator">-&gt;</span><span class="token function">enterAOI</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 销毁 Ghost（迁移完成后）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">destroyGhost</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> CellApp<span class="token operator">*</span> oldApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 通知旧 CellApp 销毁 Ghost</span></span>
<span class="line">        oldApp<span class="token operator">-&gt;</span><span class="token function">removeGhost</span><span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">getID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、不同场景的实现" tabindex="-1"><a class="header-anchor" href="#七、不同场景的实现"><span>七、不同场景的实现</span></a></h2><h3 id="场景-1-同一-space-内跨-cellapp" tabindex="-1"><a class="header-anchor" href="#场景-1-同一-space-内跨-cellapp"><span>场景 1：同一 Space 内跨 CellApp</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  单一大世界 Space                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────┐  │</span>
<span class="line">│  │                                                         │  │</span>
<span class="line">│  │   CellApp1        CellApp2        CellApp3            │  │</span>
<span class="line">│  │     ├──────────────┼──────────────┐                   │  │</span>
<span class="line">│  │     │              │              │                   │  │</span>
<span class="line">│  │  ┌────┴────┐  ┌────┴────┐  ┌────┴────┐                  │  │</span>
<span class="line">│  │  │EntityA │  │EntityB │  │EntityC │                  │  │</span>
<span class="line">│  │  └─────┬───┘  └─────┬───┘  └─────┬───┘                  │  │</span>
<span class="line">│  │        │         │         │        │                  │  │</span>
<span class="line">│  │        └─────────┴─────────┘        │                  │  │</span>
<span class="line">│  │                Entity 迁移时           │                  │  │</span>
<span class="line">│  │                    │             │                   │  │</span>
<span class="line">│  │  ┌───────────────────────────────┐                   │  │</span>
<span class="line">│  │  │ 障形边界（CellApp 边界）      │                   │  │</span>
<span class="line">│  │  └───────────────────────────────┘                   │  │</span>
<span class="line">│  └─────────────────────────────────────────────────────────┘  │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">EntityA 从 CellApp1 移动到 CellApp2：</span>
<span class="line">1. CellApp1 检测到 EntityA 跨越边界</span>
<span class="line">2. CellApp1 向 CellApp2 发起迁移</span>
<span class="line">3. CellApp2 创建 Real Entity</span>
<span class="line">4. 客户端平滑过渡</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="场景-2-不同-space-之间-传送" tabindex="-1"><a class="header-anchor" href="#场景-2-不同-space-之间-传送"><span>场景 2：不同 Space 之间（传送）</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    Space 1 (主城)                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────┐  │</span>
<span class="line">│  │                   CellApp1                              │  │</span>
<span class="line">│  │  ┌─────────┐                                               │  │</span>
<span class="line">│  │  │玩家A    │                                               │  │</span>
<span class="line">│  │  └────┬────┘                                               │  │</span>
<span class="line">│  │       │                                                    │  │</span>
<span class="line">│  │       │ 点击副本入口 NPC                                      │  │</span>
<span class="line">│  │       │                                                    │  │</span>
<span class="line">└────────┼──────────────────────────────────────────────────────┘</span>
<span class="line">         │</span>
<span class="line">         │ 传送（不是物理移动）</span>
<span class="line">         ▼</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    Space 2 (副本)                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────┐  │</span>
<span class="line">│  │                   CellApp2                              │  │</span>
<span class="line">│  │  ┌─────────┐                                               │  │</span>
<span class="line">│  │  │玩家A    │  ← 新创建的 Entity（不是迁移）                   │  │</span>
<span class="line">│  │  └─────────┘                                               │  │</span>
<span class="line">│  └─────────────────────────────────────────────────────────┘  │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">不同 Space 之间是传送，不是无缝移动：</span>
<span class="line">- 旧的 Entity 被销毁</span>
<span class="line">- 创建新的 Entity</span>
<span class="line">- 位置重置到副本入口</span>
<span class="line">- 客户端会有短暂的加载画面</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、技术难点与解决方案" tabindex="-1"><a class="header-anchor" href="#八、技术难点与解决方案"><span>八、技术难点与解决方案</span></a></h2><h3 id="难点-1-迁移期间的消息一致性" tabindex="-1"><a class="header-anchor" href="#难点-1-迁移期间的消息一致性"><span>难点 1：迁移期间的消息一致性</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题：迁移期间玩家可能同时发送操作</span>
<span class="line"></span>
<span class="line">解决方案：消息重定向</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   T0: 玩家在 CellApp1，发送攻击指令                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│   T1: 开始迁移                                               │</span>
<span class="line">│       ├─ CellApp1: 停止接收新操作，转发到 CellApp2              │</span>
<span class="line">│       └─ 客户端: 暂存操作到队列                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│   T2: 迁移完成                                               │</span>
<span class="line">│       ├─ CellApp2: 开始接收操作，处理队列中的操作                 │</span>
<span class="line">│       └─ 客户端: 发送队列中的操作                               │</span>
<span class="line">│                                                             │</span>
<span class="line">│   代码示例：                                                   │</span>
<span class="line">│   class MessageRedirector {                                  │</span>
<span class="line">│       std::queue&lt;Message&gt; pendingQueue_;                       │</span>
<span class="line">│       bool migrating_ = false;                                  │</span>
<span class="line">│       CellApp* targetApp_;                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│       void onReceiveMessage(Message&amp; msg) {                     │</span>
<span class="line">│           if (migrating_) {                                     │</span>
<span class="line">│               pendingQueue_.push(msg);                       │</span>
<span class="line">│           } else {                                               │</span>
<span class="line">│               processMessage(msg);                           │</span>
<span class="line">│           }                                                    │</span>
<span class="line">│       }                                                      │</span>
<span class="line">│   };                                                          │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="难点-2-客户端卡顿优化" tabindex="-1"><a class="header-anchor" href="#难点-2-客户端卡顿优化"><span>难点 2：客户端卡顿优化</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题：迁移期间客户端可能卡顿</span>
<span class="line"></span>
<span class="line">解决方案：多线程加载</span>
<span class="line"></span>
<span class="line">// 客户端实现</span>
<span class="line">class AsyncLoader {</span>
<span class="line">public:</span>
<span class="line">    // 预加载目标 CellApp 的资源</span>
<span class="line">    void preloadTarget(CellApp* targetApp) {</span>
<span class="line">        // 后台线程加载目标场景资源</span>
<span class="line">        std::thread([targetApp]() {</span>
<span class="line">            targetApp-&gt;loadSceneData();</span>
<span class="line">            targetApp-&gt;loadEntityData();</span>
<span class="line">        }).detach();</span>
<span class="line">    }</span>
<span class="line"></span>
<span class="line">    // 平滑过渡</span>
<span class="line">    void smoothTransition(Entity* oldEntity, Entity* newEntity) {</span>
<span class="line">        // 双端都显示一段时间</span>
<span class="line">        oldEntity-&gt;setAlpha(1.0f);</span>
<span class="line">        newEntity-&gt;setAlpha(0.0f);</span>
<span class="line"></span>
<span class="line">        // 渐变淡入淡出</span>
<span class="line">        for (int i = 0; i &lt; 30; ++i) {</span>
<span class="line">            float alpha = i / 30.0f;</span>
<span class="line">            oldEntity-&gt;setAlpha(1.0f - alpha);</span>
<span class="line">            newEntity-&gt;setAlpha(alpha);</span>
<span class="line">            std::this_thread::sleep_for(std::chrono::milliseconds(16));</span>
<span class="line">        }</span>
<span class="line"></span>
<span class="line">        oldEntity-&gt;destroy();</span>
<span class="line">    }</span>
<span class="line">};</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="难点-3-边界回弹问题" tabindex="-1"><a class="header-anchor" href="#难点-3-边界回弹问题"><span>难点 3：边界回弹问题</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题：玩家在边界反复横跳</span>
<span class="line"></span>
<span class="line">解决方案：边界滞后</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   CellApp1        边界线        CellApp2                 │</span>
<span class="line">│       │ ◄─────────────┼────────────► │                      │</span>
<span class="line">│       │                 │             │                      │</span>
<span class="line">│   ┌───┴────┐         ┌───────┴─────┐                       │</span>
<span class="line">│   │ 玩家   │         │   Ghost   │  ← 在边界附近            │</span>
<span class="line">│ │        │         │        │  │  加载了 Ghost        │</span>
<span class="line">│ └────────┘         └─────────────┘                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│   滞后策略：                                                   │</span>
<span class="line">│   - 进入边界距离：50m                                       │</span>
<span class="line">│   - 离开边界距离：60m（滞后）                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│   ─────────────────────────────────────────────               │</span>
<span class="line">│     50m    10m    │    50m    60m                       │</span>
<span class="line">│   ←────────────┼─────────────→                           │</span>
<span class="line">│     CellApp1      │      CellApp2                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  玩家在 10m 缓冲区内时，即使往回走也不会立即迁移回来         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="九、完整的跨边界移动示例" tabindex="-1"><a class="header-anchor" href="#九、完整的跨边界移动示例"><span>九、完整的跨边界移动示例</span></a></h2><h3 id="完整代码流程" tabindex="-1"><a class="header-anchor" href="#完整代码流程"><span>完整代码流程</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// CellApp 边界处理</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BoundaryHandler</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onEntityMove</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> newPos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 检查是否在边界区域</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isInBoundaryZone</span><span class="token punctuation">(</span>newPos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">handleBoundaryZone</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> newPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token comment">// 2. 检查是否需要迁移</span></span>
<span class="line">        <span class="token keyword">else</span> <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">shouldMigrate</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> newPos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">initiateMigration</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> newPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleBoundaryZone</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> pos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 在边界区域，检查是否需要预加载 Ghost</span></span>
<span class="line">        CellApp<span class="token operator">*</span> neighbor <span class="token operator">=</span> <span class="token function">findNeighborInDirection</span><span class="token punctuation">(</span>pos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>neighbor <span class="token operator">&amp;&amp;</span> <span class="token operator">!</span><span class="token function">hasGhost</span><span class="token punctuation">(</span>neighbor<span class="token punctuation">,</span> entity<span class="token operator">-&gt;</span><span class="token function">getID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">createGhostFor</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> neighbor<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">shouldMigrate</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> pos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 已经离开当前空间</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>bounds_<span class="token punctuation">.</span><span class="token function">contains</span><span class="token punctuation">(</span>pos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token comment">// 正在快速离开边界</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isLeavingFast</span><span class="token punctuation">(</span>pos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">initiateMigration</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> pos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        CellApp<span class="token operator">*</span> target <span class="token operator">=</span> <span class="token function">findTargetCellApp</span><span class="token punctuation">(</span>pos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 找不到目标 CellApp，返回</span></span>
<span class="line">            <span class="token function">handleMigrationFailed</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 开始迁移流程</span></span>
<span class="line">        <span class="token class-name">MigrationManager</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">migrate</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> target<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 迁移管理器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MigrationManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">migrate</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> CellApp<span class="token operator">*</span> targetApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 冻结源 Entity</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">freeze</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 序列化</span></span>
<span class="line">        MemoryStream stream <span class="token operator">=</span> <span class="token function">serializeEntity</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 发送到目标</span></span>
<span class="line">        targetApp<span class="token operator">-&gt;</span><span class="token function">receiveEntity</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 创建 Ghost（平滑过渡）</span></span>
<span class="line">        targetApp<span class="token operator">-&gt;</span><span class="token function">createGhost</span><span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">getID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 通知客户端</span></span>
<span class="line">        <span class="token function">notifyClientMigration</span><span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">getClientID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> targetApp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 6. 等待完成</span></span>
<span class="line">        <span class="token function">waitForCompletion</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="十、性能优化" tabindex="-1"><a class="header-anchor" href="#十、性能优化"><span>十、性能优化</span></a></h2><h3 id="优化-1-批量迁移" tabindex="-1"><a class="header-anchor" href="#优化-1-批量迁移"><span>优化 1：批量迁移</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">场景：大量玩家同时跨边界（如国战）</span>
<span class="line"></span>
<span class="line">不优化：</span>
<span class="line">玩家1 跨边界 → 迁移1</span>
<span class="line">玩家2 跨边界 → 迁移2</span>
<span class="line">...</span>
<span class="line">玩家N 跨边界 → 迁移N</span>
<span class="line"></span>
<span class="line">优化后：</span>
<span class="line">检测到批量跨边界 → 批量迁移</span>
<span class="line">                    ↓</span>
<span class="line">CellApp1 → CellApp2: [玩家1, 玩家5, 玩家9, ...]</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="优化-2-边界缓存" tabindex="-1"><a class="header-anchor" href="#优化-2-边界缓存"><span>优化 2：边界缓存</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">class</span> <span class="token class-name">BoundaryCache</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 缓存边界附近的 Entity</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">BoundaryEntity</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity<span class="token punctuation">;</span></span>
<span class="line">        Position lastKnownPosition<span class="token punctuation">;</span></span>
<span class="line">        Timestamp lastUpdate<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>BoundaryEntity<span class="token operator">&gt;</span> cached_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 定期更新缓存</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateCache</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> entry <span class="token operator">:</span> cached_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            entry<span class="token punctuation">.</span>lastKnownPosition <span class="token operator">=</span> entry<span class="token punctuation">.</span>entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            entry<span class="token punctuation">.</span>lastUpdate <span class="token operator">=</span> <span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="优化-3-异步迁移" tabindex="-1"><a class="header-anchor" href="#优化-3-异步迁移"><span>优化 3：异步迁移</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">同步迁移的问题：</span>
<span class="line"></span>
<span class="line">CellApp1:           CellApp2:</span>
<span class="line">    Real Entity  ────────►  创建 Real</span>
<span class="line">    │                  等待确认</span>
<span class="line">    ▼                  ▼</span>
<span class="line">    冻结              完成</span>
<span class="line"></span>
<span class="line">问题：迁移期间 CellApp1 停止处理该玩家</span>
<span class="line"></span>
<span class="line">异步迁移：</span>
<span class="line"></span>
<span class="line">CellApp1:           CellApp2:</span>
<span class="line">    Real Entity  ────► Shadow ──┐  创建 Real</span>
<span class="line">    │                            │  │</span>
<span class="line">    │  继续处理                │  │</span>
<span class="line">    │  ┌─────────────────────┘ │</span>
<span class="line">    │  └─────────────────────►│  同步状态</span>
<span class="line">    │                            ↓</span>
<span class="line">    │                        完成</span>
<span class="line">    ▼</span>
<span class="line">    销毁 Real</span>
<span class="line"></span>
<span class="line">优势：玩家感知更流畅</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="十一、参考资料" tabindex="-1"><a class="header-anchor" href="#十一、参考资料"><span>十一、参考资料</span></a></h2><ul><li><a href="https://www.bigworldtech.com/" target="_blank" rel="noopener noreferrer">BigWorld 无缝世界设计</a></li><li><a href="https://www.kbelab.com/guide/space/" target="_blank" rel="noopener noreferrer">KBEngine Space 管理</a></li><li><a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine Entity 迁移</a></li><li><a href="https://developer.valvesoftware.com/documentation/player-connection-and-shadow-migrating/" target="_blank" rel="noopener noreferrer">Shadow Entity 预测机制</a></li><li><a href="https://gafferongames.com/post/snapshot_interpolation/" target="_blank" rel="noopener noreferrer">网络延迟补偿技术</a></li></ul>`,53)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};