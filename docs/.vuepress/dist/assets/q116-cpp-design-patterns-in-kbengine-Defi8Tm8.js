import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/qa/q116-cpp-design-patterns-in-kbengine.html","title":"C++ 设计模式在 KBEngine 中的实践","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q116-cpp-design-patterns-in-kbengine.md","git":{"createdTime":1773965426000,"updatedTime":1773965426000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`q116-cpp-design-patterns-in-kbengine.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="c-设计模式在-kbengine-中的实践" tabindex="-1"><a class="header-anchor" href="#c-设计模式在-kbengine-中的实践"><span>C++ 设计模式在 KBEngine 中的实践</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对设计模式和 KBEngine 架构的深入理解：</p><ul><li>KBEngine 使用了哪些设计模式</li><li>为什么要这样设计</li><li>实际代码中的应用</li><li>设计权衡</li></ul><hr><h2 id="一、kbengine-核心设计模式" tabindex="-1"><a class="header-anchor" href="#一、kbengine-核心设计模式"><span>一、KBEngine 核心设计模式</span></a></h2><h3 id="_1-1-设计模式概览" tabindex="-1"><a class="header-anchor" href="#_1-1-设计模式概览"><span>1.1 设计模式概览</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              KBEngine 核心设计模式                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  创建型模式:                                                 │</span>
<span class="line">│  ├── Singleton (单例) - DBMgr, BaseAppMgr, CellAppMgr       │</span>
<span class="line">│  ├── Factory (工厂) - 实体创建                               │</span>
<span class="line">│  ├── Builder (建造者) - 消息构建                              │</span>
<span class="line">│  └── Object Pool (对象池) - 内存管理                          │</span>
<span class="line">│                                                             │</span>
<span class="line">│  结构型模式:                                                 │</span>
<span class="line">│  ├── Adapter (适配器) - 网络适配                              │</span>
<span class="line">│  ├── Bridge (桥接) - 接口与实现分离                          │</span>
<span class="line">│  ├── Composite (组合) - 空间管理                              │</span>
<span class="line">│  └── Proxy (代理) - 实体代理 (Ghost/Shadow)                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  行为型模式:                                                 │</span>
<span class="line">│  ├── Observer (观察者) - Watcher 系统                        │</span>
<span class="line">│  ├── Command (命令) - 消息处理                               │</span>
<span class="line">│  ├── Strategy (策略) - 负载均衡                               │</span>
<span class="line">│  ├── Template Method (模板方法) - 实体生命周期               │</span>
<span class="line">│  └── Chain of Responsibility (责任链) - 消息路由             │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、单例模式-singleton" tabindex="-1"><a class="header-anchor" href="#二、单例模式-singleton"><span>二、单例模式 (Singleton)</span></a></h2><h3 id="_2-1-kbengine-应用" tabindex="-1"><a class="header-anchor" href="#_2-1-kbengine-应用"><span>2.1 KBEngine 应用</span></a></h3><p><strong>应用场景</strong>: DBMgr, BaseAppMgr, CellAppMgr 等全局唯一组件</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 单例模式实现</span></span>
<span class="line"><span class="token comment">// src/server/dbmgr.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">DBMgr</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 获取单例实例</span></span>
<span class="line">    <span class="token keyword">static</span> DBMgr<span class="token operator">&amp;</span> <span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">static</span> DBMgr instance<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> instance<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 禁止拷贝和赋值</span></span>
<span class="line">    <span class="token function">DBMgr</span><span class="token punctuation">(</span><span class="token keyword">const</span> DBMgr<span class="token operator">&amp;</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token keyword">delete</span><span class="token punctuation">;</span></span>
<span class="line">    DBMgr<span class="token operator">&amp;</span> <span class="token keyword">operator</span><span class="token operator">=</span><span class="token punctuation">(</span><span class="token keyword">const</span> DBMgr<span class="token operator">&amp;</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token keyword">delete</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 私有构造函数</span></span>
<span class="line">    <span class="token function">DBMgr</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">pNetworkInterface_</span><span class="token punctuation">(</span><span class="token constant">NULL</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">,</span> <span class="token function">bufferSize_</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 成员变量</span></span>
<span class="line">    NetworkInterface<span class="token operator">*</span> pNetworkInterface_<span class="token punctuation">;</span></span>
<span class="line">    uint32 bufferSize_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用方式</span></span>
<span class="line">DBMgr<span class="token operator">&amp;</span> dbmgr <span class="token operator">=</span> <span class="token class-name">DBMgr</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-为什么要用单例" tabindex="-1"><a class="header-anchor" href="#_2-2-为什么要用单例"><span>2.2 为什么要用单例？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    单例模式设计原因                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  全局唯一性:                                                 │</span>
<span class="line">│  ├── DBMgr 只能有一个                                       │</span>
<span class="line">│  ├── 避免数据库连接冲突                                     │</span>
<span class="line">│  ├── 确保数据一致性                                         │</span>
<span class="line">│  └── 防止资源竞争                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  延迟初始化:                                                 │</span>
<span class="line">│  ├── 只在需要时创建                                         │</span>
<span class="line">│  ├── 减少启动开销                                           │</span>
<span class="line">│  ├── 按需分配资源                                           │</span>
<span class="line">│  └── C++11 保证线程安全                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  访问便利:                                                   │</span>
<span class="line">│  ├── 全局访问点                                             │</span>
<span class="line">│  ├── 简化调用代码                                           │</span>
<span class="line">│  ├── 不需要传递引用                                         │</span>
<span class="line">│  └── 代码更清晰                                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  替代方案对比:                                               │</span>
<span class="line">│  ├── 全局变量: 无法控制初始化顺序                            │</span>
<span class="line">│  ├── 传引用: 函数参数复杂                                    │</span>
<span class="line">│  ├── 依赖注入: 增加耦合                                     │</span>
<span class="line">│  └── 单例: 平衡了简洁性和控制力                              │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、工厂模式-factory" tabindex="-1"><a class="header-anchor" href="#三、工厂模式-factory"><span>三、工厂模式 (Factory)</span></a></h2><h3 id="_3-1-kbengine-实体工厂" tabindex="-1"><a class="header-anchor" href="#_3-1-kbengine-实体工厂"><span>3.1 KBEngine 实体工厂</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 实体工厂</span></span>
<span class="line"><span class="token comment">// src/server/entitydef/entitydef.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">EntityDef</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 创建实体</span></span>
<span class="line">    Entity<span class="token operator">*</span> <span class="token function">createEntity</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> pParent<span class="token punctuation">,</span> ENTITY_ID entityID<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 根据类型创建实体</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token constant">NULL</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 通过脚本创建</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>scriptModuleName_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 调用 Python 脚本创建</span></span>
<span class="line">            entity <span class="token operator">=</span> <span class="token class-name">ScriptModule</span><span class="token double-colon punctuation">::</span><span class="token function">getInstance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">-&gt;</span><span class="token function">createEntity</span><span class="token punctuation">(</span></span>
<span class="line">                scriptModuleName_<span class="token punctuation">,</span></span>
<span class="line">                <span class="token keyword">this</span><span class="token punctuation">,</span></span>
<span class="line">                pParent<span class="token punctuation">,</span></span>
<span class="line">                entityID</span>
<span class="line">            <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">else</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// C++ 内置实体</span></span>
<span class="line">            entity <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">Entity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>entity<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 初始化属性</span></span>
<span class="line">            entity<span class="token operator">-&gt;</span><span class="token function">initializeProperty</span><span class="token punctuation">(</span><span class="token operator">*</span><span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> entity<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string scriptModuleName_<span class="token punctuation">;</span>  <span class="token comment">// Python 模块名</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> PropertyDescription<span class="token operator">&gt;</span> properties_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line">EntityDef<span class="token operator">*</span> accountDef <span class="token operator">=</span> <span class="token class-name">EntityDefs</span><span class="token double-colon punctuation">::</span><span class="token function">findEntityDef</span><span class="token punctuation">(</span><span class="token string">&quot;Account&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">Entity<span class="token operator">*</span> account <span class="token operator">=</span> accountDef<span class="token operator">-&gt;</span><span class="token function">createEntity</span><span class="token punctuation">(</span><span class="token constant">NULL</span><span class="token punctuation">,</span> entityID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-为什么要用工厂" tabindex="-1"><a class="header-anchor" href="#_3-2-为什么要用工厂"><span>3.2 为什么要用工厂？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">工厂模式优势:</span>
<span class="line"></span>
<span class="line">1. 解耦创建和使用</span>
<span class="line">   ├── 调用者不需要知道具体类</span>
<span class="line">   ├── 只需要知道实体名称</span>
<span class="line">   └── 便于替换实现</span>
<span class="line"></span>
<span class="line">2. 集中管理创建逻辑</span>
<span class="line">   ├── 统一的创建入口</span>
<span class="line">   ├── 方便添加初始化逻辑</span>
<span class="line">   └── 方便记录日志</span>
<span class="line"></span>
<span class="line">3. 支持多态创建</span>
<span class="line">   ├── C++ 实体</span>
<span class="line">   ├── Python 实体</span>
<span class="line">   ├── Lua 实体 (可扩展)</span>
<span class="line">   └── 运行时决定</span>
<span class="line"></span>
<span class="line">4. 资源管理</span>
<span class="line">   ├── 对象池复用</span>
<span class="line">   ├── 延迟初始化</span>
<span class="line">   └── 自动回收</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、观察者模式-observer" tabindex="-1"><a class="header-anchor" href="#四、观察者模式-observer"><span>四、观察者模式 (Observer)</span></a></h2><h3 id="_4-1-watcher-系统" tabindex="-1"><a class="header-anchor" href="#_4-1-watcher-系统"><span>4.1 Watcher 系统</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine Watcher 系统</span></span>
<span class="line"><span class="token comment">// src/server/watcher.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Watcher</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 注册观察者</span></span>
<span class="line">    <span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addWatch</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> path<span class="token punctuation">,</span> T<span class="token operator">*</span> watcher<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        watches_<span class="token punctuation">[</span>path<span class="token punctuation">]</span><span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span><span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token operator">*</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>watcher<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 移除观察者</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">removeWatch</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> path<span class="token punctuation">,</span> <span class="token keyword">void</span><span class="token operator">*</span> watcher<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> watches_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>path<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> watches_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">auto</span><span class="token operator">&amp;</span> vec <span class="token operator">=</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">;</span></span>
<span class="line">            vec<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>std<span class="token double-colon punctuation">::</span><span class="token function">remove</span><span class="token punctuation">(</span>vec<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> vec<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> watcher<span class="token punctuation">)</span><span class="token punctuation">,</span> vec<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 通知所有观察者</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">notify</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> path<span class="token punctuation">,</span> <span class="token keyword">const</span> boost<span class="token double-colon punctuation">::</span>any<span class="token operator">&amp;</span> value<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> watches_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>path<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> watches_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">void</span><span class="token operator">*</span> watcher <span class="token operator">:</span> it<span class="token operator">-&gt;</span>second<span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">{</span></span>
<span class="line">                <span class="token generic-function"><span class="token function">static_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span>WatcherInterface<span class="token operator">*</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>watcher<span class="token punctuation">)</span><span class="token operator">-&gt;</span><span class="token function">onWatch</span><span class="token punctuation">(</span>path<span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取监控值</span></span>
<span class="line">    boost<span class="token double-colon punctuation">::</span>any <span class="token function">get</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> path<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 解析路径: &quot;stats/cpuUsage&quot;</span></span>
<span class="line">        <span class="token comment">// 返回对应的值</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token function">getValueFromPath</span><span class="token punctuation">(</span>path<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span><span class="token keyword">void</span><span class="token operator">*</span><span class="token operator">&gt;&gt;</span> watches_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ComponentMonitor</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">WatcherInterface</span></span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">init</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token class-name">Watcher</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">addWatch</span><span class="token punctuation">(</span><span class="token string">&quot;stats/cpuUsage&quot;</span><span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token class-name">Watcher</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">.</span><span class="token function">addWatch</span><span class="token punctuation">(</span><span class="token string">&quot;stats/memoryUsage&quot;</span><span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onWatch</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> path<span class="token punctuation">,</span> <span class="token keyword">const</span> boost<span class="token double-colon punctuation">::</span>any<span class="token operator">&amp;</span> value<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>path <span class="token operator">==</span> <span class="token string">&quot;stats/cpuUsage&quot;</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">float</span> cpu <span class="token operator">=</span> boost<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">any_cast</span><span class="token generic class-name"><span class="token operator">&lt;</span><span class="token keyword">float</span><span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span>value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>cpu <span class="token operator">&gt;</span> <span class="token number">80.0f</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">WARNING_MSG</span><span class="token punctuation">(</span><span class="token string">&quot;High CPU usage: {}&quot;</span><span class="token punctuation">,</span> cpu<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-为什么要用观察者" tabindex="-1"><a class="header-anchor" href="#_4-2-为什么要用观察者"><span>4.2 为什么要用观察者？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">观察者模式优势:</span>
<span class="line"></span>
<span class="line">1. 解耦监控和业务</span>
<span class="line">   ├── 监控代码不影响业务逻辑</span>
<span class="line">   ├── 业务代码不需要知道监控存在</span>
<span class="line">   └── 可以动态添加/移除监控</span>
<span class="line"></span>
<span class="line">2. 支持多个观察者</span>
<span class="line">   ├── 多个组件监控同一数据</span>
<span class="line">   ├── 独立处理各自逻辑</span>
<span class="line">   └── 互不干扰</span>
<span class="line"></span>
<span class="line">3. 延迟注册</span>
<span class="line">   ├── 运行时决定监控内容</span>
<span class="line">   ├── 不同环境不同监控</span>
<span class="line">   └── 方便调试</span>
<span class="line"></span>
<span class="line">4. 实时性</span>
<span class="line">   ├── 数据变化立即通知</span>
<span class="line">   ├── 无需轮询</span>
<span class="line">   └── 高效</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、代理模式-proxy" tabindex="-1"><a class="header-anchor" href="#五、代理模式-proxy"><span>五、代理模式 (Proxy)</span></a></h2><h3 id="_5-1-ghost-shadow-机制" tabindex="-1"><a class="header-anchor" href="#_5-1-ghost-shadow-机制"><span>5.1 Ghost/Shadow 机制</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 实体代理</span></span>
<span class="line"><span class="token comment">// src/entitydef/entity.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// Base 实体 (客户端代理)</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">BaseEntity</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Entity</span></span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">        <span class="token comment">// 远程调用 Cell 方法</span></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">callCellMethod</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> method<span class="token punctuation">,</span> <span class="token keyword">const</span> MemoryStream<span class="token operator">&amp;</span> args<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 通过网络转发到 CellApp</span></span>
<span class="line">            NetworkInterface<span class="token operator">*</span> network <span class="token operator">=</span> <span class="token class-name">NetworkInterface</span><span class="token double-colon punctuation">::</span><span class="token function">instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            network<span class="token operator">-&gt;</span><span class="token function">sendToCell</span><span class="token punctuation">(</span>entityID_<span class="token punctuation">,</span> method<span class="token punctuation">,</span> args<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 属性访问</span></span>
<span class="line">        int32 <span class="token function">getHP</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 返回缓存的值</span></span>
<span class="line">            <span class="token keyword">return</span> cachedHP_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">setHP</span><span class="token punctuation">(</span>int32 value<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            cachedHP_ <span class="token operator">=</span> value<span class="token punctuation">;</span></span>
<span class="line">            <span class="token comment">// 同步到 Cell</span></span>
<span class="line">            <span class="token function">callCellMethod</span><span class="token punctuation">(</span><span class="token string">&quot;setHP&quot;</span><span class="token punctuation">,</span> <span class="token function">MemoryStream</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;&lt;</span> value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">        ENTITY_ID entityID_<span class="token punctuation">;</span></span>
<span class="line">        int32 cachedHP_<span class="token punctuation">;</span>  <span class="token comment">// 缓存的属性值</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// Cell 实体 (真实数据)</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">CellEntity</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Entity</span></span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">        int32 <span class="token function">getHP</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> hp_<span class="token punctuation">;</span>  <span class="token comment">// 真实值</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">setHP</span><span class="token punctuation">(</span>int32 value<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            hp_ <span class="token operator">=</span> value<span class="token punctuation">;</span></span>
<span class="line">            <span class="token comment">// 通知 Base 更新</span></span>
<span class="line">            <span class="token function">notifyBase</span><span class="token punctuation">(</span><span class="token string">&quot;hp&quot;</span><span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token comment">// 通知客户端</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">hasClient</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">sendToClient</span><span class="token punctuation">(</span><span class="token string">&quot;onHPChanged&quot;</span><span class="token punctuation">,</span> value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">        int32 hp_<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-ghost-shadow-设计图" tabindex="-1"><a class="header-anchor" href="#_5-2-ghost-shadow-设计图"><span>5.2 Ghost/Shadow 设计图</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    Ghost/Shadow 架构                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  [客户端]                                                    │</span>
<span class="line">│     │                                                       │</span>
<span class="line">│     ▼                                                       │</span>
<span class="line">│  [Shadow Entity] - 客户端预测                                │</span>
<span class="line">│     ├── 本地计算位置                                         │</span>
<span class="line">│     ├── 立即响应输入                                         │</span>
<span class="line">│     └── 显示给玩家                                           │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼ 校验                              │</span>
<span class="line">│  [Ghost Entity] - BaseApp 代理                               │</span>
<span class="line">│     ├── 缓存状态数据                                         │</span>
<span class="line">│     ├── 转发消息到 Cell                                      │</span>
<span class="line">│     └── 更新客户端                                           │</span>
<span class="line">│                          │                                  │</span>
<span class="line">│                          ▼ 同步                              │</span>
<span class="line">│  [Real Entity] - CellApp 真实实体                            │</span>
<span class="line">│     ├── 真实逻辑状态                                         │</span>
<span class="line">│     ├── 权威数据源                                           │</span>
<span class="line">│     └── 广播状态变化                                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  数据流:                                                     │</span>
<span class="line">│  1. 客户端输入 → Shadow (预测) → 显示                         │</span>
<span class="line">│  2. 客户端输入 → Ghost → Cell → 真实计算                      │</span>
<span class="line">│  3. Cell → Ghost → 客户端 → 校正 Shadow                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-3-为什么要用代理" tabindex="-1"><a class="header-anchor" href="#_5-3-为什么要用代理"><span>5.3 为什么要用代理？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Ghost/Shadow 优势:</span>
<span class="line"></span>
<span class="line">1. 网络延迟隐藏</span>
<span class="line">   ├── Shadow 立即响应输入</span>
<span class="line">   ├── 玩家感觉无延迟</span>
<span class="line">   └── Ghost 异步更新</span>
<span class="line"></span>
<span class="line">2. 减少网络流量</span>
<span class="line">   ├── 只有同步时才传输</span>
<span class="line">   ├── 避免频繁请求</span>
<span class="line">   └── 节省带宽</span>
<span class="line"></span>
<span class="line">3. 安全性</span>
<span class="line">   ├── 客户端只有代理</span>
<span class="line">   ├── 真实逻辑在服务端</span>
<span class="line">   └── 防止作弊</span>
<span class="line"></span>
<span class="line">4. 分布式架构</span>
<span class="line">   ├── Base 处理非实时逻辑</span>
<span class="line">   ├── Cell 处理实时逻辑</span>
<span class="line">   └── 各司其职</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、命令模式-command" tabindex="-1"><a class="header-anchor" href="#六、命令模式-command"><span>六、命令模式 (Command)</span></a></h2><h3 id="_6-1-消息处理" tabindex="-1"><a class="header-anchor" href="#_6-1-消息处理"><span>6.1 消息处理</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 命令模式</span></span>
<span class="line"><span class="token comment">// src/network/bundle.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MessageHandler</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 抽象命令</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">Command</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token operator">~</span><span class="token function">Command</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">execute</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">virtual</span> size_t <span class="token function">getID</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 具体命令</span></span>
<span class="line">    <span class="token keyword">class</span> <span class="token class-name">LoginCommand</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Command</span></span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">        <span class="token keyword">static</span> size_t <span class="token function">ID</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> <span class="token number">100</span><span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">void</span> <span class="token function">execute</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span>string account<span class="token punctuation">,</span> password<span class="token punctuation">;</span></span>
<span class="line">            stream <span class="token operator">&gt;&gt;</span> account <span class="token operator">&gt;&gt;</span> password<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 执行登录逻辑</span></span>
<span class="line">            Account<span class="token operator">*</span> pAccount <span class="token operator">=</span> <span class="token function">AccountLogin</span><span class="token punctuation">(</span>account<span class="token punctuation">,</span> password<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment">// 返回结果</span></span>
<span class="line">            MemoryStream reply<span class="token punctuation">;</span></span>
<span class="line">            reply <span class="token operator">&lt;&lt;</span> pAccount<span class="token operator">-&gt;</span><span class="token function">getID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">sendToClient</span><span class="token punctuation">(</span>reply<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        size_t <span class="token function">getID</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token keyword">override</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> <span class="token function">ID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 命令注册器</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">registerCommand</span><span class="token punctuation">(</span>Command<span class="token operator">*</span> cmd<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        handlers_<span class="token punctuation">[</span>cmd<span class="token operator">-&gt;</span><span class="token function">getID</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">]</span> <span class="token operator">=</span> cmd<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">processMessage</span><span class="token punctuation">(</span>NetworkInterface<span class="token operator">*</span> network<span class="token punctuation">,</span> <span class="token keyword">const</span> Packet<span class="token operator">*</span> packet<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 解析消息头</span></span>
<span class="line">        uint16 msgID<span class="token punctuation">;</span></span>
<span class="line">        MemoryStream <span class="token function">stream</span><span class="token punctuation">(</span>packet<span class="token operator">-&gt;</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> packet<span class="token operator">-&gt;</span><span class="token function">length</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        stream <span class="token operator">&gt;&gt;</span> msgID<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 查找处理器</span></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> handlers_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>msgID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> handlers_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            it<span class="token operator">-&gt;</span>second<span class="token operator">-&gt;</span><span class="token function">execute</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">else</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">ERROR_MSG</span><span class="token punctuation">(</span><span class="token string">&quot;Unknown message ID: {}&quot;</span><span class="token punctuation">,</span> msgID<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span>size_t<span class="token punctuation">,</span> Command<span class="token operator">*</span><span class="token operator">&gt;</span> handlers_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-为什么要用命令模式" tabindex="-1"><a class="header-anchor" href="#_6-2-为什么要用命令模式"><span>6.2 为什么要用命令模式？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">命令模式优势:</span>
<span class="line"></span>
<span class="line">1. 解耦消息处理</span>
<span class="line">   ├── 消息ID和处理逻辑分离</span>
<span class="line">   ├── 易于添加新消息</span>
<span class="line">   └── 便于维护</span>
<span class="line"></span>
<span class="line">2. 可扩展性</span>
<span class="line">   ├── 动态注册命令</span>
<span class="line">   ├── 运行时添加</span>
<span class="line">   └── 热更新支持</span>
<span class="line"></span>
<span class="line">3. 请求封装</span>
<span class="line">   ├── 参数封装在流中</span>
<span class="line">   ├── 统一序列化</span>
<span class="line">   └── 类型安全</span>
<span class="line"></span>
<span class="line">4. 历史记录</span>
<span class="line">   ├── 可记录所有命令</span>
<span class="line">   ├── 用于调试</span>
<span class="line">   └── 用于审计</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、对象池模式-object-pool" tabindex="-1"><a class="header-anchor" href="#七、对象池模式-object-pool"><span>七、对象池模式 (Object Pool)</span></a></h2><h3 id="_7-1-kbengine-内存管理" tabindex="-1"><a class="header-anchor" href="#_7-1-kbengine-内存管理"><span>7.1 KBEngine 内存管理</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 对象池</span></span>
<span class="line"><span class="token comment">// src/lib/helpers/objectpool.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">TYPE</span><span class="token punctuation">,</span> size_t PRE_ALLOC <span class="token operator">=</span> <span class="token number">64</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ObjectPool</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token function">ObjectPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token operator">:</span> <span class="token function">pool_</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">,</span> <span class="token function">numAllocs_</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">,</span> <span class="token function">numFrees_</span><span class="token punctuation">(</span><span class="token number">0</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">~</span><span class="token function">ObjectPool</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取对象</span></span>
<span class="line">    TYPE<span class="token operator">*</span> <span class="token function">alloc</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        TYPE<span class="token operator">*</span> obj <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>freeList_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 池为空，创建新对象</span></span>
<span class="line">            obj <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">TYPE</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token operator">++</span>numAllocs_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">else</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 从池中取</span></span>
<span class="line">            obj <span class="token operator">=</span> freeList_<span class="token punctuation">.</span><span class="token function">back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">pop_back</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> obj<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 释放对象</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">free</span><span class="token punctuation">(</span>TYPE<span class="token operator">*</span> obj<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>obj<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            freeList_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>obj<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token operator">++</span>numFrees_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 清空池</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>TYPE<span class="token operator">*</span> obj <span class="token operator">:</span> freeList_<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">delete</span> obj<span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        freeList_<span class="token punctuation">.</span><span class="token function">clear</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 统计信息</span></span>
<span class="line">    size_t <span class="token function">getNumAllocs</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> numAllocs_<span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line">    size_t <span class="token function">getNumFrees</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> numFrees_<span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line">    size_t <span class="token function">getPoolSize</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> freeList_<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>TYPE<span class="token operator">*</span><span class="token operator">&gt;</span> freeList_<span class="token punctuation">;</span></span>
<span class="line">    size_t numAllocs_<span class="token punctuation">;</span></span>
<span class="line">    size_t numFrees_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line">ObjectPool<span class="token operator">&lt;</span>Packet<span class="token punctuation">,</span> <span class="token number">1024</span><span class="token operator">&gt;</span> packetPool<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">Packet<span class="token operator">*</span> packet <span class="token operator">=</span> packetPool<span class="token punctuation">.</span><span class="token function">alloc</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token comment">// ... 使用 ...</span></span>
<span class="line">packetPool<span class="token punctuation">.</span><span class="token function">free</span><span class="token punctuation">(</span>packet<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-2-为什么要用对象池" tabindex="-1"><a class="header-anchor" href="#_7-2-为什么要用对象池"><span>7.2 为什么要用对象池？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">对象池优势:</span>
<span class="line"></span>
<span class="line">1. 减少内存分配</span>
<span class="line">   ├── 避免频繁 new/delete</span>
<span class="line">   ├── 减少内存碎片</span>
<span class="line">   └── 提高性能</span>
<span class="line"></span>
<span class="line">2. 缓存友好</span>
<span class="line">   ├── 连续内存访问</span>
<span class="line">   ├── 提高 CPU 缓存命中率</span>
<span class="line">   └── 减少缺页中断</span>
<span class="line"></span>
<span class="line">3. 预分配</span>
<span class="line">   ├── 启动时分配</span>
<span class="line">   ├── 避免运行时分配</span>
<span class="line">   └── 减少分配延迟</span>
<span class="line"></span>
<span class="line">4. 可监控</span>
<span class="line">   ├── 知道分配了多少</span>
<span class="line">   ├── 知道池大小</span>
<span class="line">   └── 方便调优</span>
<span class="line"></span>
<span class="line">KBEngine 应用:</span>
<span class="line">- Packet 池</span>
<span class="line">- MemoryStream 池</span>
<span class="line">- Entity 池 (部分)</span>
<span class="line">- Channel 池</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、桥接模式-bridge" tabindex="-1"><a class="header-anchor" href="#八、桥接模式-bridge"><span>八、桥接模式 (Bridge)</span></a></h2><h3 id="_8-1-网络抽象层" tabindex="-1"><a class="header-anchor" href="#_8-1-网络抽象层"><span>8.1 网络抽象层</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 网络桥接</span></span>
<span class="line"><span class="token comment">// src/lib/network/channel.h</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 抽象接口</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token operator">~</span><span class="token function">Channel</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">send</span><span class="token punctuation">(</span><span class="token keyword">const</span> Packet<span class="token operator">*</span> packet<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">virtual</span> Packet<span class="token operator">*</span> <span class="token function">receive</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">isConnected</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    Endpoint<span class="token operator">*</span> pEndpoint_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// TCP 实现</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TCPChannel</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Channel</span></span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">send</span><span class="token punctuation">(</span><span class="token keyword">const</span> Packet<span class="token operator">*</span> packet<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// TCP 发送逻辑</span></span>
<span class="line">        <span class="token keyword">return</span> pEndpoint_<span class="token operator">-&gt;</span><span class="token function">sendtcp</span><span class="token punctuation">(</span>packet<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    Packet<span class="token operator">*</span> <span class="token function">receive</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// TCP 接收逻辑</span></span>
<span class="line">        <span class="token keyword">return</span> pEndpoint_<span class="token operator">-&gt;</span><span class="token function">recvtp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// UDP 实现</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">UDPChannel</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Channel</span></span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">send</span><span class="token punctuation">(</span><span class="token keyword">const</span> Packet<span class="token operator">*</span> packet<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// UDP 发送逻辑</span></span>
<span class="line">        <span class="token keyword">return</span> pEndpoint_<span class="token operator">-&gt;</span><span class="token function">sendudp</span><span class="token punctuation">(</span>packet<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    Packet<span class="token operator">*</span> <span class="token function">receive</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// UDP 接收逻辑</span></span>
<span class="line">        <span class="token keyword">return</span> pEndpoint_<span class="token operator">-&gt;</span><span class="token function">recvudp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 信道管理器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ChannelManager</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    Channel<span class="token operator">*</span> <span class="token function">createChannel</span><span class="token punctuation">(</span>ProtocolType type<span class="token punctuation">,</span> Endpoint<span class="token operator">*</span> endpoint<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>type<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">case</span> PROTOCOL_TCP<span class="token operator">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token keyword">new</span> <span class="token function">TCPChannel</span><span class="token punctuation">(</span>endpoint<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">case</span> PROTOCOL_UDP<span class="token operator">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token keyword">new</span> <span class="token function">UDPChannel</span><span class="token punctuation">(</span>endpoint<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">case</span> PROTOCOL_TCP_UDP<span class="token operator">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token keyword">new</span> <span class="token function">TCPUDPChannel</span><span class="token punctuation">(</span>endpoint<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">default</span><span class="token operator">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_8-2-为什么要用桥接" tabindex="-1"><a class="header-anchor" href="#_8-2-为什么要用桥接"><span>8.2 为什么要用桥接？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">桥接模式优势:</span>
<span class="line"></span>
<span class="line">1. 分离抽象和实现</span>
<span class="line">   ├── Channel 接口不变</span>
<span class="line">   ├── TCP/UDP 可替换</span>
<span class="line">   └── 易于扩展新协议</span>
<span class="line"></span>
<span class="line">2. 运行时选择</span>
<span class="line">   ├── 配置决定协议类型</span>
<span class="line">   ├── 无需重新编译</span>
<span class="line">   └── 灵活部署</span>
<span class="line"></span>
<span class="line">3. 独立变化</span>
<span class="line">   ├── 网络层独立升级</span>
<span class="line">   ├── 应用层不受影响</span>
<span class="line">   └── 降低耦合</span>
<span class="line"></span>
<span class="line">4. 测试友好</span>
<span class="line">   ├── 可以 Mock Channel</span>
<span class="line">   ├── 单元测试方便</span>
<span class="line">   └── 隔离测试</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="九、模板方法模式-template-method" tabindex="-1"><a class="header-anchor" href="#九、模板方法模式-template-method"><span>九、模板方法模式 (Template Method)</span></a></h2><h3 id="_9-1-实体生命周期" tabindex="-1"><a class="header-anchor" href="#_9-1-实体生命周期"><span>9.1 实体生命周期</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 实体生命周期</span></span>
<span class="line"><span class="token comment">// src/server/entitydef/entity.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Entity</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 模板方法：完整生命周期</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">initialize</span><span class="token punctuation">(</span><span class="token keyword">const</span> EntityDef<span class="token operator">&amp;</span> def<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 创建</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">onCreate</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 加载数据</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">onLoad</span><span class="token punctuation">(</span>def<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">onDestroy</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 进入场景</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">onEnterSpace</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">onDestroy</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 完成初始化</span></span>
<span class="line">        <span class="token function">onInitialized</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 销毁流程</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">destroy</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 离开场景</span></span>
<span class="line">        <span class="token function">onLeaveSpace</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 销毁</span></span>
<span class="line">        <span class="token function">onDestroy</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 清理</span></span>
<span class="line">        <span class="token function">onCleanup</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 钩子方法，子类可重写</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">onCreate</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">onLoad</span><span class="token punctuation">(</span><span class="token keyword">const</span> EntityDef<span class="token operator">&amp;</span> def<span class="token punctuation">)</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">onEnterSpace</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onInitialized</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onLeaveSpace</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onDestroy</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onCleanup</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用：Account 实体</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Account</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Entity</span></span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">onCreate</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 账号特定创建逻辑</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token class-name">Entity</span><span class="token double-colon punctuation">::</span><span class="token function">onCreate</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">onLoad</span><span class="token punctuation">(</span><span class="token keyword">const</span> EntityDef<span class="token operator">&amp;</span> def<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 加载账号数据</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token class-name">Entity</span><span class="token double-colon punctuation">::</span><span class="token function">onLoad</span><span class="token punctuation">(</span>def<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_9-2-为什么要用模板方法" tabindex="-1"><a class="header-anchor" href="#_9-2-为什么要用模板方法"><span>9.2 为什么要用模板方法？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">模板方法优势:</span>
<span class="line"></span>
<span class="line">1. 统一流程</span>
<span class="line">   ├── 所有实体相同生命周期</span>
<span class="line">   ├── 保证初始化步骤一致</span>
<span class="line">   └── 避免遗漏</span>
<span class="line"></span>
<span class="line">2. 可扩展</span>
<span class="line">   ├── 子类重写钩子方法</span>
<span class="line">   ├── 不改变主流程</span>
<span class="line">   └── 符合开闭原则</span>
<span class="line"></span>
<span class="line">3. 错误处理</span>
<span class="line">   ├── 统一的错误处理</span>
<span class="line">   ├── 失败自动回滚</span>
<span class="line">   └── 保证一致性</span>
<span class="line"></span>
<span class="line">4. 清晰的职责</span>
<span class="line">   ├── 框架定义流程</span>
<span class="line">   ├── 子类实现细节</span>
<span class="line">   └── 关注点分离</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="十、策略模式-strategy" tabindex="-1"><a class="header-anchor" href="#十、策略模式-strategy"><span>十、策略模式 (Strategy)</span></a></h2><h3 id="_10-1-负载均衡策略" tabindex="-1"><a class="header-anchor" href="#_10-1-负载均衡策略"><span>10.1 负载均衡策略</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 负载均衡策略</span></span>
<span class="line"><span class="token comment">// src/server/baseappmgr/baseappmgr.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LoadBalanceStrategy</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token operator">~</span><span class="token function">LoadBalanceStrategy</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 选择 BaseApp</span></span>
<span class="line">    <span class="token keyword">virtual</span> BaseApp<span class="token operator">*</span> <span class="token function">selectBaseApp</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>BaseApp<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> apps<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 轮询策略</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RoundRobinStrategy</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">LoadBalanceStrategy</span></span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    BaseApp<span class="token operator">*</span> <span class="token function">selectBaseApp</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>BaseApp<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> apps<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        size_t index <span class="token operator">=</span> currentIndex_ <span class="token operator">%</span> apps<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token operator">++</span>currentIndex_<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> apps<span class="token punctuation">[</span>index<span class="token punctuation">]</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    size_t currentIndex_ <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 最少连接策略</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LeastConnectionStrategy</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">LoadBalanceStrategy</span></span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    BaseApp<span class="token operator">*</span> <span class="token function">selectBaseApp</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>BaseApp<span class="token operator">*</span><span class="token operator">&gt;</span><span class="token operator">&amp;</span> apps<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        BaseApp<span class="token operator">*</span> selected <span class="token operator">=</span> <span class="token keyword">nullptr</span><span class="token punctuation">;</span></span>
<span class="line">        size_t minConnections <span class="token operator">=</span> SIZE_MAX<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>BaseApp<span class="token operator">*</span> app <span class="token operator">:</span> apps<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>app<span class="token operator">-&gt;</span><span class="token function">getConnectionCount</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;</span> minConnections<span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">{</span></span>
<span class="line">                minConnections <span class="token operator">=</span> app<span class="token operator">-&gt;</span><span class="token function">getConnectionCount</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                selected <span class="token operator">=</span> app<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> selected<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// BaseAppMgr 使用策略</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BaseAppMgr</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setStrategy</span><span class="token punctuation">(</span>LoadBalanceStrategy<span class="token operator">*</span> strategy<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        strategy_<span class="token punctuation">.</span><span class="token function">reset</span><span class="token punctuation">(</span>strategy<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    BaseApp<span class="token operator">*</span> <span class="token function">findBestBaseApp</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">return</span> strategy_<span class="token operator">-&gt;</span><span class="token function">selectBaseApp</span><span class="token punctuation">(</span>baseApps_<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>LoadBalanceStrategy<span class="token operator">&gt;</span> strategy_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>BaseApp<span class="token operator">*</span><span class="token operator">&gt;</span> baseApps_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_10-2-为什么要用策略模式" tabindex="-1"><a class="header-anchor" href="#_10-2-为什么要用策略模式"><span>10.2 为什么要用策略模式？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">策略模式优势:</span>
<span class="line"></span>
<span class="line">1. 算法可替换</span>
<span class="line">   ├── 不同场景不同策略</span>
<span class="line">   ├── 运行时切换</span>
<span class="line">   └── 无需修改代码</span>
<span class="line"></span>
<span class="line">2. 算法解耦</span>
<span class="line">   ├── 负载均衡逻辑独立</span>
<span class="line">   ├── 易于测试</span>
<span class="line">   └── 易于扩展</span>
<span class="line"></span>
<span class="line">3. 配置化</span>
<span class="line">   ├── 配置文件选择策略</span>
<span class="line">   ├── 无需重新编译</span>
<span class="line">   └── 灵活部署</span>
<span class="line"></span>
<span class="line">4. 性能优化</span>
<span class="line">   ├── 可以针对不同场景优化</span>
<span class="line">   ├── 不影响其他部分</span>
<span class="line">   └── 按需选择</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="十一、责任链模式-chain-of-responsibility" tabindex="-1"><a class="header-anchor" href="#十一、责任链模式-chain-of-responsibility"><span>十一、责任链模式 (Chain of Responsibility)</span></a></h2><h3 id="_11-1-消息路由" tabindex="-1"><a class="header-anchor" href="#_11-1-消息路由"><span>11.1 消息路由</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 消息处理链</span></span>
<span class="line"><span class="token comment">// src/server/components/components.cpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MessageHandler</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">using</span> HandlerFunc <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>function<span class="token operator">&lt;</span><span class="token keyword">bool</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">&amp;</span><span class="token punctuation">)</span><span class="token operator">&gt;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token function">MessageHandler</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">:</span> <span class="token function">next_</span><span class="token punctuation">(</span><span class="token keyword">nullptr</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 设置下一个处理器</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setNext</span><span class="token punctuation">(</span>MessageHandler<span class="token operator">*</span> next<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        next_ <span class="token operator">=</span> next<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理消息</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">handle</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 尝试自己处理</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">handleMessage</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">true</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 传递给下一个</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>next_<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> next_<span class="token operator">-&gt;</span><span class="token function">handle</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 注册处理器</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">registerHandler</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> msgName<span class="token punctuation">,</span> HandlerFunc func<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        handlers_<span class="token punctuation">[</span>msgName<span class="token punctuation">]</span> <span class="token operator">=</span> func<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">protected</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">bool</span> <span class="token function">handleMessage</span><span class="token punctuation">(</span>MemoryStream<span class="token operator">&amp;</span> stream<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string msgName<span class="token punctuation">;</span></span>
<span class="line">        stream <span class="token operator">&gt;&gt;</span> msgName<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">auto</span> it <span class="token operator">=</span> handlers_<span class="token punctuation">.</span><span class="token function">find</span><span class="token punctuation">(</span>msgName<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>it <span class="token operator">!=</span> handlers_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> it<span class="token operator">-&gt;</span><span class="token function">second</span><span class="token punctuation">(</span>stream<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">false</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    MessageHandler<span class="token operator">*</span> next_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> HandlerFunc<span class="token operator">&gt;</span> handlers_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例：构建处理链</span></span>
<span class="line">MessageHandler<span class="token operator">*</span> <span class="token function">buildHandlerChain</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 创建各级处理器</span></span>
<span class="line">    MessageHandler<span class="token operator">*</span> authHandler <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">MessageHandler</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    MessageHandler<span class="token operator">*</span> logicHandler <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">MessageHandler</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    MessageHandler<span class="token operator">*</span> dbHandler <span class="token operator">=</span> <span class="token keyword">new</span> <span class="token function">MessageHandler</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 构建链</span></span>
<span class="line">    authHandler<span class="token operator">-&gt;</span><span class="token function">setNext</span><span class="token punctuation">(</span>logicHandler<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    logicHandler<span class="token operator">-&gt;</span><span class="token function">setNext</span><span class="token punctuation">(</span>dbHandler<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 注册处理器</span></span>
<span class="line">    authHandler<span class="token operator">-&gt;</span><span class="token function">registerHandler</span><span class="token punctuation">(</span><span class="token string">&quot;Login&quot;</span><span class="token punctuation">,</span> handleLogin<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    logicHandler<span class="token operator">-&gt;</span><span class="token function">registerHandler</span><span class="token punctuation">(</span><span class="token string">&quot;Move&quot;</span><span class="token punctuation">,</span> handleMove<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    dbHandler<span class="token operator">-&gt;</span><span class="token function">registerHandler</span><span class="token punctuation">(</span><span class="token string">&quot;Save&quot;</span><span class="token punctuation">,</span> handleSave<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">return</span> authHandler<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_11-2-为什么要用责任链" tabindex="-1"><a class="header-anchor" href="#_11-2-为什么要用责任链"><span>11.2 为什么要用责任链？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">责任链优势:</span>
<span class="line"></span>
<span class="line">1. 灵活处理流程</span>
<span class="line">   ├── 可以动态调整处理顺序</span>
<span class="line">   ├── 添加/删除处理器</span>
<span class="line">   └── 不影响其他部分</span>
<span class="line"></span>
<span class="line">2. 职责分离</span>
<span class="line">   ├── 每个处理器专注自己的职责</span>
<span class="line">   ├── 验证 → 逻辑 → 持久化</span>
<span class="line">   └── 符合单一职责原则</span>
<span class="line"></span>
<span class="line">3. 可扩展</span>
<span class="line">   ├── 新增处理器很容易</span>
<span class="line">   ├── 组合不同链</span>
<span class="line">   └── 支持复杂业务</span>
<span class="line"></span>
<span class="line">4. 中断控制</span>
<span class="line">   ├── 处理成功可以中断</span>
<span class="line">   ├── 继续传递给下一个</span>
<span class="line">   └── 灵活控制流程</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="十二、组合模式-composite" tabindex="-1"><a class="header-anchor" href="#十二、组合模式-composite"><span>十二、组合模式 (Composite)</span></a></h2><h3 id="_12-1-空间管理" tabindex="-1"><a class="header-anchor" href="#_12-1-空间管理"><span>12.1 空间管理</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 空间组合模式</span></span>
<span class="line"><span class="token comment">// src/server/cellapp/space.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Space</span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token operator">~</span><span class="token function">Space</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 添加子空间</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">addSpace</span><span class="token punctuation">(</span>Space<span class="token operator">*</span> space<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 基础空间不实现</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 移除子空间</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">removeSpace</span><span class="token punctuation">(</span>Space<span class="token operator">*</span> space<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 基础空间不实现</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 更新空间</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token keyword">float</span> delta<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 更新自身</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 复合空间</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CompositeSpace</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Space</span></span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">addSpace</span><span class="token punctuation">(</span>Space<span class="token operator">*</span> space<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        children_<span class="token punctuation">.</span><span class="token function">push_back</span><span class="token punctuation">(</span>space<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">removeSpace</span><span class="token punctuation">(</span>Space<span class="token operator">*</span> space<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        children_<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span></span>
<span class="line">            std<span class="token double-colon punctuation">::</span><span class="token function">remove</span><span class="token punctuation">(</span>children_<span class="token punctuation">.</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> children_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> space<span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            children_<span class="token punctuation">.</span><span class="token function">end</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token keyword">float</span> delta<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 更新所有子空间</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span>Space<span class="token operator">*</span> child <span class="token operator">:</span> children_<span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">{</span></span>
<span class="line">            child<span class="token operator">-&gt;</span><span class="token function">update</span><span class="token punctuation">(</span>delta<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 更新自身</span></span>
<span class="line">        <span class="token function">updateSelf</span><span class="token punctuation">(</span>delta<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">updateSelf</span><span class="token punctuation">(</span><span class="token keyword">float</span> delta<span class="token punctuation">)</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 自身更新逻辑</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Space<span class="token operator">*</span><span class="token operator">&gt;</span> children_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 叶子空间（实际游戏空间）</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellSpace</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">Space</span></span></span>
<span class="line"><span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token keyword">float</span> delta<span class="token punctuation">)</span> <span class="token keyword">override</span></span>
<span class="line">    <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 更新空间内的实体</span></span>
<span class="line">        <span class="token function">updateEntities</span><span class="token punctuation">(</span>delta<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Entity<span class="token operator">*</span><span class="token operator">&gt;</span> entities_<span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_12-2-为什么要用组合模式" tabindex="-1"><a class="header-anchor" href="#_12-2-为什么要用组合模式"><span>12.2 为什么要用组合模式？</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">组合模式优势:</span>
<span class="line"></span>
<span class="line">1. 树形结构</span>
<span class="line">   ├── 支持嵌套空间</span>
<span class="line">   ├── 递归操作</span>
<span class="line">   └── 简化复杂结构</span>
<span class="line"></span>
<span class="line">2. 统一接口</span>
<span class="line">   ├── 单个空间和组合空间相同接口</span>
<span class="line">   ├── 客户端无需区分</span>
<span class="line">   └── 使用简单</span>
<span class="line"></span>
<span class="line">3. 易于扩展</span>
<span class="line">   ├── 添加新类型空间很容易</span>
<span class="line">   ├── 不影响现有代码</span>
<span class="line">   └── 符合开闭原则</span>
<span class="line"></span>
<span class="line">4. 递归处理</span>
<span class="line">   ├── 更新自动传播到子节点</span>
<span class="line">   ├── 自动管理生命周期</span>
<span class="line">   └── 减少手动管理</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="十三、设计权衡" tabindex="-1"><a class="header-anchor" href="#十三、设计权衡"><span>十三、设计权衡</span></a></h2><h3 id="_13-1-kbengine-设计哲学" tabindex="-1"><a class="header-anchor" href="#_13-1-kbengine-设计哲学"><span>13.1 KBEngine 设计哲学</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│              KBEngine 设计权衡与哲学                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  简单性 vs 灵活性:                                           │</span>
<span class="line">│  ├── 选择：偏向简单性                                        │</span>
<span class="line">│  ├── 固定架构，约定优于配置                                   │</span>
<span class="line">│  ├── 降低学习成本                                           │</span>
<span class="line">│  └── 适合快速开发                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  性能 vs 可维护性:                                           │</span>
<span class="line">│  ├── Python 脚本牺牲性能                                     │</span>
<span class="line">│  ├── 换取开发效率和可维护性                                  │</span>
<span class="line">│  ├── 热更新成为可能                                         │</span>
<span class="line">│  └── 降低业务逻辑复杂度                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  单体 vs 分布式:                                             │</span>
<span class="line">│  ├── 固定组件的单体分布                                      │</span>
<span class="line">│  ├── 组件间明确职责                                          │</span>
<span class="line">│  ├── 避免过度拆分                                           │</span>
<span class="line">│  └── 保持架构可控                                           │</span>
<span class="line">│                                                             │</span>
<span class="line">│  同步 vs 异步:                                               │</span>
<span class="line">│  ├── 单线程 Actor 模型                                      │</span>
<span class="line">│  ├── 避免锁竞争                                             │</span>
<span class="line">│  ├── 消息异步处理                                           │</span>
<span class="line">│  └── 简化并发                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_13-2-适用场景分析" tabindex="-1"><a class="header-anchor" href="#_13-2-适用场景分析"><span>13.2 适用场景分析</span></a></h3><table><thead><tr><th>设计模式</th><th>KBEngine 应用</th><th>适用原因</th><th>代价</th></tr></thead><tbody><tr><td>Singleton</td><td>组件管理器</td><td>全局唯一，访问方便</td><td>测试困难，隐式依赖</td></tr><tr><td>Factory</td><td>实体创建</td><td>解耦类型选择</td><td>工厂类变复杂</td></tr><tr><td>Observer</td><td>Watcher</td><td>解耦监控</td><td>运行时开销</td></tr><tr><td>Proxy</td><td>Ghost/Shadow</td><td>网络优化</td><td>复杂度增加</td></tr><tr><td>Command</td><td>消息处理</td><td>解耦消息逻辑</td><td>类数量增加</td></tr><tr><td>Object Pool</td><td>内存管理</td><td>性能优化</td><td>内存占用</td></tr><tr><td>Bridge</td><td>网络层</td><td>协议抽象</td><td>间接层</td></tr><tr><td>Template Method</td><td>生命周期</td><td>流程统一</td><td>钩子限制</td></tr><tr><td>Strategy</td><td>负载均衡</td><td>算法可换</td><td>策略类增加</td></tr><tr><td>Chain of Responsibility</td><td>消息路由</td><td>灵活处理</td><td>调试困难</td></tr><tr><td>Composite</td><td>空间管理</td><td>树形结构</td><td>递归复杂性</td></tr></tbody></table><hr><h2 id="十四、总结" tabindex="-1"><a class="header-anchor" href="#十四、总结"><span>十四、总结</span></a></h2><h3 id="kbengine-设计原则" tabindex="-1"><a class="header-anchor" href="#kbengine-设计原则"><span>KBEngine 设计原则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 设计 = 简单 + 实用 + 可扩展</span>
<span class="line"></span>
<span class="line">核心原则:</span>
<span class="line"></span>
<span class="line">1. 约定优于配置</span>
<span class="line">   ├── 固定的组件结构</span>
<span class="line">   ├── 清晰的职责划分</span>
<span class="line">   └── 降低理解成本</span>
<span class="line"></span>
<span class="line">2. 单一职责</span>
<span class="line">   ├── 每个组件做一件事</span>
<span class="line">   ├── BaseApp: 玩家数据</span>
<span class="line">   ├── CellApp: 游戏逻辑</span>
<span class="line">   └── DBMgr: 数据存储</span>
<span class="line"></span>
<span class="line">3. 脚本优先</span>
<span class="line">   ├── C++ 核心框架</span>
<span class="line">   ├── Python 业务逻辑</span>
<span class="line">   └── 热更新支持</span>
<span class="line"></span>
<span class="line">4. 渐进优化</span>
<span class="line">   ├── 不过早优化</span>
<span class="line">   ├── 先保证正确</span>
<span class="line">   └── 后优化性能</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="学习建议" tabindex="-1"><a class="header-anchor" href="#学习建议"><span>学习建议</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">理解 KBEngine 设计:</span>
<span class="line"></span>
<span class="line">1. 阅读源码</span>
<span class="line">   ├── src/server/ 核心逻辑</span>
<span class="line">   ├── src/lib/ 基础组件</span>
<span class="line">   └── scripts/ 业务实现</span>
<span class="line"></span>
<span class="line">2. 理解模式应用</span>
<span class="line">   ├── 识别每个模式的作用</span>
<span class="line">   ├── 理解为什么用这个模式</span>
<span class="line">   └── 思考替代方案</span>
<span class="line"></span>
<span class="line">3. 对比其他框架</span>
<span class="line">   ├── Skynet (Actor 模型)</span>
<span class="line">   ├── Pomelo (Node.js)</span>
<span class="line">   └── 取长补短</span>
<span class="line"></span>
<span class="line">4. 实践应用</span>
<span class="line">   ├── 模仿设计</span>
<span class="line">   ├── 改进实现</span>
<span class="line">   └── 总结经验</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine GitHub</a></li><li><a href="https://www.amazon.com/Design-Patterns-Elements-Reusable-Object-Oriented/dp/0201633612" target="_blank" rel="noopener noreferrer">Design Patterns: Elements of Reusable Object-Oriented Software</a></li><li><a href="https://www.gameprogrammingpatterns.com/" target="_blank" rel="noopener noreferrer">Game Programming Patterns</a></li></ul>`,92)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};