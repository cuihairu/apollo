import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q16-state-sync.html","title":"Q16: 什么是同步问题？客户端和服务端的状态如何同步？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q16-state-sync.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q16-state-sync.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q16-什么是同步问题-客户端和服务端的状态如何同步" tabindex="-1"><a class="header-anchor" href="#q16-什么是同步问题-客户端和服务端的状态如何同步"><span>Q16: 什么是同步问题？客户端和服务端的状态如何同步？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对网络游戏状态同步的理解：</p><ul><li>同步问题的本质和挑战</li><li>状态同步策略（状态同步 vs 帧同步）</li><li>KBEngine 的 Real/Ghost/Shadow 机制</li><li>同步优化技术</li></ul><hr><h2 id="一、同步问题本质" tabindex="-1"><a class="header-anchor" href="#一、同步问题本质"><span>一、同步问题本质</span></a></h2><h3 id="_1-1-为什么需要同步" tabindex="-1"><a class="header-anchor" href="#_1-1-为什么需要同步"><span>1.1 为什么需要同步</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    同步问题的根源                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景：多人在线游戏                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  玩家 A ──────┐                                            │</span>
<span class="line">│               │                                            │</span>
<span class="line">│  玩家 B ──────┼──► 服务器 ─────► 数据库                      │</span>
<span class="line">│               │                                            │</span>
<span class="line">│  玩家 C ──────┘                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题：                                                    │</span>
<span class="line">│  ├── 每个客户端有自己的状态视图                              │</span>
<span class="line">│  ├── 服务器有权威状态                                       │</span>
<span class="line">│  ├── 网络延迟导致状态不一致                                  │</span>
<span class="line">│  └── 不同客户端看到的游戏世界不同                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  核心矛盾：                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  延迟 vs 一致性 vs 体验                          │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  等待服务器确认 → 一致性好但延迟高               │       │</span>
<span class="line">│  │  客户端立即响应 → 体验好但可能不同步              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-同步问题的类型" tabindex="-1"><a class="header-anchor" href="#_1-2-同步问题的类型"><span>1.2 同步问题的类型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                   同步问题分类                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 位置同步                                                │</span>
<span class="line">│     ├── 玩家移动                                            │</span>
<span class="line">│     ├── NPC 巡逻                                            │</span>
<span class="line">│     ├── 投射物飞行                                          │</span>
<span class="line">│     └── 特效位置                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 状态同步                                                │</span>
<span class="line">│     ├── HP/MP 变化                                         │</span>
<span class="line">│     ├── Buff/Debuff                                        │</span>
<span class="line">│     ├── 技能 CD                                            │</span>
<span class="line">│     └── 装备耐久                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 动作同步                                                │</span>
<span class="line">│     ├── 攻击动作                                            │</span>
<span class="line">│     ├── 受击反应                                            │</span>
<span class="line">│     ├── 死亡倒地                                            │</span>
<span class="line">│     └── 交互动作                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 事件同步                                                │</span>
<span class="line">│     ├── 伤害数字                                            │</span>
<span class="line">│     ├── 技能释放                                            │</span>
<span class="line">│     ├── 音效播放                                            │</span>
<span class="line">│     └── 震动特效                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、同步策略对比" tabindex="-1"><a class="header-anchor" href="#二、同步策略对比"><span>二、同步策略对比</span></a></h2><h3 id="_2-1-状态同步-vs-帧同步" tabindex="-1"><a class="header-anchor" href="#_2-1-状态同步-vs-帧同步"><span>2.1 状态同步 vs 帧同步</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              状态同步 vs 帧同步                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  状态同步 (State Synchronization):                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  特点：                                            │       │</span>
<span class="line">│  │  ├── 服务器是权威                                  │       │</span>
<span class="line">│  │  ├── 客户端显示服务器状态                          │       │</span>
<span class="line">│  │  ├── 带宽消耗低（只同步变化）                      │       │</span>
<span class="line">│  │  └── 容易作弊预测                                  │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  流程：                                            │       │</span>
<span class="line">│  │  客户端 ──操作──► 服务器 ──验证──► 更新状态 ──广播──► 全部 │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用：MMORPG、MOBA                               │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  帧同步 (Frame Synchronization):                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  特点：                                            │       │</span>
<span class="line">│  │  ├── 所有客户端运行相同逻辑                        │       │</span>
<span class="line">│  │  ├── 同步输入指令                                  │       │</span>
<span class="line">│  │  ├── 带宽消耗高（每帧输入）                        │       │</span>
<span class="line">│  │  └── 难以作弊预测                                  │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  流程：                                            │       │</span>
<span class="line">│  │  客户端 ──输入──► 服务器 ──收集──► 广播输入 ──执行──► 全部 │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  适用：RTS、格斗游戏                               │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-同步策略对比表" tabindex="-1"><a class="header-anchor" href="#_2-2-同步策略对比表"><span>2.2 同步策略对比表</span></a></h3><table><thead><tr><th>维度</th><th>状态同步</th><th>帧同步</th><th>乐观同步</th></tr></thead><tbody><tr><td><strong>权威方</strong></td><td>服务器</td><td>服务器</td><td>客户端预测</td></tr><tr><td><strong>同步内容</strong></td><td>状态变化</td><td>输入指令</td><td>操作+状态</td></tr><tr><td><strong>带宽消耗</strong></td><td>低</td><td>高</td><td>中</td></tr><tr><td><strong>延迟敏感</strong></td><td>中</td><td>高</td><td>低</td></tr><tr><td><strong>防作弊</strong></td><td>好</td><td>中</td><td>差</td></tr><tr><td><strong>实现难度</strong></td><td>中</td><td>高</td><td>高</td></tr><tr><td><strong>典型游戏</strong></td><td>WoW、LoL</td><td>SC2、街霸</td><td>FPS、MOBA</td></tr></tbody></table><hr><h2 id="三、kbengine-状态同步机制" tabindex="-1"><a class="header-anchor" href="#三、kbengine-状态同步机制"><span>三、KBEngine 状态同步机制</span></a></h2><h3 id="_3-1-real-ghost-shadow-机制" tabindex="-1"><a class="header-anchor" href="#_3-1-real-ghost-shadow-机制"><span>3.1 Real/Ghost/Shadow 机制</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│           KBEngine Real/Ghost/Shadow 架构                    │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  Real Entity (权威实体):                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  定义：实体的权威副本                              │       │</span>
<span class="line">│  │  位置：CellApp                                   │       │</span>
<span class="line">│  │  职责：                                            │       │</span>
<span class="line">│  │  ├── 权威逻辑计算                                  │       │</span>
<span class="line">│  │  ├── 状态维护                                      │       │</span>
<span class="line">│  │  └── Ghost 同步源                                  │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  示例：玩家在某个 CellApp 的 Real 实体             │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Ghost Entity (镜像实体):                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  定义：实体的远程镜像                              │       │</span>
<span class="line">│  │  位置：其他 CellApp / BaseApp                    │       │</span>
<span class="line">│  │  职责：                                            │       │</span>
<span class="line">│  │  ├── 状态同步接收                                  │       │</span>
<span class="line">│  │  ├── 只读访问                                      │       │</span>
<span class="line">│  │  └── 不能执行逻辑                                  │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  示例：玩家看到的其他玩家                          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  Shadow Entity (阴影实体):                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  定义：客户端感兴趣实体的精简版                    │       │</span>
<span class="line">│  │  位置：客户端                                      │       │</span>
<span class="line">│  │  职责：                                            │       │</span>
<span class="line">│  │  ├── 显示同步                                      │       │</span>
<span class="line">│  │  ├── 预测执行                                      │       │</span>
<span class="line">│  │  └── 不影响游戏逻辑                                │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  示例：客户端上显示的 NPC                          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-kbengine-同步流程" tabindex="-1"><a class="header-anchor" href="#_3-2-kbengine-同步流程"><span>3.2 KBEngine 同步流程</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Entity 同步机制</span></span>
<span class="line"><span class="token comment">// src/server/entitydef/entity_checkpoint.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 实体状态检查点</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">CheckPoint</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> id<span class="token punctuation">;</span>              <span class="token comment">// 检查点 ID</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> data<span class="token punctuation">;</span> <span class="token comment">// 序列化状态</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> checksum<span class="token punctuation">;</span>        <span class="token comment">// 校验和</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取实体状态（用于同步）</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">getWitnessData</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 序列化实体 ID</span></span>
<span class="line">        stream <span class="token operator">&lt;&lt;</span> id_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 序列化位置</span></span>
<span class="line">        stream <span class="token operator">&lt;&lt;</span> position_<span class="token punctuation">.</span>x <span class="token operator">&lt;&lt;</span> position_<span class="token punctuation">.</span>y <span class="token operator">&lt;&lt;</span> position_<span class="token punctuation">.</span>z<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 序列化方向</span></span>
<span class="line">        stream <span class="token operator">&lt;&lt;</span> direction_<span class="token punctuation">.</span>yaw <span class="token operator">&lt;&lt;</span> direction_<span class="token punctuation">.</span>pitch <span class="token operator">&lt;&lt;</span> direction_<span class="token punctuation">.</span>roll<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 序列化状态（HP、MP 等）</span></span>
<span class="line">        stream <span class="token operator">&lt;&lt;</span> hp_<span class="token punctuation">;</span></span>
<span class="line">        stream <span class="token operator">&lt;&lt;</span> mp_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 序列化运动状态</span></span>
<span class="line">        stream <span class="token operator">&lt;&lt;</span> velocity_<span class="token punctuation">;</span></span>
<span class="line">        stream <span class="token operator">&lt;&lt;</span> isMoving_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 6. 序列化需要同步的属性</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> prop <span class="token operator">:</span> synchronizedProps_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            prop<span class="token operator">-&gt;</span><span class="token function">serializeTo</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 应用远程状态（Ghost 更新）</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onWitnessDataUpdate</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 从 Real 实体接收更新</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> entityId<span class="token punctuation">;</span></span>
<span class="line">        stream <span class="token operator">&gt;&gt;</span> entityId<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entityId <span class="token operator">!=</span> id_<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新位置</span></span>
<span class="line">        stream <span class="token operator">&gt;&gt;</span> position_<span class="token punctuation">.</span>x <span class="token operator">&gt;&gt;</span> position_<span class="token punctuation">.</span>y <span class="token operator">&gt;&gt;</span> position_<span class="token punctuation">.</span>z<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新方向</span></span>
<span class="line">        stream <span class="token operator">&gt;&gt;</span> direction_<span class="token punctuation">.</span>yaw <span class="token operator">&gt;&gt;</span> direction_<span class="token punctuation">.</span>pitch <span class="token operator">&gt;&gt;</span> direction_<span class="token punctuation">.</span>roll<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新状态</span></span>
<span class="line">        stream <span class="token operator">&gt;&gt;</span> hp_<span class="token punctuation">;</span></span>
<span class="line">        stream <span class="token operator">&gt;&gt;</span> mp_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新运动</span></span>
<span class="line">        stream <span class="token operator">&gt;&gt;</span> velocity_<span class="token punctuation">;</span></span>
<span class="line">        stream <span class="token operator">&gt;&gt;</span> isMoving_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新其他属性</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> prop <span class="token operator">:</span> synchronizedProps_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            prop<span class="token operator">-&gt;</span><span class="token function">deserializeFrom</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知观察者</span></span>
<span class="line">        <span class="token function">notifyWitnessesUpdated</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> id_<span class="token punctuation">;</span></span>
<span class="line">    Position position_<span class="token punctuation">;</span></span>
<span class="line">    Direction direction_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> hp_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> mp_<span class="token punctuation">;</span></span>
<span class="line">    Vector3 velocity_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> isMoving_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Property<span class="token operator">*</span><span class="token operator">&gt;</span> synchronizedProps_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-witness-机制" tabindex="-1"><a class="header-anchor" href="#_3-3-witness-机制"><span>3.3 Witness 机制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Witness 机制</span></span>
<span class="line"><span class="token comment">// src/server/entitydef/entity_alias.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// Witness（观察者列表）</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">Witness</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span> viewers<span class="token punctuation">;</span>   <span class="token comment">// 观察此实体的玩家</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> updateFlags<span class="token punctuation">;</span>            <span class="token comment">// 需要更新的标志</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 添加观察者</span></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">addViewer</span><span class="token punctuation">(</span>EntityID viewerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            viewers<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>viewerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">sendInitialState</span><span class="token punctuation">(</span>viewerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 移除观察者</span></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">removeViewer</span><span class="token punctuation">(</span>EntityID viewerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span> it <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">find</span><span class="token punctuation">(</span>viewers<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> viewers<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> viewerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> viewers<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                viewers<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">sendRemoveEntity</span><span class="token punctuation">(</span>viewerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送初始状态</span></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">sendInitialState</span><span class="token punctuation">(</span>EntityID viewerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line">            stream <span class="token operator">&lt;&lt;</span> messageId_<span class="token punctuation">;</span></span>
<span class="line">            stream <span class="token operator">&lt;&lt;</span> entityId_<span class="token punctuation">;</span></span>
<span class="line">            stream <span class="token operator">&lt;&lt;</span> position_<span class="token punctuation">;</span></span>
<span class="line">            stream <span class="token operator">&lt;&lt;</span> direction_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 发送到客户端</span></span>
<span class="line">            <span class="token class-name">Network</span><span class="token double-colon punctuation">::</span><span class="token function">send</span><span class="token punctuation">(</span>viewerId<span class="token punctuation">,</span> stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送移除消息</span></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">sendRemoveEntity</span><span class="token punctuation">(</span>EntityID viewerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line">            stream <span class="token operator">&lt;&lt;</span> MSG_REMOVE_ENTITY<span class="token punctuation">;</span></span>
<span class="line">            stream <span class="token operator">&lt;&lt;</span> entityId_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token class-name">Network</span><span class="token double-colon punctuation">::</span><span class="token function">send</span><span class="token punctuation">(</span>viewerId<span class="token punctuation">,</span> stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取 Witness</span></span>
<span class="line">    Witness<span class="token operator">*</span> <span class="token function">getWitness</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> witness_<span class="token punctuation">.</span><span class="token function">get</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 当进入其他实体的 AOI 时被调用</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onEnterWitness</span><span class="token punctuation">(</span>EntityID otherId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>witness_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            witness_<span class="token operator">-&gt;</span><span class="token function">addViewer</span><span class="token punctuation">(</span>otherId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 当离开其他实体的 AOI 时被调用</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onLeaveWitness</span><span class="token punctuation">(</span>EntityID otherId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>witness_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            witness_<span class="token operator">-&gt;</span><span class="token function">removeViewer</span><span class="token punctuation">(</span>otherId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>Witness<span class="token operator">&gt;</span> witness_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、同步优化技术" tabindex="-1"><a class="header-anchor" href="#四、同步优化技术"><span>四、同步优化技术</span></a></h2><h3 id="_4-1-优先级同步" tabindex="-1"><a class="header-anchor" href="#_4-1-优先级同步"><span>4.1 优先级同步</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                   同步优先级策略                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  高优先级 (每次同步):                                       │</span>
<span class="line">│  ├── 玩家位置变化                                          │</span>
<span class="line">│  ├── 伤害事件                                              │</span>
<span class="line">│  ├── 死亡事件                                              │</span>
<span class="line">│  └── 关键状态变化 (HP=0)                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  中优先级 (按需同步):                                       │</span>
<span class="line">│  ├── NPC 位置                                              │</span>
<span class="line">│  ├── Buff 变化                                             │</span>
<span class="line">│  ├── 技能冷却                                              │</span>
<span class="line">│  └── 装备变化                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">│  低优先级 (定期同步):                                       │</span>
<span class="line">│  ├── 玩家基本信息                                          │</span>
<span class="line">│  ├── 等级/经验变化                                          │</span>
<span class="line">│  ├── 任务进度                                              │</span>
<span class="line">│  └── 背包物品                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">│  过滤策略:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  if (距离 &gt; 视野范围) 不同步                      │       │</span>
<span class="line">│  │  if (变化量 &lt; 阈值) 不同步                        │       │</span>
<span class="line">│  │  if (客户端已预测) 压缩同步                        │       │</span>
<span class="line">│  │  if (优先级低且带宽紧张) 跳过同步                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-增量同步" tabindex="-1"><a class="header-anchor" href="#_4-2-增量同步"><span>4.2 增量同步</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 增量同步实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">DeltaSync</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 计算增量</span></span>
<span class="line">    <span class="token keyword">static</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> <span class="token function">computeDelta</span><span class="token punctuation">(</span></span>
<span class="line">        <span class="token keyword">const</span> EntityState<span class="token operator">&amp;</span> oldState<span class="token punctuation">,</span></span>
<span class="line">        <span class="token keyword">const</span> EntityState<span class="token operator">&amp;</span> newState<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> delta<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// Delta 格式: [变化类型] [字段ID] [新值]</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查位置变化</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>oldState<span class="token punctuation">.</span>position <span class="token operator">!=</span> newState<span class="token punctuation">.</span>position<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            delta<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>DELTA_POSITION<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">appendFloat3</span><span class="token punctuation">(</span>delta<span class="token punctuation">,</span> newState<span class="token punctuation">.</span>position<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查方向变化</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>oldState<span class="token punctuation">.</span>direction <span class="token operator">!=</span> newState<span class="token punctuation">.</span>direction<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            delta<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>DELTA_DIRECTION<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">appendFloat3</span><span class="token punctuation">(</span>delta<span class="token punctuation">,</span> newState<span class="token punctuation">.</span>direction<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 检查 HP 变化</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>oldState<span class="token punctuation">.</span>hp <span class="token operator">!=</span> newState<span class="token punctuation">.</span>hp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            delta<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>DELTA_HP<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">appendFloat</span><span class="token punctuation">(</span>delta<span class="token punctuation">,</span> newState<span class="token punctuation">.</span>hp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> delta<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 应用增量</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">void</span> <span class="token function">applyDelta</span><span class="token punctuation">(</span>EntityState<span class="token operator">&amp;</span> state<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint8_t</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> delta<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        size_t offset <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span>offset <span class="token operator">&lt;</span> delta<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">uint8_t</span> type <span class="token operator">=</span> delta<span class="token punctuation">[</span>offset<span class="token operator">++</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">switch</span> <span class="token punctuation">(</span>type<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">case</span> DELTA_POSITION<span class="token operator">:</span></span>
<span class="line">                    state<span class="token punctuation">.</span>position <span class="token operator">=</span> <span class="token function">readFloat3</span><span class="token punctuation">(</span>delta<span class="token punctuation">,</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    offset <span class="token operator">+=</span> <span class="token number">12</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">case</span> DELTA_DIRECTION<span class="token operator">:</span></span>
<span class="line">                    state<span class="token punctuation">.</span>direction <span class="token operator">=</span> <span class="token function">readFloat3</span><span class="token punctuation">(</span>delta<span class="token punctuation">,</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    offset <span class="token operator">+=</span> <span class="token number">12</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">case</span> DELTA_HP<span class="token operator">:</span></span>
<span class="line">                    state<span class="token punctuation">.</span>hp <span class="token operator">=</span> <span class="token function">readFloat</span><span class="token punctuation">(</span>delta<span class="token punctuation">,</span> offset<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                    offset <span class="token operator">+=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">                    <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">DeltaType</span> <span class="token punctuation">{</span></span>
<span class="line">        DELTA_POSITION <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span></span>
<span class="line">        DELTA_DIRECTION <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span></span>
<span class="line">        DELTA_HP <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">,</span></span>
<span class="line">        DELTA_MP <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-区域同步-aoi-优化" tabindex="-1"><a class="header-anchor" href="#_4-3-区域同步-aoi-优化"><span>4.3 区域同步（AOI 优化）</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 基于 AOI 的同步优化</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AOISyncManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 只同步 AOI 内的实体</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">syncToViewers</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Position pos <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 获取 AOI 内的观察者</span></span>
<span class="line">        <span class="token keyword">auto</span> viewers <span class="token operator">=</span> aoi_<span class="token operator">-&gt;</span><span class="token function">queryViewers</span><span class="token punctuation">(</span>pos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID viewerId <span class="token operator">:</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 检查是否需要同步</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">shouldSync</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> viewerId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">sendSync</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> viewerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">shouldSync</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> EntityID viewerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> viewer <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>viewerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 检查距离</span></span>
<span class="line">        <span class="token keyword">float</span> distance <span class="token operator">=</span> <span class="token function">distance</span><span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> viewer<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>distance <span class="token operator">&gt;</span> viewer<span class="token operator">-&gt;</span><span class="token function">getViewDistance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 检查视野遮挡</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isOccluded</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> viewer<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 检查优先级</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">getSyncPriority</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">==</span> Priority<span class="token double-colon punctuation">::</span>Low <span class="token operator">&amp;&amp;</span></span>
<span class="line">            viewer<span class="token operator">-&gt;</span><span class="token function">getBandwidthPressure</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0.8f</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    AOIManager<span class="token operator">*</span> aoi_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、位置同步详解" tabindex="-1"><a class="header-anchor" href="#五、位置同步详解"><span>五、位置同步详解</span></a></h2><h3 id="_5-1-位置同步策略" tabindex="-1"><a class="header-anchor" href="#_5-1-位置同步策略"><span>5.1 位置同步策略</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                   位置同步策略                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  策略 1: 服务器权威 (MMO 常用)                               │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 客户端发送移动请求                             │       │</span>
<span class="line">│  │  2. 服务器验证并更新位置                           │       │</span>
<span class="line">│  │  3. 服务器广播给附近玩家                           │       │</span>
<span class="line">│  │  4. 客户端平滑显示                                 │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 安全、一致                                   │       │</span>
<span class="line">│  │  缺点: 延迟高                                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  策略 2: 客户端预测 + 服务器校验                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 客户端立即显示移动                             │       │</span>
<span class="line">│  │  2. 发送移动到服务器                               │       │</span>
<span class="line">│  │  3. 服务器验证合法性                               │       │</span>
<span class="line">│  │  4. 如果合法确认，否则纠正                          │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 响应快                                       │       │</span>
<span class="line">│  │  缺点: 可能回退                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  策略 3: 帧同步输入 (RTS 常用)                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 收集所有玩家输入                               │       │</span>
<span class="line">│  │  2. 广播输入给所有人                               │       │</span>
<span class="line">│  │  3. 每帧执行相同逻辑                               │       │</span>
<span class="line">│  │  4. 确定帧同步                                     │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优点: 完全同步                                     │       │</span>
<span class="line">│  │  缺点: 带宽高、延迟敏感                             │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-位置平滑插值" tabindex="-1"><a class="header-anchor" href="#_5-2-位置平滑插值"><span>5.2 位置平滑插值</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 客户端位置平滑</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PositionSmoother</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">TargetPosition</span> <span class="token punctuation">{</span></span>
<span class="line">        Vector3 position<span class="token punctuation">;</span></span>
<span class="line">        Vector3 velocity<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加服务器位置</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addServerPosition</span><span class="token punctuation">(</span><span class="token keyword">const</span> TargetPosition<span class="token operator">&amp;</span> target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        targets_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>target<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 按时间排序</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span><span class="token function">sort</span><span class="token punctuation">(</span>targets_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> targets_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> a<span class="token punctuation">,</span> <span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> b<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> a<span class="token punctuation">.</span>timestamp <span class="token operator">&lt;</span> b<span class="token punctuation">.</span>timestamp<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取当前插值位置</span></span>
<span class="line">    Vector3 <span class="token function">getCurrentPosition</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> currentTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>targets_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> currentPosition_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 找到当前时间前后的目标位置</span></span>
<span class="line">        <span class="token keyword">auto</span> nextIt <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">upper_bound</span><span class="token punctuation">(</span>targets_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> targets_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            currentTime<span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> t<span class="token punctuation">,</span> <span class="token keyword">const</span> TargetPosition<span class="token operator">&amp;</span> pos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> t <span class="token operator">&lt;</span> pos<span class="token punctuation">.</span>timestamp<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>nextIt <span class="token operator">==</span> targets_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> targets_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>position<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>nextIt <span class="token operator">==</span> targets_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            currentPosition_ <span class="token operator">=</span> targets_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span>position<span class="token punctuation">;</span></span>
<span class="line">            targets_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> currentPosition_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 插值</span></span>
<span class="line">        <span class="token keyword">auto</span> prevIt <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">prev</span><span class="token punctuation">(</span>nextIt<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> t <span class="token operator">=</span> <span class="token punctuation">(</span>currentTime <span class="token operator">-</span> prevIt<span class="token operator">-&gt;</span>timestamp<span class="token punctuation">)</span> <span class="token operator">/</span></span>
<span class="line">                  <span class="token keyword">float</span><span class="token punctuation">(</span>nextIt<span class="token operator">-&gt;</span>timestamp <span class="token operator">-</span> prevIt<span class="token operator">-&gt;</span>timestamp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        currentPosition_ <span class="token operator">=</span> <span class="token function">lerp</span><span class="token punctuation">(</span>prevIt<span class="token operator">-&gt;</span>position<span class="token punctuation">,</span> nextIt<span class="token operator">-&gt;</span>position<span class="token punctuation">,</span> t<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 移除过期的目标</span></span>
<span class="line">        targets_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>targets_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> prevIt<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> currentPosition_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 外推（预测未来位置）</span></span>
<span class="line">    Vector3 <span class="token function">extrapolate</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> currentTime<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> predictTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>targets_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> <span class="token number">2</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> currentPosition_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 使用最新的两个点计算速度</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> last <span class="token operator">=</span> targets_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">&amp;</span> prev <span class="token operator">=</span> targets_<span class="token punctuation">[</span>targets_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> <span class="token number">2</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        Vector3 velocity <span class="token operator">=</span> <span class="token punctuation">(</span>last<span class="token punctuation">.</span>position <span class="token operator">-</span> prev<span class="token punctuation">.</span>position<span class="token punctuation">)</span> <span class="token operator">/</span></span>
<span class="line">                          <span class="token punctuation">(</span>last<span class="token punctuation">.</span>timestamp <span class="token operator">-</span> prev<span class="token punctuation">.</span>timestamp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">float</span> deltaTime <span class="token operator">=</span> <span class="token punctuation">(</span>predictTime <span class="token operator">-</span> currentTime<span class="token punctuation">)</span> <span class="token operator">/</span> <span class="token number">1000.0f</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> last<span class="token punctuation">.</span>position <span class="token operator">+</span> velocity <span class="token operator">*</span> deltaTime<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>TargetPosition<span class="token operator">&gt;</span> targets_<span class="token punctuation">;</span></span>
<span class="line">    Vector3 currentPosition_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> Vector3 <span class="token function">lerp</span><span class="token punctuation">(</span><span class="token keyword">const</span> Vector3<span class="token operator">&amp;</span> a<span class="token punctuation">,</span> <span class="token keyword">const</span> Vector3<span class="token operator">&amp;</span> b<span class="token punctuation">,</span> <span class="token keyword">float</span> t<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> a <span class="token operator">+</span> <span class="token punctuation">(</span>b <span class="token operator">-</span> a<span class="token punctuation">)</span> <span class="token operator">*</span> t<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、状态同步最佳实践" tabindex="-1"><a class="header-anchor" href="#六、状态同步最佳实践"><span>六、状态同步最佳实践</span></a></h2><h3 id="_6-1-同步设计原则" tabindex="-1"><a class="header-anchor" href="#_6-1-同步设计原则"><span>6.1 同步设计原则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  状态同步设计原则                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 服务器权威                                              │</span>
<span class="line">│     ├── 关键状态由服务器控制                                 │</span>
<span class="line">│     ├── 客户端只是显示                                       │</span>
<span class="line">│     └── 防止作弊                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 最小化同步                                              │</span>
<span class="line">│     ├── 只同步必要数据                                       │</span>
<span class="line">│     ├── 使用增量同步                                         │</span>
<span class="line">│     ├── 设置同步阈值                                         │</span>
<span class="line">│     └── 压缩同步数据                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 优先级管理                                              │</span>
<span class="line">│     ├── 高优先级: 玩家操作、战斗事件                          │</span>
<span class="line">│     ├── 中优先级: NPC、怪物                                  │</span>
<span class="line">│     ├── 低优先级: 环境对象                                   │</span>
<span class="line">│     └── 动态调整同步频率                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 容错处理                                                │</span>
<span class="line">│     ├── 处理丢包                                            │</span>
<span class="line">│     ├── 处理乱序                                            │</span>
<span class="line">│     ├── 状态恢复                                            │</span>
<span class="line">│     └── 最终一致性                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 性能优化                                                │</span>
<span class="line">│     ├── 使用对象池                                           │</span>
<span class="line">│     ├── 批量处理同步                                         │</span>
<span class="line">│     ├── 多线程处理                                           │</span>
<span class="line">│     └── 避免不必要的拷贝                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-同步频率建议" tabindex="-1"><a class="header-anchor" href="#_6-2-同步频率建议"><span>6.2 同步频率建议</span></a></h3><table><thead><tr><th>实体类型</th><th>同步频率</th><th>优先级</th><th>说明</th></tr></thead><tbody><tr><td><strong>本地玩家</strong></td><td>20-30Hz</td><td>最高</td><td>自己的移动</td></tr><tr><td><strong>附近玩家</strong></td><td>10-15Hz</td><td>高</td><td>视野内其他玩家</td></tr><tr><td><strong>战斗怪物</strong></td><td>10-15Hz</td><td>高</td><td>正在战斗</td></tr><tr><td><strong>巡逻 NPC</strong></td><td>2-5Hz</td><td>中</td><td>非战斗状态</td></tr><tr><td><strong>环境对象</strong></td><td>0.5-1Hz</td><td>低</td><td>树木、建筑等</td></tr><tr><td><strong>静态物体</strong></td><td>按需</td><td>最低</td><td>只在变化时同步</td></tr></tbody></table><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="同步方案选择" tabindex="-1"><a class="header-anchor" href="#同步方案选择"><span>同步方案选择</span></a></h3><table><thead><tr><th>场景</th><th>推荐方案</th><th>理由</th></tr></thead><tbody><tr><td><strong>MMORPG</strong></td><td>状态同步</td><td>带宽友好、安全</td></tr><tr><td><strong>MOBA</strong></td><td>状态同步 + 预测</td><td>平衡体验和安全</td></tr><tr><td><strong>RTS</strong></td><td>帧同步</td><td>确定性、一致性好</td></tr><tr><td><strong>FPS</strong></td><td>客户端预测 + 回退</td><td>低延迟优先</td></tr><tr><td><strong>格斗游戏</strong></td><td>帧同步 + 延迟补偿</td><td>精确同步</td></tr></tbody></table><h3 id="kbengine-同步机制总结" tabindex="-1"><a class="header-anchor" href="#kbengine-同步机制总结"><span>KBEngine 同步机制总结</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 使用 Real/Ghost/Shadow 机制:</span>
<span class="line"></span>
<span class="line">Real Entity    → CellApp 上的权威实体，负责逻辑计算</span>
<span class="line">Ghost Entity   → 其他 CellApp/BaseApp 上的镜像，只读状态</span>
<span class="line">Shadow Entity  → 客户端上的显示实体，用于预测</span>
<span class="line"></span>
<span class="line">同步流程:</span>
<span class="line">1. Real Entity 状态变化</span>
<span class="line">2. 通知所有 Ghost Entity</span>
<span class="line">3. Ghost 通知对应的 Shadow (通过 BaseApp)</span>
<span class="line">4. Shadow 在客户端显示</span>
<span class="line"></span>
<span class="line">优势:</span>
<span class="line">- 分布式架构，支持大规模</span>
<span class="line">- AOI 过滤，减少同步量</span>
<span class="line">- 权威服务器，防止作弊</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/server/entitydef/entity_alias.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - Entity 同步机制</a></li><li><a href="https://www.bigworldtech.com/" target="_blank" rel="noopener noreferrer">BigWorld 技术文档 - Entity 同步</a></li><li><a href="https://gafferongames.com/post/networked_physics_2004/" target="_blank" rel="noopener noreferrer">Gaffer On Games - 网络游戏同步</a></li></ul>`,53)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};