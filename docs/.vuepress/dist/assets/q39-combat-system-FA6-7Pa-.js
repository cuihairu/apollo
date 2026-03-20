import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q39-combat-system.html","title":"Q39: 如何设计战斗系统？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q39-combat-system.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q39-combat-system.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q39-如何设计战斗系统" tabindex="-1"><a class="header-anchor" href="#q39-如何设计战斗系统"><span>Q39: 如何设计战斗系统？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对战斗系统设计的理解：</p><ul><li>战斗系统的核心组成</li><li>伤害计算流程</li><li>技能释放机制</li><li>战斗同步问题</li></ul><hr><h2 id="一、战斗系统架构" tabindex="-1"><a class="header-anchor" href="#一、战斗系统架构"><span>一、战斗系统架构</span></a></h2><h3 id="_1-1-战斗系统组成" tabindex="-1"><a class="header-anchor" href="#_1-1-战斗系统组成"><span>1.1 战斗系统组成</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    战斗系统架构                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  输入层:                                                    │</span>
<span class="line">│  ├── 玩家操作 (点击目标、释放技能)                           │</span>
<span class="line">│  ├── AI 决策 (自动攻击、技能选择)                            │</span>
<span class="line">│  └── 系统触发 (定时伤害、环境效果)                           │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼                                  │</span>
<span class="line">│  验证层:                                                    │</span>
<span class="line">│  ├── 范围检查 (是否在攻击距离内)                            │</span>
<span class="line">│  ├── 条件检查 (冷却、资源、状态)                             │</span>
<span class="line">│  ├── 目标验证 (敌对关系、存活状态)                           │</span>
<span class="line">│  └── 防作弊验证 (操作频率、异常行为)                           │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼                                  │</span>
<span class="line">│  计算层:                                                    │</span>
<span class="line">│  ├── 命中计算 (物理命中检测)                                  │</span>
<span class="line">│  ├── 伤害计算 (攻击力 - 防御力)                             │</span>
<span class="line">│  ├── 暴击计算 (暴击率、暴击伤害)                               │</span>
<span class="line">│  ├── 闪避计算 (闪避率)                                        │</span>
<span class="line">│  └── 吸收/格挡计算                                           │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼                                  │</span>
<span class="line">│  应用层:                                                    │</span>
<span class="line">│  ├── 扣除血量                                               │</span>
<span class="line">│  ├── 添加 Buff/Debuff                                       │</span>
<span class="line">│  ├── 触发被动效果                                             │</span>
<span class="line">│  └── 更新战斗状态                                           │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼                                  │</span>
<span class="line">│  同步层:                                                    │</span>
<span class="line">│  ├── 通知客户端战斗结果                                      │</span>
<span class="line">│  ├── 同步给其他玩家 (AOI 广播)                               │</span>
<span class="line">│  └── 保存战斗日志                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、伤害计算" tabindex="-1"><a class="header-anchor" href="#二、伤害计算"><span>二、伤害计算</span></a></h2><h3 id="_2-1-基础伤害公式" tabindex="-1"><a class="header-anchor" href="#_2-1-基础伤害公式"><span>2.1 基础伤害公式</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    伤害计算公式                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  基础伤害 = (攻击力 - 防御力) × 技能倍率 × 暴击倍率          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  详细公式:                                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  // 1. 基础伤害                                     │       │</span>
<span class="line">│  │  base_damage = attacker.attack - defender.defense   │       │</span>
<span class="line">│  │  base_damage = MAX(1, base_damage)  // 最小1点    │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  // 2. 技能加成                                     │       │</span>
<span class="line">│  │  skill_damage = base_damage × skill.multiplier  │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  // 3. 属性克制                                     │       │</span>
<span class="line">│  │  element_bonus = getElementBonus(                    │       │</span>
<span class="line">│  │      attacker.element, defender.element)             │       │</span>
<span class="line">│  │  )                                               │       │</span>
<span class="line">│  │  damage = skill_damage × (1 + element_bonus * 0.5)    │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  // 4. 暴击                                         │       │</span>
<span class="line">│  │  if (isCritical()) {                              │       │</span>
<span class="line">│  │      damage *= (1 + critical_rate)                 │       │</span>
<span class="line">│  │  }                                               │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  // 5. 防御加成                                     │       │</span>
<span class="line">│  │  damage *= (1 - defense_rate)                      │       │</span>
<span class="line">│  │  damage = MAX(1, (int)damage)                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-伤害计算实现" tabindex="-1"><a class="header-anchor" href="#_2-2-伤害计算实现"><span>2.2 伤害计算实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 战斗系统 - 伤害计算</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CombatSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 计算伤害</span></span>
<span class="line">    DamageResult <span class="token function">calculateDamage</span><span class="token punctuation">(</span><span class="token keyword">const</span> AttackRequest<span class="token operator">&amp;</span> req<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        DamageResult result<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 获取攻击者和防御者</span></span>
<span class="line">        Entity<span class="token operator">*</span> attacker <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>req<span class="token punctuation">.</span>attackerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        Entity<span class="token operator">*</span> defender <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>req<span class="token punctuation">.</span>defenderId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>attacker <span class="token operator">||</span> <span class="token operator">!</span>defender<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            result<span class="token punctuation">.</span>valid <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> result<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 验证攻击条件</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">validateAttack</span><span class="token punctuation">(</span>attacker<span class="token punctuation">,</span> defender<span class="token punctuation">,</span> req<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            result<span class="token punctuation">.</span>valid <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">            result<span class="token punctuation">.</span>reason <span class="token operator">=</span> <span class="token string">&quot;Attack not valid&quot;</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> result<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 计算基础伤害</span></span>
<span class="line">        <span class="token keyword">int</span> baseDamage <span class="token operator">=</span> attacker<span class="token operator">-&gt;</span><span class="token function">getAttack</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> defender<span class="token operator">-&gt;</span><span class="token function">getDefense</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        baseDamage <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">max</span><span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> baseDamage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 应用技能倍率</span></span>
<span class="line">        Skill<span class="token operator">*</span> skill <span class="token operator">=</span> <span class="token function">getSkill</span><span class="token punctuation">(</span>req<span class="token punctuation">.</span>skillId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>skill<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            baseDamage <span class="token operator">=</span> baseDamage <span class="token operator">*</span> skill<span class="token operator">-&gt;</span><span class="token function">getDamageMultiplier</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 属性克制</span></span>
<span class="line">        <span class="token keyword">float</span> elementBonus <span class="token operator">=</span> <span class="token function">getElementBonus</span><span class="token punctuation">(</span></span>
<span class="line">            attacker<span class="token operator">-&gt;</span><span class="token function">getElement</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            defender<span class="token operator">-&gt;</span><span class="token function">getElement</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        baseDamage <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">int</span><span class="token punctuation">)</span><span class="token punctuation">(</span>baseDamage <span class="token operator">*</span> <span class="token punctuation">(</span><span class="token number">1</span> <span class="token operator">+</span> elementBonus <span class="token operator">*</span> <span class="token number">0.5f</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 6. 暴击计算</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">rollCritical</span><span class="token punctuation">(</span>attacker<span class="token operator">-&gt;</span><span class="token function">getCriticalRate</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            baseDamage <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">int</span><span class="token punctuation">)</span><span class="token punctuation">(</span>baseDamage <span class="token operator">*</span> <span class="token punctuation">(</span><span class="token number">1</span> <span class="token operator">+</span> attacker<span class="token operator">-&gt;</span><span class="token function">getCriticalDamage</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            result<span class="token punctuation">.</span>isCritical <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 7. 闪避计算</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">rollDodge</span><span class="token punctuation">(</span>defender<span class="token operator">-&gt;</span><span class="token function">getDodgeRate</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            result<span class="token punctuation">.</span>damage <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">            result<span class="token punctuation">.</span>dodged <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            result<span class="token punctuation">.</span>valid <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> result<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 8. 格挡计算</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>defender<span class="token operator">-&gt;</span><span class="token function">isBlocking</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            baseDamage <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">int</span><span class="token punctuation">)</span><span class="token punctuation">(</span>baseDamage <span class="token operator">*</span> <span class="token number">0.5</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token comment">// 格挡减半伤</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 9. 应用防御加成</span></span>
<span class="line">        <span class="token keyword">float</span> defenseRate <span class="token operator">=</span> defender<span class="token operator">-&gt;</span><span class="token function">getDefenseRate</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        baseDamage <span class="token operator">=</span> <span class="token punctuation">(</span><span class="token keyword">int</span><span class="token punctuation">)</span><span class="token punctuation">(</span>baseDamage <span class="token operator">*</span> <span class="token punctuation">(</span><span class="token number">1</span> <span class="token operator">-</span> defenseRate<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        result<span class="token punctuation">.</span>damage <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">max</span><span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">,</span> baseDamage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        result<span class="token punctuation">.</span>valid <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> result<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理攻击</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processAttack</span><span class="token punctuation">(</span><span class="token keyword">const</span> AttackRequest<span class="token operator">&amp;</span> req<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 计算伤害</span></span>
<span class="line">        DamageResult result <span class="token operator">=</span> <span class="token function">calculateDamage</span><span class="token punctuation">(</span>req<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>result<span class="token punctuation">.</span>valid<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">sendError</span><span class="token punctuation">(</span>req<span class="token punctuation">.</span>attackerId<span class="token punctuation">,</span> result<span class="token punctuation">.</span>reason<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 应用伤害</span></span>
<span class="line">        Entity<span class="token operator">*</span> defender <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>req<span class="token punctuation">.</span>defenderId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        defender<span class="token operator">-&gt;</span><span class="token function">takeDamage</span><span class="token punctuation">(</span>result<span class="token punctuation">.</span>damage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 处理死亡</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>defender<span class="token operator">-&gt;</span><span class="token function">getHP</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;=</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">handleDeath</span><span class="token punctuation">(</span>defender<span class="token punctuation">,</span> req<span class="token punctuation">.</span>attackerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通知客户端</span></span>
<span class="line">        <span class="token function">broadcastDamageResult</span><span class="token punctuation">(</span>req<span class="token punctuation">,</span> result<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">validateAttack</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> attacker<span class="token punctuation">,</span> Entity<span class="token operator">*</span> defender<span class="token punctuation">,</span></span>
<span class="line">                         <span class="token keyword">const</span> AttackRequest<span class="token operator">&amp;</span> req<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 检查距离</span></span>
<span class="line">        <span class="token keyword">float</span> distance <span class="token operator">=</span> attacker<span class="token operator">-&gt;</span><span class="token function">distanceTo</span><span class="token punctuation">(</span>defender<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>distance <span class="token operator">&gt;</span> attacker<span class="token operator">-&gt;</span><span class="token function">getAttackRange</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 检查冷却</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>attacker<span class="token operator">-&gt;</span><span class="token function">isSkillOnCooldown</span><span class="token punctuation">(</span>req<span class="token punctuation">.</span>skillId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 检查资源</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>attacker<span class="token operator">-&gt;</span><span class="token function">getMP</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> req<span class="token punctuation">.</span>manaCost<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 检查状态</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>attacker<span class="token operator">-&gt;</span><span class="token function">isStunned</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> attacker<span class="token operator">-&gt;</span><span class="token function">isSilenced</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 检查目标</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>defender<span class="token operator">-&gt;</span><span class="token function">isAlive</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span> defender<span class="token operator">-&gt;</span><span class="token function">isInvincible</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">rollCritical</span><span class="token punctuation">(</span><span class="token keyword">float</span> rate<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span><span class="token function">rand</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">%</span> <span class="token number">10000</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> rate <span class="token operator">*</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">rollDodge</span><span class="token punctuation">(</span><span class="token keyword">float</span> rate<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span><span class="token function">rand</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">%</span> <span class="token number">10000</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> rate <span class="token operator">*</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">float</span> <span class="token function">getElementBonus</span><span class="token punctuation">(</span>ElementType attack<span class="token punctuation">,</span> ElementType defense<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 火克水、水克火、火克木、木克金、金克火</span></span>
<span class="line">        <span class="token keyword">const</span> <span class="token keyword">float</span> bonus<span class="token punctuation">[</span><span class="token number">6</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token number">6</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">//物理  火    水    木    金    土</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token number">0.0f</span><span class="token punctuation">,</span>  <span class="token number">0.5f</span><span class="token punctuation">,</span> <span class="token operator">-</span><span class="token number">0.5f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">}</span><span class="token punctuation">,</span> <span class="token comment">// 物理</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token number">0.0f</span><span class="token punctuation">,</span>  <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">}</span><span class="token punctuation">,</span> <span class="token comment">// 火</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token operator">-</span><span class="token number">0.5f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">}</span><span class="token punctuation">,</span> <span class="token comment">// 水</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">}</span><span class="token punctuation">,</span> <span class="token comment">// 木</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">}</span><span class="token punctuation">,</span> <span class="token comment">// 金</span></span>
<span class="line">            <span class="token punctuation">{</span><span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">,</span> <span class="token number">0.0f</span><span class="token punctuation">}</span>  <span class="token comment">// 土</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> bonus<span class="token punctuation">[</span><span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">int</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>attack<span class="token punctuation">)</span><span class="token punctuation">]</span><span class="token punctuation">[</span><span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">int</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>defense<span class="token punctuation">)</span><span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleDeath</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> victim<span class="token punctuation">,</span> <span class="token keyword">uint64_t</span> killerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 死亡处理</span></span>
<span class="line">        victim<span class="token operator">-&gt;</span><span class="token function">setAlive</span><span class="token punctuation">(</span><span class="token boolean">false</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 击杀者奖励</span></span>
<span class="line">        Entity<span class="token operator">*</span> killer <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>killerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>killer<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            killer<span class="token operator">-&gt;</span><span class="token function">addExp</span><span class="token punctuation">(</span>victim<span class="token operator">-&gt;</span><span class="token function">getRewardExp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token comment">// 可能掉落</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">rollDrop</span><span class="token punctuation">(</span>victim<span class="token operator">-&gt;</span><span class="token function">getDropRate</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">dropLoot</span><span class="token punctuation">(</span>victim<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 复活处理</span></span>
<span class="line">        <span class="token function">scheduleRevive</span><span class="token punctuation">(</span>victim<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcastDamageResult</span><span class="token punctuation">(</span><span class="token keyword">const</span> AttackRequest<span class="token operator">&amp;</span> req<span class="token punctuation">,</span></span>
<span class="line">                                <span class="token keyword">const</span> DamageResult<span class="token operator">&amp;</span> result<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 广播给 AOI 内的玩家</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">:</span> <span class="token function">getAOIEntities</span><span class="token punctuation">(</span>req<span class="token punctuation">.</span>defenderId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">sendDamageNotification</span><span class="token punctuation">(</span>entity<span class="token operator">-&gt;</span><span class="token function">getId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> req<span class="token punctuation">,</span> result<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 数据结构</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">AttackRequest</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> attackerId<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> defenderId<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> skillId<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> manaCost<span class="token punctuation">;</span></span>
<span class="line">    Vector3 targetPosition<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">DamageResult</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">bool</span> valid <span class="token operator">=</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> damage <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> isCritical <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> dodged <span class="token operator">=</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string reason<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、技能系统" tabindex="-1"><a class="header-anchor" href="#三、技能系统"><span>三、技能系统</span></a></h2><h3 id="_3-技能释放流程" tabindex="-1"><a class="header-anchor" href="#_3-技能释放流程"><span>3. 技能释放流程</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    技能释放流程                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 客户端请求                                             │</span>
<span class="line">│     │</span>
<span class="line">│     ▼                                                      │</span>
<span class="line">│  2. 服务器验证                                               │</span>
<span class="line">│     ├── 检查技能是否学习                                   │</span>
<span class="line">│     ├── 检查冷却时间                                       │</span>
<span class="line">│     ├── 检查魔法值 (MP)                                   │</span>
<span class="line">│     ├── 检查目标是否有效                                    │</span>
<span class="line">│     └── 检查施法距离                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│     │  验证失败 → 返回错误                               │</span>
<span class="line">│     │  验证成功 → 继续                                     │</span>
<span class="line">│     │                                                      │</span>
<span class="line">│     ▼                                                      │</span>
<span class="line">│  3. 执行技能效果                                             │</span>
<span class="line">│     ├── 消耗魔法值                                         │</span>
<span class="line">│     ├── 设置冷却时间                                       │</span>
<span class="line">│     ├── 计算伤害                                           │</span>
<span class="line">│     ├── 应用技能效果                                        │</span>
<span class="line">│     └── 返回结果                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│     ▼                                                      │</span>
<span class="line">│  4. 同步结果                                                │</span>
<span class="line">│     ├── 通知客户端                                         │</span>
<span class="line">│     ├── 更新玩家状态                                        │</span>
<span class="line">│     ├── 通知 AOI 玩家                                      │</span>
<span class="line">│     └── 保存战斗日志                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、战斗同步" tabindex="-1"><a class="header-anchor" href="#四、战斗同步"><span>四、战斗同步</span></a></h2><h3 id="_4-1-延迟补偿" tabindex="-1"><a class="header-anchor" href="#_4-1-延迟补偿"><span>4.1 延迟补偿</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    战斗同步问题                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题: 玩家和服务器之间有延迟                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  玩家客户端 ──延迟───► 服务器                      │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  t=0: 玩家点击攻击                                 │       │</span>
<span class="line">│  │  t=50ms: 服务器收到请求                             │       │</span>
<span class="line">│  │  t=80ms: 玩家收到结果                               │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  问题: 50-80ms 的延迟让玩家感觉卡顿                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  解决方案: 客户端预测                                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 玩家点击攻击立即播放攻击动画                       │       │</span>
<span class="line">│  │  2. 同时发送请求到服务器                             │       │</span>
<span class="line">│  │  3. 服务器计算伤害后返回                           │       │</span>
<span class="line">│  │  4. 客户端根据服务器结果校正:                        │       │</span>
<span class="line">│  │     - 如果一致 → 无需调整                             │       │</span>
<span class="line">│  │     - 如果差异大 → 强制校正位置                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、最佳实践" tabindex="-1"><a class="header-anchor" href="#五、最佳实践"><span>五、最佳实践</span></a></h2><h3 id="_5-1-战斗系统设计建议" tabindex="-1"><a class="header-anchor" href="#_5-1-战斗系统设计建议"><span>5.1 战斗系统设计建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>服务器权威</strong></td><td>所有伤害计算在服务器</td></tr><tr><td><strong>客户端预测</strong></td><td>提升体验，需校正</td></tr><tr><td><strong>AOI 广播</strong></td><td>只同步可见玩家</td></tr><tr><td><strong>技能队列</strong></td><td>防止技能连发</td></tr><tr><td><strong>状态锁</strong></td><td>防止状态异常</td></tr></tbody></table><h3 id="_5-2-常见问题" tabindex="-1"><a class="header-anchor" href="#_5-2-常见问题"><span>5.2 常见问题</span></a></h3><table><thead><tr><th>问题</th><th>解决方案</th></tr></thead><tbody><tr><td><strong>伤害计算不准</strong></td><td>服务器权威计算</td></tr><tr><td>**技能连发</td><td>技能队列 + 冷却检查</td></tr><tr><td><strong>延迟大</strong></td><td>客户端预测 + 插值</td></tr><tr><td><strong>不同步</strong></td><td>时间戳 + 序列号</td></tr></tbody></table><hr><h2 id="六、总结" tabindex="-1"><a class="header-anchor" href="#六、总结"><span>六、总结</span></a></h2><h3 id="战斗系统核心" tabindex="-1"><a class="header-anchor" href="#战斗系统核心"><span>战斗系统核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">战斗系统 = 伤害计算 + 技能系统 + 状态同步</span>
<span class="line">- 服务器权威计算，防止作弊</span>
<span class="line">- 客户端预测，提升体验</span>
<span class="line">- AOI 优化同步范围</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.gamedev.net/" target="_blank" rel="noopener noreferrer">MMO 战斗系统设计</a></li><li><a href="https://gafferongames.com/post/networked_physics_2004/" target="_blank" rel="noopener noreferrer">游戏战斗同步</a></li></ul>`,35)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};