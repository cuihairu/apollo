import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/BigWorld%E6%9E%B6%E6%9E%84%E6%B7%B1%E5%BA%A6%E8%A7%A3%E6%9E%90.html","title":"BigWorld 架构深度解析","lang":"en-US","frontmatter":{},"filePathRelative":"BigWorld架构深度解析.md","git":{"createdTime":1773827303000,"updatedTime":1773827303000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`BigWorld架构深度解析.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="bigworld-架构深度解析" tabindex="-1"><a class="header-anchor" href="#bigworld-架构深度解析"><span>BigWorld 架构深度解析</span></a></h1><h2 id="目录" tabindex="-1"><a class="header-anchor" href="#目录"><span>目录</span></a></h2><ul><li><a href="#%E4%BB%80%E4%B9%88%E6%98%AF-bigworld">什么是 BigWorld</a></li><li><a href="#bigworld-%E6%A0%B8%E5%BF%83%E6%A6%82%E5%BF%B5">BigWorld 核心概念</a></li><li><a href="#bigworld-vs-%E4%BC%A0%E7%BB%9F%E6%9E%B6%E6%9E%84%E5%AF%B9%E6%AF%94">BigWorld vs 传统架构对比</a></li><li><a href="#bigworld-%E6%A0%B8%E5%BF%83%E7%BB%84%E4%BB%B6">BigWorld 核心组件</a></li><li><a href="#bigworld-%E7%9A%84%E7%89%B9%E7%82%B9">BigWorld 的特点</a></li><li><a href="#%E9%80%82%E7%94%A8%E5%9C%BA%E6%99%AF">适用场景</a></li><li><a href="#bigworld-%E5%9C%A8-apollo-%E4%B8%AD%E7%9A%84%E5%85%BC%E5%AE%B9%E5%B1%82">BigWorld 在 Apollo 中的兼容层</a></li></ul><hr><h2 id="一、什么是-bigworld" tabindex="-1"><a class="header-anchor" href="#一、什么是-bigworld"><span>一、什么是 BigWorld？</span></a></h2><p><strong>BigWorld</strong> 是一个专为 MMO 设计的服务器引擎技术，最著名的应用是 Wargaming 的《坦克世界》、《战舰世界》。</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">与传统游戏服务器对比:</span>
<span class="line"></span>
<span class="line">传统MMO架构:           BigWorld架构:</span>
<span class="line">┌─────────┐            ┌─────────────────────────┐</span>
<span class="line">│  Gate   │            │      CellApp (单元格)   │</span>
<span class="line">├─────────┤            ├─────────────────────────┤</span>
<span class="line">│  Logic  │            │      BaseApp (数据库)   │</span>
<span class="line">├─────────┤            ├─────────────────────────┤</span>
<span class="line">│  DB     │            │     LoginApp (登录)     │</span>
<span class="line">└─────────┘            │     ChatApp (聊天)      │</span>
<span class="line">                        │      ... (专用服务)      │</span>
<span class="line">                        └─────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、bigworld-核心概念" tabindex="-1"><a class="header-anchor" href="#二、bigworld-核心概念"><span>二、BigWorld 核心概念</span></a></h2><h3 id="_1-空间分割架构-cellapps" tabindex="-1"><a class="header-anchor" href="#_1-空间分割架构-cellapps"><span>1. 空间分割架构 (CellApps)</span></a></h3><p>BigWorld 将游戏世界划分为<strong>空间单元</strong>，每个单元由独立的 <strong>CellApp</strong> 进程管理。</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">世界地图按区域划分:</span>
<span class="line">┌────────┬────────┬────────┬────────┐</span>
<span class="line">│CellApp0│CellApp1│CellApp2│CellApp3│</span>
<span class="line">│  区域A  │  区域B  │  区域C  │  区域D  │</span>
<span class="line">├────────┼────────┼────────┼────────┤</span>
<span class="line">│CellApp4│CellApp5│CellApp6│CellApp7│</span>
<span class="line">│  区域E  │  区域F  │  区域G  │  区域H  │</span>
<span class="line">└────────┴────────┴────────┴────────┘</span>
<span class="line"></span>
<span class="line">玩家跨区域移动时:</span>
<span class="line">Zone A ──(无缝迁移)──&gt; Zone B</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>特点：</strong></p><ul><li>每个区域独立进程，<strong>故障隔离</strong></li><li>负载均衡：热点区域可独立扩展</li><li><strong>无缝世界</strong>：玩家感觉不到边界</li></ul><h3 id="_2-实体-组件分离-entity-real-shadow" tabindex="-1"><a class="header-anchor" href="#_2-实体-组件分离-entity-real-shadow"><span>2. 实体-组件分离 (Entity + Real/Shadow)</span></a></h3><p>BigWorld 引入了 <strong>Real</strong> 和 <strong>Shadow</strong> 实体的概念：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// Real Entity - 真实的、有状态的服务器端实体</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">RealEntity</span> <span class="token punctuation">{</span></span>
<span class="line">    Position position<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int</span> health<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>Spell<span class="token operator">&gt;</span> activeSpells<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">update</span><span class="token punctuation">(</span><span class="token keyword">float</span> dt<span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 每帧更新逻辑</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// Shadow Entity - 影子实体，只做状态同步</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ShadowEntity</span> <span class="token punctuation">{</span></span>
<span class="line">    EntityID id<span class="token punctuation">;</span></span>
<span class="line">    Position position<span class="token punctuation">;</span>     <span class="token comment">// 只同步位置</span></span>
<span class="line">    <span class="token keyword">int</span> health<span class="token punctuation">;</span>            <span class="token comment">// 只同步血量</span></span>
<span class="line">    <span class="token comment">// 没有逻辑，只是数据容器</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>设计理念：</strong></p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">在本地区域的玩家 → Real Entity (有逻辑)</span>
<span class="line">在远处区域看到的玩家 → Shadow Entity (只同步)</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-唯一id系统" tabindex="-1"><a class="header-anchor" href="#_3-唯一id系统"><span>3. 唯一ID系统</span></a></h3><p>BigWorld 使用 <strong>64位唯一ID</strong> 标识所有实体：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">struct</span> <span class="token class-name">EntityID</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> id<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 格式: [类型(8位) | 服务器ID(16位) | 实体索引(40位)]</span></span>
<span class="line">    <span class="token keyword">uint8_t</span>  type<span class="token punctuation">;</span>      <span class="token comment">// Entity/Player/Monster等</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> serverId<span class="token punctuation">;</span>  <span class="token comment">// 来源服务器</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> index<span class="token punctuation">;</span>     <span class="token comment">// 本地索引</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、bigworld-vs-传统架构对比" tabindex="-1"><a class="header-anchor" href="#三、bigworld-vs-传统架构对比"><span>三、BigWorld vs 传统架构对比</span></a></h2><h3 id="对比表" tabindex="-1"><a class="header-anchor" href="#对比表"><span>对比表</span></a></h3><table><thead><tr><th>特性</th><th>传统MMO架构</th><th>BigWorld架构</th></tr></thead><tbody><tr><td><strong>世界划分</strong></td><td>单一世界/分线</td><td>空间分割 CellApps</td></tr><tr><td><strong>负载均衡</strong></td><td>玩家数量分线</td><td>按区域动态迁移</td></tr><tr><td><strong>扩展方式</strong></td><td>垂直扩展</td><td>水平扩展</td></tr><tr><td><strong>故障影响</strong></td><td>整个世界/分线</td><td>只影响单个区域</td></tr><tr><td><strong>无缝切换</strong></td><td>需要切换场景</td><td>真正的无缝世界</td></tr><tr><td><strong>实体管理</strong></td><td>集中式</td><td>分布式 + Ghost模式</td></tr><tr><td><strong>位置同步</strong></td><td>AOI九宫格</td><td>CellApps间协商</td></tr><tr><td><strong>数据持久化</strong></td><td>定时保存</td><td>BaseApp 专用服务</td></tr></tbody></table><hr><h2 id="四、bigworld-核心组件" tabindex="-1"><a class="header-anchor" href="#四、bigworld-核心组件"><span>四、BigWorld 核心组件</span></a></h2><h3 id="_1-cellapp-游戏逻辑服务器" tabindex="-1"><a class="header-anchor" href="#_1-cellapp-游戏逻辑服务器"><span>1. CellApp - 游戏逻辑服务器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">class</span> <span class="token class-name">CellApp</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 管理一个空间区域</span></span>
<span class="line">    Rect bounds<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>RealEntity<span class="token operator">*</span><span class="token operator">&gt;</span> entities<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 跨区域通信</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onEntityEnter</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> e<span class="token punctuation">,</span> CellApp<span class="token operator">*</span> from<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onEntityLeave</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> e<span class="token punctuation">,</span> CellApp<span class="token operator">*</span> to<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 与相邻 CellApp 同步边界实体</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">syncGhosts</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 关键：区域交接</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token class-name">CellApp</span><span class="token double-colon punctuation">::</span><span class="token function">handoverEntity</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> e<span class="token punctuation">,</span> Position newPos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>myBounds<span class="token punctuation">.</span><span class="token function">contains</span><span class="token punctuation">(</span>newPos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        CellApp<span class="token operator">*</span> neighbor <span class="token operator">=</span> <span class="token function">findNeighbor</span><span class="token punctuation">(</span>newPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        neighbor<span class="token operator">-&gt;</span><span class="token function">receiveEntity</span><span class="token punctuation">(</span>e<span class="token punctuation">,</span> <span class="token keyword">this</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 迁移实体</span></span>
<span class="line">        entities<span class="token punctuation">.</span><span class="token function">erase</span><span class="token punctuation">(</span>e<span class="token operator">-&gt;</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-baseapp-数据库服务器" tabindex="-1"><a class="header-anchor" href="#_2-baseapp-数据库服务器"><span>2. BaseApp - 数据库服务器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">class</span> <span class="token class-name">BaseApp</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 专门处理数据库操作</span></span>
<span class="line">    DatabaseConnection db<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 异步加载数据</span></span>
<span class="line">    Future<span class="token operator">&lt;</span>PlayerData<span class="token operator">&gt;</span> <span class="token function">loadPlayer</span><span class="token punctuation">(</span><span class="token keyword">uint64_t</span> playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 定时保存</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">savePlayer</span><span class="token punctuation">(</span>PlayerData<span class="token operator">&amp;</span> data<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 不处理游戏逻辑，只做数据CRUD</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>设计理念：数据库IO与应用逻辑分离</strong></p><h3 id="_3-loginapp-登录服务器" tabindex="-1"><a class="header-anchor" href="#_3-loginapp-登录服务器"><span>3. LoginApp - 登录服务器</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">class</span> <span class="token class-name">LoginApp</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 处理认证</span></span>
<span class="line">    AuthManager auth<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 分配玩家到合适的 CellApp</span></span>
<span class="line">    CellApp<span class="token operator">*</span> <span class="token function">assignCellApp</span><span class="token punctuation">(</span>Player<span class="token operator">*</span> p<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 创建 Proxy</span></span>
<span class="line">    Proxy<span class="token operator">*</span> <span class="token function">createProxy</span><span class="token punctuation">(</span>Player<span class="token operator">*</span> p<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-proxy-玩家连接代理" tabindex="-1"><a class="header-anchor" href="#_4-proxy-玩家连接代理"><span>4. Proxy - 玩家连接代理</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">class</span> <span class="token class-name">Proxy</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 代表玩家与服务器通信</span></span>
<span class="line">    NetworkConnection connection<span class="token punctuation">;</span></span>
<span class="line">    EntityID controlledEntity<span class="token punctuation">;</span>  <span class="token comment">// 控制的实体ID</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 转发客户端消息到 CellApp</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">forwardToCell</span><span class="token punctuation">(</span>ClientMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 从 CellApp 接收更新</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendToClient</span><span class="token punctuation">(</span>EntityUpdate<span class="token operator">&amp;</span> update<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、bigworld-的特点" tabindex="-1"><a class="header-anchor" href="#五、bigworld-的特点"><span>五、BigWorld 的特点</span></a></h2><h3 id="✅-优点" tabindex="-1"><a class="header-anchor" href="#✅-优点"><span>✅ 优点</span></a></h3><h4 id="_1-真正的无缝世界" tabindex="-1"><a class="header-anchor" href="#_1-真正的无缝世界"><span>1. 真正的无缝世界</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">玩家从地图一端走到另一端:</span>
<span class="line">传统: ┌────┐ ┌────┐ ┌────┐  需要加载/切换场景</span>
<span class="line">      │地图1│→│地图2│→│地图3│</span>
<span class="line">      └────┘ └────┘ └────┘</span>
<span class="line"></span>
<span class="line">BigWorld: ┌─────────────────────────┐</span>
<span class="line">           │   单一连续世界          │  玩家感觉不到边界</span>
<span class="line">           │                         │</span>
<span class="line">           └─────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="_2-水平扩展能力强" tabindex="-1"><a class="header-anchor" href="#_2-水平扩展能力强"><span>2. 水平扩展能力强</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">热点区域(如主城)压力大:</span>
<span class="line">传统: 整个分线都卡</span>
<span class="line">BigWorld: 只给主城区域加 CellApp</span>
<span class="line">          ┌───────────────┐</span>
<span class="line">          │主城:3个CellApp│  独立扩展</span>
<span class="line">          └───────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="_3-故障隔离" tabindex="-1"><a class="header-anchor" href="#_3-故障隔离"><span>3. 故障隔离</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">一个 CellApp 崩溃:</span>
<span class="line">影响范围: 只有一个区域的玩家掉线</span>
<span class="line">其他区域: 正常运行</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="_4-负载均衡" tabindex="-1"><a class="header-anchor" href="#_4-负载均衡"><span>4. 负载均衡</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">玩家自动迁移到低负载 CellApp:</span>
<span class="line">CellApp A (1000人) ──→── CellApp B (300人)</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="❌-缺点" tabindex="-1"><a class="header-anchor" href="#❌-缺点"><span>❌ 缺点</span></a></h3><h4 id="_1-复杂度高" tabindex="-1"><a class="header-anchor" href="#_1-复杂度高"><span>1. 复杂度高</span></a></h4><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 跨边界实体同步逻辑复杂</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">updateEntityAtBorder</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> e<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 1. 检查是否在边界</span></span>
<span class="line">    <span class="token comment">// 2. 通知相邻 CellApp</span></span>
<span class="line">    <span class="token comment">// 3. 创建 Ghost 实体</span></span>
<span class="line">    <span class="token comment">// 4. 同步状态</span></span>
<span class="line">    <span class="token comment">// 5. 处理边界碰撞</span></span>
<span class="line">    <span class="token comment">// ... 100+ 行代码</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="_2-一致性问题" tabindex="-1"><a class="header-anchor" href="#_2-一致性问题"><span>2. 一致性问题</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">问题: 玩家在边界附近</span>
<span class="line">       CellApp A 认为在左边</span>
<span class="line">       CellApp B 认为在右边</span>
<span class="line"></span>
<span class="line">需要: 复杂的协商机制</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="_3-调试困难" tabindex="-1"><a class="header-anchor" href="#_3-调试困难"><span>3. 调试困难</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">一个 bug 涉及多个 CellApp:</span>
<span class="line">- 无法单步调试</span>
<span class="line">- 日志分散在多个进程</span>
<span class="line">- 时序问题难以重现</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="_4-边界处理特殊" tabindex="-1"><a class="header-anchor" href="#_4-边界处理特殊"><span>4. 边界处理特殊</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">边界附近的技能/碰撞需要特殊处理:</span>
<span class="line">- 技能跨越边界</span>
<span class="line">- 子弹跨越边界</span>
<span class="line">- NPC巡逻跨越边界</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、cellapp-水平扩展详解" tabindex="-1"><a class="header-anchor" href="#六、cellapp-水平扩展详解"><span>六、CellApp 水平扩展详解</span></a></h2><h3 id="核心思想-按位置-空间-拆分" tabindex="-1"><a class="header-anchor" href="#核心思想-按位置-空间-拆分"><span>核心思想：按位置（空间）拆分</span></a></h3><p>BigWorld 的水平扩展<strong>核心就是按位置（空间分割）</strong>。</p><h3 id="传统-vs-bigworld-扩展方式对比" tabindex="-1"><a class="header-anchor" href="#传统-vs-bigworld-扩展方式对比"><span>传统 vs BigWorld 扩展方式对比</span></a></h3><h4 id="传统mmo-垂直扩展-分线复制" tabindex="-1"><a class="header-anchor" href="#传统mmo-垂直扩展-分线复制"><span>传统MMO：垂直扩展 + 分线复制</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────┐</span>
<span class="line">│          游戏世界 (完整副本)             │</span>
<span class="line">│                                          │</span>
<span class="line">│  🏰城堡──────────────🌲森林             │</span>
<span class="line">│                          🏠村庄          │</span>
<span class="line">│                                          │</span>
<span class="line">└─────────────────────────────────────────┘</span>
<span class="line">         ↓ 复制整个世界</span>
<span class="line">┌─────────────────────────────────────────┐</span>
<span class="line">│          分线1 (完整副本)                 │</span>
<span class="line">│  🏰城堡──────────────🌲森林             │</span>
<span class="line">│                          🏠村庄          │</span>
<span class="line">└─────────────────────────────────────────┘</span>
<span class="line">┌─────────────────────────────────────────┐</span>
<span class="line">│          分线2 (完整副本)                 │</span>
<span class="line">│  🏰城堡──────────────🌲森林             │</span>
<span class="line">│                          🏠村庄          │</span>
<span class="line">└─────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">扩展方式 = 加更多服务器，每个服务器跑完整世界</span>
<span class="line">问题 = 每个分线是独立的，玩家无法跨分线交互</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="bigworld-空间水平切割" tabindex="-1"><a class="header-anchor" href="#bigworld-空间水平切割"><span>BigWorld：空间水平切割</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────┐</span>
<span class="line">│                  单一游戏世界                        │</span>
<span class="line">│                                                     │</span>
<span class="line">│   ┌─────────┬─────────┬─────────┬─────────┐        │</span>
<span class="line">│   │CellApp0 │CellApp1 │CellApp2 │CellApp3 │        │</span>
<span class="line">│   │ 雪山区域 │ 城堡区域│ 森林区域│ 村庄区域│        │</span>
<span class="line">│   │  500人  │ 2000人  │  800人  │ 1200人  │        │</span>
<span class="line">│   ├─────────┼─────────┼─────────┼─────────┤        │</span>
<span class="line">│   │CellApp4 │CellApp5 │CellApp6 │CellApp7 │        │</span>
<span class="line">│   │ 荒原区域│ 主城区域│ 沙漠区域│ 海域区域│        │</span>
<span class="line">│   │  300人  │ 5000人⭐│  600人  │  400人  │        │</span>
<span class="line">│   └─────────┴─────────┴─────────┴─────────┘        │</span>
<span class="line">│                                                     │</span>
<span class="line">└─────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">⭐ 主城人多 = 可以单独为主城加更多服务器资源</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="动态负载均衡" tabindex="-1"><a class="header-anchor" href="#动态负载均衡"><span>动态负载均衡</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">主城区域玩家过多:</span>
<span class="line">┌────────────────────────────┐</span>
<span class="line">│   CellApp5 (主城)          │</span>
<span class="line">│   💥 5000玩家 = 拥挤      │</span>
<span class="line">└────────────────────────────┘</span>
<span class="line">          ↓ 自动分割</span>
<span class="line">┌───────────────┬───────────────┐</span>
<span class="line">│ CellApp5a     │ CellApp5b     │</span>
<span class="line">│ 西半主城 2500 │ 东半主城 2500 │</span>
<span class="line">└───────────────┴───────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>关键</strong>：分割对玩家<strong>透明</strong>！玩家感觉不到自己被分配到了不同的 CellApp。</p><hr><h2 id="七、跨边界处理" tabindex="-1"><a class="header-anchor" href="#七、跨边界处理"><span>七、跨边界处理</span></a></h2><h3 id="_1-玩家跨-cellapp-移动" tabindex="-1"><a class="header-anchor" href="#_1-玩家跨-cellapp-移动"><span>1. 玩家跨 CellApp 移动</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">玩家从 CellApp0 移动到 CellApp1:</span>
<span class="line"></span>
<span class="line">CellApp0                  CellApp1</span>
<span class="line">   │                          │</span>
<span class="line">   │ 1. 玩家接近边界          │</span>
<span class="line">   ├──────────────────────────&gt;│</span>
<span class="line">   │ 2. 创建 Shadow 实体       │</span>
<span class="line">   │                          │</span>
<span class="line">   │ 3. 转移 Real 实体控制权  │</span>
<span class="line">   │&lt;──────────────────────────┤</span>
<span class="line">   │ 4. 删除 Shadow 实体       │</span>
<span class="line">   ▼                          ▼</span>
<span class="line">完成迁移，玩家感觉无感</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-边界交互" tabindex="-1"><a class="header-anchor" href="#_2-边界交互"><span>2. 边界交互</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">两个玩家在边界附近开战:</span>
<span class="line"></span>
<span class="line">┌──────────────┬──────────────┐</span>
<span class="line">│  CellApp0    │  CellApp1    │</span>
<span class="line">│              │              │</span>
<span class="line">│   🧙‍♂️法师 ←───→🏹射手       │  跨边界攻击</span>
<span class="line">│              │              │</span>
<span class="line">└──────────────┴──────────────┘</span>
<span class="line"></span>
<span class="line">处理方式:</span>
<span class="line">1. 射手在 CellApp1 创建 法师的 Shadow</span>
<span class="line">2. 法师在 CellApp0 创建 射手的 Shadow</span>
<span class="line">3. 伤害计算在各自 CellApp</span>
<span class="line">4. 通过消息同步结果</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、适用场景" tabindex="-1"><a class="header-anchor" href="#八、适用场景"><span>八、适用场景</span></a></h2><h3 id="bigworld-适合的场景-✅" tabindex="-1"><a class="header-anchor" href="#bigworld-适合的场景-✅"><span>BigWorld 适合的场景 ✅</span></a></h3><table><thead><tr><th>场景</th><th>原因</th></tr></thead><tbody><tr><td><strong>大型开放世界MMO</strong></td><td>无缝世界体验</td></tr><tr><td><strong>车辆/战车游戏</strong></td><td>载具跨区域移动</td></tr><tr><td><strong>高并发热点</strong></td><td>主城可独立扩展</td></tr><tr><td><strong>长期运营</strong></td><td>模块化便于维护</td></tr></tbody></table><h3 id="bigworld-不适合的场景-❌" tabindex="-1"><a class="header-anchor" href="#bigworld-不适合的场景-❌"><span>BigWorld 不适合的场景 ❌</span></a></h3><table><thead><tr><th>场景</th><th>原因</th></tr></thead><tbody><tr><td><strong>副本/闯关</strong></td><td>不需要复杂的空间分割</td></tr><tr><td><strong>回合制卡牌</strong></td><td>无需位置同步</td></tr><tr><td><strong>小规模游戏</strong></td><td>架构过于复杂</td></tr><tr><td><strong>强一致性要求</strong></td><td>分布式一致性难保证</td></tr></tbody></table><hr><h2 id="九、bigworld-在-apollo-中的兼容层" tabindex="-1"><a class="header-anchor" href="#九、bigworld-在-apollo-中的兼容层"><span>九、BigWorld 在 Apollo 中的兼容层</span></a></h2><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// modules/bigworld/include/apollo/bigworld/runtime.hpp</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">namespace</span> BigWorld <span class="token punctuation">{</span>  <span class="token comment">// 全局命名空间兼容</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 实体ID兼容</span></span>
<span class="line">    <span class="token keyword">using</span> EntityID <span class="token operator">=</span> <span class="token keyword">uint64_t</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">using</span> SpaceID <span class="token operator">=</span> <span class="token keyword">uint32_t</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 实体接口</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">IEntity</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">virtual</span> SpaceID <span class="token function">spaceID</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">virtual</span> Position <span class="token function">position</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">teleport</span><span class="token punctuation">(</span>Position pos<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">destroy</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 回调接口</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">ICallback</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onEntityEnter</span><span class="token punctuation">(</span>IEntity<span class="token operator">*</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onEntityLeave</span><span class="token punctuation">(</span>IEntity<span class="token operator">*</span><span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">onSpaceGeometryChanged</span><span class="token punctuation">(</span>SpaceID<span class="token punctuation">)</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 运行时接口</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">IRuntime</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">virtual</span> IEntity<span class="token operator">*</span> <span class="token function">createEntity</span><span class="token punctuation">(</span>SpaceID<span class="token punctuation">,</span> <span class="token keyword">const</span> <span class="token keyword">char</span><span class="token operator">*</span> type<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">destroyEntity</span><span class="token punctuation">(</span>EntityID<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">registerCallback</span><span class="token punctuation">(</span>ICallback<span class="token operator">*</span><span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 全局访问点 (BigWorld 风格)</span></span>
<span class="line">    IRuntime<span class="token operator">*</span> <span class="token function">Runtime</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    IEntity<span class="token operator">*</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="十、总结" tabindex="-1"><a class="header-anchor" href="#十、总结"><span>十、总结</span></a></h2><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">传统MMO架构          vs          BigWorld架构</span>
<span class="line">    │                           │</span>
<span class="line">    ├─ 单一世界                ├─ 空间分割</span>
<span class="line">    ├─ 垂直扩展                ├─ 水平扩展</span>
<span class="line">    ├─ 简单直观                ├─ 复杂但强大</span>
<span class="line">    ├─ 全局故障                ├─ 故障隔离</span>
<span class="line">    ├─ 场景切换                ├─ 无缝世界</span>
<span class="line">    └─ 适合副本/小规模         └─ 适合大世界MMO</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>BigWorld 的本质</strong>：用<strong>空间换时间</strong>，通过<strong>水平扩展</strong>和<strong>区域隔离</strong>，实现超大规模无缝世界的MMO服务器。</p><p>在 Apollo 中，BigWorld 模块作为<strong>兼容层</strong>，允许现有 BigWorld 代码平滑迁移到新的模块化架构。</p>`,89)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};