import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q27-entity-ghost-shadow-relationship.html","title":"Q27: Real、Ghost、Shadow Entity 之间如何转换？如何高效同步？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q27-entity-ghost-shadow-relationship.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q27-entity-ghost-shadow-relationship.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q27-real、ghost、shadow-entity-之间如何转换-如何高效同步" tabindex="-1"><a class="header-anchor" href="#q27-real、ghost、shadow-entity-之间如何转换-如何高效同步"><span>Q27: Real、Ghost、Shadow Entity 之间如何转换？如何高效同步？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对 BigWorld/KBEngine 中 <strong>Entity 三态模型</strong> 的深入理解：</p><ul><li>Real、Ghost、Shadow 的定义和区别</li><li>三者之间的转换条件和机制</li><li>高效同步的策略</li></ul><hr><h2 id="entity-三态定义" tabindex="-1"><a class="header-anchor" href="#entity-三态定义"><span>Entity 三态定义</span></a></h2><h3 id="概念对比表" tabindex="-1"><a class="header-anchor" href="#概念对比表"><span>概念对比表</span></a></h3><table><thead><tr><th>维度</th><th>Real Entity</th><th>Ghost Entity</th><th>Shadow Entity</th></tr></thead><tbody><tr><td><strong>定义</strong></td><td>权威的、可写的 Entity</td><td>跨边界的只读副本</td><td>客户端预测的本地副本</td></tr><tr><td><strong>位置</strong></td><td>所属 CellApp 上</td><td>相邻 CellApp 上</td><td>客户端上</td></tr><tr><td><strong>可写性</strong></td><td>✅ 可写</td><td>❌ 只读</td><td>✅ 可写（预测）</td></tr><tr><td><strong>权威性</strong></td><td>权威数据源</td><td>非权威，从 Real 同步</td><td>非权威，预测后需校验</td></tr><tr><td><strong>同步方向</strong></td><td>Real → Ghost/Shadow</td><td>Real → Ghost</td><td>Client ↔ Server（预测校验）</td></tr><tr><td><strong>用途</strong></td><td>实际逻辑处理</td><td>跨边界交互</td><td>客户端预测、减少延迟感</td></tr></tbody></table><h3 id="三态关系图" tabindex="-1"><a class="header-anchor" href="#三态关系图"><span>三态关系图</span></a></h3>`,9),i(d,{code:`eJxLy8kvT85ILCpRCHHiUgCC4tKk9KLEggyF4NSistSiaKVnc3qfdi18vnq9UixYAYoi59ScHMeCAsNoJShLwRBJGQgEpSbmAKVBlIJrXklmSaVNUpG+3bO5zU9XND5uaHzav/5p20wkTal5KTjtMULYY4Rmj3tGfnEJ0CIwjWzT0/5VL9bvftq5/tmcNVisAdGoHnfOyUzNK4lWerpu0bOO7ageD85ITMkvj1aC0MjWvFzU8mxrN4o1cKPBYaCgq2tXo/R0Qs+ztUufd2171tCoVAN1NboiiDREKVARxDKwIqi9YFUQG1/sm/y0FY+qZ5N7n+yd82L99mcbm4CqwLZAPVxSmZMKtTYtMyfHStnSwNXV0gBJEuI8qKybm6uJkymSLNQaiKyrmauZmyMXANmI0Mc=`}),o[1]||=e(`<hr><h2 id="三种-entity-详解" tabindex="-1"><a class="header-anchor" href="#三种-entity-详解"><span>三种 Entity 详解</span></a></h2><h3 id="real-entity-权威实体" tabindex="-1"><a class="header-anchor" href="#real-entity-权威实体"><span>Real Entity（权威实体）</span></a></h3><p><strong>特性</strong>：</p><ul><li>Entity 的&quot;真身&quot;，所有权威逻辑的执行者</li><li>只有 Real Entity 可以修改状态</li><li>每个 Entity 同时只能有一个 Real</li></ul><p><strong>典型场景</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">玩家 A 在 CellApp1 管理的区域</span>
<span class="line">├── Real Entity 在 CellApp1</span>
<span class="line">├── 可以攻击、移动、拾取物品</span>
<span class="line">└── 状态变化会同步到：</span>
<span class="line">    ├── Ghost Entity（CellApp2）</span>
<span class="line">    └── Shadow Entity（客户端）</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="ghost-entity-影子实体" tabindex="-1"><a class="header-anchor" href="#ghost-entity-影子实体"><span>Ghost Entity（影子实体）</span></a></h3><p><strong>特性</strong>：</p><ul><li>跨 CellApp 边界的只读副本</li><li>用于边界交互（如跨边界攻击）</li><li>状态由 Real 同步，不能本地修改</li></ul><p><strong>为什么需要 Ghost</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">场景：玩家在边界攻击对面的人</span>
<span class="line"></span>
<span class="line">┌─────────────────┬─────────────────┐</span>
<span class="line">│   CellApp 1     │   CellApp 2     │</span>
<span class="line">│                 │                 │</span>
<span class="line">│  [Real: 玩家A]   │  [Real: 怪物B]   │</span>
<span class="line">│       │         │       ▲         │</span>
<span class="line">│       │ 攻击     │       │         │</span>
<span class="line">│       ┼─────────┼───────┘         │</span>
<span class="line">│       ▼         │                 │</span>
<span class="line">│ [Ghost: 怪物B]  │                 │</span>
<span class="line">│  (只读副本)      │                 │</span>
<span class="line">└─────────────────┴─────────────────┘</span>
<span class="line"></span>
<span class="line">如果没有 Ghost：</span>
<span class="line">- 玩家A 无法&quot;看到&quot;怪物B 的引用</span>
<span class="line">- 无法发起攻击</span>
<span class="line">- 或者需要跨 CellApp 实时查询（性能差）</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="shadow-entity-阴影实体" tabindex="-1"><a class="header-anchor" href="#shadow-entity-阴影实体"><span>Shadow Entity（阴影实体）</span></a></h3><p><strong>特性</strong>：</p><ul><li>客户端的预测副本</li><li>本地可写，用于客户端预测</li><li>需要与服务器状态校验</li></ul><p><strong>为什么需要 Shadow</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">客户端预测流程：</span>
<span class="line"></span>
<span class="line">1. 玩家按下移动键</span>
<span class="line">2. 客户端立即更新 Shadow 位置（无延迟感）</span>
<span class="line">3. 同时发送移动请求到服务器</span>
<span class="line">4. 服务器计算 Real 位置</span>
<span class="line">5. 服务器将真实位置同步回客户端</span>
<span class="line">6. 客户端校正 Shadow 位置（如有偏差）</span>
<span class="line"></span>
<span class="line">没有 Shadow 的问题：</span>
<span class="line">- 每次操作都要等服务器响应</span>
<span class="line">- 100ms 延迟下操作感极其卡顿</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="entity-转换机制" tabindex="-1"><a class="header-anchor" href="#entity-转换机制"><span>Entity 转换机制</span></a></h2><h3 id="real-↔-ghost-转换" tabindex="-1"><a class="header-anchor" href="#real-↔-ghost-转换"><span>Real ↔ Ghost 转换</span></a></h3><p><strong>触发条件</strong>：Entity 跨越 CellApp 边界</p>`,21),i(d,{code:`eJwrLkksSXXJTEwvSszVLTPiUgCCaK1YBV1dO4Wg1MQcKwXXvJLMkkqFpx2zn+7eBZYHiYMVuGfkF5dYKbzYP/tp69Lns3e8bNqt4Jyak+NYUKDwfFaLgqO/J1gDWB2mkS/2Nz5fvvtpx4Znc9bA9KFaAHQJXPXLKQ3P1jeimQdW8HzZ7qd7Gl7s2/l8ag/YTrCivPySVIWizPSMEoX8NLCRYGEQeDa3+emKxqfr5j3ZOxku+LR//dO2mc+7tj1rgNiSmpcCNgSbcWAXIGld9WL97qed64EeQQium/VszvynE3qerV2Kah4AZr2NUg==`}),o[2]||=e(`<p><strong>Real → Ghost（进入边界）</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">玩家从 CellApp1 移动到 CellApp2 边界：</span>
<span class="line"></span>
<span class="line">1. CellApp1 检测到玩家进入边界区域</span>
<span class="line">2. CellApp1 向 CellApp2 发送创建 Ghost 请求</span>
<span class="line">3. CellApp2 创建 Ghost Entity</span>
<span class="line">4. CellApp1 定期同步状态到 CellApp2</span>
<span class="line"></span>
<span class="line">同步内容：</span>
<span class="line">- 位置 (x, y, z)</span>
<span class="line">- 朝向 (yaw, pitch, roll)</span>
<span class="line">- 速度 (vx, vy, vz)</span>
<span class="line">- 可见状态（血量、装备等）</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>Ghost → Real（Entity 迁移）</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">玩家完全进入 CellApp2 区域：</span>
<span class="line"></span>
<span class="line">1. CellApp1 开始迁移流程</span>
<span class="line">2. CellApp1 将完整状态发送到 CellApp2</span>
<span class="line">3. CellApp2 创建 Real Entity（从 Ghost 升级）</span>
<span class="line">4. CellApp1 通知各相关方：Entity 已迁移</span>
<span class="line">5. CellApp1 销毁 Real Entity</span>
<span class="line">6. 其他 CellApp 上的 Ghost 更新目标</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="real-↔-shadow-转换" tabindex="-1"><a class="header-anchor" href="#real-↔-shadow-转换"><span>Real ↔ Shadow 转换</span></a></h3><p><strong>这是同步关系而非所有权转移</strong>：</p>`,6),i(d,{code:`eJx1UMtOwkAU3fMVE1eQaNizYFPXLuwXNDDRJrVgqbotj0QECRpRYmjKwxhIgLZEEBJs/BnmTvsXlg4YHjrLe86cVwZfXmE5gY9F4UwRLkLIf2lBUcWEmBZkFXFIyCBOErGs7mH8EuOxco0VFD7FghQJBZyTlIpRannlDvkYguEbmc3Iwz0M32GSpb1ywOKP4nEuhmjpE7QsQ1F44VSoY0bJyACtGwl4HONBYwwvNuLPhWTq5hdYyj9VFo7uWlMY5f70J2YHilPat7xOASbMfC2qD4huszsK0+6clHpRqM3J7XzLnPbLpPLxbwTylfPTk+qjp2VXQdYNfdQ129SsU71JTIO13ezvftdIw9hCA1iQVMSC+fvRthkcNyKxFKv5oN7ydM218/A8DohYyuDVd6/26lrW7ndotX3dtQop2nv5dpb0W1YtN++AfndAjYG/1QEUmsxMToZ+ANW0AUI=`}),o[3]||=e(`<p><strong>Shadow 状态管理</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Shadow 的几种状态：</span>
<span class="line"></span>
<span class="line">1. 创建态</span>
<span class="line">   - 玩家登录</span>
<span class="line">   - 新 Entity 进入 AOI</span>
<span class="line"></span>
<span class="line">2. 预测态</span>
<span class="line">   - 客户端输入产生变化</span>
<span class="line">   - 等待服务器确认</span>
<span class="line"></span>
<span class="line">3. 同步态</span>
<span class="line">   - 收到服务器确认</span>
<span class="line">   - Shadow 与 Real 一致</span>
<span class="line"></span>
<span class="line">4. 校正态</span>
<span class="line">   - 预测与真实不符</span>
<span class="line">   - 插值平滑过渡到正确状态</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="ghost-↔-shadow-关系" tabindex="-1"><a class="header-anchor" href="#ghost-↔-shadow-关系"><span>Ghost ↔ Shadow 关系</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Ghost 和 Shadow 都是非权威副本，但服务于不同目的：</span>
<span class="line"></span>
<span class="line">Ghost（服务端）：</span>
<span class="line">- 用于跨 CellApp 边界交互</span>
<span class="line">- 服务端组件之间同步</span>
<span class="line">- 保证服务端逻辑完整性</span>
<span class="line"></span>
<span class="line">Shadow（客户端）：</span>
<span class="line">- 用于客户端预测</span>
<span class="line">- 服务器到客户端同步</span>
<span class="line">- 提升用户体验</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="高效同步策略" tabindex="-1"><a class="header-anchor" href="#高效同步策略"><span>高效同步策略</span></a></h2><h3 id="_1-状态同步优化" tabindex="-1"><a class="header-anchor" href="#_1-状态同步优化"><span>1. 状态同步优化</span></a></h3><p><strong>增量同步</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">每次只同步变化的状态：</span>
<span class="line"></span>
<span class="line">不做：</span>
<span class="line">- 每次发送整个 Entity 状态</span>
<span class="line"></span>
<span class="line">要做：</span>
<span class="line">- 维护脏标记（Dirty Flags）</span>
<span class="line">- 只发送变化的属性</span>
<span class="line">- 使用位掩码表示变化字段</span>
<span class="line"></span>
<span class="line">示例协议：</span>
<span class="line">struct EntitySync {</span>
<span class="line">    uint64 entity_id;</span>
<span class="line">    uint32 dirty_flags;    // 每个位代表一个属性</span>
<span class="line">    float x, y, z;         // 仅当位置脏时发送</span>
<span class="line">    uint32 hp;             // 仅当血量脏时发送</span>
<span class="line">    // ...</span>
<span class="line">};</span>
<span class="line"></span>
<span class="line">Dirty Flags:</span>
<span class="line">bit 0: 位置</span>
<span class="line">bit 1: 朝向</span>
<span class="line">bit 2: 速度</span>
<span class="line">bit 3: 血量</span>
<span class="line">// ...</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>优先级同步</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">根据重要性分级同步：</span>
<span class="line"></span>
<span class="line">高优先级（每帧）：</span>
<span class="line">- 玩家自己</span>
<span class="line">- 攻击目标</span>
<span class="line"></span>
<span class="line">中优先级（每 3-5 帧）：</span>
<span class="line">- 附近玩家</span>
<span class="line">- 附近 NPC</span>
<span class="line"></span>
<span class="line">低优先级（每 10+ 帧）：</span>
<span class="line">- 远处 Entity</span>
<span class="line">- 装饰品</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-ghost-同步策略" tabindex="-1"><a class="header-anchor" href="#_2-ghost-同步策略"><span>2. Ghost 同步策略</span></a></h3>`,12),i(d,{code:`eJxLy8kvT85ILCpR8AniUgCCoNTEnGgQoeCaV5JZUhmroKtrp+CckZiXnhr9vGvbs4bGp/0znvZMiwUrh0hA1BRllmQmJ+ZUP5ux/umEZU9bN7+css6+lguiDioJUlkDVFCj4Jmbm5qSmVgCNHV199PezU8n9DxbuxRqKrJqoFk1Ck6JJckZqSnRzzp3vmzvh6kFK4YbBHaFe0Z+cYlhNJhSeDZ7y7NpGyBmQk0AKwrJzE0tin66btaz6duezlzxYvmyp/0TIcrAUgiTjGAmge2FmQdWWVxSmZOKZHtaZk6OlbKbm5OZsyGSApi9EGkLc2dXVycuACNNlBg=`}),o[4]||=e(`<p><strong>关键状态立即同步</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">立即同步：</span>
<span class="line">- 血量变化</span>
<span class="line">- 死亡状态</span>
<span class="line">- 装备变化</span>
<span class="line">- 技能释放</span>
<span class="line"></span>
<span class="line">批量同步（每 50-100ms）：</span>
<span class="line">- 位置微调</span>
<span class="line">- 朝向变化</span>
<span class="line">- 动画状态</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-shadow-同步策略" tabindex="-1"><a class="header-anchor" href="#_3-shadow-同步策略"><span>3. Shadow 同步策略</span></a></h3><p><strong>插值与外推</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">客户端位置同步：</span>
<span class="line"></span>
<span class="line">1. 接收服务器位置更新</span>
<span class="line">2. 不直接设置，而是插值过渡</span>
<span class="line"></span>
<span class="line">插值算法：</span>
<span class="line">current_pos = lerp(shadow_pos, server_pos, factor)</span>
<span class="line"></span>
<span class="line">factor 取值：</span>
<span class="line">- 0.1-0.3：平滑但延迟感强</span>
<span class="line">- 0.5-0.8：响应快但可能有抖动</span>
<span class="line">- 动态调整：根据网络状况自适应</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>预测校正平滑</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">当预测错误时，不要直接跳转：</span>
<span class="line"></span>
<span class="line">错误做法：</span>
<span class="line">shadow_pos = server_pos  // 会导致瞬移</span>
<span class="line"></span>
<span class="line">正确做法：</span>
<span class="line">// 在若干帧内平滑过渡到正确位置</span>
<span class="line">correction_speed = 0.2f</span>
<span class="line">shadow_pos += (server_pos - shadow_pos) * correction_speed</span>
<span class="line"></span>
<span class="line">当偏差过大时：</span>
<span class="line">- 才使用强制校正</span>
<span class="line">- 同时播放&quot;传送&quot;特效掩盖</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-带宽优化" tabindex="-1"><a class="header-anchor" href="#_4-带宽优化"><span>4. 带宽优化</span></a></h3><p><strong>量化压缩</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">浮点数量化：</span>
<span class="line">float position → uint16 (量化到厘米级别)</span>
<span class="line">float angle → uint8 (量化到 1-2 度)</span>
<span class="line"></span>
<span class="line">示例：</span>
<span class="line">原始：3 x float (12 bytes) → position</span>
<span class="line">压缩：3 x uint16 (6 bytes) → position</span>
<span class="line"></span>
<span class="line">节省 50% 带宽，精度足够游戏使用</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>Delta 编码</strong>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">对于连续值，发送变化量：</span>
<span class="line"></span>
<span class="line">不用：</span>
<span class="line">position_x = 1234.56</span>
<span class="line"></span>
<span class="line">改用：</span>
<span class="line">delta_x = current_x - last_x</span>
<span class="line">         = 0.05  // 很小的值</span>
<span class="line"></span>
<span class="line">小值可以用更少的字节编码</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="转换流程图" tabindex="-1"><a class="header-anchor" href="#转换流程图"><span>转换流程图</span></a></h2><h3 id="entity-完整生命周期" tabindex="-1"><a class="header-anchor" href="#entity-完整生命周期"><span>Entity 完整生命周期</span></a></h3>`,15),i(d,{code:`eJx1Us9LAkEYvfdXDHQWDEqwQ6GydtIguy0eFh1TmlxZ12RRIQ9WyBYS2A86RmkHrZNtSvXPNLv7Z7Q736yOYHOa+d573/fezBSIWs8VFU1Hh/E15K2M7h1k5/qVjifOw4x+9bMoFNpBCQ0rOj7ACpHp5SOdTZG/R1JZL+lGlkkXFEGRKSp5tR5o4DRXCTKO+MK0qp0opGGPnqhlOd2JfdZuMSYAjJNST/GG7AxmtDuE6awCg4s4dxxXa+W8ohkN92PoTrru96fTN3dbfKTI8DVN2ntp8v7/MOz7tyb3uldUq3oQiR1WJQLAN8R2GaOck6EGmWjPtEfPYH7OWARIlY40r0uDjk3aGbo/bS/skn9OWNhf8ryax0LwggyeEbTm1gUySuM6e2/79h0lMCGxSgUJbw/OOYkJ9kkersa+G/B7oVcXznTwa4mSgAZZCVbKtYpsWx2nd+4LWTt6Y0KH4EqBJvwPqFd1g2CQFEqEbK9Hw5IUDQsYjAIwmZQ241sCyH8doFJEiiRja38oryt7`}),o[5]||=e(`<hr><h2 id="实现示例" tabindex="-1"><a class="header-anchor" href="#实现示例"><span>实现示例</span></a></h2><h3 id="ghost-创建代码示例" tabindex="-1"><a class="header-anchor" href="#ghost-创建代码示例"><span>Ghost 创建代码示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// CellApp1: 玩家进入边界，需要创建 Ghost</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">CellApp</span><span class="token double-colon punctuation">::</span><span class="token function">onEntityEnterBoundary</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> CellApp<span class="token operator">*</span> neighborApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 1. 序列化 Entity 状态</span></span>
<span class="line">    MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line">    entity<span class="token operator">-&gt;</span><span class="token function">serializeTo</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 2. 发送创建 Ghost 请求</span></span>
<span class="line">    CreateGhostPacket packet<span class="token punctuation">;</span></span>
<span class="line">    packet<span class="token punctuation">.</span>entity_id <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    packet<span class="token punctuation">.</span>data <span class="token operator">=</span> stream<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    neighborApp<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>packet<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// CellApp2: 接收创建 Ghost 请求</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">CellApp</span><span class="token double-colon punctuation">::</span><span class="token function">onCreateGhost</span><span class="token punctuation">(</span><span class="token keyword">const</span> CreateGhostPacket<span class="token operator">&amp;</span> packet<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 1. 创建 Ghost Entity</span></span>
<span class="line">    GhostEntity<span class="token operator">*</span> ghost <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">GhostEntity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    ghost<span class="token operator">-&gt;</span><span class="token function">setGhostMode</span><span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 标记为只读</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 2. 反序列化状态</span></span>
<span class="line">    MemoryStream <span class="token function">stream</span><span class="token punctuation">(</span>packet<span class="token punctuation">.</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    ghost<span class="token operator">-&gt;</span><span class="token function">deserializeFrom</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 3. 注册到 Ghost 管理</span></span>
<span class="line">    ghostEntities_<span class="token punctuation">[</span>packet<span class="token punctuation">.</span>entity_id<span class="token punctuation">]</span> <span class="token operator">=</span> ghost<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 4. 加入 AOI（只读，不触发进入事件）</span></span>
<span class="line">    aoi_<span class="token operator">-&gt;</span><span class="token function">addGhost</span><span class="token punctuation">(</span>ghost<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="shadow-同步代码示例" tabindex="-1"><a class="header-anchor" href="#shadow-同步代码示例"><span>Shadow 同步代码示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 客户端：接收服务器状态更新</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">Client</span><span class="token double-colon punctuation">::</span><span class="token function">onEntitySync</span><span class="token punctuation">(</span><span class="token keyword">const</span> EntitySyncPacket<span class="token operator">&amp;</span> packet<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>packet<span class="token punctuation">.</span>entity_id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 不直接设置，而是记录服务器状态</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setServerState</span><span class="token punctuation">(</span>packet<span class="token punctuation">.</span>state<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setLastSyncTime</span><span class="token punctuation">(</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 插值平滑会在 update 中处理</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 客户端：每帧更新</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">Client</span><span class="token double-colon punctuation">::</span><span class="token function">update</span><span class="token punctuation">(</span><span class="token keyword">float</span> delta_time<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">:</span> entities_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">hasServerState</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 平滑插值到服务器状态</span></span>
<span class="line">            <span class="token keyword">float</span> t <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">min</span><span class="token punctuation">(</span>delta_time <span class="token operator">*</span> interpolation_speed<span class="token punctuation">,</span> <span class="token number">1.0f</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">setPosition</span><span class="token punctuation">(</span><span class="token function">lerp</span><span class="token punctuation">(</span></span>
<span class="line">                entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                entity<span class="token operator">-&gt;</span><span class="token function">getServerPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                t</span>
<span class="line">            <span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="常见问题" tabindex="-1"><a class="header-anchor" href="#常见问题"><span>常见问题</span></a></h2><h3 id="q1-ghost-和-real-数据不一致怎么办" tabindex="-1"><a class="header-anchor" href="#q1-ghost-和-real-数据不一致怎么办"><span>Q1: Ghost 和 Real 数据不一致怎么办？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题：网络延迟导致 Ghost 状态落后</span>
<span class="line"></span>
<span class="line">解决方案：</span>
<span class="line">1. Ghost 只用于可见性，不做权威判断</span>
<span class="line">2. 关键操作（攻击）通过 Real 验证</span>
<span class="line">3. Ghost 定期全量同步（每 1-2 秒）</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="q2-shadow-预测错误太多怎么办" tabindex="-1"><a class="header-anchor" href="#q2-shadow-预测错误太多怎么办"><span>Q2: Shadow 预测错误太多怎么办？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题：频繁的预测校正导致画面抖动</span>
<span class="line"></span>
<span class="line">解决方案：</span>
<span class="line">1. 调整预测算法保守度</span>
<span class="line">2. 提高服务器发送频率</span>
<span class="line">3. 使用更平滑的校正曲线</span>
<span class="line">4. 根据网络质量动态调整策略</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="q3-边界频繁切换导致大量-ghost-创建-销毁" tabindex="-1"><a class="header-anchor" href="#q3-边界频繁切换导致大量-ghost-创建-销毁"><span>Q3: 边界频繁切换导致大量 Ghost 创建/销毁？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题：玩家在边界反复横跳</span>
<span class="line"></span>
<span class="line">解决方案：</span>
<span class="line">1. 设置边界缓冲区</span>
<span class="line">2. Ghost 进入后不立即销毁，延迟一段时间</span>
<span class="line">3. 使用引用计数管理 Ghost 生命周期</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine Entity 机制</a></li><li><a href="https://www.bigworldtech.com/" target="_blank" rel="noopener noreferrer">BigWorld Entity Architecture</a></li><li><a href="https://gafferongames.com/categories/networking/" target="_blank" rel="noopener noreferrer">Gaffer on Games - Networking</a></li><li><a href="https://docs.unrealengine.com/5.0/en-us/Networking/Replication/" target="_blank" rel="noopener noreferrer">Unreal Network Replication</a></li></ul>`,17)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};