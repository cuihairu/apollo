import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q18-ghost-mechanism.html","title":"Q18: 什么是 Ghost/Shadow 机制？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q18-ghost-mechanism.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q18-ghost-mechanism.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="q18-什么是-ghost-shadow-机制" tabindex="-1"><a class="header-anchor" href="#q18-什么是-ghost-shadow-机制"><span>Q18: 什么是 Ghost/Shadow 机制？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对分布式游戏实体管理的理解：</p><ul><li>Ghost/Shadow 机制的设计原理</li><li>KBEngine 的 Real/Ghost/Shadow 实现</li><li>BigWorld 的实体同步机制</li><li>跨服务器的实体状态同步</li></ul><hr><h2 id="一、ghost-shadow-机制概述" tabindex="-1"><a class="header-anchor" href="#一、ghost-shadow-机制概述"><span>一、Ghost/Shadow 机制概述</span></a></h2><h3 id="_1-1-基本概念" tabindex="-1"><a class="header-anchor" href="#_1-1-基本概念"><span>1.1 基本概念</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              Ghost/Shadow 机制核心概念                         │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题：分布式环境下的实体管理                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  一个玩家实体需要存在于多个地方：                  │       │</span>
<span class="line">│  │                                                   │       │</span>
<span class="line">│  │  CellApp1 ──► 玩家 A 的 Real (权威)              │       │</span>
<span class="line">│  │  CellApp2 ──► 玩家 A 的 Ghost (镜像)             │       │</span>
<span class="line">│  │  BaseApp  ──► 玩家 A 的 Ghost (镜像)             │       │</span>
<span class="line">│  │  客户端   ──► 玩家 A 的 Shadow (显示)            │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  定义：                                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  Real    → 权威实体，可执行逻辑                   │       │</span>
<span class="line">│  │  Ghost   → 镜像实体，只读状态                     │       │</span>
<span class="line">│  │  Shadow  → 客户端实体，用于显示                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  核心思想：                                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 每个实体只有一个 Real 副本                    │       │</span>
<span class="line">│  │  2. 可以有多个 Ghost 副本                         │       │</span>
<span class="line">│  │  3. Real 负责权威逻辑计算                          │       │</span>
<span class="line">│  │  4. Ghost 从 Real 同步状态                         │       │</span>
<span class="line">│  │  5. Shadow 从 Ghost 同步显示                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-为什么需要-ghost-shadow" tabindex="-1"><a class="header-anchor" href="#_1-2-为什么需要-ghost-shadow"><span>1.2 为什么需要 Ghost/Shadow</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              Ghost/Shadow 的必要性                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 1: 玩家跨 CellApp 交互                                 │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  CellApp1              CellApp2                   │       │</span>
<span class="line">│  │  ┌─────────┐          ┌─────────┐               │       │</span>
<span class="line">│  │  │ 玩家 A   │ ───交互───→│ 玩家 B   │               │       │</span>
<span class="line">│  │  │ (Real)  │          │ (Real)  │               │       │</span>
<span class="line">│  │  └─────────┘          └─────────┘               │       │</span>
<span class="line">│  │       │                    │                    │       │</span>
<span class="line">│  │       └────Ghost────────────┘                    │       │</span>
<span class="line">│  │                   │                              │       │</span>
<span class="line">│  │              (互相看到对方)                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│  → 玩家 A 在 CellApp1 需要知道玩家 B 的状态                   │</span>
<span class="line">│  → 玩家 B 在 CellApp2 需要知道玩家 A 的状态                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 2: BaseApp 需要访问实体数据                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  BaseApp                                       │       │</span>
<span class="line">│  │  ┌─────────┐                                    │       │</span>
<span class="line">│  │  │ 玩家 A   │ ←── Ghost 同步 ──→ CellApp      │       │</span>
<span class="line">│  │  │ (Ghost) │                                    │       │</span>
<span class="line">│  │  └─────────┘                                    │       │</span>
<span class="line">│  │       │                                         │       │</span>
<span class="line">│  │       ├── 查询玩家状态                            │       │</span>
<span class="line">│  │       ├── 发送邮件                                │       │</span>
<span class="line">│  │       ├── 更新任务进度                            │       │</span>
<span class="line">│  │       └── 处理交易                                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│  → BaseApp 需要 CellApp 实体的状态副本                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景 3: 客户端需要显示其他玩家                               │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  客户端 A                                       │       │</span>
<span class="line">│  │  ┌─────────────────────────────────────┐        │       │</span>
<span class="line">│  │  │ 本地玩家: Shadow (可预测)           │        │       │</span>
<span class="line">│  │  │ 其他玩家: Shadow (只显示)           │        │       │</span>
<span class="line">│  │  │                                      │        │       │</span>
<span class="line">│  │  │ ┌─────┐ ┌─────┐ ┌─────┐           │        │       │</span>
<span class="line">│  │  │ │我   │ │玩家B│ │玩家C│  ← Shadow │        │       │</span>
<span class="line">│  │  │ └─────┘ └─────┘ └─────┘           │        │       │</span>
<span class="line">│  │  └─────────────────────────────────────┘        │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│  → 客户端需要服务器实体的显示副本                             │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、kbengine-real-ghost-shadow-实现" tabindex="-1"><a class="header-anchor" href="#二、kbengine-real-ghost-shadow-实现"><span>二、KBEngine Real/Ghost/Shadow 实现</span></a></h2><h3 id="_2-1-架构总览" tabindex="-1"><a class="header-anchor" href="#_2-1-架构总览"><span>2.1 架构总览</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│           KBEngine Real/Ghost/Shadow 架构                     │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│                    ┌──────────────┐                        │</span>
<span class="line">│                    │   BaseApp    │                        │</span>
<span class="line">│                    │              │                        │</span>
<span class="line">│   ┌────────────────┤  Ghost Pool  ├────────────────┐       │</span>
<span class="line">│   │                │              │                │       │</span>
<span class="line">│   │                └───────┬──────┘                │       │</span>
<span class="line">│   │                        │                       │       │</span>
<span class="line">│   │         ┌──────────────┼──────────────┐       │       │</span>
<span class="line">│   │         │              │              │       │       │</span>
<span class="line">│   │    ┌────▼────┐    ┌────▼────┐    ┌────▼────┐  │       │</span>
<span class="line">│   │    │CellApp1 │    │CellApp2 │    │CellApp3 │  │       │</span>
<span class="line">│   │    │         │    │         │    │         │  │       │</span>
<span class="line">│   │    │Real Pool│    │Real Pool│    │Real Pool│  │       │</span>
<span class="line">│   │    │         │    │         │    │         │  │       │</span>
<span class="line">│   │    │Ghost    │    │Ghost    │    │Ghost    │  │       │</span>
<span class="line">│   │    │Pool     │    │Pool     │    │Pool     │  │       │</span>
<span class="line">│   │    └────┬────┘    └────┬────┘    └────┬────┘  │       │</span>
<span class="line">│   │         │              │              │         │       │</span>
<span class="line">│   │         └──────┬───────┴──────────────┘         │       │</span>
<span class="line">│   │                │ Inter-CellApp Comm            │       │</span>
<span class="line">│   │                │                               │       │</span>
<span class="line">│   │         ┌──────▼──────┐                        │       │</span>
<span class="line">│   │         │  CellAppMgr │                        │       │</span>
<span class="line">│   │         └─────────────┘                        │       │</span>
<span class="line">│   │                                                │       │</span>
<span class="line">│   │    ┌─────────────────────────────────────┐     │       │</span>
<span class="line">│   │    │            客户端                    │     │       │</span>
<span class="line">│   │    │  ┌─────┐ ┌─────┐ ┌─────┐           │     │       │</span>
<span class="line">│   │    │  │Shadow│ │Shadow│ │Shadow│  显示层 │     │       │</span>
<span class="line">│   │    │  └─────┘ └─────┘ └─────┘           │     │       │</span>
<span class="line">│   │    └─────────────────────────────────────┘     │       │</span>
<span class="line">│   │                                                │       │</span>
<span class="line">│   └────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-kbengine-源码分析" tabindex="-1"><a class="header-anchor" href="#_2-2-kbengine-源码分析"><span>2.2 KBEngine 源码分析</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Entity 类型定义</span></span>
<span class="line"><span class="token comment">// src/server/entitydef/entity_def.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Entity 实体类型</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">EntityType</span> <span class="token punctuation">{</span></span>
<span class="line">    ENTITY_TYPE_NULL <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">    ENTITY_TYPE_CLIENT <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span>      <span class="token comment">// 客户端实体</span></span>
<span class="line">    ENTITY_TYPE_BASE <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span>        <span class="token comment">// BaseApp 实体</span></span>
<span class="line">    ENTITY_TYPE_CELL <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">,</span>        <span class="token comment">// CellApp 实体</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Entity 基类</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">EntityFlags</span> <span class="token punctuation">{</span></span>
<span class="line">        ENTITY_FLAG_NORMAL <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">        ENTITY_FLAG_DESTROYED <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">0</span><span class="token punctuation">,</span>    <span class="token comment">// 已销毁</span></span>
<span class="line">        ENTITY_FLAG_IN_GRID <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">1</span><span class="token punctuation">,</span>      <span class="token comment">// 在网格中</span></span>
<span class="line">        ENTITY_FLAG_HAS_GHOST <span class="token operator">=</span> <span class="token number">1</span> <span class="token operator">&lt;&lt;</span> <span class="token number">2</span><span class="token punctuation">,</span>    <span class="token comment">// 有 Ghost</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 实体 ID</span></span>
<span class="line">    <span class="token keyword">typedef</span> <span class="token keyword">uint32_t</span> ID<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 构造函数</span></span>
<span class="line">    <span class="token function">Entity</span><span class="token punctuation">(</span>ID id<span class="token punctuation">)</span><span class="token operator">:</span></span>
<span class="line">        <span class="token function">id_</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token function">type_</span><span class="token punctuation">(</span>ENTITY_TYPE_NULL<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token function">flags_</span><span class="token punctuation">(</span>ENTITY_FLAG_NORMAL<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token function">pWitness_</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token operator">~</span><span class="token function">Entity</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取实体 ID</span></span>
<span class="line">    ID <span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> id_<span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 是否是 Real</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isReal</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 在 CellApp 上且不是 Ghost 就是 Real</span></span>
<span class="line">        <span class="token keyword">return</span> pWitness_ <span class="token operator">!=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 是否是 Ghost</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isGhost</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span>flags_ <span class="token operator">&amp;</span> ENTITY_FLAG_HAS_GHOST<span class="token punctuation">)</span> <span class="token operator">!=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    ID id_<span class="token punctuation">;</span>                        <span class="token comment">// 实体 ID</span></span>
<span class="line">    EntityType type_<span class="token punctuation">;</span>              <span class="token comment">// 实体类型</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> flags_<span class="token punctuation">;</span>               <span class="token comment">// 实体标志</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Witness（观察者管理器）</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">Witness</span><span class="token operator">*</span> pWitness_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// CellApp 上的实体（可以是 Real 或 Ghost）</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellApp</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Entity</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 创建 Ghost</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">createGhost</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">,</span> <span class="token keyword">const</span> MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 从数据流创建 Ghost 实体</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token class-name">EntityFactory</span><span class="token double-colon punctuation">::</span><span class="token function">create</span><span class="token punctuation">(</span>id<span class="token punctuation">,</span> ENTITY_TYPE_CELL<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>entity<span class="token punctuation">)</span> <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 设置为 Ghost</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span>flags_ <span class="token operator">|=</span> ENTITY_FLAG_HAS_GHOST<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 反序列化初始状态</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">createFromStream</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 添加到 Ghost 列表</span></span>
<span class="line">        ghostEntities_<span class="token punctuation">[</span>id<span class="token punctuation">]</span> <span class="token operator">=</span> entity<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新 Ghost 状态</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateGhost</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">,</span> <span class="token keyword">const</span> MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> ghostEntities_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> ghostEntities_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            it<span class="token operator">-&gt;</span>second<span class="token operator">-&gt;</span><span class="token function">onRemoteUpdate</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 销毁 Ghost</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">destroyGhost</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> ghostEntities_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> ghostEntities_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">delete</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">            ghostEntities_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>EntityID<span class="token punctuation">,</span> Entity<span class="token operator">*</span><span class="token operator">&gt;</span> ghostEntities_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-ghost-同步机制" tabindex="-1"><a class="header-anchor" href="#_2-3-ghost-同步机制"><span>2.3 Ghost 同步机制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Ghost 同步实现</span></span>
<span class="line"><span class="token comment">// src/server/cellapp/cellapp_interface.hpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellAppInterface</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 向其他 CellApp 广播实体状态（用于 Ghost 更新）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">broadcastEntityData</span><span class="token punctuation">(</span>EntityID entityId<span class="token punctuation">,</span> <span class="token keyword">const</span> MemoryStream<span class="token operator">&amp;</span> data<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 获取实体的 AOI 观察者</span></span>
<span class="line">        <span class="token keyword">auto</span> viewers <span class="token operator">=</span> <span class="token function">getWitnesses</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 按 CellApp 分组</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>ComponentID<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;&gt;</span> byCellApp<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID viewer <span class="token operator">:</span> viewers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            ComponentID cellAppId <span class="token operator">=</span> <span class="token function">getEntityCellApp</span><span class="token punctuation">(</span>viewer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            byCellApp<span class="token punctuation">[</span>cellAppId<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>viewer<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 发送到每个 CellApp</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">const</span> <span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>cellAppId<span class="token punctuation">,</span> entityIds<span class="token punctuation">]</span> <span class="token operator">:</span> byCellApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>cellAppId <span class="token operator">==</span> <span class="token keyword">this</span><span class="token operator">-&gt;</span><span class="token function">componentID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 本地 CellApp，直接更新 Ghost</span></span>
<span class="line">                <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID entityId <span class="token operator">:</span> entityIds<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">updateLocalGhost</span><span class="token punctuation">(</span>entityId<span class="token punctuation">,</span> data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 远程 CellApp，网络发送</span></span>
<span class="line">                <span class="token function">sendToCellApp</span><span class="token punctuation">(</span>cellAppId<span class="token punctuation">,</span> entityId<span class="token punctuation">,</span> data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendToCellApp</span><span class="token punctuation">(</span>ComponentID cellAppId<span class="token punctuation">,</span> EntityID entityId<span class="token punctuation">,</span></span>
<span class="line">                      <span class="token keyword">const</span> MemoryStream<span class="token operator">&amp;</span> data<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Bundle<span class="token operator">*</span> pBundle <span class="token operator">=</span> <span class="token class-name">CellAppInterface</span><span class="token double-colon punctuation">::</span><span class="token function">createBundle</span><span class="token punctuation">(</span></span>
<span class="line">            cellapp<span class="token punctuation">,</span><span class="token double-colon punctuation">::</span>remoteOnUpdateData<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>pBundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> entityId<span class="token punctuation">;</span></span>
<span class="line">        pBundle<span class="token operator">-&gt;</span><span class="token function">append</span><span class="token punctuation">(</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token class-name">CellAppInterface</span><span class="token double-colon punctuation">::</span><span class="token function">send</span><span class="token punctuation">(</span>cellapp<span class="token punctuation">,</span> pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateLocalGhost</span><span class="token punctuation">(</span>EntityID ghostId<span class="token punctuation">,</span> <span class="token keyword">const</span> MemoryStream<span class="token operator">&amp;</span> data<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> ghost <span class="token operator">=</span> <span class="token function">findEntity</span><span class="token punctuation">(</span>ghostId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>ghost <span class="token operator">&amp;&amp;</span> ghost<span class="token operator">-&gt;</span><span class="token function">isGhost</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            ghost<span class="token operator">-&gt;</span><span class="token function">onRemoteUpdate</span><span class="token punctuation">(</span>data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-4-witness-机制" tabindex="-1"><a class="header-anchor" href="#_2-4-witness-机制"><span>2.4 Witness 机制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Witness 机制</span></span>
<span class="line"><span class="token comment">// src/server/entitydef/witness.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> KBEngine <span class="token punctuation">{</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Witness：管理实体的观察者</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Witness</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">Witness</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> pEntity<span class="token punctuation">)</span><span class="token operator">:</span></span>
<span class="line">        <span class="token function">pEntity_</span><span class="token punctuation">(</span>pEntity<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">        <span class="token function">lastUpdateTime_</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加观察者</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">attach</span><span class="token punctuation">(</span>EntityID viewerID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>viewers_<span class="token punctuation">.</span><span class="token function">count</span><span class="token punctuation">(</span>viewerID<span class="token punctuation">)</span> <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            viewers_<span class="token punctuation">.</span><span class="token function">insert</span><span class="token punctuation">(</span>viewerID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 发送实体创建消息</span></span>
<span class="line">            <span class="token function">sendCreateToViewer</span><span class="token punctuation">(</span>viewerID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 移除观察者</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">detach</span><span class="token punctuation">(</span>EntityID viewerID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>viewers_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>viewerID<span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 发送实体销毁消息</span></span>
<span class="line">            <span class="token function">sendRemoveToViewer</span><span class="token punctuation">(</span>viewerID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新观察者状态</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateViewers</span><span class="token punctuation">(</span><span class="token keyword">const</span> MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>EntityID viewerID <span class="token operator">:</span> viewers_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">sendUpdateToViewer</span><span class="token punctuation">(</span>viewerID<span class="token punctuation">,</span> stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取所有观察者</span></span>
<span class="line">    <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>unordered_set<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span><span class="token operator">&amp;</span> <span class="token function">viewers</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> viewers_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    Entity<span class="token operator">*</span> pEntity_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_set<span class="token operator">&lt;</span>EntityID<span class="token operator">&gt;</span> viewers_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">uint32_t</span> lastUpdateTime_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendCreateToViewer</span><span class="token punctuation">(</span>EntityID viewerID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Bundle<span class="token operator">*</span> pBundle <span class="token operator">=</span> <span class="token class-name">ClientInterface</span><span class="token double-colon punctuation">::</span><span class="token function">createBundle</span><span class="token punctuation">(</span></span>
<span class="line">            client<span class="token punctuation">,</span><span class="token double-colon punctuation">::</span>onRemoteCreateEntity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>pBundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> pEntity_<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        pEntity_<span class="token operator">-&gt;</span><span class="token function">addPositionToBundle</span><span class="token punctuation">(</span>pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        pEntity_<span class="token operator">-&gt;</span><span class="token function">addDirectionToBundle</span><span class="token punctuation">(</span>pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token class-name">ClientInterface</span><span class="token double-colon punctuation">::</span><span class="token function">send</span><span class="token punctuation">(</span>viewerID<span class="token punctuation">,</span> pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendRemoveToViewer</span><span class="token punctuation">(</span>EntityID viewerID<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Bundle<span class="token operator">*</span> pBundle <span class="token operator">=</span> <span class="token class-name">ClientInterface</span><span class="token double-colon punctuation">::</span><span class="token function">createBundle</span><span class="token punctuation">(</span></span>
<span class="line">            client<span class="token punctuation">,</span><span class="token double-colon punctuation">::</span>onRemoteRemoveEntity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>pBundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> pEntity_<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token class-name">ClientInterface</span><span class="token double-colon punctuation">::</span><span class="token function">send</span><span class="token punctuation">(</span>viewerID<span class="token punctuation">,</span> pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendUpdateToViewer</span><span class="token punctuation">(</span>EntityID viewerID<span class="token punctuation">,</span> <span class="token keyword">const</span> MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Bundle<span class="token operator">*</span> pBundle <span class="token operator">=</span> <span class="token class-name">ClientInterface</span><span class="token double-colon punctuation">::</span><span class="token function">createBundle</span><span class="token punctuation">(</span></span>
<span class="line">            client<span class="token punctuation">,</span><span class="token double-colon punctuation">::</span>onRemoteUpdateEntity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token punctuation">(</span><span class="token operator">*</span>pBundle<span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> pEntity_<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        pBundle<span class="token operator">-&gt;</span><span class="token function">append</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token class-name">ClientInterface</span><span class="token double-colon punctuation">::</span><span class="token function">send</span><span class="token punctuation">(</span>viewerID<span class="token punctuation">,</span> pBundle<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token punctuation">}</span> <span class="token comment">// namespace KBEngine</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、bigworld-ghost-机制" tabindex="-1"><a class="header-anchor" href="#三、bigworld-ghost-机制"><span>三、BigWorld Ghost 机制</span></a></h2><h3 id="_3-1-bigworld-架构" tabindex="-1"><a class="header-anchor" href="#_3-1-bigworld-架构"><span>3.1 BigWorld 架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│            BigWorld Real/Ghost/Shadow 架构                    │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│                    ┌──────────────┐                        │</span>
<span class="line">│                    │   BaseApp    │                        │</span>
<span class="line">│                    │              │                        │</span>
<span class="line">│   ┌────────────────┤  Base Entity ├────────────────┐       │</span>
<span class="line">│   │                │   (Ghost)   │                │       │</span>
<span class="line">│   │                └───────┬──────┘                │       │</span>
<span class="line">│   │                        │                       │       │</span>
<span class="line">│   │         ┌──────────────┼──────────────┐       │       │</span>
<span class="line">│   │         │              │              │       │       │</span>
<span class="line">│   │    ┌────▼────┐    ┌────▼────┐    ┌────▼────┐  │       │</span>
<span class="line">│   │    │CellApp1 │    │CellApp2 │    │CellApp3 │  │       │</span>
<span class="line">│   │    │         │    │         │    │         │  │       │</span>
<span class="line">│   │    │Cell     │    │Cell     │    │Cell     │  │       │</span>
<span class="line">│   │    │Entity   │    │Entity   │    │Entity   │  │       │</span>
<span class="line">│   │    │(Real)   │    │(Real)   │    │(Real)   │  │       │</span>
<span class="line">│   │    │         │    │         │    │         │  │       │</span>
<span class="line">│   │    │Ghost    │    │Ghost    │    │Ghost    │  │       │</span>
<span class="line">│   │    │Entity   │    │Entity   │    │Entity   │  │       │</span>
<span class="line">│   │    └────┬────┘    └────┬────┘    └────┬────┘  │       │</span>
<span class="line">│   │         │              │              │         │       │</span>
<span class="line">│   │         └──────┬───────┴──────────────┘         │       │</span>
<span class="line">│   │                │ Inter-CellApp Comm            │       │</span>
<span class="line">│   │                                                │       │</span>
<span class="line">│   │    ┌─────────────────────────────────────┐     │       │</span>
<span class="line">│   │    │            客户端                    │     │       │</span>
<span class="line">│   │    │  ┌─────┐ ┌─────┐ ┌─────┐           │     │       │</span>
<span class="line">│   │    │  │Shadow│ │Shadow│ │Shadow│  显示层 │     │       │</span>
<span class="line">│   │    │  │Entity│ │Entity│ │Entity│         │     │       │</span>
<span class="line">│   │    │  └─────┘ └─────┘ └─────┘           │     │       │</span>
<span class="line">│   │    └─────────────────────────────────────┘     │       │</span>
<span class="line">│   │                                                │       │</span>
<span class="line">│   └────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-bigworld-ghost-同步流程" tabindex="-1"><a class="header-anchor" href="#_3-2-bigworld-ghost-同步流程"><span>3.2 BigWorld Ghost 同步流程</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│            BigWorld Ghost 同步流程                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  Real Entity 更新:                                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 玩家 A 在 CellApp1 移动                        │       │</span>
<span class="line">│  │  2. Real Entity 状态变化                           │       │</span>
<span class="line">│  │  3. 检查 AOI 观察者                                │       │</span>
<span class="line">│  │  4. 发现玩家 B 在 CellApp2                         │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                         │                                   │</span>
<span class="line">│                         ▼                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  5. 发送 Ghost 更新到 CellApp2                    │       │</span>
<span class="line">│  │  消息: {                                          │       │</span>
<span class="line">│  │    entityType: &quot;Player&quot;,                          │       │</span>
<span class="line">│  │    entityId: 12345,                               │       │</span>
<span class="line">│  │    position: {x: 100, y: 0, z: 200},            │       │</span>
<span class="line">│  │    velocity: {x: 5, y: 0, z: 3}                  │       │</span>
<span class="line">│  │  }                                                │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                         │                                   │</span>
<span class="line">│                         ▼                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  6. CellApp2 更新本地 Ghost                      │       │</span>
<span class="line">│  │  7. Ghost 更新玩家 A 的位置                       │       │</span>
<span class="line">│  │  8. 检查玩家 B 的 AOI                             │       │</span>
<span class="line">│  │  9. 通知玩家 B 的客户端                           │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                         │                                   │</span>
<span class="line">│                         ▼                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  10. 客户端 B 接收更新                             │       │</span>
<span class="line">│  │  11. 更新 Shadow Entity                          │       │</span>
<span class="line">│  │  12. 平滑插值显示                                  │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、real-ghost-转换" tabindex="-1"><a class="header-anchor" href="#四、real-ghost-转换"><span>四、Real/Ghost 转换</span></a></h2><h3 id="_4-1-跨-cellapp-迁移" tabindex="-1"><a class="header-anchor" href="#_4-1-跨-cellapp-迁移"><span>4.1 跨 CellApp 迁移</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              Real/Ghost 跨 CellApp 迁移                        │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  场景：玩家从 CellApp1 移动到 CellApp2                        │</span>
<span class="line">│                                                             │</span>
<span class="line">│  步骤 1: 检测边界                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  CellApp1          CellApp2                       │       │</span>
<span class="line">│  │  ┌─────┬────┐    ┌────┬─────┐                   │       │</span>
<span class="line">│  │  │     │    │    │    │     │                   │       │</span>
<span class="line">│  │  │     │ A  │───→│    │     │  ← A 跨越边界      │       │</span>
<span class="line">│  │  │     │    │    │    │     │                   │       │</span>
<span class="line">│  │  └─────┴────┘    └────┴─────┘                   │       │</span>
<span class="line">│  │         │                │                       │       │</span>
<span class="line">│  │    Real Entity        Ghost Entity              │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  步骤 2: 开始迁移                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. CellApp1 检测 A 越出边界                      │       │</span>
<span class="line">│  │  2. 向 CellAppMgr 请求迁移                        │       │</span>
<span class="line">│  │  3. CellAppMgr 确定 CellApp2                     │       │</span>
<span class="line">│  │  4. 通知 CellApp1 和 CellApp2                     │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  步骤 3: 迁移 Real                                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. CellApp1 序列化 Real 状态                     │       │</span>
<span class="line">│  │  2. 发送完整实体数据到 CellApp2                    │       │</span>
<span class="line">│  │  3. CellApp2 创建新 Real                          │       │</span>
<span class="line">│  │  4. CellApp1 将 Real 转为 Ghost                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  步骤 4: 更新观察者                                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  1. 通知客户端切换连接                             │       │</span>
<span class="line">│  │  2. 更新所有相关 Ghost                            │       │</span>
<span class="line">│  │  3. 重新计算 AOI                                  │       │</span>
<span class="line">│  │  4. 旧 Real 在延迟后销毁                           │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-迁移代码示例" tabindex="-1"><a class="header-anchor" href="#_4-2-迁移代码示例"><span>4.2 迁移代码示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 跨 CellApp 实体迁移</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellAppEntityMigration</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 开始迁移</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">startMigration</span><span class="token punctuation">(</span>EntityID entityId<span class="token punctuation">,</span> CellAppID targetCellApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>entity <span class="token operator">||</span> <span class="token operator">!</span>entity<span class="token operator">-&gt;</span><span class="token function">isReal</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 序列化实体状态</span></span>
<span class="line">        MemoryStream entityData<span class="token punctuation">;</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">serializeTo</span><span class="token punctuation">(</span>entityData<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 发送到目标 CellApp</span></span>
<span class="line">        <span class="token function">sendMigrationData</span><span class="token punctuation">(</span>targetCellApp<span class="token punctuation">,</span> entityId<span class="token punctuation">,</span> entityData<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 转换为 Ghost</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setGhost</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        migratingEntities_<span class="token punctuation">[</span>entityId<span class="token punctuation">]</span> <span class="token operator">=</span> targetCellApp<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 设置迁移超时</span></span>
<span class="line">        <span class="token function">scheduleMigrationTimeout</span><span class="token punctuation">(</span>entityId<span class="token punctuation">,</span> <span class="token number">5000</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token comment">// 5秒</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 接收迁移数据</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onMigrationData</span><span class="token punctuation">(</span>EntityID entityId<span class="token punctuation">,</span> <span class="token keyword">const</span> MemoryStream<span class="token operator">&amp;</span> data<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 创建新的 Real 实体</span></span>
<span class="line">        Entity<span class="token operator">*</span> newEntity <span class="token operator">=</span> <span class="token function">createEntityFromData</span><span class="token punctuation">(</span>entityId<span class="token punctuation">,</span> data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>newEntity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 迁移失败，通知源 CellApp</span></span>
<span class="line">            <span class="token function">sendMigrationFailed</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 确认为 Real</span></span>
<span class="line">        newEntity<span class="token operator">-&gt;</span><span class="token function">setReal</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 通知源 CellApp 迁移成功</span></span>
<span class="line">        <span class="token function">sendMigrationSuccess</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 迁移成功</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onMigrationSuccess</span><span class="token punctuation">(</span>EntityID entityId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> migratingEntities_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> migratingEntities_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 1. 清理迁移状态</span></span>
<span class="line">            migratingEntities_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 2. 销毁旧的 Ghost</span></span>
<span class="line">            Entity<span class="token operator">*</span> oldGhost <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>oldGhost<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">destroyEntity</span><span class="token punctuation">(</span>oldGhost<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 迁移失败</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onMigrationFailed</span><span class="token punctuation">(</span>EntityID entityId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> migratingEntities_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> migratingEntities_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 1. 恢复为 Real</span></span>
<span class="line">            Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                entity<span class="token operator">-&gt;</span><span class="token function">setReal</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">            migratingEntities_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>it<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>EntityID<span class="token punctuation">,</span> CellAppID<span class="token operator">&gt;</span> migratingEntities_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、shadow-entity-客户端" tabindex="-1"><a class="header-anchor" href="#五、shadow-entity-客户端"><span>五、Shadow Entity（客户端）</span></a></h2><h3 id="_5-1-shadow-与-ghost-的关系" tabindex="-1"><a class="header-anchor" href="#_5-1-shadow-与-ghost-的关系"><span>5.1 Shadow 与 Ghost 的关系</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│           Shadow Entity 与 Ghost 的关系                       │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  服务器端:                                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  CellApp                                        │       │</span>
<span class="line">│  │  ┌─────────────┐        ┌─────────────┐        │       │</span>
<span class="line">│  │  │  Real       │        │  Ghost      │        │       │</span>
<span class="line">│  │  │  Entity     │───────→│  Entity     │        │       │</span>
<span class="line">│  │  │             │ 同步   │             │        │       │</span>
<span class="line">│  │  └─────────────┘        └─────────────┘        │       │</span>
<span class="line">│  │         │                       │               │       │</span>
<span class="line">│  │         │                       │               │       │</span>
<span class="line">│  │    ┌────▼───────────────────────▼────┐         │       │</span>
<span class="line">│  │    │        BaseApp                 │         │       │</span>
<span class="line">│  │    │  ┌─────────────────────────┐  │         │       │</span>
<span class="line">│  │    │  │  Ghost Entity           │  │         │       │</span>
<span class="line">│  │    │  └─────────────────────────┘  │         │       │</span>
<span class="line">│  │    │            │                  │         │       │</span>
<span class="line">│  │    └────────────┼──────────────────┘         │       │</span>
<span class="line">│  │                 │                            │       │</span>
<span class="line">│  └─────────────────┼────────────────────────────┘       │</span>
<span class="line">│                    │ 网络同步                              │</span>
<span class="line">│                    ▼                                       │</span>
<span class="line">│  客户端:                                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  ┌─────────────────────────────────────┐        │       │</span>
<span class="line">│  │  │        Shadow Entity               │        │       │</span>
<span class="line">│  │  │                                     │        │       │</span>
<span class="line">│  │  │  ┌─────────────────────────────┐    │        │       │</span>
<span class="line">│  │  │  │  Visual Component          │    │        │       │</span>
<span class="line">│  │  │  │  - 3D 模型                  │    │        │       │</span>
<span class="line">│  │  │  │  - 动画                     │    │        │       │</span>
<span class="line">│  │  │  │  - 特效                     │    │        │       │</span>
<span class="line">│  │  │  └─────────────────────────────┘    │        │       │</span>
<span class="line">│  │  │                                     │        │       │</span>
<span class="line">│  │  │  ┌─────────────────────────────┐    │        │       │</span>
<span class="line">│  │  │  │  Predictive Component      │    │        │       │</span>
<span class="line">│  │  │  │  - 客户端预测 (仅本地)      │    │        │       │</span>
<span class="line">│  │  │  └─────────────────────────────┘    │        │       │</span>
<span class="line">│  │  │                                     │        │       │</span>
<span class="line">│  │  │  ┌─────────────────────────────┐    │        │       │</span>
<span class="line">│  │  │  │  Smoothing Component       │    │        │       │</span>
<span class="line">│  │  │  │  - 插值平滑                  │    │        │       │</span>
<span class="line">│  │  │  │  - 外推预测                  │    │        │       │</span>
<span class="line">│  │  │  └─────────────────────────────┘    │        │       │</span>
<span class="line">│  │  └─────────────────────────────────────┘        │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-shadow-更新流程" tabindex="-1"><a class="header-anchor" href="#_5-2-shadow-更新流程"><span>5.2 Shadow 更新流程</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 客户端 Shadow Entity 更新</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ShadowEntity</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 从服务器更新创建/更新 Shadow</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onServerUpdate</span><span class="token punctuation">(</span><span class="token keyword">const</span> EntityUpdate<span class="token operator">&amp;</span> update<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 更新服务器位置</span></span>
<span class="line">        serverStates_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">            update<span class="token punctuation">.</span>position<span class="token punctuation">,</span></span>
<span class="line">            update<span class="token punctuation">.</span>direction<span class="token punctuation">,</span></span>
<span class="line">            update<span class="token punctuation">.</span>velocity<span class="token punctuation">,</span></span>
<span class="line">            update<span class="token punctuation">.</span>timestamp</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 限制历史数量</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>serverStates_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> MAX_SERVER_STATES<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            serverStates_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>serverStates_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 如果是本地玩家，重置预测</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>isLocalPlayer_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">resetPrediction</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 每帧更新</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token keyword">float</span> deltaTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>isLocalPlayer_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 本地玩家：使用预测位置</span></span>
<span class="line">            <span class="token function">updatePrediction</span><span class="token punctuation">(</span>deltaTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 其他玩家：插值服务器位置</span></span>
<span class="line">            <span class="token function">updateInterpolation</span><span class="token punctuation">(</span>deltaTime<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateInterpolation</span><span class="token punctuation">(</span><span class="token keyword">float</span> deltaTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>serverStates_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> <span class="token number">2</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 找到当前时间的插值点</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> renderTime <span class="token operator">=</span> <span class="token function">getCurrentTime</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> INTERPOLATION_DELAY<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> nextIt <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">upper_bound</span><span class="token punctuation">(</span>serverStates_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            serverStates_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> renderTime<span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> t<span class="token punctuation">,</span> <span class="token keyword">const</span> ServerState<span class="token operator">&amp;</span> s<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> t <span class="token operator">&lt;</span> s<span class="token punctuation">.</span>timestamp<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>nextIt <span class="token operator">==</span> serverStates_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">||</span></span>
<span class="line">            nextIt <span class="token operator">==</span> serverStates_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> prevIt <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span><span class="token function">prev</span><span class="token punctuation">(</span>nextIt<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 计算插值系数</span></span>
<span class="line">        <span class="token keyword">float</span> t <span class="token operator">=</span> <span class="token punctuation">(</span>renderTime <span class="token operator">-</span> prevIt<span class="token operator">-&gt;</span>timestamp<span class="token punctuation">)</span> <span class="token operator">/</span></span>
<span class="line">                  <span class="token keyword">float</span><span class="token punctuation">(</span>nextIt<span class="token operator">-&gt;</span>timestamp <span class="token operator">-</span> prevIt<span class="token operator">-&gt;</span>timestamp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 插值位置</span></span>
<span class="line">        displayPosition_ <span class="token operator">=</span> <span class="token function">lerp</span><span class="token punctuation">(</span>prevIt<span class="token operator">-&gt;</span>position<span class="token punctuation">,</span> nextIt<span class="token operator">-&gt;</span>position<span class="token punctuation">,</span> t<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        displayDirection_ <span class="token operator">=</span> <span class="token function">slerp</span><span class="token punctuation">(</span>prevIt<span class="token operator">-&gt;</span>direction<span class="token punctuation">,</span> nextIt<span class="token operator">-&gt;</span>direction<span class="token punctuation">,</span> t<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updatePrediction</span><span class="token punctuation">(</span><span class="token keyword">float</span> deltaTime<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 本地玩家使用客户端预测</span></span>
<span class="line">        predictedPosition_ <span class="token operator">+=</span> clientInput_<span class="token punctuation">.</span>velocity <span class="token operator">*</span> deltaTime<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 服务器确认后校正</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>serverStates_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            ServerState<span class="token operator">&amp;</span> latest <span class="token operator">=</span> serverStates_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            Vector3 error <span class="token operator">=</span> latest<span class="token punctuation">.</span>position <span class="token operator">-</span> predictedPosition_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 平滑校正</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">length</span><span class="token punctuation">(</span>error<span class="token punctuation">)</span> <span class="token operator">&gt;</span> MAX_CORRECTION_DISTANCE<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                predictedPosition_ <span class="token operator">=</span> latest<span class="token punctuation">.</span>position<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">                predictedPosition_ <span class="token operator">+=</span> error <span class="token operator">*</span> CORRECTION_SPEED<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        displayPosition_ <span class="token operator">=</span> predictedPosition_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">ServerState</span> <span class="token punctuation">{</span></span>
<span class="line">        Vector3 position<span class="token punctuation">;</span></span>
<span class="line">        Quaternion direction<span class="token punctuation">;</span></span>
<span class="line">        Vector3 velocity<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint32_t</span> timestamp<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>ServerState<span class="token operator">&gt;</span> serverStates_<span class="token punctuation">;</span></span>
<span class="line">    Vector3 displayPosition_<span class="token punctuation">;</span></span>
<span class="line">    Quaternion displayDirection_<span class="token punctuation">;</span></span>
<span class="line">    Vector3 predictedPosition_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">bool</span> isLocalPlayer_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    ClientInput clientInput_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">uint32_t</span> INTERPOLATION_DELAY <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span> <span class="token comment">// ms</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> size_t MAX_SERVER_STATES <span class="token operator">=</span> <span class="token number">60</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">float</span> MAX_CORRECTION_DISTANCE <span class="token operator">=</span> <span class="token number">5.0f</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">float</span> CORRECTION_SPEED <span class="token operator">=</span> <span class="token number">0.2f</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-ghost-shadow-设计原则" tabindex="-1"><a class="header-anchor" href="#_6-1-ghost-shadow-设计原则"><span>6.1 Ghost/Shadow 设计原则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              Ghost/Shadow 设计原则                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 权威唯一性                                              │</span>
<span class="line">│     ├── 每个实体只有一个 Real                               │</span>
<span class="line">│     ├── Real 负责所有权威计算                               │</span>
<span class="line">│     ├── Ghost 只读、不执行逻辑                               │</span>
<span class="line">│     └── Shadow 仅用于显示                                   │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 同步优化                                                │</span>
<span class="line">│     ├── 使用 AOI 过滤同步对象                                │</span>
<span class="line">│     ├── 设置同步优先级                                      │</span>
<span class="line">│     ├── 使用增量同步                                         │</span>
<span class="line">│     └── 压缩同步数据                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 容错处理                                                │</span>
<span class="line">│     ├── 处理同步丢失                                         │</span>
<span class="line">│     ├── 处理乱序消息                                         │</span>
<span class="line">│     ├── 定期全量同步                                         │</span>
<span class="line">│     └── 超时重连机制                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  4. 平滑处理                                                │</span>
<span class="line">│     ├── 客户端插值显示                                       │</span>
<span class="line">│     ├── 预测本地玩家                                         │</span>
<span class="line">│     ├── 外推减少延迟感                                       │</span>
<span class="line">│     └── 校正平滑过渡                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  5. 性能考虑                                                │</span>
<span class="line">│     ├── 限制 Ghost 数量                                      │</span>
<span class="line">│     ├── 批量处理更新                                         │</span>
<span class="line">│     ├── 使用对象池                                           │</span>
<span class="line">│     └── 避免不必要的拷贝                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-常见问题与解决方案" tabindex="-1"><a class="header-anchor" href="#_6-2-常见问题与解决方案"><span>6.2 常见问题与解决方案</span></a></h3><table><thead><tr><th>问题</th><th>原因</th><th>解决方案</th></tr></thead><tbody><tr><td><strong>实体卡顿</strong></td><td>同步频率低</td><td>增加同步频率 + 客户端插值</td></tr><tr><td><strong>位置跳变</strong></td><td>丢包后校正</td><td>使用平滑校正 + 历史插值</td></tr><tr><td><strong>Ghost 不同步</strong></td><td>网络分区</td><td>心跳检测 + 重连机制</td></tr><tr><td><strong>迁移卡住</strong></td><td>目标不可达</td><td>超时回滚 + 重新路由</td></tr><tr><td><strong>Shadow 抖动</strong></td><td>同步不稳定</td><td>增加插值延迟 + 平滑因子</td></tr></tbody></table><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="real-ghost-shadow-机制总结" tabindex="-1"><a class="header-anchor" href="#real-ghost-shadow-机制总结"><span>Real/Ghost/Shadow 机制总结</span></a></h3><table><thead><tr><th>类型</th><th>位置</th><th>职责</th><th>可写?</th><th>可执行逻辑?</th></tr></thead><tbody><tr><td><strong>Real</strong></td><td>CellApp</td><td>权威实体</td><td>是</td><td>是</td></tr><tr><td><strong>Ghost</strong></td><td>CellApp/BaseApp</td><td>镜像实体</td><td>否</td><td>否</td></tr><tr><td><strong>Shadow</strong></td><td>客户端</td><td>显示实体</td><td>否</td><td>仅预测</td></tr></tbody></table><h3 id="同步路径" tabindex="-1"><a class="header-anchor" href="#同步路径"><span>同步路径</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Real Entity (CellApp)</span>
<span class="line">    │</span>
<span class="line">    ├─→ Ghost Entity (其他 CellApp) ──→ Shadow (客户端)</span>
<span class="line">    │</span>
<span class="line">    └─→ Ghost Entity (BaseApp) ──→ 数据操作</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/server/entitydef/entity_def.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - Entity 定义</a></li><li><a href="https://github.com/kbengine/kbengine/blob/master/kbe/src/server/entitydef/witness.h" target="_blank" rel="noopener noreferrer">KBEngine GitHub - Witness 机制</a></li><li><a href="https://wiki.bigworldtech.com/" target="_blank" rel="noopener noreferrer">BigWhite Wiki - Entity System</a></li></ul>`,53)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};