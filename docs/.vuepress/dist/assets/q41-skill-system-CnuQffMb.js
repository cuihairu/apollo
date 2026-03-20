import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q41-skill-system.html","title":"Q41: 如何设计技能系统？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q41-skill-system.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q41-skill-system.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q41-如何设计技能系统" tabindex="-1"><a class="header-anchor" href="#q41-如何设计技能系统"><span>Q41: 如何设计技能系统？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对技能系统设计的理解：</p><ul><li>技能系统的核心要素</li><li>数据驱动设计</li><li>服务器验证</li><li>KBEngine 的技能实现</li></ul><hr><h2 id="一、技能系统核心" tabindex="-1"><a class="header-anchor" href="#一、技能系统核心"><span>一、技能系统核心</span></a></h2><h3 id="_1-1-核心要素" tabindex="-1"><a class="header-anchor" href="#_1-1-核心要素"><span>1.1 核心要素</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    技能系统核心要素                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 技能定义                                                │</span>
<span class="line">│     ├── 技能 ID                                            │</span>
<span class="line">│     ├── 技能名称                                            │</span>
<span class="line">│     ├── 技能类型 (主动/被动/通道)                           │</span>
<span class="line">│     ├── 技能图标                                            │</span>
<span class="line">│     └── 冷却时间                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 技能效果                                                │</span>
<span class="line">│     ├── 伤害效果                                            │</span>
<span class="line">│     ├── Buff/Debuff                                         │</span>
<span class="line">│     ├── 治疗/恢复                                           │</span>
<span class="line">│     ├── 召唤/创造                                           │</span>
<span class="line">│     └── 移动/位移                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 释放条件                                                │</span>
<span class="line">│     ├── 等级要求                                            │</span>
<span class="line">│     ├── 消耗资源 (MP/Item)                                 │</span>
<span class="line">│     ├── 武器/装备要求                                       │</span>
<span class="line">│     ├── 状态要求 (战斗中/骑乘中)                           │</span>
<span class="line">│     └── 位置/方向要求                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 目标选择                                                │</span>
<span class="line">│     ├── 自身                                               │</span>
<span class="line">│     ├── 选中目标                                           │</span>
<span class="line">│     ├── 区域内目标                                         │</span>
<span class="line">│     └── 指定位置                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、数据驱动设计" tabindex="-1"><a class="header-anchor" href="#二、数据驱动设计"><span>二、数据驱动设计</span></a></h2><h3 id="_2-1-技能配置表" tabindex="-1"><a class="header-anchor" href="#_2-1-技能配置表"><span>2.1 技能配置表</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    技能配置表设计                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  技能基础表 (skill_base)：                                   │</span>
<span class="line">│  ┌──────┬────────┬──────────┬─────────┬──────────┐         │</span>
<span class="line">│  │ id   │  name   │   type    │ icon_id  │ cooldown  │         │</span>
<span class="line">│  ├──────┼────────┼──────────┼─────────┼──────────┤         │</span>
<span class="line">│  │ 1001 │ 火球术 │  ACTIVE   │ icon_101 │    3000  │         │</span>
<span class="line">│  │ 1002 │ 治疗术 │  ACTIVE   │ icon_102 │    5000  │         │</span>
<span class="line">│  │ 1003 │ 强力攻击│ PASSIVE  │ icon_103 │    0     │         │</span>
<span class="line">│  │ 1004 │ 烈剑    │ CHANNEL  │ icon_104 │    0     │         │</span>
<span class="line">│  └──────┴────────┴──────────┴─────────┴──────────┘         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  技能效果表 (skill_effect)：                                  │</span>
<span class="line">│  ┌──────┬──────────┬─────────┬──────────┬─────────────┐    │</span>
<span class="line">│  │ id   │ effect_id │ target  │ value     │ duration     │    │</span>
<span class="line">│  ├──────┼──────────┼─────────┼──────────┼─────────────┤    │</span>
<span class="line">│  │ 1001 │ 1         │ ENEMY   │ -100      │ 0           │    │</span>
<span class="line">│  │ 1001 │ 2         │ ENEMY   │ DOT_FIRE  │ 5000        │    │</span>
<span class="line">│  │ 1002 │ 3         │ FRIEND  │ 100       │ 0           │    │</span>
<span class="line">│  │ 1004 │ 4         │ SELF    │ ATK_UP    │ 10000       │    │</span>
<span class="line">│  └──────┴──────────┴─────────┴──────────┴─────────────┘    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  技能需求表 (skill_requirement)：                              │</span>
<span class="line">│  ┌──────┬──────────┬──────────┬──────────┬─────────────┐    │</span>
<span class="line">│  │ id   │ level    │ mp_cost  │ item_cost │ weapon_type │    │</span>
<span class="line">│  ├──────┼──────────┼──────────┼──────────┼─────────────┤    │</span>
<span class="line">│  │ 1001 │ 10       │ 50       │ 0         │ STAFF       │    │</span>
<span class="line">│  │ 1002 │ 5        │ 100      │ 0         │ STAFF       │    │</span>
<span class="line">│  │ 1003 │ 1        │ 0        │ 0         │ ANY         │    │</span>
<span class="line">│  └──────┴──────────┴──────────┴──────────┴─────────────┘    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-数据结构设计" tabindex="-1"><a class="header-anchor" href="#_2-2-数据结构设计"><span>2.2 数据结构设计</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 技能数据结构</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 技能类型</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">SkillType</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint8_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">    ACTIVE <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span>    <span class="token comment">// 主动技能：需要手动释放</span></span>
<span class="line">    PASSIVE <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span>    <span class="token comment">// 被动技能：自动触发</span></span>
<span class="line">    CHANNEL <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span>    <span class="token comment">// 通道技能：持续释放</span></span>
<span class="line">    TOGGLE <span class="token operator">=</span> <span class="token number">3</span>      <span class="token comment">// 开关技能：切换状态</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 目标类型</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">TargetType</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint8_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">    NONE <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span>       <span class="token comment">// 无需目标</span></span>
<span class="line">    SELF <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span>       <span class="token comment">// 自身</span></span>
<span class="line">    SINGLE <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span>     <span class="token comment">// 单个目标</span></span>
<span class="line">    AREA <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">,</span>       <span class="token comment">// 区域内目标</span></span>
<span class="line">    DIRECTION <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">,</span>  <span class="token comment">// 方向</span></span>
<span class="line">    POSITION <span class="token operator">=</span> <span class="token number">5</span>    <span class="token comment">// 指定位置</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 技能定义</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">SkillConfig</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> skillId<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string name<span class="token punctuation">;</span></span>
<span class="line">    SkillType type<span class="token punctuation">;</span></span>
<span class="line">    TargetType targetType<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> cooldownMs<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> castTimeMs<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> mpCost<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> minRange<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> maxRange<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> radius<span class="token punctuation">;</span>           <span class="token comment">// 区域半径</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> effectIds<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 技能效果</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">SkillEffect</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> effectId<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">Type</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint8_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">        DAMAGE<span class="token punctuation">,</span>        <span class="token comment">// 伤害</span></span>
<span class="line">        HEAL<span class="token punctuation">,</span>          <span class="token comment">// 治疗</span></span>
<span class="line">        BUFF<span class="token punctuation">,</span>          <span class="token comment">// Buff</span></span>
<span class="line">        DEBUFF<span class="token punctuation">,</span>        <span class="token comment">// Debuff</span></span>
<span class="line">        SUMMON<span class="token punctuation">,</span>        <span class="token comment">// 召唤</span></span>
<span class="line">        TELEPORT<span class="token punctuation">,</span>      <span class="token comment">// 位移</span></span>
<span class="line">        KNOCKBACK<span class="token punctuation">,</span>     <span class="token comment">// 击退</span></span>
<span class="line">        STUN           <span class="token comment">// 眩晕</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    Type type<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> value<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> duration<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> buffId<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 玩家技能状态</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">SkillState</span> <span class="token punctuation">{</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> <span class="token keyword">uint64_t</span><span class="token operator">&gt;</span> lastCastTime<span class="token punctuation">;</span>  <span class="token comment">// skill_id -&gt; timestamp</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> <span class="token keyword">bool</span><span class="token operator">&gt;</span> learnedSkills<span class="token punctuation">;</span>      <span class="token comment">// skill_id -&gt; learned</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> <span class="token keyword">uint8_t</span><span class="token operator">&gt;</span> skillLevels<span class="token punctuation">;</span>       <span class="token comment">// skill_id -&gt; level</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、技能释放流程" tabindex="-1"><a class="header-anchor" href="#三、技能释放流程"><span>三、技能释放流程</span></a></h2><h3 id="_3-1-释放流程图" tabindex="-1"><a class="header-anchor" href="#_3-1-释放流程图"><span>3.1 释放流程图</span></a></h3>`,17),i(d,{code:`eJx1UklLw0AUvvsrHp4qaMHlVKiX6EEQhQZ6H5JpGYxJzUwFb3XBuiMuiFqX4oIKxoNQrEH8L9LMpP/CyaS4NDq39973vuUxFM+VsW3gMYKKLprtAflKyGXEICVkM9AAUdAsgm2WmOnRTMfuPHaTs3g4QyxLX6AMJ5nzESCPLGIiRhy7RwGmHIbBkYSgZWAwDWLnPvAaYqkZVH2+UQmX3/jWenvPU2BtYHRUz4CGKFNCuSgLZSkaFRNmPzDkFjGbMPu6yeXWUBp4bTvYqAfHd+2HrfBpUWF0yZnPQNzpKJ7VW34jpkAWg7huvWxzvx42nlU/evkBuStt//BDS45NccpwTJwdz+Wmc/0wS4vZ3g5x7SF4vG01L3v7FAm2zW6VhIRy91HbB35V4Rc37cpJ+F6Nt7oiyozDMqNSEs++8C+C6xWxu/qF1aOsESz06sI76ng6XOPntSSmXVnnm/fi1OOX1eQ0eD0QB3ed3b/MZGAkDUHzne89Cn//l8J/R6Nlw8CUZplbxvF9fsAVdLxQwAaTKqRADPWJUli1Jgll3yf9BIGlHdE=`}),o[1]||=e(`<h3 id="_3-2-验证逻辑" tabindex="-1"><a class="header-anchor" href="#_3-2-验证逻辑"><span>3.2 验证逻辑</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 技能验证</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SkillValidator</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 验证是否可以释放</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">CastResult</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint8_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">        SUCCESS <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">        NOT_LEARNED<span class="token punctuation">,</span></span>
<span class="line">        COOLDOWN<span class="token punctuation">,</span></span>
<span class="line">        NO_MP<span class="token punctuation">,</span></span>
<span class="line">        INVALID_TARGET<span class="token punctuation">,</span></span>
<span class="line">        OUT_OF_RANGE<span class="token punctuation">,</span></span>
<span class="line">        CASTING_INTERRUPTED<span class="token punctuation">,</span></span>
<span class="line">        SILENCED<span class="token punctuation">,</span></span>
<span class="line">        STUNNED</span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    CastResult <span class="token function">canCastSkill</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> caster<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> skillId<span class="token punctuation">,</span></span>
<span class="line">                             Entity<span class="token operator">*</span> target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 检查技能是否学习</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">hasLearnedSkill</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> skillId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> CastResult<span class="token double-colon punctuation">::</span>NOT_LEARNED<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 检查冷却</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isOnCooldown</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> skillId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> CastResult<span class="token double-colon punctuation">::</span>COOLDOWN<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 检查 MP</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> skillConfig <span class="token operator">=</span> <span class="token function">getSkillConfig</span><span class="token punctuation">(</span>skillId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>caster<span class="token operator">-&gt;</span><span class="token function">getMP</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> skillConfig<span class="token operator">-&gt;</span>mpCost<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> CastResult<span class="token double-colon punctuation">::</span>NO_MP<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 检查状态</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>caster<span class="token operator">-&gt;</span><span class="token function">hasState</span><span class="token punctuation">(</span>State<span class="token double-colon punctuation">::</span>STUNNED<span class="token punctuation">)</span> <span class="token operator">||</span></span>
<span class="line">            caster<span class="token operator">-&gt;</span><span class="token function">hasState</span><span class="token punctuation">(</span>State<span class="token double-colon punctuation">::</span>SILENCED<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> CastResult<span class="token double-colon punctuation">::</span>CASTING_INTERRUPTED<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 检查目标</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> validation <span class="token operator">=</span> <span class="token function">getTargetValidation</span><span class="token punctuation">(</span>skillConfig<span class="token operator">-&gt;</span>targetType<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>validation<span class="token operator">-&gt;</span><span class="token function">isValid</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> target<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> CastResult<span class="token double-colon punctuation">::</span>INVALID_TARGET<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 6. 检查距离</span></span>
<span class="line">        <span class="token keyword">float</span> distance <span class="token operator">=</span> <span class="token function">getDistance</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> target<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>distance <span class="token operator">&lt;</span> skillConfig<span class="token operator">-&gt;</span>minRange <span class="token operator">||</span></span>
<span class="line">            distance <span class="token operator">&gt;</span> skillConfig<span class="token operator">-&gt;</span>maxRange<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> CastResult<span class="token double-colon punctuation">::</span>OUT_OF_RANGE<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> CastResult<span class="token double-colon punctuation">::</span>SUCCESS<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isOnCooldown</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> caster<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> skillId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span><span class="token operator">*</span> skillState <span class="token operator">=</span> caster<span class="token operator">-&gt;</span><span class="token generic-function"><span class="token function">getComponent</span><span class="token generic class-name"><span class="token operator">&lt;</span>SkillState<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> now <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> lastCast <span class="token operator">=</span> skillState<span class="token operator">-&gt;</span>lastCastTime<span class="token punctuation">[</span>skillId<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> cooldown <span class="token operator">=</span> <span class="token function">getSkillConfig</span><span class="token punctuation">(</span>skillId<span class="token punctuation">)</span><span class="token operator">-&gt;</span>cooldownMs<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span>now <span class="token operator">-</span> lastCast<span class="token punctuation">)</span> <span class="token operator">&lt;</span> cooldown<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、技能效果实现" tabindex="-1"><a class="header-anchor" href="#四、技能效果实现"><span>四、技能效果实现</span></a></h2><h3 id="_4-1-效果系统" tabindex="-1"><a class="header-anchor" href="#_4-1-效果系统"><span>4.1 效果系统</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 技能效果系统</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SkillEffectSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 应用技能效果</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">applyEffects</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> caster<span class="token punctuation">,</span> <span class="token keyword">const</span> SkillConfig<span class="token operator">*</span> skill<span class="token punctuation">,</span></span>
<span class="line">                       <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> targets<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint32_t</span> effectId <span class="token operator">:</span> skill<span class="token operator">-&gt;</span>effectIds<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">*</span> effect <span class="token operator">=</span> <span class="token function">getSkillEffect</span><span class="token punctuation">(</span>effectId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>Entity<span class="token operator">*</span> target <span class="token operator">:</span> targets<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">switch</span> <span class="token punctuation">(</span>effect<span class="token operator">-&gt;</span>type<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token keyword">case</span> SkillEffect<span class="token double-colon punctuation">::</span>Type<span class="token double-colon punctuation">::</span>DAMAGE<span class="token operator">:</span></span>
<span class="line">                        <span class="token function">applyDamage</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> target<span class="token punctuation">,</span> effect<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                        <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                    <span class="token keyword">case</span> SkillEffect<span class="token double-colon punctuation">::</span>Type<span class="token double-colon punctuation">::</span>HEAL<span class="token operator">:</span></span>
<span class="line">                        <span class="token function">applyHeal</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> target<span class="token punctuation">,</span> effect<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                        <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                    <span class="token keyword">case</span> SkillEffect<span class="token double-colon punctuation">::</span>Type<span class="token double-colon punctuation">::</span>BUFF<span class="token operator">:</span></span>
<span class="line">                        <span class="token function">applyBuff</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> target<span class="token punctuation">,</span> effect<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                        <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                    <span class="token keyword">case</span> SkillEffect<span class="token double-colon punctuation">::</span>Type<span class="token double-colon punctuation">::</span>DEBUFF<span class="token operator">:</span></span>
<span class="line">                        <span class="token function">applyDebuff</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> target<span class="token punctuation">,</span> effect<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                        <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                    <span class="token keyword">case</span> SkillEffect<span class="token double-colon punctuation">::</span>Type<span class="token double-colon punctuation">::</span>SUMMON<span class="token operator">:</span></span>
<span class="line">                        <span class="token function">applySummon</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> effect<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                        <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                    <span class="token keyword">case</span> SkillEffect<span class="token double-colon punctuation">::</span>Type<span class="token double-colon punctuation">::</span>TELEPORT<span class="token operator">:</span></span>
<span class="line">                        <span class="token function">applyTeleport</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> effect<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                        <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                    <span class="token keyword">case</span> SkillEffect<span class="token double-colon punctuation">::</span>Type<span class="token double-colon punctuation">::</span>STUN<span class="token operator">:</span></span>
<span class="line">                        <span class="token function">applyStun</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> target<span class="token punctuation">,</span> effect<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                        <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 伤害效果</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">applyDamage</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> caster<span class="token punctuation">,</span> Entity<span class="token operator">*</span> target<span class="token punctuation">,</span></span>
<span class="line">                     <span class="token keyword">const</span> SkillEffect<span class="token operator">*</span> effect<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 计算伤害</span></span>
<span class="line">        <span class="token keyword">float</span> baseDamage <span class="token operator">=</span> effect<span class="token operator">-&gt;</span>value<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> attackPower <span class="token operator">=</span> caster<span class="token operator">-&gt;</span><span class="token function">getAttackPower</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> defense <span class="token operator">=</span> target<span class="token operator">-&gt;</span><span class="token function">getDefense</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">float</span> finalDamage <span class="token operator">=</span> <span class="token function">calculateDamage</span><span class="token punctuation">(</span>baseDamage<span class="token punctuation">,</span> attackPower<span class="token punctuation">,</span> defense<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 应用伤害</span></span>
<span class="line">        target<span class="token operator">-&gt;</span><span class="token function">takeDamage</span><span class="token punctuation">(</span>finalDamage<span class="token punctuation">,</span> caster<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 广播伤害事件</span></span>
<span class="line">        <span class="token function">broadcastDamageEvent</span><span class="token punctuation">(</span>caster<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> target<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> finalDamage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Buff 效果</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">applyBuff</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> caster<span class="token punctuation">,</span> Entity<span class="token operator">*</span> target<span class="token punctuation">,</span></span>
<span class="line">                    <span class="token keyword">const</span> SkillEffect<span class="token operator">*</span> effect<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Buff<span class="token operator">*</span> buff <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">Buff</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        buff<span class="token operator">-&gt;</span>buffId <span class="token operator">=</span> effect<span class="token operator">-&gt;</span>buffId<span class="token punctuation">;</span></span>
<span class="line">        buff<span class="token operator">-&gt;</span>casterId <span class="token operator">=</span> caster<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        buff<span class="token operator">-&gt;</span>duration <span class="token operator">=</span> effect<span class="token operator">-&gt;</span>duration<span class="token punctuation">;</span></span>
<span class="line">        buff<span class="token operator">-&gt;</span>value <span class="token operator">=</span> effect<span class="token operator">-&gt;</span>value<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        target<span class="token operator">-&gt;</span><span class="token function">addBuff</span><span class="token punctuation">(</span>buff<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">float</span> <span class="token function">calculateDamage</span><span class="token punctuation">(</span><span class="token keyword">float</span> base<span class="token punctuation">,</span> <span class="token keyword">float</span> attack<span class="token punctuation">,</span> <span class="token keyword">float</span> defense<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 伤害公式：base * attack / (attack + defense)</span></span>
<span class="line">        <span class="token keyword">return</span> base <span class="token operator">*</span> attack <span class="token operator">/</span> <span class="token punctuation">(</span>attack <span class="token operator">+</span> defense<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-目标选择" tabindex="-1"><a class="header-anchor" href="#_4-2-目标选择"><span>4.2 目标选择</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 目标选择系统</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TargetSelector</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 根据目标类型选择目标</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> <span class="token function">selectTargets</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> caster<span class="token punctuation">,</span></span>
<span class="line">                                       TargetType targetType<span class="token punctuation">,</span></span>
<span class="line">                                       <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> position<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>targetType<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> TargetType<span class="token double-colon punctuation">::</span>SELF<span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token punctuation">{</span>caster<span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> TargetType<span class="token double-colon punctuation">::</span>SINGLE<span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token function">selectSingleTarget</span><span class="token punctuation">(</span>caster<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> TargetType<span class="token double-colon punctuation">::</span>AREA<span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token function">selectAreaTargets</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> position<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> TargetType<span class="token double-colon punctuation">::</span>DIRECTION<span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token function">selectDirectionTargets</span><span class="token punctuation">(</span>caster<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">default</span><span class="token operator">:</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 区域选择</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> <span class="token function">selectAreaTargets</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> caster<span class="token punctuation">,</span></span>
<span class="line">                                         <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> center<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> targets<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 获取技能范围</span></span>
<span class="line">        <span class="token keyword">float</span> radius <span class="token operator">=</span> <span class="token function">getCurrentSkillRadius</span><span class="token punctuation">(</span>caster<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 查找范围内的实体</span></span>
<span class="line">        <span class="token keyword">auto</span> entities <span class="token operator">=</span> <span class="token function">getEntitiesInArea</span><span class="token punctuation">(</span>center<span class="token punctuation">,</span> radius<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 过滤敌对实体</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">:</span> entities<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">isEnemy</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> entity<span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span> <span class="token operator">!</span>entity<span class="token operator">-&gt;</span><span class="token function">isDead</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                targets<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> targets<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 方向选择 (扇形区域)</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> <span class="token function">selectDirectionTargets</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> caster<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> targets<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        Position casterPos <span class="token operator">=</span> caster<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> direction <span class="token operator">=</span> caster<span class="token operator">-&gt;</span><span class="token function">getRotation</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> range <span class="token operator">=</span> <span class="token function">getCurrentSkillRange</span><span class="token punctuation">(</span>caster<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> angle <span class="token operator">=</span> <span class="token number">60.0f</span><span class="token punctuation">;</span>  <span class="token comment">// 60度扇形</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> entity <span class="token operator">:</span> <span class="token function">getVisibleEntities</span><span class="token punctuation">(</span>caster<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            Position entityPos <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">getPosition</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 计算距离和角度</span></span>
<span class="line">            <span class="token keyword">float</span> distance <span class="token operator">=</span> <span class="token function">distance</span><span class="token punctuation">(</span>casterPos<span class="token punctuation">,</span> entityPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">float</span> entityAngle <span class="token operator">=</span> <span class="token function">calculateAngle</span><span class="token punctuation">(</span>casterPos<span class="token punctuation">,</span> entityPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>distance <span class="token operator">&lt;=</span> range<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">float</span> angleDiff <span class="token operator">=</span> <span class="token function">normalizeAngle</span><span class="token punctuation">(</span>entityAngle <span class="token operator">-</span> direction<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">abs</span><span class="token punctuation">(</span>angleDiff<span class="token punctuation">)</span> <span class="token operator">&lt;=</span> angle <span class="token operator">/</span> <span class="token number">2</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    targets<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> targets<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、kbengine-技能实现" tabindex="-1"><a class="header-anchor" href="#五、kbengine-技能实现"><span>五、KBEngine 技能实现</span></a></h2><h3 id="_5-1-kbengine-技能定义" tabindex="-1"><a class="header-anchor" href="#_5-1-kbengine-技能定义"><span>5.1 KBEngine 技能定义</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 技能系统实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Skill</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> skillId<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>skillId <span class="token operator">=</span> skillId</span>
<span class="line">        self<span class="token punctuation">.</span>config <span class="token operator">=</span> SkillConfig<span class="token punctuation">.</span>getConfig<span class="token punctuation">(</span>skillId<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">canCast</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> player<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        检查是否可以释放</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 检查是否学习</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> player<span class="token punctuation">.</span>hasSkill<span class="token punctuation">(</span>self<span class="token punctuation">.</span>skillId<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span><span class="token punctuation">,</span> <span class="token string">&quot;技能未学习&quot;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 检查冷却</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>isOnCooldown<span class="token punctuation">(</span>player<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span><span class="token punctuation">,</span> <span class="token string">&quot;技能冷却中&quot;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 检查 MP</span></span>
<span class="line">        <span class="token keyword">if</span> player<span class="token punctuation">.</span>mp <span class="token operator">&lt;</span> self<span class="token punctuation">.</span>config<span class="token punctuation">.</span>mpCost<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span><span class="token punctuation">,</span> <span class="token string">&quot;MP不足&quot;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 检查状态</span></span>
<span class="line">        <span class="token keyword">if</span> player<span class="token punctuation">.</span>isStunned<span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">or</span> player<span class="token punctuation">.</span>isSilenced<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span><span class="token punctuation">,</span> <span class="token string">&quot;状态异常&quot;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">True</span><span class="token punctuation">,</span> <span class="token string">&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">cast</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> player<span class="token punctuation">,</span> target<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        释放技能</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        canCast<span class="token punctuation">,</span> msg <span class="token operator">=</span> self<span class="token punctuation">.</span>canCast<span class="token punctuation">(</span>player<span class="token punctuation">,</span> target<span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> canCast<span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span><span class="token punctuation">,</span> msg</span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 扣除 MP</span></span>
<span class="line">        player<span class="token punctuation">.</span>mp <span class="token operator">-=</span> self<span class="token punctuation">.</span>config<span class="token punctuation">.</span>mpCost</span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 设置冷却</span></span>
<span class="line">        player<span class="token punctuation">.</span>setSkillCooldown<span class="token punctuation">(</span>self<span class="token punctuation">.</span>skillId<span class="token punctuation">,</span> self<span class="token punctuation">.</span>config<span class="token punctuation">.</span>cooldown<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 应用效果</span></span>
<span class="line">        <span class="token keyword">for</span> effect <span class="token keyword">in</span> self<span class="token punctuation">.</span>config<span class="token punctuation">.</span>effects<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>applyEffect<span class="token punctuation">(</span>player<span class="token punctuation">,</span> target<span class="token punctuation">,</span> effect<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">True</span><span class="token punctuation">,</span> <span class="token string">&quot;&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-kbengine-技能效果" tabindex="-1"><a class="header-anchor" href="#_5-2-kbengine-技能效果"><span>5.2 KBEngine 技能效果</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 技能效果</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SkillEffect</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">applyDamage</span><span class="token punctuation">(</span>player<span class="token punctuation">,</span> target<span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        伤害效果</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 计算伤害</span></span>
<span class="line">        damage <span class="token operator">=</span> value <span class="token operator">*</span> player<span class="token punctuation">.</span>attackPower <span class="token operator">/</span> <span class="token punctuation">(</span>player<span class="token punctuation">.</span>attackPower <span class="token operator">+</span> target<span class="token punctuation">.</span>defense<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 扣除血量</span></span>
<span class="line">        target<span class="token punctuation">.</span>hp <span class="token operator">-=</span> <span class="token builtin">int</span><span class="token punctuation">(</span>damage<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 通知客户端</span></span>
<span class="line">        target<span class="token punctuation">.</span>client<span class="token punctuation">.</span>onDamage<span class="token punctuation">(</span>player<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">,</span> damage<span class="token punctuation">)</span></span>
<span class="line">        player<span class="token punctuation">.</span>client<span class="token punctuation">.</span>onDamageDealt<span class="token punctuation">(</span>target<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">,</span> damage<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">applyHeal</span><span class="token punctuation">(</span>player<span class="token punctuation">,</span> target<span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        治疗效果</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        heal <span class="token operator">=</span> value <span class="token operator">*</span> player<span class="token punctuation">.</span>healPower</span>
<span class="line"></span>
<span class="line">        target<span class="token punctuation">.</span>hp <span class="token operator">=</span> <span class="token builtin">min</span><span class="token punctuation">(</span>target<span class="token punctuation">.</span>hp <span class="token operator">+</span> <span class="token builtin">int</span><span class="token punctuation">(</span>heal<span class="token punctuation">)</span><span class="token punctuation">,</span> target<span class="token punctuation">.</span>maxHp<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 通知客户端</span></span>
<span class="line">        target<span class="token punctuation">.</span>client<span class="token punctuation">.</span>onHeal<span class="token punctuation">(</span>player<span class="token punctuation">.</span><span class="token builtin">id</span><span class="token punctuation">,</span> heal<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token decorator annotation punctuation">@staticmethod</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">applyBuff</span><span class="token punctuation">(</span>player<span class="token punctuation">,</span> target<span class="token punctuation">,</span> buffId<span class="token punctuation">,</span> duration<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        Buff 效果</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        buff <span class="token operator">=</span> KBEngine<span class="token punctuation">.</span>createEntity<span class="token punctuation">(</span>Buff<span class="token punctuation">)</span></span>
<span class="line">        buff<span class="token punctuation">.</span>buffId <span class="token operator">=</span> buffId</span>
<span class="line">        buff<span class="token punctuation">.</span>casterId <span class="token operator">=</span> player<span class="token punctuation">.</span><span class="token builtin">id</span></span>
<span class="line">        buff<span class="token punctuation">.</span>duration <span class="token operator">=</span> duration</span>
<span class="line"></span>
<span class="line">        target<span class="token punctuation">.</span>addBuff<span class="token punctuation">(</span>buff<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、性能优化" tabindex="-1"><a class="header-anchor" href="#六、性能优化"><span>六、性能优化</span></a></h2><h3 id="_6-1-技能缓存" tabindex="-1"><a class="header-anchor" href="#_6-1-技能缓存"><span>6.1 技能缓存</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 技能配置缓存</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">SkillConfigCache</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 技能配置缓存</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> SkillConfig<span class="token operator">*</span><span class="token operator">&gt;</span> configCache_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取技能配置</span></span>
<span class="line">    SkillConfig<span class="token operator">*</span> <span class="token function">getSkillConfig</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> skillId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> configCache_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>skillId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> configCache_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 从数据库加载</span></span>
<span class="line">        SkillConfig<span class="token operator">*</span> config <span class="token operator">=</span> <span class="token function">loadFromDB</span><span class="token punctuation">(</span>skillId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>config<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            configCache_<span class="token punctuation">[</span>skillId<span class="token punctuation">]</span> <span class="token operator">=</span> config<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> config<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 预加载热门技能</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">preloadPopularSkills</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> popular <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token number">1001</span><span class="token punctuation">,</span> <span class="token number">1002</span><span class="token punctuation">,</span> <span class="token number">1003</span><span class="token punctuation">,</span> <span class="token number">1004</span><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">uint32_t</span> skillId <span class="token operator">:</span> popular<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">getSkillConfig</span><span class="token punctuation">(</span>skillId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-效果池化" tabindex="-1"><a class="header-anchor" href="#_6-2-效果池化"><span>6.2 效果池化</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 技能效果对象池</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EffectPool</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    T<span class="token operator">*</span> <span class="token function">acquire</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>freeList_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            T<span class="token operator">*</span> effect <span class="token operator">=</span> freeList_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">pop_back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> effect<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token keyword">new</span> <span class="token function">T</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">release</span><span class="token punctuation">(</span>T<span class="token operator">*</span> effect<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        effect<span class="token operator">-&gt;</span><span class="token function">reset</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        freeList_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>effect<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>T<span class="token operator">*</span><span class="token operator">&gt;</span> freeList_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line">EffectPool<span class="token operator">&lt;</span>SkillEffect<span class="token operator">&gt;</span> effectPool<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">applyEffect</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> target<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    SkillEffect<span class="token operator">*</span> effect <span class="token operator">=</span> effectPool<span class="token punctuation">.</span><span class="token function">acquire</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token comment">// 使用效果</span></span>
<span class="line">    effectPool<span class="token punctuation">.</span><span class="token function">release</span><span class="token punctuation">(</span>effect<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="技能系统设计要点" tabindex="-1"><a class="header-anchor" href="#技能系统设计要点"><span>技能系统设计要点</span></a></h3><table><thead><tr><th>要点</th><th>说明</th></tr></thead><tbody><tr><td><strong>数据驱动</strong></td><td>技能配置化，便于策划调整</td></tr><tr><td><strong>服务器验证</strong></td><td>所有计算在服务器端</td></tr><tr><td><strong>状态检查</strong></td><td>冷却、MP、状态等条件</td></tr><tr><td><strong>目标选择</strong></td><td>支持多种目标类型</td></tr><tr><td><strong>效果系统</strong></td><td>模块化效果，易于扩展</td></tr><tr><td><strong>性能优化</strong></td><td>缓存、池化、批量处理</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 数据驱动设计</span>
<span class="line">   - 技能配置表化</span>
<span class="line">   - 策划可配置</span>
<span class="line">   - 热更新支持</span>
<span class="line"></span>
<span class="line">2. 服务器权威</span>
<span class="line">   - 服务器计算所有效果</span>
<span class="line">   - 客户端只显示结果</span>
<span class="line">   - 防作弊验证</span>
<span class="line"></span>
<span class="line">3. 模块化设计</span>
<span class="line">   - 效果独立</span>
<span class="line">   - 易于添加新效果</span>
<span class="line">   - 代码复用</span>
<span class="line"></span>
<span class="line">4. 性能优化</span>
<span class="line">   - 技能配置缓存</span>
<span class="line">   - 效果对象池</span>
<span class="line">   - 批量处理</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/tree/master/kbe/src/server/skills" target="_blank" rel="noopener noreferrer">KBEngine 技能系统</a></li><li><a href="https://www.gamedeveloper.com/12-awesome-game-ability-systems/" target="_blank" rel="noopener noreferrer">游戏技能系统设计</a></li></ul>`,29)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};