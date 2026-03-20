import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q5-dynamic-scaling.html","title":"Q5: 服务端如何设计才能支持动态扩容？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q5-dynamic-scaling.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q5-dynamic-scaling.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q5-服务端如何设计才能支持动态扩容" tabindex="-1"><a class="header-anchor" href="#q5-服务端如何设计才能支持动态扩容"><span>Q5: 服务端如何设计才能支持动态扩容？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察服务端 <strong>动态扩容（Dynamic Scaling）</strong> 的设计：</p><ul><li>如何架构才能支持水平扩展</li><li>负载均衡策略</li><li>KBEngine/BigWorld 的具体实现和源码证据</li></ul><hr><h2 id="一、动态扩容的核心概念" tabindex="-1"><a class="header-anchor" href="#一、动态扩容的核心概念"><span>一、动态扩容的核心概念</span></a></h2><h3 id="什么是动态扩容" tabindex="-1"><a class="header-anchor" href="#什么是动态扩容"><span>什么是动态扩容</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">静态扩容 vs 动态扩容：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  静态扩容（传统方式）                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  服务器 A (固定) ──── 负载达到上限 ──── 停服维护 ───→        │</span>
<span class="line">│  增加服务器 B ──── 重启服务 ──── 恢复                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题：                                                       │</span>
<span class="line">│  - 需要停服维护                                               │</span>
<span class="line">│  - 扩容不灵活                                               │</span>
<span class="line">│  - 资源浪费（低峰期）                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  动态扩容（现代方式）                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  服务器 A + B + C (池化)                                     │</span>
<span class="line">│       │                                                      │</span>
<span class="line">│       │  负载上升 ────→ 自动增加服务器 D                      │</span>
<span class="line">│       │  负载下降 ────→ 自动移除服务器 D                      │</span>
<span class="line">│       │                                                      │</span>
<span class="line">│  优势：                                                       │</span>
<span class="line">│  - 无需停服                                                 │</span>
<span class="line">│  - 按需扩容/缩容                                             │</span>
<span class="line">│  - 成本优化                                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="扩容的两种类型" tabindex="-1"><a class="header-anchor" href="#扩容的两种类型"><span>扩容的两种类型</span></a></h3><table><thead><tr><th>类型</th><th>说明</th><th>实现难度</th></tr></thead><tbody><tr><td><strong>垂直扩容（Scale Up）</strong></td><td>增加单机配置（CPU/内存）</td><td>简单</td></tr><tr><td><strong>水平扩容（Scale Out）</strong></td><td>增加服务器数量</td><td>复杂</td></tr></tbody></table><hr><h2 id="二、kbengine-的动态扩容架构" tabindex="-1"><a class="header-anchor" href="#二、kbengine-的动态扩容架构"><span>二、KBEngine 的动态扩容架构</span></a></h2><h3 id="官方设计理念" tabindex="-1"><a class="header-anchor" href="#官方设计理念"><span>官方设计理念</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine GitHub</a> 官方描述：</p><blockquote><p><strong>底层架构被设计为多进程分布式动态负载均衡方案。理论上只需要不断扩展硬件就能够不断增加承载上限。单台机器的承载上限取决于游戏逻辑本身的复杂度。</strong></p></blockquote><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 扩容架构：</span>
<span class="line"></span>
<span class="line">                    ┌─────────────────────────────────────┐</span>
<span class="line">                    │         KBEngine 集群                 │</span>
<span class="line">                    │                                     │</span>
<span class="line">                    │  ┌──────────┐  ┌──────────┐         │</span>
<span class="line">                    │  │ LoginApp │  │ BaseApp  │         │</span>
<span class="line">                    │  │  (可扩容) │  │  (可扩容) │         │</span>
<span class="line">                    │  └──────────┘  └────┬─────┘         │</span>
<span class="line">                    │                      │              │</span>
<span class="line">                    │  ┌─────────────────────┴───────┐     │</span>
<span class="line">                    │  │        CellAppMgr           │     │</span>
<span class="line">                    │  │    (负载均衡协调器) ★        │     │</span>
<span class="line">                    │  └─────────────────────┬───────┘     │</span>
<span class="line">                    │                        │              │</span>
<span class="line">                    │     ┌──────────────────┼──────────┐  │</span>
<span class="line">                    │     │                  │          │  │</span>
<span class="line">                    │  ┌───▼────┐  ┌────────▼──┐  ┌───▼──┐│</span>
<span class="line">                    │  │CellApp1│  │  CellApp2 │  │Cell..││</span>
<span class="line">                    │  │ 负载30%│  │  负载60%  │  │ 负载..││</span>
<span class="line">                    │  └────────┘  └─────┬─────┘  └──────┘│</span>
<span class="line">                    │                     │               │</span>
<span class="line">                    └─────────────────────┴───────────────┘</span>
<span class="line"></span>
<span class="line">                          可动态增加 CellApp 进程</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="源码证据-cellappmgr-的负载均衡" tabindex="-1"><a class="header-anchor" href="#源码证据-cellappmgr-的负载均衡"><span>源码证据：CellAppMgr 的负载均衡</span></a></h3><p>根据 <a href="https://www.cnblogs.com/losophy/p/9416954.html" target="_blank" rel="noopener noreferrer">KBEngine 源码分析</a>：</p><blockquote><p><strong>CellappMgr</strong>：负责协调所有 Cellapp 的工作，包括负载均衡处理等。一个 KBE 架构中，只会出现一个 CellappMgr。</p></blockquote><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 源码位置：kbe/src/server/cellappmgr/</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// CellappMgr 类定义（简化版）</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellappMgr</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">ServerApp</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 负载均衡核心方法</span></span>
<span class="line">    COMPONENT_ID <span class="token function">findFreeCellapp</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 管理 Cellapp 列表</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span>COMPONENT_ID<span class="token punctuation">,</span> Cellapp<span class="token operator">&gt;</span> cellapps_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 负载均衡定时器</span></span>
<span class="line">    TimerHandle loadBalanceTimer_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 源码分析：findFreeCellapp 实现</span></span>
<span class="line">COMPONENT_ID <span class="token class-name">Cellappmgr</span><span class="token double-colon punctuation">::</span><span class="token function">findFreeCellapp</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    COMPONENT_ID bestCellapp <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> minLoad <span class="token operator">=</span> <span class="token number">1.0f</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 遍历所有 Cellapp，找到负载最低的</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span>COMPONENT_ID<span class="token punctuation">,</span> Cellapp<span class="token operator">&gt;</span><span class="token double-colon punctuation">::</span>iterator iter <span class="token operator">=</span> cellapps_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token punctuation">;</span> iter <span class="token operator">!=</span> cellapps_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>iter<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Cellapp<span class="token operator">&amp;</span> cellapp <span class="token operator">=</span> iter<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 跳过未初始化或过载的</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>cellapp<span class="token punctuation">.</span><span class="token function">isInitialized</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> cellapp<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;=</span> <span class="token number">1.0f</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">continue</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>cellapp<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> minLoad<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            minLoad <span class="token operator">=</span> cellapp<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            bestCellapp <span class="token operator">=</span> iter<span class="token operator">-&gt;</span>first<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> bestCellapp<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、bigworld-的动态负载均衡算法" tabindex="-1"><a class="header-anchor" href="#三、bigworld-的动态负载均衡算法"><span>三、BigWorld 的动态负载均衡算法</span></a></h2><p>根据 <a href="https://blog.csdn.net/antsmall/article/details/139994315" target="_blank" rel="noopener noreferrer">BigWorld Load Balance 算法分析</a>：</p><h3 id="核心算法-动态区域分割-动态边界调整" tabindex="-1"><a class="header-anchor" href="#核心算法-动态区域分割-动态边界调整"><span>核心算法：动态区域分割 + 动态边界调整</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">BigWorld 负载均衡流程：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  阶段 1：初始状态                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│     ┌─────────────────────────────────────┐                │</span>
<span class="line">│     │                                      │                │</span>
<span class="line">│     │         单个 Cell (CellApp1)         │                │</span>
<span class="line">│     │         负载：60%                    │                │</span>
<span class="line">│     │                                      │                │</span>
<span class="line">│     └─────────────────────────────────────┘                │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line">                            ↓</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  阶段 2：负载过高，触发分割                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│     ┌────────────────┬────────────────┐                    │</span>
<span class="line">│     │   Cell 1       │   Cell 2       │  ← 新增 Cell      │</span>
<span class="line">│     │   CellApp1     │   CellApp2     │                    │</span>
<span class="line">│     │   负载：85%     │   负载：0%     │  (初始面积为0)     │</span>
<span class="line">│     └────────────────┴────────────────┘                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│     平均负载 &gt; 阈值 → metaLoadBalance() → addCell()         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line">                            ↓</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  阶段 3：动态边界调整                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│     ┌────────────────┬────────────────┐                    │</span>
<span class="line">│     │   Cell 1       │   Cell 2       │                    │</span>
<span class="line">│     │   负载：50%     │   负载：35%    │  ← Entity 迁移     │</span>
<span class="line">│     │   面积：60%     │   面积：40%     │                    │</span>
<span class="line">│     └────────────────┴────────────────┘                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│     loadBalance() → balance() → 调整边界使负载均衡            │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="源码证据-bsp-树分割" tabindex="-1"><a class="header-anchor" href="#源码证据-bsp-树分割"><span>源码证据：BSP 树分割</span></a></h3><p>根据 BigWorld 源码分析：</p><table><thead><tr><th>类名</th><th>说明</th><th>源码文件</th></tr></thead><tbody><tr><td><strong>BSPNode</strong></td><td>BSP 节点基类</td><td><code>cellappmgr/bsp_node.cpp</code></td></tr><tr><td><strong>CellData</strong></td><td>BSP 叶子节点类</td><td><code>cellappmgr/cell_data.cpp</code></td></tr><tr><td><strong>InternalNode</strong></td><td>BSP 中间节点类</td><td><code>cellappmgr/internal_node.cpp</code></td></tr></tbody></table><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// BigWorld 源码：cellappmgr/cell_data.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// CellData::balance - 动态边界调整</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">CellData</span><span class="token double-colon punctuation">::</span><span class="token function">balance</span><span class="token punctuation">(</span><span class="token keyword">bool</span> isHorizontal<span class="token punctuation">)</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 计算左右子树的负载差</span></span>
<span class="line">    <span class="token keyword">float</span> leftLoad <span class="token operator">=</span> pLeft_<span class="token operator">-&gt;</span><span class="token function">load</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> rightLoad <span class="token operator">=</span> pRight_<span class="token operator">-&gt;</span><span class="token function">load</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> diff <span class="token operator">=</span> rightLoad <span class="token operator">-</span> leftLoad<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 如果负载差异过大，调整边界</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">fabs</span><span class="token punctuation">(</span>diff<span class="token punctuation">)</span> <span class="token operator">&gt;</span> loadBalanceThreshold_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 获取 EntityBoundLevels - 负载分布信息</span></span>
<span class="line">        EntityBoundLevels<span class="token operator">&amp;</span> levels <span class="token operator">=</span> entityBoundLevels_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 根据负载分布决定移动多少边界</span></span>
<span class="line">        <span class="token keyword">float</span> boundaryAdjust <span class="token operator">=</span> <span class="token function">calculateBoundaryAdjust</span><span class="token punctuation">(</span>diff<span class="token punctuation">,</span> levels<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 移动边界</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>isHorizontal<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            range_<span class="token punctuation">.</span>x_ <span class="token operator">+=</span> boundaryAdjust<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            range_<span class="token punctuation">.</span>z_ <span class="token operator">+=</span> boundaryAdjust<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 需要迁移 Entity</span></span>
<span class="line">        <span class="token function">markEntitiesForMigration</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、动态扩容的关键设计" tabindex="-1"><a class="header-anchor" href="#四、动态扩容的关键设计"><span>四、动态扩容的关键设计</span></a></h2><h3 id="_1-服务无状态化" tabindex="-1"><a class="header-anchor" href="#_1-服务无状态化"><span>1. 服务无状态化</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">有状态 vs 无状态：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  有状态服务（难以扩容）                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Server A: [玩家1数据, 玩家2数据, 玩家3数据]                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  扩容问题：                                                   │</span>
<span class="line">│  - 玩家数据绑定到特定服务器                                   │</span>
<span class="line">│  - 扩容需要迁移大量状态                                       │</span>
<span class="line">│  - 宕机导致数据丢失                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  无状态服务（易于扩容）                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Server A: [处理逻辑]  ───→  Redis/DB [共享状态]            │</span>
<span class="line">│  Server B: [处理逻辑]  ───→  Redis/DB [共享状态]            │</span>
<span class="line">│  Server C: [处理逻辑]  ───→  Redis/DB [共享状态]            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优势：                                                       │</span>
<span class="line">│  - 任意服务器可处理任意请求                                    │</span>
<span class="line">│  - 扩容只需增加实例                                           │</span>
<span class="line">│  - 宕机自动故障转移                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="kbengine-的状态分离设计" tabindex="-1"><a class="header-anchor" href="#kbengine-的状态分离设计"><span>KBEngine 的状态分离设计</span></a></h3><p>根据 <a href="https://www.cnblogs.com/losophy/p/9416954.html" target="_blank" rel="noopener noreferrer">KBEngine 组件方案</a>：</p><table><thead><tr><th>组件</th><th>状态</th><th>职责</th><th>扩容性</th></tr></thead><tbody><tr><td><strong>LoginApp</strong></td><td>无状态</td><td>登录验证</td><td>易扩容</td></tr><tr><td><strong>BaseApp</strong></td><td>有状态（玩家数据）</td><td>玩家管理、数据同步</td><td>可扩容（需迁移）</td></tr><tr><td><strong>CellApp</strong></td><td>有状态（空间数据）</td><td>游戏、空间、位置逻辑</td><td>可扩容（需迁移）</td></tr><tr><td><strong>DBMgr</strong></td><td>状态代理</td><td>数据库访问</td><td>可扩容（分库分表）</td></tr></tbody></table><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 脚本：动态创建实体到负载最低的进程</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># scripts/base/spaces.py</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">createSpaceOnTimer</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> tid<span class="token punctuation">,</span> tno<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot; 创建 Space 到负载最低的 BaseApp/CellApp &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token builtin">len</span><span class="token punctuation">(</span>self<span class="token punctuation">.</span>_tmpDatas<span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">:</span></span>
<span class="line">        spaceUType <span class="token operator">=</span> self<span class="token punctuation">.</span>_tmpDatas<span class="token punctuation">.</span>pop<span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># createBaseAnywhere 会自动选择负载最低的 BaseApp</span></span>
<span class="line">        self<span class="token punctuation">.</span>_spaceAllocs<span class="token punctuation">[</span>spaceUType<span class="token punctuation">]</span><span class="token punctuation">.</span>init<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># KBEngine 内部实现（简化）</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">createBaseAnywhere</span><span class="token punctuation">(</span>entityType<span class="token punctuation">,</span> params<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot; 自动选择负载最低的进程创建实体 &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token keyword">if</span> entityType <span class="token operator">==</span> <span class="token string">&quot;Space&quot;</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 查询 BaseAppMgr 获取负载最低的 BaseApp</span></span>
<span class="line">        bestBaseapp <span class="token operator">=</span> BaseAppMgr<span class="token punctuation">.</span>findBestBaseapp<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> bestBaseapp<span class="token punctuation">.</span>createEntity<span class="token punctuation">(</span>entityType<span class="token punctuation">,</span> params<span class="token punctuation">)</span></span>
<span class="line">    <span class="token keyword">elif</span> entityType <span class="token operator">==</span> <span class="token string">&quot;Avatar&quot;</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 查询 CellAppMgr 获取负载最低的 CellApp</span></span>
<span class="line">        bestCellapp <span class="token operator">=</span> CellAppMgr<span class="token punctuation">.</span>findFreeCellapp<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">return</span> bestCellapp<span class="token punctuation">.</span>createEntity<span class="token punctuation">(</span>entityType<span class="token punctuation">,</span> params<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-服务发现机制" tabindex="-1"><a class="header-anchor" href="#_2-服务发现机制"><span>2. 服务发现机制</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">服务发现的两种方案：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  方案 1：中心化服务发现                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│     所有组件 ──注册──→  服务注册中心 (CellAppMgr/Machine)     │</span>
<span class="line">│        │                         │                          │</span>
<span class="line">│        │──查询服务列表──────────→│                          │</span>
<span class="line">│        │←────────返回地址────────│                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  KBEngine 使用：                                             │</span>
<span class="line">│  - CellAppMgr 作为 CellApp 的注册中心                        │</span>
<span class="line">│  - BaseAppMgr 作为 BaseApp 的注册中心                        │</span>
<span class="line">│  - Machine 服务作为硬件节点治理                               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  方案 2：去中心化服务发现（UDP 广播）                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│     CellApp1  ◄────UDP 广播────►  CellApp2                  │</span>
<span class="line">│         │                              │                     │</span>
<span class="line">│         └──────────发现彼此──────────┘                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  KBEngine Machine 服务：                                     │</span>
<span class="line">│  - 使用 UDP 广播通知所有 Machine 服务                         │</span>
<span class="line">│  - 每个节点收集本机组件信息                                    │</span>
<span class="line">│  - 提供组件查询接口                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="源码证据-kbengine-服务发现" tabindex="-1"><a class="header-anchor" href="#源码证据-kbengine-服务发现"><span>源码证据：KBEngine 服务发现</span></a></h3><p>根据 <a href="https://blog.csdn.net/u013272009/article/details/147629050" target="_blank" rel="noopener noreferrer">KBEngine 组网逻辑分析</a>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Machine 服务：UDP 广播服务发现</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Machine</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">ServerApp</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 接收组件的 UDP 广播</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onReceiveUDPBroadcast</span><span class="token punctuation">(</span>Bundle<span class="token operator">*</span> bundle<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string componentType <span class="token operator">=</span> bundle<span class="token operator">-&gt;</span><span class="token function">readString</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string address <span class="token operator">=</span> bundle<span class="token operator">-&gt;</span><span class="token function">readString</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> port <span class="token operator">=</span> bundle<span class="token operator">-&gt;</span><span class="token function">readUint16</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 记录组件信息</span></span>
<span class="line">        <span class="token function">registerComponent</span><span class="token punctuation">(</span>componentType<span class="token punctuation">,</span> address<span class="token punctuation">,</span> port<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 向查询的组件返回组件列表</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendComponentList</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> requestor<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> comp <span class="token operator">:</span> components_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">sendComponentInfo</span><span class="token punctuation">(</span>requestor<span class="token punctuation">,</span> comp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 组件启动时的广播注册</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">ServerApp</span><span class="token double-colon punctuation">::</span><span class="token function">registerToMachine</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    BundleBroadcast broadcast<span class="token punctuation">;</span></span>
<span class="line">    broadcast<span class="token punctuation">.</span><span class="token function">bind</span><span class="token punctuation">(</span>udpPort<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 广播自己的信息</span></span>
<span class="line">    broadcast<span class="token punctuation">.</span><span class="token function">broadcast</span><span class="token punctuation">(</span>componentType_<span class="token punctuation">,</span> internalIP_<span class="token punctuation">,</span> internalPort_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-负载均衡策略" tabindex="-1"><a class="header-anchor" href="#_3-负载均衡策略"><span>3. 负载均衡策略</span></a></h3><h3 id="策略-1-最小负载优先" tabindex="-1"><a class="header-anchor" href="#策略-1-最小负载优先"><span>策略 1：最小负载优先</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine: findFreeCellapp 实现</span></span>
<span class="line">COMPONENT_ID <span class="token class-name">Cellappmgr</span><span class="token double-colon punctuation">::</span><span class="token function">findFreeCellapp</span><span class="token punctuation">(</span><span class="token keyword">void</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    COMPONENT_ID best <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> minLoad <span class="token operator">=</span> FLT_MAX<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> kv <span class="token operator">:</span> cellapps_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Cellapp<span class="token operator">&amp;</span> app <span class="token operator">=</span> kv<span class="token punctuation">.</span>second<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>app<span class="token punctuation">.</span><span class="token function">isInitialized</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> app<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> minLoad<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            minLoad <span class="token operator">=</span> app<span class="token punctuation">.</span><span class="token function">load</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            best <span class="token operator">=</span> kv<span class="token punctuation">.</span>first<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> best<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="策略-2-加权轮询" tabindex="-1"><a class="header-anchor" href="#策略-2-加权轮询"><span>策略 2：加权轮询</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 加权轮询实现</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">WeightedRoundRobin</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Server<span class="token operator">&gt;</span> servers_<span class="token punctuation">;</span></span>
<span class="line">    size_t currentIndex_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> currentWeight_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    Server<span class="token operator">*</span> <span class="token function">selectServer</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            currentIndex_ <span class="token operator">=</span> <span class="token punctuation">(</span>currentIndex_ <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">)</span> <span class="token operator">%</span> servers_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>currentIndex_ <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                currentWeight_ <span class="token operator">-=</span> <span class="token function">gcdWeights</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>currentWeight_ <span class="token operator">&lt;=</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    currentWeight_ <span class="token operator">=</span> <span class="token function">maxWeight</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">if</span> <span class="token punctuation">(</span>currentWeight_ <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            Server<span class="token operator">&amp;</span> server <span class="token operator">=</span> servers_<span class="token punctuation">[</span>currentIndex_<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>server<span class="token punctuation">.</span>weight <span class="token operator">&gt;=</span> currentWeight_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token operator">&amp;</span>server<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="策略-3-一致性哈希" tabindex="-1"><a class="header-anchor" href="#策略-3-一致性哈希"><span>策略 3：一致性哈希</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 一致性哈希：用于数据分片</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ConsistentHash</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span>size_t<span class="token punctuation">,</span> Server<span class="token operator">&gt;</span> ring_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addServer</span><span class="token punctuation">(</span><span class="token keyword">const</span> Server<span class="token operator">&amp;</span> server<span class="token punctuation">,</span> <span class="token keyword">int</span> replicas <span class="token operator">=</span> <span class="token number">150</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">int</span> i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> replicas<span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            size_t hash <span class="token operator">=</span> <span class="token function">hashKey</span><span class="token punctuation">(</span>server<span class="token punctuation">.</span>id <span class="token operator">+</span> <span class="token string">&quot;_&quot;</span> <span class="token operator">+</span> std<span class="token double-colon punctuation">::</span><span class="token function">to_string</span><span class="token punctuation">(</span>i<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            ring_<span class="token punctuation">[</span>hash<span class="token punctuation">]</span> <span class="token operator">=</span> server<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    Server<span class="token operator">*</span> <span class="token function">getServer</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> key<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t hash <span class="token operator">=</span> <span class="token function">hashKey</span><span class="token punctuation">(</span>key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> ring_<span class="token punctuation">.</span><span class="token function">lower_bound</span><span class="token punctuation">(</span>hash<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">==</span> ring_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            it <span class="token operator">=</span> ring_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 环形</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token operator">&amp;</span>it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、动态扩容的实现步骤" tabindex="-1"><a class="header-anchor" href="#五、动态扩容的实现步骤"><span>五、动态扩容的实现步骤</span></a></h2><h3 id="步骤-1-监控负载" tabindex="-1"><a class="header-anchor" href="#步骤-1-监控负载"><span>步骤 1：监控负载</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 负载监控接口</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LoadMonitor</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">LoadInfo</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">float</span> cpuUsage<span class="token punctuation">;</span>        <span class="token comment">// CPU 使用率</span></span>
<span class="line">        <span class="token keyword">float</span> memoryUsage<span class="token punctuation">;</span>     <span class="token comment">// 内存使用率</span></span>
<span class="line">        <span class="token keyword">int</span> entityCount<span class="token punctuation">;</span>       <span class="token comment">// Entity 数量</span></span>
<span class="line">        <span class="token keyword">float</span> networkBandwidth<span class="token punctuation">;</span> <span class="token comment">// 网络带宽</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">virtual</span> LoadInfo <span class="token function">getLoad</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">isOverloaded</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// CellApp 负载监控</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellApp</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">LoadMonitor</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    LoadInfo <span class="token function">getLoad</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">override</span> <span class="token punctuation">{</span></span>
<span class="line">        LoadInfo info<span class="token punctuation">;</span></span>
<span class="line">        info<span class="token punctuation">.</span>cpuUsage <span class="token operator">=</span> <span class="token function">calculateCPUUsage</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        info<span class="token punctuation">.</span>memoryUsage <span class="token operator">=</span> <span class="token function">calculateMemoryUsage</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        info<span class="token punctuation">.</span>entityCount <span class="token operator">=</span> entities_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        info<span class="token punctuation">.</span>networkBandwidth <span class="token operator">=</span> networkInterface_<span class="token operator">-&gt;</span><span class="token function">getBandwidth</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> info<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isOverloaded</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">override</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">getLoad</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>cpuUsage <span class="token operator">&gt;</span> loadThreshold_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="步骤-2-触发扩容" tabindex="-1"><a class="header-anchor" href="#步骤-2-触发扩容"><span>步骤 2：触发扩容</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 扩容决策器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ScalingDecision</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 检查是否需要扩容</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">shouldScaleUp</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>CellApp<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> apps<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">float</span> avgLoad <span class="token operator">=</span> <span class="token function">calculateAverageLoad</span><span class="token punctuation">(</span>apps<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 条件 1：平均负载过高</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>avgLoad <span class="token operator">&gt;</span> scaleUpThreshold_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 条件 2：所有应用都过载</span></span>
<span class="line">        <span class="token keyword">bool</span> allOverloaded <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> app <span class="token operator">:</span> apps<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>app<span class="token operator">-&gt;</span><span class="token function">isOverloaded</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                allOverloaded <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> allOverloaded<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 检查是否需要缩容</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">shouldScaleDown</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>CellApp<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> apps<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>apps<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;=</span> minInstances_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查是否有空闲实例</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> app <span class="token operator">:</span> apps<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>app<span class="token operator">-&gt;</span><span class="token function">getLoad</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>cpuUsage <span class="token operator">&lt;</span> scaleDownThreshold_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="步骤-3-执行扩容" tabindex="-1"><a class="header-anchor" href="#步骤-3-执行扩容"><span>步骤 3：执行扩容</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 扩容执行器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ScalingExecutor</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 扩容：新增 CellApp</span></span>
<span class="line">    CellApp<span class="token operator">*</span> <span class="token function">scaleUp</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 通知 Machine 启动新进程</span></span>
<span class="line">        Machine<span class="token operator">*</span> targetMachine <span class="token operator">=</span> <span class="token function">selectBestMachine</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 启动新的 CellApp 进程</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string command <span class="token operator">=</span> <span class="token string">&quot;start_cellapp.sh&quot;</span><span class="token punctuation">;</span></span>
<span class="line">        targetMachine<span class="token operator">-&gt;</span><span class="token function">executeCommand</span><span class="token punctuation">(</span>command<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 等待新 CellApp 注册</span></span>
<span class="line">        CellApp<span class="token operator">*</span> newApp <span class="token operator">=</span> <span class="token function">waitForRegistration</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 通知 CellAppMgr 新 CellApp 已就绪</span></span>
<span class="line">        <span class="token class-name">CellAppMgr</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">onNewCellApp</span><span class="token punctuation">(</span>newApp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> newApp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 缩容：移除 CellApp</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">scaleDown</span><span class="token punctuation">(</span>CellApp<span class="token operator">*</span> app<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 检查是否有 Entity</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>app<span class="token operator">-&gt;</span><span class="token function">getEntityCount</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 迁移 Entity 到其他 CellApp</span></span>
<span class="line">            <span class="token function">migrateEntities</span><span class="token punctuation">(</span>app<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 通知 CellAppMgr 移除</span></span>
<span class="line">        <span class="token class-name">CellAppMgr</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">onRemoveCellApp</span><span class="token punctuation">(</span>app<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 关闭进程</span></span>
<span class="line">        app<span class="token operator">-&gt;</span><span class="token function">shutdown</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">migrateEntities</span><span class="token punctuation">(</span>CellApp<span class="token operator">*</span> sourceApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 遍历所有 Entity</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">:</span> sourceApp<span class="token operator">-&gt;</span>entities_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 找到负载最低的目标 CellApp</span></span>
<span class="line">            CellApp<span class="token operator">*</span> targetApp <span class="token operator">=</span> <span class="token class-name">CellAppMgr</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">findFreeCellapp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 迁移 Entity</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">migrateTo</span><span class="token punctuation">(</span>targetApp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、entity-迁移机制" tabindex="-1"><a class="header-anchor" href="#六、entity-迁移机制"><span>六、Entity 迁移机制</span></a></h2><h3 id="迁移流程" tabindex="-1"><a class="header-anchor" href="#迁移流程"><span>迁移流程</span></a></h3>`,60),i(d,{code:`eJx1kE1OwkAUx/ecYg6gF2BBAuLChXCGBiekCUItVeOuVYMVS8CgYJQETUzAaAoGApHScBnfTLvyCg4zbVJpnN3k/d77f1Tx0TEuF3BWloqqdJhA7CmSqskFWZHKGsqXDpBURaQ7QDu4VEorSgzJ4VOOdMb/IvtFdY0EY/aLEdkMv3E/Jg0bFu0EB3IVDaPKCVbXNraymSQi129gf5HujD5eot2yJmtnyFsZdOCIDcZtp1JMIIm8ad9zXW915b8//CwtbzQnn+din6MMYigzn0TQGkF9uBmAjcJbZDKEmgW2RcyWEBLbTI5tL3UY3AgX/sUQzFrgLOJIgDWHOu3QNq3PiG5EGGGleevrRsiIPoIyuJ0AasCiCWYXrE5UK4KQuQP1ZzDHKJ3fQ3TClPsRhnf5NF1HDqS+3QZ1/0iJFnmsIPlGcPpie/ZrjIgk9u90MjJI7wN6oVDiF29LCYc=`}),o[1]||=e(`<h3 id="代码实现" tabindex="-1"><a class="header-anchor" href="#代码实现"><span>代码实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Entity 迁移实现</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EntityMigration</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 源端：发起迁移</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">migrateTo</span><span class="token punctuation">(</span>CellApp<span class="token operator">*</span> targetApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 冻结 Entity</span></span>
<span class="line">        <span class="token function">freeze</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 序列化状态</span></span>
<span class="line">        MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">serializeTo</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 发送到目标</span></span>
<span class="line">        targetApp<span class="token operator">-&gt;</span><span class="token function">receiveEntity</span><span class="token punctuation">(</span>id_<span class="token punctuation">,</span> stream<span class="token punctuation">.</span><span class="token function">getData</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> stream<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 等待确认</span></span>
<span class="line">        <span class="token function">waitForConfirmation</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 目标端：接收 Entity</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onReceiveEntity</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">,</span> <span class="token keyword">const</span> <span class="token keyword">void</span><span class="token operator">*</span> data<span class="token punctuation">,</span> size_t size<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 反序列化创建 Entity</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">deserializeEntity</span><span class="token punctuation">(</span>id<span class="token punctuation">,</span> data<span class="token punctuation">,</span> size<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 添加到 CoordinateSystem (AOI)</span></span>
<span class="line">        coordinateSystem_<span class="token operator">-&gt;</span><span class="token function">insert</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 恢复状态</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">unfreeze</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 通知源端完成</span></span>
<span class="line">        sourceApp_<span class="token operator">-&gt;</span><span class="token function">onMigrationComplete</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、动态扩容的挑战与解决方案" tabindex="-1"><a class="header-anchor" href="#七、动态扩容的挑战与解决方案"><span>七、动态扩容的挑战与解决方案</span></a></h2><h3 id="挑战-1-状态一致性" tabindex="-1"><a class="header-anchor" href="#挑战-1-状态一致性"><span>挑战 1：状态一致性</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题：迁移期间玩家可能继续操作</span>
<span class="line"></span>
<span class="line">解决方案：双写缓冲</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   T0: 开始迁移                                               │</span>
<span class="line">│       ├─ 源 Entity: 冻结，不接受新操作                         │</span>
<span class="line">│       ├─ 目标 Entity: 创建                                   │</span>
<span class="line">│       └─ 客户端: 操作缓存到队列                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│   T1: 迁移中                                                 │</span>
<span class="line">│       ├─ 源 Entity: 双写操作到目标                            │</span>
<span class="line">│       ├─ 目标 Entity: 接收并处理操作                          │</span>
<span class="line">│       └─ 客户端: 切换连接到新 CellApp                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│   T2: 迁移完成                                               │</span>
<span class="line">│       ├─ 源 Entity: 销毁                                     │</span>
<span class="line">│       ├─ 目标 Entity: 成为唯一 Real                           │</span>
<span class="line">│       └─ 客户端: 恢复正常操作                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="挑战-2-热点数据" tabindex="-1"><a class="header-anchor" href="#挑战-2-热点数据"><span>挑战 2：热点数据</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题：某些区域玩家过于集中</span>
<span class="line"></span>
<span class="line">解决方案：空间分割 + 副本</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   大地图分割：                                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌────┬────┬────┬────┐                                     │</span>
<span class="line">│   │ C1 │ C2 │ C3 │ C4 │  ← 不同 CellApp 管理               │</span>
<span class="line">│   ├────┼────┼────┼────┤                                     │</span>
<span class="line">│   │ C5 │主城│ C7 │ C8 │  ← 主城可能单独一个 CellApp         │</span>
<span class="line">│   ├────┼────┼────┼────┤                                     │</span>
<span class="line">│   │ C9 │C10 │C11 │C12│                                     │</span>
<span class="line">│   └────┴────┴────┴────┘                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│   副本分流：                                                  │</span>
<span class="line">│   - 主城使用分线（位面）                                      │</span>
<span class="line">│   - 玩家自动分配到不同副本                                     │</span>
<span class="line">│   - 副本间玩家不可见                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="挑战-3-数据库瓶颈" tabindex="-1"><a class="header-anchor" href="#挑战-3-数据库瓶颈"><span>挑战 3：数据库瓶颈</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题：单机数据库无法支撑大量写入</span>
<span class="line"></span>
<span class="line">解决方案：分库分表</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   分库策略：                                                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│   按 UserID 分库：                                            │</span>
<span class="line">│   ┌──────────┬──────────┬──────────┬──────────┐            │</span>
<span class="line">│   │  DB_0    │  DB_1    │  DB_2    │  DB_3    │            │</span>
<span class="line">│   │ User%4=0 │ User%4=1 │ User%4=2 │ User%4=3 │            │</span>
<span class="line">│   └──────────┴──────────┴──────────┴──────────┘            │</span>
<span class="line">│                                                             │</span>
<span class="line">│   KBEngine DBMgr 支持：                                       │</span>
<span class="line">│   - 最多可挂 65535 个数据库                                   │</span>
<span class="line">│   - 通过算法指定存储位置                                       │</span>
<span class="line">│   - 支持多数据库负载均衡                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、实际部署架构" tabindex="-1"><a class="header-anchor" href="#八、实际部署架构"><span>八、实际部署架构</span></a></h2><h3 id="推荐架构-三层分离" tabindex="-1"><a class="header-anchor" href="#推荐架构-三层分离"><span>推荐架构：三层分离</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                        完整的 MMO 架构                             │</span>
<span class="line">│                                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────┐ │</span>
<span class="line">│  │                     接入层 (可水平扩展)                      │ │</span>
<span class="line">│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │ │</span>
<span class="line">│  │  │LoginApp 1│  │LoginApp 2│  │LoginApp N│  │  Gateway │   │ │</span>
<span class="line">│  │  └──────────┘  └──────────┘  └──────────┘  │  (LVS/Nginx)│ │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────┘ │</span>
<span class="line">│                                    │                            │</span>
<span class="line">│  ┌────────────────────────────────┼─────────────────────────────┐│</span>
<span class="line">│  │                                ▼                             ││</span>
<span class="line">│  │                     逻辑层 (可水平扩展)                      ││</span>
<span class="line">│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐                    ││</span>
<span class="line">│  │  │BaseApp 1 │  │BaseApp 2 │  │BaseApp N │                    ││</span>
<span class="line">│  │  │(玩家数据) │  │(玩家数据) │  │(玩家数据) │                   ││</span>
<span class="line">│  │  └────┬─────┘  └────┬─────┘  └────┬─────┘                    ││</span>
<span class="line">│  │       │             │             │                          ││</span>
<span class="line">│  │       └──────┬──────┴──────┬──────┘                          ││</span>
<span class="line">│  │              │             │                                 ││</span>
<span class="line">│  │       ┌──────▼─────────────▼──────┐                         ││</span>
<span class="line">│  │       │    BaseAppMgr (负载均衡)   │                         ││</span>
<span class="line">│  │       └───────────────────────────┘                         ││</span>
<span class="line">│  └─────────────────────────────────────────────────────────────┘│</span>
<span class="line">│                                  │                               │</span>
<span class="line">│  ┌───────────────────────────────┼─────────────────────────────┐│</span>
<span class="line">│  │                               ▼                             ││</span>
<span class="line">│  │                     游戏逻辑层 (可水平扩展)                   ││</span>
<span class="line">│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐                    ││</span>
<span class="line">│  │  │CellApp 1 │  │CellApp 2 │  │CellApp N │                    ││</span>
<span class="line">│  │  │(空间A)   │  │(空间B)   │  │(空间N)   │                    ││</span>
<span class="line">│  │  └────┬─────┘  └────┬─────┘  └────┬─────┘                    ││</span>
<span class="line">│  │       │             │             │                          ││</span>
<span class="line">│  │       └──────┬──────┴──────┬──────┘                          ││</span>
<span class="line">│  │              │             │                                 ││</span>
<span class="line">│  │       ┌──────▼─────────────▼──────┐                         ││</span>
<span class="line">│  │       │   CellAppMgr (空间分割)   │                         ││</span>
<span class="line">│  │       └───────────────────────────┘                         ││</span>
<span class="line">│  └─────────────────────────────────────────────────────────────┘│</span>
<span class="line">│                                  │                               │</span>
<span class="line">│  ┌───────────────────────────────┼─────────────────────────────┐│</span>
<span class="line">│  │                               ▼                             ││</span>
<span class="line">│  │                     数据层 (可水平扩展)                       ││</span>
<span class="line">│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐                    ││</span>
<span class="line">│  │  │  DB_0    │  │  DB_1    │  │  DB_N    │                    ││</span>
<span class="line">│  │  │(分片0)   │  │(分片1)   │  │(分片N)   │                    ││</span>
<span class="line">│  │  └──────────┘  └──────────┘  └──────────┘                    ││</span>
<span class="line">│  │       ┌─────────────────────────────┐                         ││</span>
<span class="line">│  │       │     DBMgr (分片协调)        │                         ││</span>
<span class="line">│  │       └─────────────────────────────┘                         ││</span>
<span class="line">│  └─────────────────────────────────────────────────────────────┘│</span>
<span class="line">│                                                                 │</span>
<span class="line">└─────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="九、性能监控与自动扩容" tabindex="-1"><a class="header-anchor" href="#九、性能监控与自动扩容"><span>九、性能监控与自动扩容</span></a></h2><h3 id="监控指标" tabindex="-1"><a class="header-anchor" href="#监控指标"><span>监控指标</span></a></h3><table><thead><tr><th>指标</th><th>阈值</th><th>操作</th></tr></thead><tbody><tr><td>CPU 使用率</td><td>&gt; 80%</td><td>扩容</td></tr><tr><td>CPU 使用率</td><td>&lt; 30%</td><td>缩容</td></tr><tr><td>内存使用率</td><td>&gt; 85%</td><td>扩容</td></tr><tr><td>网络带宽</td><td>&gt; 80%</td><td>扩容</td></tr><tr><td>Entity 数量</td><td>&gt; 阈值</td><td>扩容</td></tr></tbody></table><h3 id="自动扩容脚本" tabindex="-1"><a class="header-anchor" href="#自动扩容脚本"><span>自动扩容脚本</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token shebang important">#!/bin/bash</span></span>
<span class="line"><span class="token comment"># auto_scale.sh - KBEngine 自动扩容脚本</span></span>
<span class="line"></span>
<span class="line"><span class="token assign-left variable">THRESHOLD_CPU</span><span class="token operator">=</span><span class="token number">80</span></span>
<span class="line"><span class="token assign-left variable">MIN_INSTANCES</span><span class="token operator">=</span><span class="token number">2</span></span>
<span class="line"><span class="token assign-left variable">MAX_INSTANCES</span><span class="token operator">=</span><span class="token number">10</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 获取当前 CellApp 数量</span></span>
<span class="line"><span class="token assign-left variable">current_count</span><span class="token operator">=</span><span class="token variable"><span class="token variable">$(</span><span class="token function">ps</span> aux <span class="token operator">|</span> <span class="token function">grep</span> CellApp <span class="token operator">|</span> <span class="token function">grep</span> <span class="token parameter variable">-v</span> <span class="token function">grep</span> <span class="token operator">|</span> <span class="token function">wc</span> <span class="token parameter variable">-l</span><span class="token variable">)</span></span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 获取平均 CPU</span></span>
<span class="line"><span class="token assign-left variable">avg_cpu</span><span class="token operator">=</span><span class="token variable"><span class="token variable">$(</span><span class="token function">top</span> <span class="token parameter variable">-bn1</span> <span class="token operator">|</span> <span class="token function">grep</span> CellApp <span class="token operator">|</span> <span class="token function">awk</span> <span class="token string">&#39;{sum+=$9; count++} END {print sum/count}&#39;</span><span class="token variable">)</span></span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 判断是否需要扩容</span></span>
<span class="line"><span class="token keyword">if</span> <span class="token variable"><span class="token punctuation">((</span> $<span class="token punctuation">(</span>echo &quot;$avg_cpu <span class="token operator">&gt;</span> $THRESHOLD_CPU&quot; <span class="token operator">|</span> bc <span class="token operator">-</span>l<span class="token punctuation">)</span> <span class="token punctuation">))</span></span><span class="token punctuation">;</span> <span class="token keyword">then</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">[</span> <span class="token variable">$current_count</span> <span class="token parameter variable">-lt</span> <span class="token variable">$MAX_INSTANCES</span> <span class="token punctuation">]</span><span class="token punctuation">;</span> <span class="token keyword">then</span></span>
<span class="line">        <span class="token builtin class-name">echo</span> <span class="token string">&quot;Scaling UP: CPU <span class="token variable">$avg_cpu</span>% &gt; <span class="token variable">$THRESHOLD_CPU</span>%&quot;</span></span>
<span class="line">        ./start_cellapp.sh</span>
<span class="line">    <span class="token keyword">fi</span></span>
<span class="line"><span class="token keyword">fi</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 判断是否需要缩容</span></span>
<span class="line"><span class="token keyword">if</span> <span class="token variable"><span class="token punctuation">((</span> $<span class="token punctuation">(</span>echo &quot;$avg_cpu <span class="token operator">&lt;</span> <span class="token number">30</span>&quot; <span class="token operator">|</span> bc <span class="token operator">-</span>l<span class="token punctuation">)</span> <span class="token punctuation">))</span></span><span class="token punctuation">;</span> <span class="token keyword">then</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">[</span> <span class="token variable">$current_count</span> <span class="token parameter variable">-gt</span> <span class="token variable">$MIN_INSTANCES</span> <span class="token punctuation">]</span><span class="token punctuation">;</span> <span class="token keyword">then</span></span>
<span class="line">        <span class="token builtin class-name">echo</span> <span class="token string">&quot;Scaling DOWN: CPU <span class="token variable">$avg_cpu</span>% &lt; 30%&quot;</span></span>
<span class="line">        <span class="token comment"># 需要先迁移 Entity</span></span>
<span class="line">        ./stop_cellapp.sh <span class="token parameter variable">--graceful</span></span>
<span class="line">    <span class="token keyword">fi</span></span>
<span class="line"><span class="token keyword">fi</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="十、总结" tabindex="-1"><a class="header-anchor" href="#十、总结"><span>十、总结</span></a></h2><h3 id="kbengine-动态扩容的核心设计" tabindex="-1"><a class="header-anchor" href="#kbengine-动态扩容的核心设计"><span>KBEngine 动态扩容的核心设计</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 动态扩容设计总结：</span>
<span class="line"></span>
<span class="line">1. 架构设计</span>
<span class="line">   ├─ 多进程分布式架构</span>
<span class="line">   ├─ 组件职责分离（LoginApp/BaseApp/CellApp）</span>
<span class="line">   └─ 通过 Manager 协调负载均衡</span>
<span class="line"></span>
<span class="line">2. 负载均衡</span>
<span class="line">   ├─ CellAppMgr: findFreeCellapp() 找到负载最低的 CellApp</span>
<span class="line">   ├─ BaseAppMgr: 协调 BaseApp 负载</span>
<span class="line">   └─ 动态 Entity 迁移</span>
<span class="line"></span>
<span class="line">3. 服务发现</span>
<span class="line">   ├─ Machine 服务: UDP 广播发现</span>
<span class="line">   ├─ 组件注册到 Manager</span>
<span class="line">   └─ Manager 维护组件列表</span>
<span class="line"></span>
<span class="line">4. 扩容流程</span>
<span class="line">   ├─ 监控负载 → 触发扩容</span>
<span class="line">   ├─ 启动新进程 → 注册到 Manager</span>
<span class="line">   └─ 迁移 Entity → 负载重新分配</span>
<span class="line"></span>
<span class="line">5. 限制</span>
<span class="line">   ├─ 单 Space 内 Entity 数量受单机限制</span>
<span class="line">   ├─ 跨 CellApp 边界有同步开销</span>
<span class="line">   └─ DBMgr 可能成为瓶颈</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h3><table><thead><tr><th>来源</th><th>链接</th><th>说明</th></tr></thead><tbody><tr><td><strong>KBEngine GitHub</strong></td><td><a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">github.com/kbengine/kbengine</a></td><td>官方源码</td></tr><tr><td><strong>KBEngine 组件方案</strong></td><td><a href="https://www.cnblogs.com/losophy/p/9416954.html" target="_blank" rel="noopener noreferrer">博客园</a></td><td>组件职责说明</td></tr><tr><td><strong>KBEngine 组网逻辑</strong></td><td><a href="https://blog.csdn.net/u013272009/article/details/147629050" target="_blank" rel="noopener noreferrer">CSDN</a></td><td>服务发现机制</td></tr><tr><td><strong>BigWorld 负载均衡</strong></td><td><a href="https://blog.csdn.net/antsmall/article/details/139994315" target="_blank" rel="noopener noreferrer">CSDN</a></td><td>BSP 树算法</td></tr><tr><td><strong>KBEngine 源码：Entity</strong></td><td><a href="https://www.cnblogs.com/losophy/p/9277278.html" target="_blank" rel="noopener noreferrer">博客园</a></td><td>动态负载均衡方案</td></tr></tbody></table>`,26)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};