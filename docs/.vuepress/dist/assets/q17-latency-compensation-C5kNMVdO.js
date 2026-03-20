import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q17-latency-compensation.html","title":"Q16: 什么是同步问题？客户端和服务端的状态如何同步？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q17-latency-compensation.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q17-latency-compensation.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q16-什么是同步问题-客户端和服务端的状态如何同步" tabindex="-1"><a class="header-anchor" href="#q16-什么是同步问题-客户端和服务端的状态如何同步"><span>Q16: 什么是同步问题？客户端和服务端的状态如何同步？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对状态同步技术的理解：</p><ul><li>客户端与服务端状态分离的原因</li><li>状态同步的核心算法</li><li>延迟补偿和预测</li><li>KBEngine 的状态同步机制</li></ul><hr><h2 id="一、状态同步基础" tabindex="-1"><a class="header-anchor" href="#一、状态同步基础"><span>一、状态同步基础</span></a></h2><h3 id="_1-1-为什么需要同步" tabindex="-1"><a class="header-anchor" href="#_1-1-为什么需要同步"><span>1.1 为什么需要同步</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    状态同步的原因                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题：客户端和服务端有各自的状态副本                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────┐          ┌─────────────────┐                  │</span>
<span class="line">│  │   客户端          │          │   服务端          │                  │</span>
<span class="line">│  │  ┌─────────────┐  │          │  ┌─────────────┐  │                  │</span>
<span class="line">│  │  │Player: HP:100│  │          │  │Player: HP:100│  │                  │</span>
<span class="line">│  │  │      MP: 50 │  │          │  │      MP: 50 │  │                  │</span>
<span class="line">│  │  │  │  ┌─────┐   │  │          │  │  ┌─────┐   │  │                  │</span>
<span class="line">│  │  │  │ │Bag: │  │  │          │  │  │Bag: │  │  │                  │</span>
<span class="line">│  │  │  │ └─────┘   │  │          │  │  └─────┘   │  │                  │</span>
<span class="line">│  │  └─────────────┘  │          │  └─────────────┘  │                  │</span>
<span class="line">│  └─────────────────┘          └─────────────────┘                  │</span>
<span class="line">│         │                                │                         │</span>
<span class="line">│         │  玩家移动后...                      │                         │</span>
<span class="line">│         │  ┌─────────────────┐          ┌─────────────────┐                  │</span>
<span class="line">│  │  │Client: HP:100, x:150│  ◄─► ❌ 不同步！│Server: HP:100, x:155│                  │</span>
<span class="line">│  │  └─────────────────┘          └─────────────────┘                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  原因：                                                    │</span>
<span class="line">│  ├── 客户端预测移动，与服务端位置不一致                      │</span>
<span class="line">│  ├── 网络延迟导致状态不同步                                │</span>
<span class="line">│  ├── 客户端先显示，服务端后确认                              │</span>
<span class="line">│  └── 作弊客户端可能修改本地状态                             │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-同步方式" tabindex="-1"><a class="header-anchor" href="#_1-2-同步方式"><span>1.2 同步方式</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    状态同步方式                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 状态同步 (State Sync)                                   │</span>
<span class="line">│     ├── 定期同步完整状态                                   │</span>
<span class="line">│     ├── 适合慢节奏游戏                                     │</span>
<span class="line">│     └── 带宽占用较大                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 快照同步 (Snapshot Sync)                                 │</span>
<span class="line">│     ├── 定期发送完整快照                                   │</span>
<span class="line">│     ├── 增量同步变化                                       │</span>
<span class="line">│     └── 带宽占用较小                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│   3. 事件同步 (Event Sync)                                   │</span>
<span class="line">│     ├── 只同步变化事件                                     │</span>
<span class="line">│     ├── 带宽占用最小                                       │</span>
<span class="line">│     └── 需要可靠的事件传输                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 确定性同步 (Deterministic Sync)                           │</span>
<span class="line">│     ├── 客户端和服务端运行相同逻辑                           │</span>
<span class="line">│     ├── 初始状态相同，结果相同                               │</span>
<span class="line">│     └── 带宽占用为0，但需要 CPU                         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、状态同步算法" tabindex="-1"><a class="header-anchor" href="#二、状态同步算法"><span>二、状态同步算法</span></a></h2><h3 id="_2-1-快照同步" tabindex="-1"><a class="header-anchor" href="#_2-1-快照同步"><span>2.1 快照同步</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    快照同步流程                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  服务端定期发送完整状态快照：                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  服务端                                              │       │</span>
<span class="line">│  │    │  每 100ms 发送快照                               │       │</span>
<span class="line">│  │    ├─────────────────────────────────────┐           │       │</span>
<span class="line">│  │    │ 快照:                                         │           │       │</span>
<span class="line">│  │    │  {                                             │           │       │</span>
<span class="line">│  │    │    &quot;entities&quot;: [                              │           │       │</span>
<span class="line">│  │    │      { &quot;id&quot;: 1, &quot;hp&quot;: 100, &quot;x&quot;: 10 },          │           │       │</span>
<span class="line">│  │    │      { &quot;id&quot;: 2, &quot;hp&quot;: 80, &quot;x&quot;: 15 }           │           │       │</span>
<span class="line">│  │  │    ]                                             │           │       │</span>
<span class="line">│  │    │  }                                             │           │       │</span>
<span class="line">│  │    └─────────────────────────────────────┘           │       │</span>
<span class="line">│  │            │                                       │       │</span>
<span class="line">│  │            └─────────► 客户端                  │           │       │</span>
<span class="line">│  │                         接收快照                  │           │       │</span>
<span class="line">│  │                         │                          │           │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────────────┐  │       │</span>
<span class="line">│  │  │  客户端                                            │  │       │</span>
<span class="line">│  │  │    │  本地状态                                      │  │       │</span>
<span class="line">│  │  │    │  ┌─────────────────────────────────┐   │  │       │</span>
<span class="line">│  │  │    │  │ 实体1, 实体2, ...              │   │  │       │</span>
<span class="line">│  │  │    │  │ hp:100, hp:80, x:10, x:15       │   │  │       │</span>
<span class="line">│  │  │    │  └─────────────────────────────────┘   │  │       │</span>
<span class="line">│  │  │    └─────────────────────────────────────┘   │ │       │</span>
<span class="line">│  │                                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  客户端使用最新快照显示                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-增量同步" tabindex="-1"><a class="header-anchor" href="#_2-2-增量同步"><span>2.2 增量同步</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    增量同步                                   │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  服务端只发送变化部分：                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  服务端                                              │       │</span>
<span class="line">│  │    │  检测到 Entity1 HP 从 100 → 80                │       │</span>
<span class="line">│  │    │  ┌─────────────────────────────────────┐         │       │</span>
<span class="line">│  │    │  │ 增量更新:                               │         │       │</span>
<span class="line">│  │    │  │ {                                     │         │       │</span>
<span class="line">│  │    │  │   &quot;entityId&quot;: 1,                      │         │       │</span>
<span class="line">│  │    │  │   &quot;changes&quot;: {                          │         │       │</span>
<span class="line">│  │    │  │     &quot;hp&quot;: {                             │         │       │</span>
<span class="line">│    │  │  │       &quot;old&quot;: 100,                       │         │       │</span>
<span class="line">│    │  │  │       &quot;new&quot;: 80                         │         │       │</span>
<span class="line">│    │  │  │     }                                     │         │       │</span>
<span class="line">│  │    │  │   }                                     │         │       │</span>
<span class="line">│  │    │  └─────────────────────────────────────┘         │       │</span>
<span class="line">│  │    └──────────────► 客户端                         │       │</span>
<span class="line">│  │                                                     │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────────────┐  │       │</span>
<span class="line">│  │  │  客户端                                            │  │       │</span>
<span class="line">│  │  │    │  合并增量到本地状态                         │ │       │</span>
<span class="line">│  │  │    │  │  entity1.hp = 80                        │ │       │</span>
<span class="line">│  │  │    └─────────────────────────────────────┘  │       │</span>
<span class="line">│  │  └─────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、延迟补偿" tabindex="-1"><a class="header-anchor" href="#三、延迟补偿"><span>三、延迟补偿</span></a></h2><h3 id="_3-1-客户端预测" tabindex="-1"><a class="header-anchor" href="#_3-1-客户端预测"><span>3.1 客户端预测</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    客户端位置预测                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题描述：                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  客户端                                             │       │</span>
<span class="line">│  │    │  实时输入移动指令                                 │       │</span>
<span class="line">│  │    │  │  ┌──────┐                            │       │</span>
<span class="line">│  │    ▼──┤  │ 网络 │ ──────▶ 服务端              │       │</span>
<span class="line">│  │    │  │ 100ms │                              │       │</span>
<span class="line">│  │    │  │       │                              │       │</span>
<span class="line">│  │    │  └──────┘                              │       │</span>
<span class="line">│  │    │                                            │       │</span>
<span class="line">│  │    │  ←─────────────────── 响应                   │       │</span>
<span class="line">│  │    │        200ms 后才能收到确认                   │       │</span>
<span class="line">│  │    │        卡顿、不流畅！                        │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  解决方案：客户端预测                                      │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 立即更新本地位置                               │       │</span>
<span class="line">│  │   │  x += vx * dt  (本地计算)                   │       │</span>
<span class="line">│  │  │                                              │       │</span>
<span class="line">│  │  2. 发送移动请求到服务端                             │       │</span>
<span class="line">│  │  │  │  ┌──────┐                              │       │</span>
<span class="line">│  │  │  └──┤  网络 │                              │       │</span>
<span class="line">│  │  │  │ 100ms │                              │       │</span>
<span class="line">│  │  │  └──────┘                              │       │</span>
<span class="line">│  │  │                                              │       │</span>
<span class="line">│  │  3. 服务端确认后，校正位置                            │       │</span>
<span class="line">│  │  │  │  ┌──────────────────┐                     │       │</span>
<span class="line">│  │  │  └──┤  服务器确认   │                     │       │</span>
<span class="line">│  │  │      │  &quot;你其实在 x:155&quot;                        │       │</span>
<span class="line">│  │  │      │  平滑插值到正确位置                    │       │</span>
<span class="line">│  │  │  └──────────────────┘                     │       │</span>
<span class="line">│  │  │                                              │       │</span>
<span class="line">│  │  └─────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  结果：流畅的移动体验                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-延迟补偿实现" tabindex="-1"><a class="header-anchor" href="#_3-2-延迟补偿实现"><span>3.2 延迟补偿实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 延迟补偿实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LatencyCompensator</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">State</span> <span class="token punctuation">{</span></span>
<span class="line">        Position position<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint16_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收服务端状态</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onServerState</span><span class="token punctuation">(</span><span class="token keyword">const</span> State<span class="token operator">&amp;</span> serverState<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 查找对应的服务端状态（根据序列号）</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> localState <span class="token operator">=</span> localStates_<span class="token punctuation">[</span>serverState<span class="token punctuation">.</span>sequence<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>serverState<span class="token punctuation">.</span>timestamp <span class="token operator">&gt;</span> localState<span class="token punctuation">.</span>timestamp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 计算位置差异</span></span>
<span class="line">            Vector3 diff <span class="token operator">=</span> serverState<span class="token punctuation">.</span>position <span class="token operator">-</span> localState<span class="token punctuation">.</span>position<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 平滑插值校正</span></span>
<span class="line">            <span class="token keyword">float</span> correctionRate <span class="token operator">=</span> <span class="token number">0.2f</span><span class="token punctuation">;</span>  <span class="token comment">// 每帧校正 20%</span></span>
<span class="line">            localState<span class="token punctuation">.</span>position <span class="token operator">+=</span> diff <span class="token operator">*</span> correctionRate<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 如果差异过大，强制校正</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>diff<span class="token punctuation">.</span><span class="token function">length</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">5.0f</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                localState<span class="token punctuation">.</span>position <span class="token operator">=</span> serverState<span class="token punctuation">.</span>position<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 预测本地状态</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">predictLocal</span><span class="token punctuation">(</span><span class="token keyword">float</span> deltaTime<span class="token punctuation">,</span> <span class="token keyword">const</span> Velocity<span class="token operator">&amp;</span> vel<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> state <span class="token operator">=</span> <span class="token function">getCurrentState</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 本地预测</span></span>
<span class="line">        state<span class="token punctuation">.</span>position <span class="token operator">+=</span> vel <span class="token operator">*</span> deltaTime<span class="token punctuation">;</span></span>
<span class="line">        state<span class="token punctuation">.</span>timestamp <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint16_t</span><span class="token punctuation">,</span> State<span class="token operator">&gt;</span> localStates_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-状态同步" tabindex="-1"><a class="header-anchor" href="#四、kbengine-状态同步"><span>四、KBEngine 状态同步</span></a></h2><h3 id="_4-1-entity-同步机制" tabindex="-1"><a class="header-anchor" href="#_4-1-entity-同步机制"><span>4.1 Entity 同步机制</span></a></h3><p>根据 <a href="https://www.kbelab.com/manual/entity-ghost-shadow.html" target="_blank" rel="noopener noreferrer">KBEngine Lab</a>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              KBEngine 三态同步机制                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  三种状态：                                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. Real Entity (服务端真实实体)                             │</span>
<span class="line">│     ├── 在 CellApp 上运行                                    │</span>
<span class="line">│     ├── 权威数据源                                        │</span>
<span class="line">│     └── 状态变化时同步到其他进程                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. Ghost Entity (观察者实体)                              │</span>
<span class="line">│     ├── 在客户端/其他 CellApp 上                            │</span>
<span class="line">│     ├── Real Entity 的镜像                                    │</span>
<span class="line">│     └── 用于 AOI 显示                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. Shadow Entity (客户端预测实体)                           │</span>
<span class="line">│     ├── 在客户端上运行                                    │</span>
<span class="line">│     ├── 客户端预测的本地状态                                │</span>
<span class="line">│     └── 被 Ghost 校正                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-ghost-同步" tabindex="-1"><a class="header-anchor" href="#_4-2-ghost-同步"><span>4.2 Ghost 同步</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Ghost 同步</span></span>
<span class="line"><span class="token comment">// src/server/cellapp/real.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">RealEntity</span><span class="token double-colon punctuation">::</span><span class="token function">onPositionChanged</span><span class="token punctuation">(</span><span class="token keyword">const</span> Position<span class="token operator">&amp;</span> newPos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 更新真实位置</span></span>
<span class="line">    position_ <span class="token operator">=</span> newPos<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 通知所有 Ghost（观察者）</span></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> ghost <span class="token operator">:</span> ghosts_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        ghost<span class="token operator">-&gt;</span><span class="token function">onPositionChanged</span><span class="token punctuation">(</span>newPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Ghost 接收同步</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">GhostEntity</span><span class="token double-colon punctuation">::</span><span class="token function">onPositionChanged</span><span class="token punctuation">(</span><span class="token keyword">const</span> Position<span class="token operator">&amp;</span> pos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    position_ <span class="token operator">=</span> pos<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 通知客户端</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span>client_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        client_<span class="token operator">-&gt;</span><span class="token function">onEntityPositionChanged</span><span class="token punctuation">(</span>id_<span class="token punctuation">,</span> pos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-校正机制" tabindex="-1"><a class="header-anchor" href="#_4-3-校正机制"><span>4.3 校正机制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 客户端校正</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ShadowEntity</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 服务端确认后校正</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onServerConfirmed</span><span class="token punctuation">(</span><span class="token keyword">const</span> Position<span class="token operator">&amp;</span> serverPos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>initialized_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 初始化位置</span></span>
<span class="line">            position_ <span class="token operator">=</span> serverPos<span class="token punctuation">;</span></span>
<span class="line">            initialized_ <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 计算偏移</span></span>
<span class="line">            Vector3 delta <span class="token operator">=</span> serverPos <span class="token operator">-</span> position_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 如果偏移太大，直接跳转</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>delta<span class="token punctuation">.</span><span class="token function">length</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> MAX_CORRECTION_DISTANCE<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                position_ <span class="token operator">=</span> serverPos<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 平滑校正</span></span>
<span class="line">                position_ <span class="token operator">+=</span> delta <span class="token operator">*</span> CORRECTION_RATE<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">float</span> MAX_CORRECTION_DISTANCE <span class="token operator">=</span> <span class="token number">2.0f</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">float</span> CORRECTION_RATE <span class="token operator">=</span> <span class="token number">0.3f</span><span class="token punctuation">;</span></span>
<span class="line">    Position position_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> initialized_ <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、同步优化" tabindex="-1"><a class="header-anchor" href="#五、同步优化"><span>五、同步优化</span></a></h2><h3 id="_5-1-优先级同步" tabindex="-1"><a class="header-anchor" href="#_5-1-优先级同步"><span>5.1 优先级同步</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  优先级同步策略                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  高优先级（每帧同步）：                                    │</span>
<span class="line">│  ├── 自己位置变化                                         │</span>
<span class="line">│  ├── 生命值变化（严重）                                   │</span>
<span class="line">│  ├── 攻击/被攻击                                            │</span>
<span class="line">│  └── 技能释放                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">│  中优先级（100-200ms）：                                  │</span>
<span class="line">│  ├── 附近玩家变化                                       │</span>
<span class="line">│  ├── 实体进出视野                                         │</span>
<span class="line">│  ├── Buff 状态变化                                         │</span>
<span class="line">│  └── 冷却时间变化                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  低优先级（500ms-1s）：                                    │</span>
<span class="line">│  ├── 远距离玩家位置                                       │</span>
<span class="line">│  ├── 实体属性变化                                         │</span>
<span class="line">│  └── 环境变化                                             │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-批量同步" tabindex="-1"><a class="header-anchor" href="#_5-2-批量同步"><span>5.2 批量同步</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 批量同步优化</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BatchSyncManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">SyncBatch</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>EntityUpdate<span class="token operator">&gt;</span> updates<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加待同步实体</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addEntity</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">isDirty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            pendingUpdates_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">                entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                entity<span class="token operator">-&gt;</span><span class="token function">getDirtyFields</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送批量同步</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">flushSync</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>pendingUpdates_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        SyncBatch batch<span class="token punctuation">;</span></span>
<span class="line">        batch<span class="token punctuation">.</span>sequence <span class="token operator">=</span> <span class="token function">getNextSequence</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        batch<span class="token punctuation">.</span>updates <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">move</span><span class="token punctuation">(</span>pendingUpdates_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送给客户端</span></span>
<span class="line">        <span class="token function">broadcast</span><span class="token punctuation">(</span>batch<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        pendingUpdates_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 客户端应用批量更新</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">applyBatch</span><span class="token punctuation">(</span><span class="token keyword">const</span> SyncBatch<span class="token operator">&amp;</span> batch<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> update <span class="token operator">:</span> batch<span class="token punctuation">.</span>updates<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>update<span class="token punctuation">.</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                entity<span class="token operator">-&gt;</span><span class="token function">applyUpdate</span><span class="token punctuation">(</span>update<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>EntityUpdate<span class="token operator">&gt;</span> pendingUpdates_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、特殊场景处理" tabindex="-1"><a class="header-anchor" href="#六、特殊场景处理"><span>六、特殊场景处理</span></a></h2><h3 id="_6-1-延迟隐藏" tabindex="-1"><a class="header-anchor" href="#_6-1-延迟隐藏"><span>6.1 延迟隐藏</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 延迟隐藏技术</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LatencyHider</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 延迟隐藏：平滑插值</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">hideLatency</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> targetPos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 不直接设置目标位置</span></span>
<span class="line">        <span class="token comment">// 而是缓慢移动过去</span></span>
<span class="line"></span>
<span class="line">        Position currentPos <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        Vector3 diff <span class="token operator">=</span> targetPos <span class="token operator">-</span> currentPos<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 计算需要的时间（根据距离和速度）</span></span>
<span class="line">        <span class="token keyword">float</span> distance <span class="token operator">=</span> diff<span class="token punctuation">.</span><span class="token function">length</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> speed <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getSpeed</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> duration <span class="token operator">=</span> distance <span class="token operator">/</span> speed<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 限制最大延迟时间</span></span>
<span class="line">        duration <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">min</span><span class="token punctuation">(</span>duration<span class="token punctuation">,</span> MAX_HIDE_LATENCY<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 设置移动</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setMoveTarget</span><span class="token punctuation">(</span>currentPos<span class="token punctuation">,</span> targetPos<span class="token punctuation">,</span> duration<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 每帧更新移动</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateMove</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> <span class="token keyword">float</span> deltaTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">isMoving</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">updateMove</span><span class="token punctuation">(</span>deltaTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">float</span> MAX_HIDE_LATENCY <span class="token operator">=</span> <span class="token number">0.5f</span><span class="token punctuation">;</span>  <span class="token comment">// 最大隐藏 500ms</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-快照插值" tabindex="-1"><a class="header-anchor" href="#_6-2-快照插值"><span>6.2 快照插值</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 快照插值同步</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SnapshotInterpolation</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Snapshot</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> sequence<span class="token punctuation">;</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>EntityID<span class="token punctuation">,</span> EntityState<span class="token operator">&gt;</span> entities<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>deque<span class="token operator">&lt;</span>Snapshot<span class="token operator">&gt;</span> snapshots_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加快照</span></span>
<span class="line">    <span class="token function">addSnapshot</span><span class="token punctuation">(</span><span class="token keyword">const</span> Snapshot<span class="token operator">&amp;</span> snapshot<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        snapshots_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>snapshot<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 只保留最近的快照</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>snapshots_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> MAX_SNAPSHOTS<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            snapshots_<span class="token punctuation">.</span><span class="token function">pop_front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 插值计算实体状态</span></span>
<span class="line">    EntityState <span class="token function">interpolate</span><span class="token punctuation">(</span>EntityID entityId<span class="token punctuation">,</span> <span class="token keyword">float</span> t<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 找到 t 时间的两个快照</span></span>
<span class="line">        Snapshot<span class="token operator">*</span> from <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">        Snapshot<span class="token operator">*</span> to <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> snapshot <span class="token operator">:</span> snapshots_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>snapshot<span class="token punctuation">.</span>timestamp <span class="token operator">&lt;=</span> t<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                from <span class="token operator">=</span> <span class="token generic-function"><span class="token function">const_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span>Snapshot<span class="token operator">*</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token operator">&amp;</span>snapshot<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>snapshot<span class="token punctuation">.</span>timestamp <span class="token operator">&gt;=</span> t <span class="token operator">&amp;&amp;</span> from <span class="token operator">!=</span> <span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                to <span class="token operator">=</span> <span class="token generic-function"><span class="token function">const_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span>Snapshot<span class="token operator">*</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token operator">&amp;</span>snapshot<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>from <span class="token operator">||</span> <span class="token operator">!</span>to <span class="token operator">||</span> from <span class="token operator">==</span> to<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">;</span>  <span class="token comment">// 无快照</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 插值计算</span></span>
<span class="line">        EntityState result<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> ratio <span class="token operator">=</span> <span class="token punctuation">(</span>t <span class="token operator">-</span> from<span class="token operator">-&gt;</span>timestamp<span class="token punctuation">)</span> <span class="token operator">/</span></span>
<span class="line">                       <span class="token punctuation">(</span>to<span class="token operator">-&gt;</span>timestamp <span class="token operator">-</span> from<span class="token operator">-&gt;</span>timestamp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>id<span class="token punctuation">,</span> state<span class="token punctuation">]</span> <span class="token operator">:</span> from<span class="token operator">-&gt;</span>entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> toState <span class="token operator">=</span> to<span class="token operator">-&gt;</span>entities<span class="token punctuation">[</span>id<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">            EntityState<span class="token operator">&amp;</span> interpolated <span class="token operator">=</span> result<span class="token punctuation">[</span>id<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            interpolated<span class="token punctuation">.</span>position <span class="token operator">=</span> <span class="token function">lerp</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>position<span class="token punctuation">,</span> toState<span class="token punctuation">.</span>position<span class="token punctuation">,</span> ratio<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            interpolated<span class="token punctuation">.</span>rotation <span class="token operator">=</span> <span class="token function">lerp</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>rotation<span class="token punctuation">,</span> toState<span class="token punctuation">.</span>rotation<span class="token punctuation">,</span> ratio<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            interpolated<span class="token punctuation">.</span>hp <span class="token operator">=</span> <span class="token function">lerp</span><span class="token punctuation">(</span>state<span class="token punctuation">.</span>hp<span class="token punctuation">,</span> toState<span class="token punctuation">.</span>hp<span class="token punctuation">,</span> ratio<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token comment">// ... 其他属性</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> result<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t MAX_SNAPSHOTS <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span>  <span class="token comment">// 最多保留 10 个快照</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="状态同步方案对比" tabindex="-1"><a class="header-anchor" href="#状态同步方案对比"><span>状态同步方案对比</span></a></h3><table><thead><tr><th>方案</th><th>延迟</th><th>带宽</th><th>CPU</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>状态同步</strong></td><td>低</td><td>高</td><td>低</td><td>卡牌/回合制</td></tr><tr><td><strong>快照同步</strong></td><td>中</td><td>中</td><td>中</td><td>RPG/MO</td></tr><tr><td><strong>增量同步</strong></td><td>中</td><td>低</td><td>中</td><td>大多数游戏</td></tr><tr><td><strong>确定性同步</strong></td><td>0</td><td>0</td><td>高</td><td>RTT高</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 客户端预测 + 服务端校正</span>
<span class="line">   - 客户端本地预测显示</span>
<span class="line">   - 服务端权威校正</span>
<span class="line">   - 平滑处理差异</span>
<span class="line"></span>
<span class="line">2. 优先级同步</span>
<span class="line">   - 关键数据优先同步</span>
<span class="line">   - 次要数据延迟同步</span>
<span class="line">   - 背景数据定期同步</span>
<span class="line"></span>
<span class="line">3. 批量同步</span>
<span class="line">   - 减少同步频率</span>
<span class="line">   - 提高网络利用率</span>
<span class="line">   - 降低 CPU 开销</span>
<span class="line"></span>
<span class="line">4. 快照插值</span>
<span class="line">   - 减少网络传输</span>
<span class="line">   - 提高显示效果</span>
<span class="line">   - 补偿网络抖动</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.kbelab.com/manual/entity-ghost-shadow.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - Entity/Ghost/Shadow</a></li><li><a href="https://gafferongithub.io/" target="_blank" rel="noopener noreferrer">GafferNet 状态同步</a></li><li><a href="https://www.gamedeveloper.com/network/network-synchronization/" target="_blank" rel="noopener noreferrer">网游网络同步技术</a></li></ul>`,52)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};