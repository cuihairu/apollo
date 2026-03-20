import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q40-ecs-architecture.html","title":"Q40: ECS 架构是什么？在游戏中有什么优势？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q40-ecs-architecture.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q40-ecs-architecture.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q40-ecs-架构是什么-在游戏中有什么优势" tabindex="-1"><a class="header-anchor" href="#q40-ecs-架构是什么-在游戏中有什么优势"><span>Q40: ECS 架构是什么？在游戏中有什么优势？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对 ECS（Entity Component System）架构的理解：</p><ul><li>ECS 的核心概念和设计思想</li><li>与传统 OOP 的区别</li><li>在游戏开发中的优势</li><li>Unity DOTS、Unreal Mass 的实现</li></ul><hr><h2 id="一、ecs-基础概念" tabindex="-1"><a class="header-anchor" href="#一、ecs-基础概念"><span>一、ECS 基础概念</span></a></h2><h3 id="_1-1-三大核心元素" tabindex="-1"><a class="header-anchor" href="#_1-1-三大核心元素"><span>1.1 三大核心元素</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    ECS 三大元素                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  E - Entity (实体)                                          │</span>
<span class="line">│  ├── 只是一个 ID，没有数据和行为                            │</span>
<span class="line">│  ├── 玩家、NPC、怪物、子弹等                                │</span>
<span class="line">│  └── 示例：EntityID = 1001                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  C - Component (组件)                                       │</span>
<span class="line">│  ├── 纯数据结构，没有行为                                   │</span>
<span class="line">│  ├── Position, Velocity, Health, Sprite 等                  │</span>
<span class="line">│  └── 示例：struct Position { float x, y, z; }             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  S - System (系统)                                           │</span>
<span class="line">│  ├── 纯行为逻辑，操作数据                                   │</span>
<span class="line">│  ├── MovementSystem, CollisionSystem, RenderSystem 等        │</span>
<span class="line">│  └── 示例：void update(Position&amp;, Velocity&amp;)               │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-与-oop-对比" tabindex="-1"><a class="header-anchor" href="#_1-2-与-oop-对比"><span>1.2 与 OOP 对比</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  OOP vs ECS 对比                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  传统 OOP：                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  class Player : public Entity {                   │       │</span>
<span class="line">│  │    private:                                      │       │</span>
<span class="line">│  │    Position position_;                           │       │</span>
<span class="line">│  │    Velocity velocity_;                           │       │</span>
<span class="line">│  │    Health health_;                               │       │</span>
<span class="line">│  │    Sprite sprite_;                               │       │</span>
<span class="line">│  │                                                  │       │</span>
<span class="line">│  │  public:                                        │       │</span>
<span class="line">│  │    void update(float dt) {                       │       │</span>
<span class="line">│  │      position_ += velocity_ * dt;               │       │</span>
<span class="line">│  │      sprite_.setPosition(position_);            │       │</span>
<span class="line">│  │    }                                             │       │</span>
<span class="line">│  │                                                  │       │</span>
<span class="line">│  │    void render() {                             │       │</span>
<span class="line">│  │      sprite_.draw();                           │       │</span>
<span class="line">│  │    }                                             │       │</span>
<span class="line">│  │  };                                              │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  问题：                                            │       │</span>
<span class="line">│  │  ├── 继承层次深                                   │       │</span>
<span class="line">│  │  ├── 耦合度高                                     │       │</span>
<span class="line">│  │  ├── 难以扩展                                     │       │</span>
<span class="line">│  │  └── 缓存不友好                                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ECS 架构：                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  // Entity: 只是 ID                                │       │</span>
<span class="line">│  │  Entity player = entityManager.create();        │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  // Component: 纯数据                            │       │</span>
<span class="line">│  │  player.add&lt;Position&gt;(100, 0, 200);            │       │</span>
<span class="line">│  │  player.add&lt;Velocity&gt;(5, 0, 0);                 │       │</span>
<span class="line">│  │  player.add&lt;Health&gt;(100);                        │       │</span>
<span class="line">│  │  player.add&lt;Sprite&gt;(&quot;player.png&quot;);              │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  // System: 纯逻辑                              │       │</span>
<span class="line">│  │  movementSystem.update(dt);                     │       │</span>
<span class="line">│  │  renderSystem.update();                         │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  优势：                                            │       │</span>
<span class="line">│  │  ├── 组合优于继承                                 │       │</span>
<span class="line">│  │  ├── 解耦合                                       │       │</span>
<span class="line">│  │  ├── 易扩展                                       │       │</span>
<span class="line">│  │  └── 缓存友好                                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、ecs-详细设计" tabindex="-1"><a class="header-anchor" href="#二、ecs-详细设计"><span>二、ECS 详细设计</span></a></h2><h3 id="_2-1-component-设计" tabindex="-1"><a class="header-anchor" href="#_2-1-component-设计"><span>2.1 Component 设计</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ECS Component 设计 - 纯数据</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 位置组件</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">Position</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">float</span> x<span class="token punctuation">,</span> y<span class="token punctuation">,</span> z<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 速度组件</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">Velocity</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">float</span> vx<span class="token punctuation">,</span> vy<span class="token punctuation">,</span> vz<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 生命值组件</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">Health</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">float</span> current<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> max<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 渲染组件</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">Sprite</span> <span class="token punctuation">{</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string texture<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> width<span class="token punctuation">,</span> height<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> scale<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 碰撞盒组件</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">Collider</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">float</span> radius<span class="token punctuation">;</span>    <span class="token comment">// 球形碰撞体</span></span>
<span class="line">    <span class="token keyword">bool</span> isTrigger<span class="token punctuation">;</span> <span class="token comment">// 是否触发器</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 玩家标签组件</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">PlayerTag</span> <span class="token punctuation">{</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string name<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> level<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 怪物 AI 组件</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">MonsterAI</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">float</span> attackRange<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">float</span> chaseSpeed<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> targetEntityId<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-system-设计" tabindex="-1"><a class="header-anchor" href="#_2-2-system-设计"><span>2.2 System 设计</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ECS System 设计 - 纯逻辑</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MovementSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 查询：需要 Position 和 Velocity 的实体</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span>EntityManager<span class="token operator">&amp;</span> em<span class="token punctuation">,</span> <span class="token keyword">float</span> dt<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 遍历符合条件的实体</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span> entity <span class="token operator">:</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">query</span><span class="token generic class-name"><span class="token operator">&lt;</span>Position<span class="token punctuation">,</span> Velocity<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> pos <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>Position<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> vel <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>Velocity<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 更新位置</span></span>
<span class="line">            pos<span class="token punctuation">.</span>x <span class="token operator">+=</span> vel<span class="token punctuation">.</span>vx <span class="token operator">*</span> dt<span class="token punctuation">;</span></span>
<span class="line">            pos<span class="token punctuation">.</span>y <span class="token operator">+=</span> vel<span class="token punctuation">.</span>vy <span class="token operator">*</span> dt<span class="token punctuation">;</span></span>
<span class="line">            pos<span class="token punctuation">.</span>z <span class="token operator">+=</span> vel<span class="token punctuation">.</span>vz <span class="token operator">*</span> dt<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RenderSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span>EntityManager<span class="token operator">&amp;</span> em<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 查询：需要 Position 和 Sprite 的实体</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span> entity <span class="token operator">:</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">query</span><span class="token generic class-name"><span class="token operator">&lt;</span>Position<span class="token punctuation">,</span> Sprite<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> pos <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>Position<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> sprite <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>Sprite<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 渲染精灵</span></span>
<span class="line">            renderer<span class="token operator">-&gt;</span><span class="token function">drawSprite</span><span class="token punctuation">(</span></span>
<span class="line">                sprite<span class="token punctuation">.</span>texture<span class="token punctuation">,</span></span>
<span class="line">                pos<span class="token punctuation">.</span>x<span class="token punctuation">,</span> pos<span class="token punctuation">.</span>y<span class="token punctuation">,</span> pos<span class="token punctuation">.</span>z<span class="token punctuation">,</span></span>
<span class="line">                sprite<span class="token punctuation">.</span>scale</span>
<span class="line">            <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CollisionSystem</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span>EntityManager<span class="token operator">&amp;</span> em<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 查询：需要 Position 和 Collider 的实体</span></span>
<span class="line">        <span class="token keyword">auto</span> entities <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">query</span><span class="token generic class-name"><span class="token operator">&lt;</span>Position<span class="token punctuation">,</span> Collider<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 两两检测碰撞</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t i <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> i <span class="token operator">&lt;</span> entities<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>i<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span>size_t j <span class="token operator">=</span> i <span class="token operator">+</span> <span class="token number">1</span><span class="token punctuation">;</span> j <span class="token operator">&lt;</span> entities<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token operator">++</span>j<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                Entity a <span class="token operator">=</span> entities<span class="token punctuation">[</span>i<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">                Entity b <span class="token operator">=</span> entities<span class="token punctuation">[</span>j<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token keyword">auto</span><span class="token operator">&amp;</span> posA <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>Position<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>a<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">auto</span><span class="token operator">&amp;</span> colA <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>Collider<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>a<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">auto</span><span class="token operator">&amp;</span> posB <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>Position<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>b<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">auto</span><span class="token operator">&amp;</span> colB <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>Collider<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>b<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment">// 检测碰撞</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">checkCollision</span><span class="token punctuation">(</span>posA<span class="token punctuation">,</span> colA<span class="token punctuation">,</span> posB<span class="token punctuation">,</span> colB<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token comment">// 触发碰撞事件</span></span>
<span class="line">                    <span class="token function">onCollision</span><span class="token punctuation">(</span>a<span class="token punctuation">,</span> b<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">checkCollision</span><span class="token punctuation">(</span><span class="token keyword">const</span> Position<span class="token operator">&amp;</span> pa<span class="token punctuation">,</span> <span class="token keyword">const</span> Collider<span class="token operator">&amp;</span> ca<span class="token punctuation">,</span></span>
<span class="line">                       <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> pb<span class="token punctuation">,</span> <span class="token keyword">const</span> Collider<span class="token operator">&amp;</span> cb<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">float</span> dx <span class="token operator">=</span> pa<span class="token punctuation">.</span>x <span class="token operator">-</span> pb<span class="token punctuation">.</span>x<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> dy <span class="token operator">=</span> pa<span class="token punctuation">.</span>y <span class="token operator">-</span> pb<span class="token punctuation">.</span>y<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> dz <span class="token operator">=</span> pa<span class="token punctuation">.</span>z <span class="token operator">-</span> pb<span class="token punctuation">.</span>z<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">float</span> distance <span class="token operator">=</span> <span class="token function">sqrt</span><span class="token punctuation">(</span>dx<span class="token operator">*</span>dx <span class="token operator">+</span> dy<span class="token operator">*</span>dy <span class="token operator">+</span> dz<span class="token operator">*</span>dz<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> distance <span class="token operator">&lt;</span> <span class="token punctuation">(</span>ca<span class="token punctuation">.</span>radius <span class="token operator">+</span> cb<span class="token punctuation">.</span>radius<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-entity-manager" tabindex="-1"><a class="header-anchor" href="#_2-3-entity-manager"><span>2.3 Entity Manager</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ECS Entity Manager</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EntityManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 创建实体</span></span>
<span class="line">    Entity <span class="token function">create</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        EntityID id <span class="token operator">=</span> nextId_<span class="token operator">++</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 分配组件存储</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>type<span class="token punctuation">,</span> storage<span class="token punctuation">]</span> <span class="token operator">:</span> componentStorages_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            storage<span class="token operator">-&gt;</span><span class="token function">create</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> id<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加组件</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line">    T<span class="token operator">&amp;</span> <span class="token function">add</span><span class="token punctuation">(</span>Entity entity<span class="token punctuation">,</span> <span class="token keyword">const</span> T<span class="token operator">&amp;</span> component <span class="token operator">=</span> T<span class="token punctuation">{</span><span class="token punctuation">}</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> storage <span class="token operator">=</span> <span class="token generic-function"><span class="token function">getStorage</span><span class="token generic class-name"><span class="token operator">&lt;</span>T<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        storage<span class="token operator">-&gt;</span><span class="token function">add</span><span class="token punctuation">(</span>entity<span class="token punctuation">.</span>id<span class="token punctuation">,</span> component<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> storage<span class="token operator">-&gt;</span><span class="token function">get</span><span class="token punctuation">(</span>entity<span class="token punctuation">.</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取组件</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line">    T<span class="token operator">&amp;</span> <span class="token function">get</span><span class="token punctuation">(</span>Entity entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> storage <span class="token operator">=</span> <span class="token generic-function"><span class="token function">getStorage</span><span class="token generic class-name"><span class="token operator">&lt;</span>T<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> storage<span class="token operator">-&gt;</span><span class="token function">get</span><span class="token punctuation">(</span>entity<span class="token punctuation">.</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 查询实体</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">.</span> Components<span class="token operator">&gt;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">&gt;</span> <span class="token function">query</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">&gt;</span> result<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID id <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span> id <span class="token operator">&lt;</span> nextId_<span class="token punctuation">;</span> <span class="token operator">++</span>id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token generic-function"><span class="token function">hasAll</span><span class="token generic class-name"><span class="token operator">&lt;</span>Components<span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token punctuation">.</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                result<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">{</span>id<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> result<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    EntityID nextId_ <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>type_index<span class="token punctuation">,</span> IComponentStorage<span class="token operator">*</span><span class="token operator">&gt;</span> componentStorages_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、ecs-优势分析" tabindex="-1"><a class="header-anchor" href="#三、ecs-优势分析"><span>三、ECS 优势分析</span></a></h2><h3 id="_3-1-性能优势" tabindex="-1"><a class="header-anchor" href="#_3-1-性能优势"><span>3.1 性能优势</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  ECS 性能优势                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 缓存友好                                               │</span>
<span class="line">│     ├── Component 数组连续存储                             │</span>
<span class="line">│     ├── 顺序遍历，CPU 缓存命中高                          │</span>
<span class="line">│     └── SIMD 友好                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 并行友好                                               │</span>
<span class="line">│     ├── System 之间无共享状态                              │</span>
<span class="line">│     ├── 可并行执行多个 System                              │</span>
<span class="line">│     └── 充分利用多核 CPU                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 按需更新                                               │</span>
<span class="line">│     ├── 只处理有特定组件的实体                             │</span>
<span class="line">│     ├── 减少不必要的计算                                   │</span>
<span class="line">│     └── 提高整体效率                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  内存布局：                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  OOP: 对象分散在内存中                              │       │</span>
<span class="line">│  │  [Obj1][   ][Obj2][   ][Obj3]   ...               │       │</span>
<span class="line">│  │         缓存未命中 → 性能差                        │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  ECS: 相同数据连续存储                              │       │</span>
<span class="line">│  │  [Pos1][Pos2][Pos3][Pos4]...                      │       │</span>
<span class="line">│  │   顺序遍历 → 缓存命中高 → 性能好                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-设计优势" tabindex="-1"><a class="header-anchor" href="#_3-2-设计优势"><span>3.2 设计优势</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  ECS 设计优势                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 组合优于继承                                           │</span>
<span class="line">│     ├── 灵活组合功能                                       │</span>
<span class="line">│     ├── 避免继承地狱                                       │</span>
<span class="line">│     └── 添加新功能不影响现有代码                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 关注点分离                                             │</span>
<span class="line">│     ├── Component = 数据                                  │</span>
<span class="line">│     ├── System = 行为                                     │</span>
<span class="line">│     └── Entity = 标识符                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 易于扩展                                               │</span>
<span class="line">│     ├── 新增 Component 不影响 System                       │</span>
<span class="line">│     ├── 新增 System 不影响 Component                       │</span>
<span class="line">│     └── 代码耦合度低                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  示例：添加飞行功能                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  OOP:                                              │       │</span>
<span class="line">│  │  - 需要修改 Entity 基类                          │       │</span>
<span class="line">│  │  - 或创建 FlyingEntity 子类                       │       │</span>
<span class="line">│  │  - 影响现有类                                     │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  ECS:                                              │       │</span>
<span class="line">│  │  - 新建 Flying Component                          │       │</span>
<span class="line">│  │  - 新建 FlyingSystem                              │       │</span>
<span class="line">│  │  - entity.add&lt;Flying&gt;()                           │       │</span>
<span class="line">│  │  - 完全不影响现有代码                             │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、unity-dots-实现" tabindex="-1"><a class="header-anchor" href="#四、unity-dots-实现"><span>四、Unity DOTS 实现</span></a></h2><h3 id="_4-1-unity-dots-架构" tabindex="-1"><a class="header-anchor" href="#_4-1-unity-dots-架构"><span>4.1 Unity DOTS 架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  Unity DOTS 数据导向技术栈                    │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. C# Job System (多线程作业)                             │</span>
<span class="line">│     ├── 并行执行独立任务                                    │</span>
<span class="line">│     ├── 安全的数据访问                                     │</span>
<span class="line">│     └── 充分利用多核 CPU                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. ECS 核心库                                              │</span>
<span class="line">│     ├── Entities                                           │</span>
<span class="line">│     ├── Components                                         │</span>
<span class="line">│     └── Systems                                            │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. Burst Compiler                                         │</span>
<span class="line">│     ├── 高效的编译器                                        │</span>
<span class="line">│     ├── SIMD 优化                                          │</span>
<span class="line">│     └── 跨平台编译                                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. Collections                                          │</span>
<span class="line">│     ├── 高性能容器                                          │</span>
<span class="line">│     ├── NativeArray                                        │</span>
<span class="line">│     └── NativeHashMap                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-unity-dots-示例" tabindex="-1"><a class="header-anchor" href="#_4-2-unity-dots-示例"><span>4.2 Unity DOTS 示例</span></a></h3><div class="language-csharp line-numbers-mode" data-highlighter="prismjs" data-ext="cs"><pre><code class="language-csharp"><span class="line"><span class="token comment">// Unity DOTS 示例</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">using</span> <span class="token namespace">Unity<span class="token punctuation">.</span>Entities</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token keyword">using</span> <span class="token namespace">Unity<span class="token punctuation">.</span>Transforms</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token keyword">using</span> <span class="token namespace">Unity<span class="token punctuation">.</span>Collections</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 组件定义</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">RotationSpeed</span> <span class="token punctuation">:</span> <span class="token type-list"><span class="token class-name">IComponentData</span></span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span> <span class="token class-name"><span class="token keyword">float</span></span> Value<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// System 定义</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RotationSystem</span> <span class="token punctuation">:</span> <span class="token type-list"><span class="token class-name">SystemBase</span></span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">protected</span> <span class="token keyword">override</span> <span class="token return-type class-name"><span class="token keyword">void</span></span> <span class="token function">OnUpdate</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 查询有 RotationSpeed 组件的实体</span></span>
<span class="line">        Entities</span>
<span class="line">            <span class="token punctuation">.</span><span class="token generic-method"><span class="token function">WithAll</span><span class="token generic class-name"><span class="token punctuation">&lt;</span>RotationSpeed<span class="token punctuation">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">.</span><span class="token function">ForEach</span><span class="token punctuation">(</span><span class="token punctuation">(</span><span class="token class-name">Entity</span> entity<span class="token punctuation">,</span> <span class="token keyword">ref</span> <span class="token class-name">RotationSpeed</span> speed<span class="token punctuation">)</span> <span class="token operator">=&gt;</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 获取 Translation 组件</span></span>
<span class="line">                <span class="token class-name"><span class="token keyword">var</span></span> rotation <span class="token operator">=</span> EntityManager<span class="token punctuation">.</span><span class="token generic-method"><span class="token function">GetComponentData</span><span class="token generic class-name"><span class="token punctuation">&lt;</span>Rotation<span class="token punctuation">&gt;</span></span></span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                <span class="token comment">// 更新旋转</span></span>
<span class="line">                rotation<span class="token punctuation">.</span>Value <span class="token operator">=</span> quaternion<span class="token punctuation">.</span><span class="token function">Euler</span><span class="token punctuation">(</span></span>
<span class="line">                    <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">                    math<span class="token punctuation">.</span><span class="token function">radians</span><span class="token punctuation">(</span>speed<span class="token punctuation">.</span>Value <span class="token operator">*</span> Time<span class="token punctuation">.</span>DeltaTime<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                    <span class="token number">0</span></span>
<span class="line">                <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">                EntityManager<span class="token punctuation">.</span><span class="token function">SetComponentData</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> rotation<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">.</span><span class="token function">ScheduleParallel</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Job System</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">MovementJob</span> <span class="token punctuation">:</span> <span class="token type-list"><span class="token class-name">IJobParallelFor</span></span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span> <span class="token class-name">NativeArray<span class="token punctuation">&lt;</span>float3<span class="token punctuation">&gt;</span></span> positions<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">public</span> <span class="token class-name">NativeArray<span class="token punctuation">&lt;</span>float3<span class="token punctuation">&gt;</span></span> velocities<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">public</span> <span class="token class-name"><span class="token keyword">float</span></span> deltaTime<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">public</span> <span class="token return-type class-name"><span class="token keyword">void</span></span> <span class="token function">Execute</span><span class="token punctuation">(</span><span class="token class-name"><span class="token keyword">int</span></span> index<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        positions<span class="token punctuation">[</span>index<span class="token punctuation">]</span> <span class="token operator">+=</span> velocities<span class="token punctuation">[</span>index<span class="token punctuation">]</span> <span class="token operator">*</span> deltaTime<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MovementSystem</span> <span class="token punctuation">:</span> <span class="token type-list"><span class="token class-name">SystemBase</span></span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">protected</span> <span class="token keyword">override</span> <span class="token return-type class-name"><span class="token keyword">void</span></span> <span class="token function">OnUpdate</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token class-name"><span class="token keyword">var</span></span> job <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token constructor-invocation class-name">MovementJob</span> <span class="token punctuation">{</span></span>
<span class="line">            deltaTime <span class="token operator">=</span> Time<span class="token punctuation">.</span>DeltaTime</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 调度 Job</span></span>
<span class="line">        <span class="token class-name">JobHandle</span> handle <span class="token operator">=</span> job<span class="token punctuation">.</span><span class="token function">Schedule</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        handle <span class="token operator">=</span> <span class="token keyword">this</span><span class="token punctuation">.</span>Dependency <span class="token operator">=</span> handle<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、游戏服务器中的-ecs" tabindex="-1"><a class="header-anchor" href="#五、游戏服务器中的-ecs"><span>五、游戏服务器中的 ECS</span></a></h2><h3 id="_5-1-mmo-服务器-ecs-设计" tabindex="-1"><a class="header-anchor" href="#_5-1-mmo-服务器-ecs-设计"><span>5.1 MMO 服务器 ECS 设计</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              MMO 服务器中的 ECS 应用                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  实体类型：                                                 │</span>
<span class="line">│  ├── Player Entity                                         │</span>
<span class="line">│  │   ├── Position Component                              │       │</span>
<span class="line">│  │   ├── Velocity Component                              │       │</span>
<span class="line">│  │   ├── Health Component                                │       │</span>
<span class="line">│  │   ├── Inventory Component                             │       │</span>
<span class="line">│  │   ├── SkillSet Component                             │       │</span>
<span class="line">│  │   └── PlayerTag Component                            │       │</span>
<span class="line">│  │                                                       │       │</span>
<span class="line">│  ├── NPC Entity                                            │</span>
<span class="line">│  │   ├── Position Component                              │       │</span>
<span class="line">│  │   ├── AI Component                                    │       │</span>
<span class="line">│  │   ├── Dialog Component                                │       │</span>
<span class="line">│  │   └── NPCTag Component                               │       │</span>
<span class="line">│  │                                                       │       │</span>
<span class="line">│  └── Bullet Entity                                         │</span>
<span class="line">│      ├── Position Component                              │       │</span>
<span class="line">│      ├── Velocity Component                              │       │</span>
<span class="line">│      ├── Damage Component                                │       │</span>
<span class="line">│      ├── Lifetime Component                             │       │</span>
<span class="line">│      └── BulletTag Component                            │       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  System 分类：                                              │</span>
<span class="line">│  ├── NetworkSystem (网络同步)                             │</span>
<span class="line">│  ├── MovementSystem (移动)                                │</span>
<span class="line">│  ├── CollisionSystem (碰撞)                               │</span>
<span class="line">│  ├── CombatSystem (战斗)                                  │</span>
<span class="line">│  ├── AISystem (NPC AI)                                     │</span>
<span class="line">│  ├── SkillSystem (技能)                                   │</span>
<span class="line">│  └── ItemSystem (物品)                                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-网络同步-system" tabindex="-1"><a class="header-anchor" href="#_5-2-网络同步-system"><span>5.2 网络同步 System</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 网络同步 System 实现</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">NetworkSyncSystem</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">System</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span>EntityManager<span class="token operator">&amp;</span> em<span class="token punctuation">,</span> <span class="token keyword">float</span> dt<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 同步间隔控制</span></span>
<span class="line">        syncTimer_ <span class="token operator">+=</span> dt<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>syncTimer_ <span class="token operator">&lt;</span> SYNC_INTERVAL<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        syncTimer_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 查询需要同步的实体（有 Position 且是 Player）</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span> entity <span class="token operator">:</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">query</span><span class="token generic class-name"><span class="token operator">&lt;</span>Position<span class="token punctuation">,</span> PlayerTag<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> pos <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>Position<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> player <span class="token operator">=</span> em<span class="token punctuation">.</span><span class="token generic-function"><span class="token function">get</span><span class="token generic class-name"><span class="token operator">&lt;</span>PlayerTag<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 检查位置是否变化</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">hasMoved</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> pos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 发送位置同步消息</span></span>
<span class="line">                <span class="token function">sendPositionUpdate</span><span class="token punctuation">(</span>player<span class="token punctuation">.</span>clientId<span class="token punctuation">,</span> entity<span class="token punctuation">.</span>id<span class="token punctuation">,</span> pos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送位置更新</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendPositionUpdate</span><span class="token punctuation">(</span>ClientID clientId<span class="token punctuation">,</span> EntityID entityId<span class="token punctuation">,</span></span>
<span class="line">                          <span class="token keyword">const</span> Position<span class="token operator">&amp;</span> pos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        PositionUpdateMsg msg<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>entityId <span class="token operator">=</span> entityId<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>x <span class="token operator">=</span> pos<span class="token punctuation">.</span>x<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>y <span class="token operator">=</span> pos<span class="token punctuation">.</span>y<span class="token punctuation">;</span></span>
<span class="line">        msg<span class="token punctuation">.</span>z <span class="token operator">=</span> pos<span class="token punctuation">.</span>z<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        networkManager_<span class="token operator">-&gt;</span><span class="token function">sendToClient</span><span class="token punctuation">(</span>clientId<span class="token punctuation">,</span> msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">float</span> syncTimer_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">float</span> SYNC_INTERVAL <span class="token operator">=</span> <span class="token number">0.1f</span><span class="token punctuation">;</span>  <span class="token comment">// 100ms</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-设计原则" tabindex="-1"><a class="header-anchor" href="#_6-1-设计原则"><span>6.1 设计原则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  ECS 设计原则                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. Component 纯数据                                       │</span>
<span class="line">│     ├── 只包含数据，不包含逻辑                             │</span>
<span class="line">│     ├── 使用 struct 而非 class                            │</span>
<span class="line">│     ├── 避免虚函数                                         │</span>
<span class="line">│     └── 保持简单，单一职责                                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. System 纯逻辑                                         │</span>
<span class="line">│     ├── 只包含逻辑，不存储数据                             │</span>
<span class="line">│     ├── 操作 Component 数据                                │</span>
<span class="line">│     ├── 保持独立，避免跨 System 调用                       │</span>
<span class="line">│     └── 单一职责                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. Entity 只是 ID                                         │</span>
<span class="line">│     ├── 不存储数据，不包含行为                             │</span>
<span class="line">│     ├── 只用于标识和关联 Component                          │</span>
<span class="line">│     └── 避免在 Entity 上存储业务逻辑                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 合理划分 Component                                     │</span>
<span class="line">│     ├── 按功能划分，而非按实体类型                         │</span>
<span class="line">│     ├── 保持 Component 独立                               │</span>
<span class="line">│     └── 避免相互依赖                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-常见陷阱" tabindex="-1"><a class="header-anchor" href="#_6-2-常见陷阱"><span>6.2 常见陷阱</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                  ECS 常见陷阱                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  ❌ 错误做法：                                              │</span>
<span class="line">│  ├── Component 包含逻辑                                    │</span>
<span class="line">│  │   struct Position {                                     │       │</span>
<span class="line">│  │       float x, y, z;                                    │       │</span>
<span class="line">│  │       void move(float dx, float dy) { ... }             │       │</span>
<span class="line">│  │   };                                                   │       │</span>
<span class="line">│  │                                                        │       │</span>
<span class="line">│  ├── System 存储状态                                       │</span>
<span class="line">│  │   class MovementSystem {                               │       │</span>
<span class="line">│  │       std::vector&lt;Entity&gt; entities_; // ❌              │       │</span>
<span class="line">│  │   };                                                   │       │</span>
<span class="line">│  │                                                        │       │</span>
<span class="line">│  ├── Entity 包含数据                                       │</span>
<span class="line">│  │   class Entity {                                       │       │</span>
<span class="line">│  │       Position position_; // ❌                         │       │</span>
<span class="line">│  │   };                                                   │       │</span>
<span class="line">│  │                                                        │       │</span>
<span class="line">│  ✅ 正确做法：                                              │</span>
<span class="line">│  ├── Component 纯数据                                       │</span>
<span class="line">│  │   struct Position { float x, y, z; };                  │       │</span>
<span class="line">│  │                                                        │       │</span>
<span class="line">│  ├── System 只操作数据                                      │</span>
<span class="line">│  │   class MovementSystem {                               │       │</span>
<span class="line">│  │       void update(EntityManager&amp; em);                   │       │</span>
<span class="line">│  │   };                                                   │       │</span>
<span class="line">│  │                                                        │       │</span>
<span class="line">│  └── Entity 只是个 ID                                       │</span>
<span class="line">│      using Entity = uint32_t;                              │       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="ecs-vs-oop-总结" tabindex="-1"><a class="header-anchor" href="#ecs-vs-oop-总结"><span>ECS vs OOP 总结</span></a></h3><table><thead><tr><th>维度</th><th>OOP</th><th>ECS</th></tr></thead><tbody><tr><td><strong>数据组织</strong></td><td>对象分散</td><td>类型连续</td></tr><tr><td><strong>缓存友好</strong></td><td>低</td><td>高</td></tr><tr><td><strong>并行性</strong></td><td>困难</td><td>容易</td></tr><tr><td><strong>扩展性</strong></td><td>继承受限</td><td>组合灵活</td></tr><tr><td><strong>学习曲线</strong></td><td>低</td><td>中</td></tr><tr><td><strong>适用场景</strong></td><td>逻辑简单</td><td>大量实体</td></tr></tbody></table><h3 id="何时使用-ecs" tabindex="-1"><a class="header-anchor" href="#何时使用-ecs"><span>何时使用 ECS</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">推荐使用 ECS 的场景：</span>
<span class="line">✅ 大量相似实体 (1000+)</span>
<span class="line">✅ 高性能要求 (60+ FPS)</span>
<span class="line">✅ 频繁添加/移除组件</span>
<span class="line">✅ 需要数据并行处理</span>
<span class="line"></span>
<span class="line">不推荐使用 ECS 的场景：</span>
<span class="line">❌ 实体数量少 (&lt; 100)</span>
<span class="line">❌ 逻辑关系复杂</span>
<span class="line">❌ 团队不熟悉 ECS</span>
<span class="line">❌ 开发时间紧迫</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://docs.unity3d.com/Packages/com.unity.entities@latest" target="_blank" rel="noopener noreferrer">Unity DOTS 文档</a></li><li><a href="https://docs.unrealengine.com/5.0/en-US/Programming/Development/Architecture/MassEntity/" target="_blank" rel="noopener noreferrer">Unreal Mass Entity Component System</a></li><li><a href="https://github.com/SanderMertens/ecs-faq" target="_blank" rel="noopener noreferrer">ECS FAQ</a></li></ul>`,51)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};