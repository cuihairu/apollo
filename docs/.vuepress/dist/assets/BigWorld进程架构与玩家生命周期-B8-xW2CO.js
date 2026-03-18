import{i as e,r as t,s as n,t as r}from"./app-DuH0mZgh.js";var i=JSON.parse(`{"path":"/BigWorld%E8%BF%9B%E7%A8%8B%E6%9E%B6%E6%9E%84%E4%B8%8E%E7%8E%A9%E5%AE%B6%E7%94%9F%E5%91%BD%E5%91%A8%E6%9C%9F.html","title":"BigWorld 进程架构与玩家生命周期","lang":"en-US","frontmatter":{},"filePathRelative":"BigWorld进程架构与玩家生命周期.md","git":{"createdTime":1773827303000,"updatedTime":1773827303000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),a={name:`BigWorld进程架构与玩家生命周期.md`};function o(r,i,a,o,s,c){return n(),t(`div`,null,[...i[0]||=[e(`<h1 id="bigworld-进程架构与玩家生命周期" tabindex="-1"><a class="header-anchor" href="#bigworld-进程架构与玩家生命周期"><span>BigWorld 进程架构与玩家生命周期</span></a></h1><h2 id="目录" tabindex="-1"><a class="header-anchor" href="#目录"><span>目录</span></a></h2><ul><li><a href="#bigworld-%E8%BF%9B%E7%A8%8B%E6%9E%B6%E6%9E%84%E5%9B%BE">BigWorld 进程架构图</a></li><li><a href="#%E8%BF%9B%E7%A8%8B%E9%97%B4%E9%80%9A%E4%BF%A1%E5%85%B3%E7%B3%BB">进程间通信关系</a></li><li><a href="#%E7%8E%A9%E5%AE%B6%E7%99%BB%E5%BD%95%E6%B5%81%E7%A8%8B">玩家登录流程</a></li><li><a href="#%E7%8E%A9%E5%AE%B6%E6%B8%B8%E6%88%8F%E6%B5%81%E7%A8%8B">玩家游戏流程</a></li><li><a href="#%E7%8E%A9%E5%AE%B6%E5%AF%B9%E6%88%98%E6%B5%81%E7%A8%8B">玩家对战流程</a></li><li><a href="#%E7%8E%A9%E5%AE%B6%E8%81%8A%E5%A4%A9%E6%B5%81%E7%A8%8B">玩家聊天流程</a></li><li><a href="#%E7%8E%A9%E5%AE%B6%E4%B8%8B%E7%BA%BF%E6%B5%81%E7%A8%8B">玩家下线流程</a></li></ul><hr><h2 id="bigworld-进程架构图" tabindex="-1"><a class="header-anchor" href="#bigworld-进程架构图"><span>BigWorld 进程架构图</span></a></h2><h3 id="整体架构" tabindex="-1"><a class="header-anchor" href="#整体架构"><span>整体架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                              客户端 (Client)                                 │</span>
<span class="line">│                    Unity/Unreal/WebGL + 网络层                               │</span>
<span class="line">└────────────────────────────┬────────────────────────────────────────────────┘</span>
<span class="line">                             │ TCP/WebSocket</span>
<span class="line">                             ▼</span>
<span class="line">┌─────────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                           LoginApp (登录服务器)                              │</span>
<span class="line">│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────────────┐              │</span>
<span class="line">│  │  认证模块  │  │ 网关分配   │  │  负载均衡  │  │ 会话令牌生成 │              │</span>
<span class="line">│  └───────────┘  └───────────┘  └───────────┘  └────────────┘              │</span>
<span class="line">│  职责: 处理登录认证、分配网关、创建初始会话                                    │</span>
<span class="line">└────────────────────────────┬────────────────────────────────────────────────┘</span>
<span class="line">                             │ 返回网关地址 + 会话令牌</span>
<span class="line">                             ▼</span>
<span class="line">┌─────────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                         GatewayApp (网关服务器)                              │</span>
<span class="line">│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────────────┐              │</span>
<span class="line">│  │ 连接管理   │  │ 消息路由   │  │  心跳检测  │  │  流量控制   │              │</span>
<span class="line">│  └───────────┘  └───────────┘  └───────────┘  └────────────┘              │</span>
<span class="line">│  职责: 维持客户端连接、路由消息到后端、防止攻击                                │</span>
<span class="line">└────────┬──────────────────────────────┬─────────────────────────────────────┘</span>
<span class="line">         │                              │</span>
<span class="line">         │ 游戏逻辑消息                  │ 聊天消息</span>
<span class="line">         ▼                              ▼</span>
<span class="line">┌──────────────────────────┐  ┌──────────────────────────┐</span>
<span class="line">│    BaseApp (数据库服务器)  │  │    ChatApp (聊天服务器)   │</span>
<span class="line">│  ┌───────────┐            │  │  ┌───────────┐           │</span>
<span class="line">│  │ 数据库连接池 │           │  │  │ 频道管理   │           │</span>
<span class="line">│  │ ORM映射    │            │  │  │ 消息广播   │           │</span>
<span class="line">│  │ 缓存管理   │            │  │  │ 敏感词过滤 │           │</span>
<span class="line">│  │ 异步加载   │            │  │  │ 历史记录   │           │</span>
<span class="line">│  └───────────┘            │  │  └───────────┘           │</span>
<span class="line">│  职责: 玩家数据CRUD、缓存  │  │  职责: 聊天频道、广播    │</span>
<span class="line">└────────┬─────────────────┘  └──────────────────────────┘</span>
<span class="line">         │</span>
<span class="line">         │ 分配/加载实体</span>
<span class="line">         ▼</span>
<span class="line">┌─────────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                        CellApp 游戏逻辑服务器集群                            │</span>
<span class="line">│                                                                              │</span>
<span class="line">│  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐                    │</span>
<span class="line">│  │  CellApp #1   │  │  CellApp #2   │  │  CellApp #N   │                    │</span>
<span class="line">│  │  (主城区域)   │  │  (野外区域)   │  │  (副本区域)   │                    │</span>
<span class="line">│  ├───────────────┤  ├───────────────┤  ├───────────────┤                    │</span>
<span class="line">│  │ AOI系统       │  │ AOI系统       │  │ AOI系统       │                    │</span>
<span class="line">│  │ 实体管理      │  │ 实体管理      │  │ 实体管理      │                    │</span>
<span class="line">│  │ 战斗逻辑      │  │ 战斗逻辑      │  │ 战斗逻辑      │                    │</span>
<span class="line">│  │ 技能系统      │  │ 技能系统      │  │ 技能系统      │                    │</span>
<span class="line">│  │ NPC AI       │  │ NPC AI       │  │ NPC AI       │                    │</span>
<span class="line">│  │ 跨边界同步    │  │ 跨边界同步    │  │ 跨边界同步    │                    │</span>
<span class="line">│  └───────────────┘  └───────────────┘  └───────────────┘                    │</span>
<span class="line">│                                                                              │</span>
<span class="line">│  职责: 游戏核心逻辑、AOI视野管理、实体状态更新、跨区域迁移                     │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────────┘</span>
<span class="line">                             ▲</span>
<span class="line">                             │ 数据保存/加载</span>
<span class="line">                    ┌────────┴────────┐</span>
<span class="line">                    │   数据库集群      │</span>
<span class="line">                    │ MySQL + Redis    │</span>
<span class="line">                    └─────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="进程职责总结" tabindex="-1"><a class="header-anchor" href="#进程职责总结"><span>进程职责总结</span></a></h3><table><thead><tr><th>进程</th><th>缩写</th><th>核心职责</th><th>通信对象</th></tr></thead><tbody><tr><td><strong>LoginApp</strong></td><td>登录服务器</td><td>认证、网关分配</td><td>Client, BaseApp</td></tr><tr><td><strong>GatewayApp</strong></td><td>网关服务器</td><td>连接管理、消息路由</td><td>Client, CellApp, ChatApp, BaseApp</td></tr><tr><td><strong>BaseApp</strong></td><td>数据库服务器</td><td>数据CRUD、缓存</td><td>GatewayApp, CellApp, Database</td></tr><tr><td><strong>CellApp</strong></td><td>游戏逻辑服务器</td><td>游戏逻辑、AOI、战斗</td><td>GatewayApp, 其他CellApp</td></tr><tr><td><strong>ChatApp</strong></td><td>聊天服务器</td><td>聊天频道、广播</td><td>GatewayApp</td></tr></tbody></table><hr><h2 id="进程间通信关系" tabindex="-1"><a class="header-anchor" href="#进程间通信关系"><span>进程间通信关系</span></a></h2><h3 id="通信拓扑图" tabindex="-1"><a class="header-anchor" href="#通信拓扑图"><span>通信拓扑图</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">                    ┌─────────────┐</span>
<span class="line">                    │   Database  │</span>
<span class="line">                    │  (MySQL)    │</span>
<span class="line">                    └──────┬──────┘</span>
<span class="line">                           │</span>
<span class="line">                    ┌──────▼──────┐        ┌─────────────┐</span>
<span class="line">         ┌──────────│   BaseApp   │◄───────│  LoginApp   │</span>
<span class="line">         │          └──────┬──────┘        └──────┬──────┘</span>
<span class="line">         │                 │                      │</span>
<span class="line">         │                 │                      │</span>
<span class="line">┌────────┴────────┐   ┌────▼────┐          ┌────▼────┐</span>
<span class="line">│     Client      │   │GatewayApp│          │GatewayApp│</span>
<span class="line">│  (Unity/Unreal) │◄──┤  Pool    │◄─────────┤  Pool    │</span>
<span class="line">└────────┬────────┘   └────┬────┘          └────┬────┘</span>
<span class="line">         │                 │                      │</span>
<span class="line">         │        ┌────────┴────────┐   ┌────────┴────────┐</span>
<span class="line">         │        │  ┌─────┬─────┐  │   │  ┌─────┬─────┐  │</span>
<span class="line">         └───────►│  │ C#1 │ C#2 │...│   │  │ C#N │...  │  │</span>
<span class="line">                  │  └──┬──┴──┬──┘  │   │  └──┬──┴────┘  │</span>
<span class="line">                  └─────┼─────┼─────┘   └─────┼──────────┘</span>
<span class="line">                        │     │               │</span>
<span class="line">                        └──┬──┴───────┬───────┘</span>
<span class="line">                           │          │</span>
<span class="line">                    ┌──────▼─────┐    │</span>
<span class="line">                    │  ChatApp   │    │</span>
<span class="line">                    └────────────┘    │</span>
<span class="line">                          ▲           │</span>
<span class="line">                          └───────────┘</span>
<span class="line"></span>
<span class="line">    Legend:</span>
<span class="line">    C#N = CellApp #N</span>
<span class="line">    ───► 消息流向</span>
<span class="line">    ◄──── 同步/返回</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="玩家登录流程" tabindex="-1"><a class="header-anchor" href="#玩家登录流程"><span>玩家登录流程</span></a></h2><h3 id="流程图" tabindex="-1"><a class="header-anchor" href="#流程图"><span>流程图</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Client                    LoginApp                BaseApp                GatewayApp              CellApp</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │ 1. 连接请求               │                       │                       │                       │</span>
<span class="line">  ├─────────────────────────►│                       │                       │                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │ 2. 登录请求(账号/密码)     │                       │                       │                       │</span>
<span class="line">  ├─────────────────────────►│                       │                       │                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │                          │ 3. 验证账号密码        │                       │                       │</span>
<span class="line">  │                          ├──────────────────────►│                       │                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │                          │ 4. 返回玩家数据        │                       │                       │</span>
<span class="line">  │                          │◄──────────────────────┤                       │                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │                          │ 5. 查找最优Gateway    │                       │                       │</span>
<span class="line">  │                          ├──────────────────────────────────────────────►│                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │                          │ 6. 返回Gateway地址    │                       │                       │</span>
<span class="line">  │                          │◄──────────────────────────────────────────────┤                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │ 7. 返回网关地址+会话令牌   │                       │                       │                       │</span>
<span class="line">  │◄─────────────────────────┤                       │                       │                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │ 8. 断开与LoginApp的连接   │                       │                       │                       │</span>
<span class="line">  ├─────────────────────────✘│                       │                       │                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │ 9. 连接到GatewayApp      │                       │                       │                       │</span>
<span class="line">  ├─────────────────────────────────────────────────────────────────────────►│                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │ 10. 进入游戏请求(令牌)    │                       │                       │                       │</span>
<span class="line">  ├─────────────────────────────────────────────────────────────────────────►│                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │                          │ 11. 验证令牌           │                       │                       │</span>
<span class="line">  │                          ├──────────────────────►│                       │                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │                          │ 12. 加载完整玩家数据   │                       │                       │</span>
<span class="line">  │                          │◄──────────────────────┤                       │                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │                          │ 13. 分配目标CellApp   │                       │                       │</span>
<span class="line">  │                          ├──────────────────────────────────────────────────────────────────────►│</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │                          │ 14. 创建玩家实体       │                       │                       │</span>
<span class="line">  │                          │◄──────────────────────────────────────────────────────────────────────┤</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │ 15. 返回登录成功          │                       │                       │                       │</span>
<span class="line">  │◄─────────────────────────────────────────────────────────────────────────┤                       │</span>
<span class="line">  │                          │                       │                       │                       │</span>
<span class="line">  │ 16. 开始接收游戏世界数据  │                       │                       │                       │</span>
<span class="line">  │◄═════════════════════════════════════════════════════════════════════════╪═══════════════════════►│</span>
<span class="line">                           │                       │                       │                       │</span>
<span class="line">                           └── 登录流程完成 ─────────┘                       │                       │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="代码示例" tabindex="-1"><a class="header-anchor" href="#代码示例"><span>代码示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// LoginApp 处理登录</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">LoginApp</span> <span class="token punctuation">{</span></span>
<span class="line">    Response <span class="token function">handleLogin</span><span class="token punctuation">(</span><span class="token keyword">const</span> LoginRequest<span class="token operator">&amp;</span> req<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 验证账号密码</span></span>
<span class="line">        <span class="token keyword">auto</span> playerData <span class="token operator">=</span> baseApp_<span class="token operator">-&gt;</span><span class="token function">verifyAccount</span><span class="token punctuation">(</span>req<span class="token punctuation">.</span>username<span class="token punctuation">,</span> req<span class="token punctuation">.</span>password<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>playerData<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token class-name">Response</span><span class="token double-colon punctuation">::</span><span class="token function">Error</span><span class="token punctuation">(</span><span class="token string">&quot;账号或密码错误&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 查找最优Gateway (负载最低的)</span></span>
<span class="line">        <span class="token keyword">auto</span> gateway <span class="token operator">=</span> gatewayManager_<span class="token operator">-&gt;</span><span class="token function">selectBestGateway</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 生成会话令牌</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string token <span class="token operator">=</span> <span class="token function">generateSessionToken</span><span class="token punctuation">(</span>playerData<span class="token operator">-&gt;</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 记录会话</span></span>
<span class="line">        sessionManager_<span class="token operator">-&gt;</span><span class="token function">createSession</span><span class="token punctuation">(</span>token<span class="token punctuation">,</span> playerData<span class="token operator">-&gt;</span>id<span class="token punctuation">,</span> gateway<span class="token operator">-&gt;</span><span class="token function">address</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> LoginSuccessResponse<span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">.</span>gatewayAddress <span class="token operator">=</span> gateway<span class="token operator">-&gt;</span><span class="token function">address</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">.</span>gatewayPort <span class="token operator">=</span> gateway<span class="token operator">-&gt;</span><span class="token function">port</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">.</span>sessionToken <span class="token operator">=</span> token</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// GatewayApp 处理进入游戏</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">GatewayApp</span> <span class="token punctuation">{</span></span>
<span class="line">    Response <span class="token function">handleEnterGame</span><span class="token punctuation">(</span><span class="token keyword">const</span> EnterGameRequest<span class="token operator">&amp;</span> req<span class="token punctuation">,</span> Connection<span class="token operator">*</span> conn<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 验证令牌</span></span>
<span class="line">        <span class="token keyword">auto</span> session <span class="token operator">=</span> loginApp_<span class="token operator">-&gt;</span><span class="token function">verifySession</span><span class="token punctuation">(</span>req<span class="token punctuation">.</span>token<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>session<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token class-name">Response</span><span class="token double-colon punctuation">::</span><span class="token function">Error</span><span class="token punctuation">(</span><span class="token string">&quot;无效的会话令牌&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 从BaseApp加载完整玩家数据</span></span>
<span class="line">        <span class="token keyword">auto</span> playerData <span class="token operator">=</span> baseApp_<span class="token operator">-&gt;</span><span class="token function">loadPlayerData</span><span class="token punctuation">(</span>session<span class="token operator">-&gt;</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>playerData<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token class-name">Response</span><span class="token double-colon punctuation">::</span><span class="token function">Error</span><span class="token punctuation">(</span><span class="token string">&quot;加载玩家数据失败&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 根据玩家位置分配CellApp</span></span>
<span class="line">        <span class="token keyword">auto</span> cellApp <span class="token operator">=</span> cellAppManager_<span class="token operator">-&gt;</span><span class="token function">assignByPosition</span><span class="token punctuation">(</span>playerData<span class="token operator">-&gt;</span>lastPosition<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 在CellApp中创建玩家实体</span></span>
<span class="line">        <span class="token keyword">auto</span> entity <span class="token operator">=</span> cellApp_<span class="token operator">-&gt;</span><span class="token function">createPlayerEntity</span><span class="token punctuation">(</span><span class="token operator">*</span>playerData<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 创建Proxy (客户端连接代理)</span></span>
<span class="line">        <span class="token keyword">auto</span> proxy <span class="token operator">=</span> <span class="token function">createProxy</span><span class="token punctuation">(</span>conn<span class="token punctuation">,</span> entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 6. 关联Proxy和CellApp</span></span>
<span class="line">        proxy<span class="token operator">-&gt;</span><span class="token function">bindToCellApp</span><span class="token punctuation">(</span>cellApp<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> EnterGameSuccessResponse<span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">.</span>entityId <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">.</span>position <span class="token operator">=</span> playerData<span class="token operator">-&gt;</span>lastPosition</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="玩家游戏流程" tabindex="-1"><a class="header-anchor" href="#玩家游戏流程"><span>玩家游戏流程</span></a></h2><h3 id="游戏主循环消息流" tabindex="-1"><a class="header-anchor" href="#游戏主循环消息流"><span>游戏主循环消息流</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Client                    GatewayApp                CellApp</span>
<span class="line">  │                          │                       │</span>
<span class="line">  │ 1. 移动请求               │                       │</span>
<span class="line">  ├─────────────────────────►│                       │</span>
<span class="line">  │                          │                       │</span>
<span class="line">  │                          │ 2. 转发到CellApp       │</span>
<span class="line">  │                          ├──────────────────────►│</span>
<span class="line">  │                          │                       │</span>
<span class="line">  │                          │                       │ 3. 验证移动合法性</span>
<span class="line">  │                          │                       │ 4. 更新位置</span>
<span class="line">  │                          │                       │ 5. 计算AOI变化</span>
<span class="line">  │                          │                       │</span>
<span class="line">  │                          │                       │ 6. 发现新玩家进入视野</span>
<span class="line">  │                          │                       │    ┌──────────────────┐</span>
<span class="line">  │                          │                       │    │ EntityAppear{e1} │</span>
<span class="line">  │                          │◄──────────────────────┤    │ EntityAppear{e2} │</span>
<span class="line">  │ 7. 周围实体出现           │                       │    └──────────────────┘</span>
<span class="line">  │◄─────────────────────────┤                       │</span>
<span class="line">  │                          │                       │</span>
<span class="line">  │                          │                       │ 8. 广播给周围玩家</span>
<span class="line">  │                          │                       │    ┌──────────────────┐</span>
<span class="line">  │                          │                       │    │ EntityMove{me}   │</span>
<span class="line">  │                          │                       │    └──────────────────┘</span>
<span class="line">  │                          │                       │</span>
<span class="line">  │                          │ 9. 转发给其他Gateway   │</span>
<span class="line">  │                          ├─────────────────────────────────────────►</span>
<span class="line">  │                          │                       │</span>
<span class="line">  │ 10. 其他客户端看到移动    │                       │</span>
<span class="line">  │◄═════════════════════════════════════════════════╪═══════════════════════╪═══════════════════</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="cellapp-内部处理" tabindex="-1"><a class="header-anchor" href="#cellapp-内部处理"><span>CellApp 内部处理</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// CellApp 处理玩家移动</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellApp</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleMove</span><span class="token punctuation">(</span>EntityID entityId<span class="token punctuation">,</span> Position newPos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>entityId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>entity<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        Position oldPos <span class="token operator">=</span> entity<span class="token operator">-&gt;</span><span class="token function">position</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 验证移动合法性</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">validateMove</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> oldPos<span class="token punctuation">,</span> newPos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span> <span class="token comment">// 非法移动，忽略</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 更新位置</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setPosition</span><span class="token punctuation">(</span>newPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 检查是否跨格子</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token function">getGridId</span><span class="token punctuation">(</span>oldPos<span class="token punctuation">)</span> <span class="token operator">!=</span> <span class="token function">getGridId</span><span class="token punctuation">(</span>newPos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">handleCrossGridMove</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> oldPos<span class="token punctuation">,</span> newPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 检查是否跨CellApp</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>bounds_<span class="token punctuation">.</span><span class="token function">contains</span><span class="token punctuation">(</span>newPos<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">handleCrossCellAppMove</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> newPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span> <span class="token comment">// 实体已迁移</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 计算AOI变化</span></span>
<span class="line">        <span class="token keyword">auto</span> viewChange <span class="token operator">=</span> aoi_<span class="token operator">-&gt;</span><span class="token function">calculateViewChange</span><span class="token punctuation">(</span>oldPos<span class="token punctuation">,</span> newPos<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 6. 通知离开视野的玩家</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> leaver <span class="token operator">:</span> viewChange<span class="token punctuation">.</span>leavers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            gateway_<span class="token operator">-&gt;</span><span class="token function">sendToClient</span><span class="token punctuation">(</span>leaver<span class="token operator">-&gt;</span>clientId<span class="token punctuation">,</span> EntityLeaveMsg<span class="token punctuation">{</span>entityId<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 7. 通知进入视野的玩家</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> enterer <span class="token operator">:</span> viewChange<span class="token punctuation">.</span>enterers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            gateway_<span class="token operator">-&gt;</span><span class="token function">sendToClient</span><span class="token punctuation">(</span>enterer<span class="token operator">-&gt;</span>clientId<span class="token punctuation">,</span> EntityAppearMsg<span class="token punctuation">{</span>entityId<span class="token punctuation">,</span> newPos<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 8. 广播移动给一直在视野内的玩家</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">*</span> stayer <span class="token operator">:</span> viewChange<span class="token punctuation">.</span>stayers<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            gateway_<span class="token operator">-&gt;</span><span class="token function">sendToClient</span><span class="token punctuation">(</span>stayer<span class="token operator">-&gt;</span>clientId<span class="token punctuation">,</span> EntityMoveMsg<span class="token punctuation">{</span>entityId<span class="token punctuation">,</span> oldPos<span class="token punctuation">,</span> newPos<span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="玩家对战流程" tabindex="-1"><a class="header-anchor" href="#玩家对战流程"><span>玩家对战流程</span></a></h2><h3 id="战斗消息流" tabindex="-1"><a class="header-anchor" href="#战斗消息流"><span>战斗消息流</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Client A (攻击者)         GatewayApp              CellApp              Client B (目标)</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │ 1. 释放技能请求           │                       │                       │</span>
<span class="line">  ├─────────────────────────►│                       │                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │ 2. 转发技能请求        │                       │</span>
<span class="line">  │                          ├──────────────────────►│                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 3. 验证技能</span>
<span class="line">  │                          │                       │ 4. 计算伤害</span>
<span class="line">  │                          │                       │ 5. 扣除血量</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 6. B收到伤害事件</span>
<span class="line">  │                          │                       │    ┌────────────────┐</span>
<span class="line">  │                          │                       │    │ EntityDamage   │</span>
<span class="line">  │                          │                       │    │ hp: 100→80     │</span>
<span class="line">  │ 7. B受到伤害通知          │                       │    └────────────────┘</span>
<span class="line">  │◄═════════════════════════════════════════════════╪═══════════════════════►│</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 8. 播放特效</span>
<span class="line">  │                          │                       │ 9. 广播伤害结果</span>
<span class="line">  │                          │                       │    ┌────────────────┐</span>
<span class="line">  │                          │                       │    │ SkillHitResult │</span>
<span class="line">  │ 10. 周围玩家看到伤害      │                       │    └────────────────┘</span>
<span class="line">  │◄═════════════════════════════════════════════════╪═══════════════════════►│</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 11. 如果B死亡</span>
<span class="line">  │                          │                       │    ┌────────────────┐</span>
<span class="line">  │                          │                       │    │ EntityDeath    │</span>
<span class="line">  │                          │                       │    └────────────────┘</span>
<span class="line">  │ 12. 看到B死亡             │                       │                       │</span>
<span class="line">  │◄═════════════════════════════════════════════════╪════════════════════════════════════════════►│</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="跨cellapp战斗" tabindex="-1"><a class="header-anchor" href="#跨cellapp战斗"><span>跨CellApp战斗</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">CellApp A                 CellApp B</span>
<span class="line">(边界附近)                (边界附近)</span>
<span class="line">  │                          │</span>
<span class="line">  │                          │</span>
<span class="line">  │ A区玩家攻击B区目标        │</span>
<span class="line">  │                          │</span>
<span class="line">  │ 1. 在A创建目标的Shadow    │</span>
<span class="line">  │ ◄───────────────────────►│</span>
<span class="line">  │                          │</span>
<span class="line">  │ 2. 计算伤害(A)           │</span>
<span class="line">  │                          │</span>
<span class="line">  │ 3. 同步伤害结果到B        │</span>
<span class="line">  ├─────────────────────────►│</span>
<span class="line">  │                          │</span>
<span class="line">  │                          │ 4. 应用伤害(B)</span>
<span class="line">  │                          │</span>
<span class="line">  │ 5. 同步死亡事件           │</span>
<span class="line">  │◄────────────────────────►│</span>
<span class="line">  │                          │</span>
<span class="line">  ▼                          ▼</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="战斗代码示例" tabindex="-1"><a class="header-anchor" href="#战斗代码示例"><span>战斗代码示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// CellApp 处理技能释放</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellApp</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleCastSkill</span><span class="token punctuation">(</span>EntityID casterId<span class="token punctuation">,</span> <span class="token keyword">uint32_t</span> skillId<span class="token punctuation">,</span> EntityID targetId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> caster <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>casterId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        Entity<span class="token operator">*</span> target <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>targetId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>caster <span class="token operator">||</span> <span class="token operator">!</span>target<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 验证技能条件</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>caster<span class="token operator">-&gt;</span><span class="token function">canCastSkill</span><span class="token punctuation">(</span>skillId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span> <span class="token comment">// CD中、蓝量不足等</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 验证目标在射程内</span></span>
<span class="line">        <span class="token keyword">float</span> distance <span class="token operator">=</span> <span class="token function">calcDistance</span><span class="token punctuation">(</span>caster<span class="token operator">-&gt;</span><span class="token function">position</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> target<span class="token operator">-&gt;</span><span class="token function">position</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>distance <span class="token operator">&gt;</span> <span class="token function">getSkillRange</span><span class="token punctuation">(</span>skillId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span> <span class="token comment">// 超出射程</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 计算伤害</span></span>
<span class="line">        <span class="token keyword">int32_t</span> damage <span class="token operator">=</span> <span class="token function">calculateDamage</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> target<span class="token punctuation">,</span> skillId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 应用伤害</span></span>
<span class="line">        <span class="token function">applyDamage</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> target<span class="token punctuation">,</span> damage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 5. 触发技能特效</span></span>
<span class="line">        <span class="token function">spawnSkillEffect</span><span class="token punctuation">(</span>skillId<span class="token punctuation">,</span> target<span class="token operator">-&gt;</span><span class="token function">position</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 6. 广播技能释放</span></span>
<span class="line">        aoi_<span class="token operator">-&gt;</span><span class="token function">broadcastToViewers</span><span class="token punctuation">(</span>caster<span class="token punctuation">,</span> SkillCastMsg<span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">.</span>casterId <span class="token operator">=</span> casterId<span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">.</span>skillId <span class="token operator">=</span> skillId<span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">.</span>targetId <span class="token operator">=</span> targetId</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 7. 广播伤害结果</span></span>
<span class="line">        aoi_<span class="token operator">-&gt;</span><span class="token function">broadcastToViewers</span><span class="token punctuation">(</span>target<span class="token punctuation">,</span> SkillHitMsg<span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">.</span>targetId <span class="token operator">=</span> targetId<span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">.</span>damage <span class="token operator">=</span> damage<span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">.</span>currentHp <span class="token operator">=</span> target<span class="token operator">-&gt;</span><span class="token function">hp</span><span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 8. 检查死亡</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>target<span class="token operator">-&gt;</span><span class="token function">hp</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&lt;=</span> <span class="token number">0</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token function">handleEntityDeath</span><span class="token punctuation">(</span>target<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">applyDamage</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> attacker<span class="token punctuation">,</span> Entity<span class="token operator">*</span> target<span class="token punctuation">,</span> <span class="token keyword">int32_t</span> damage<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 如果目标在其他CellApp (边界情况)</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>target<span class="token operator">-&gt;</span><span class="token function">isShadow</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 转发伤害到目标所属CellApp</span></span>
<span class="line">            target<span class="token operator">-&gt;</span><span class="token function">ownerCellApp</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">-&gt;</span><span class="token function">receiveRemoteDamage</span><span class="token punctuation">(</span>target<span class="token operator">-&gt;</span><span class="token function">id</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> damage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span> <span class="token keyword">else</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 本地实体，直接应用</span></span>
<span class="line">            target<span class="token operator">-&gt;</span><span class="token function">takeDamage</span><span class="token punctuation">(</span>damage<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="玩家聊天流程" tabindex="-1"><a class="header-anchor" href="#玩家聊天流程"><span>玩家聊天流程</span></a></h2><h3 id="聊天消息流" tabindex="-1"><a class="header-anchor" href="#聊天消息流"><span>聊天消息流</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Client A                  GatewayApp                ChatApp                Client B</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │ 1. 发送聊天消息           │                       │                       │</span>
<span class="line">  │   /say 你好              │                       │                       │</span>
<span class="line">  ├─────────────────────────►│                       │                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │ 2. 验证会话            │                       │</span>
<span class="line">  │                          │ 3. 检查禁言状态        │                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │ 4. 转发到ChatApp      │                       │</span>
<span class="line">  │                          ├──────────────────────►│                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 5. 敏感词过滤</span>
<span class="line">  │                          │                       │ 6. 查找频道成员</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 7. 确定接收者</span>
<span class="line">  │                          │                       │    ┌──────────────┐</span>
<span class="line">  │                          │                       │    │ A: &quot;你好&quot;     │</span>
<span class="line">  │ 8. 广播给频道成员          │                       │    └──────────────┘</span>
<span class="line">  │◄═════════════════════════════════════════════════╪═══════════════════════►│</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="聊天频道类型" tabindex="-1"><a class="header-anchor" href="#聊天频道类型"><span>聊天频道类型</span></a></h3><table><thead><tr><th>频道类型</th><th>范围</th><th>ChatApp处理</th><th>示例</th></tr></thead><tbody><tr><td><strong>当前频道</strong></td><td>AOI内玩家</td><td>查询CellApp获取AOI内玩家</td><td><code>/say 你好</code></td></tr><tr><td><strong>世界频道</strong></td><td>全服玩家</td><td>查询所有在线玩家</td><td><code>/world 求组</code></td></tr><tr><td><strong>私聊频道</strong></td><td>指定玩家</td><td>查询目标玩家所在Gateway</td><td><code>/tell Player 消息</code></td></tr><tr><td><strong>公会频道</strong></td><td>公会成员</td><td>查询公会成员列表</td><td><code>/guild 打BOSS</code></td></tr><tr><td><strong>队伍频道</strong></td><td>队伍成员</td><td>查询队伍成员</td><td><code>/party 集合</code></td></tr></tbody></table><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// ChatApp 处理聊天</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ChatApp</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleChatMessage</span><span class="token punctuation">(</span><span class="token keyword">const</span> ChatMessage<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 1. 验证发送者</span></span>
<span class="line">        Player<span class="token operator">*</span> sender <span class="token operator">=</span> <span class="token function">getPlayer</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>senderId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>sender <span class="token operator">||</span> sender<span class="token operator">-&gt;</span><span class="token function">isMuted</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">return</span><span class="token punctuation">;</span> <span class="token comment">// 玩家不存在或被禁言</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 敏感词过滤</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string filtered <span class="token operator">=</span> <span class="token function">filterSensitiveWords</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>content<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 根据频道类型分发</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>msg<span class="token punctuation">.</span>channel<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> Channel<span class="token double-colon punctuation">::</span>CURRENT<span class="token operator">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 当前频道：发送给AOI内玩家</span></span>
<span class="line">                <span class="token keyword">auto</span> viewers <span class="token operator">=</span> cellApp_<span class="token operator">-&gt;</span><span class="token function">getAOIViewers</span><span class="token punctuation">(</span>sender<span class="token operator">-&gt;</span><span class="token function">position</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">broadcastToPlayers</span><span class="token punctuation">(</span>viewers<span class="token punctuation">,</span> ChatMsg<span class="token punctuation">{</span></span>
<span class="line">                    <span class="token punctuation">.</span>sender <span class="token operator">=</span> sender<span class="token operator">-&gt;</span><span class="token function">name</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                    <span class="token punctuation">.</span>content <span class="token operator">=</span> filtered<span class="token punctuation">,</span></span>
<span class="line">                    <span class="token punctuation">.</span>channel <span class="token operator">=</span> Channel<span class="token double-colon punctuation">::</span>CURRENT</span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token keyword">case</span> Channel<span class="token double-colon punctuation">::</span>WORLD<span class="token operator">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 世界频道：发送给全服玩家</span></span>
<span class="line">                <span class="token keyword">auto</span> allPlayers <span class="token operator">=</span> <span class="token function">getAllOnlinePlayers</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">broadcastToPlayers</span><span class="token punctuation">(</span>allPlayers<span class="token punctuation">,</span> ChatMsg<span class="token punctuation">{</span></span>
<span class="line">                    <span class="token punctuation">.</span>sender <span class="token operator">=</span> sender<span class="token operator">-&gt;</span><span class="token function">name</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                    <span class="token punctuation">.</span>content <span class="token operator">=</span> filtered<span class="token punctuation">,</span></span>
<span class="line">                    <span class="token punctuation">.</span>channel <span class="token operator">=</span> Channel<span class="token double-colon punctuation">::</span>WORLD</span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token keyword">case</span> Channel<span class="token double-colon punctuation">::</span>PRIVATE<span class="token operator">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 私聊：发送给指定玩家</span></span>
<span class="line">                Player<span class="token operator">*</span> receiver <span class="token operator">=</span> <span class="token function">getPlayerByName</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>targetName<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token punctuation">(</span>receiver<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                    <span class="token function">sendToPlayer</span><span class="token punctuation">(</span>receiver<span class="token punctuation">,</span> ChatMsg<span class="token punctuation">{</span></span>
<span class="line">                        <span class="token punctuation">.</span>sender <span class="token operator">=</span> sender<span class="token operator">-&gt;</span><span class="token function">name</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                        <span class="token punctuation">.</span>content <span class="token operator">=</span> filtered<span class="token punctuation">,</span></span>
<span class="line">                        <span class="token punctuation">.</span>channel <span class="token operator">=</span> Channel<span class="token double-colon punctuation">::</span>PRIVATE</span>
<span class="line">                    <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token punctuation">}</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">            <span class="token keyword">case</span> Channel<span class="token double-colon punctuation">::</span>GUILD<span class="token operator">:</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 公会频道：发送给公会成员</span></span>
<span class="line">                <span class="token keyword">auto</span> guildMembers <span class="token operator">=</span> guildManager_<span class="token operator">-&gt;</span><span class="token function">getMembers</span><span class="token punctuation">(</span>sender<span class="token operator">-&gt;</span><span class="token function">guildId</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token function">broadcastToPlayers</span><span class="token punctuation">(</span>guildMembers<span class="token punctuation">,</span> ChatMsg<span class="token punctuation">{</span></span>
<span class="line">                    <span class="token punctuation">.</span>sender <span class="token operator">=</span> sender<span class="token operator">-&gt;</span><span class="token function">name</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">                    <span class="token punctuation">.</span>content <span class="token operator">=</span> filtered<span class="token punctuation">,</span></span>
<span class="line">                    <span class="token punctuation">.</span>channel <span class="token operator">=</span> Channel<span class="token double-colon punctuation">::</span>GUILD</span>
<span class="line">                <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 4. 记录聊天历史 (用于审计)</span></span>
<span class="line">        chatHistory_<span class="token operator">-&gt;</span><span class="token function">record</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span>senderId<span class="token punctuation">,</span> msg<span class="token punctuation">.</span>channel<span class="token punctuation">,</span> filtered<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="玩家下线流程" tabindex="-1"><a class="header-anchor" href="#玩家下线流程"><span>玩家下线流程</span></a></h2><h3 id="正常下线" tabindex="-1"><a class="header-anchor" href="#正常下线"><span>正常下线</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Client                    GatewayApp                CellApp                BaseApp</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │ 1. 请求退出游戏           │                       │                       │</span>
<span class="line">  ├─────────────────────────►│                       │                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │ 2. 通知CellApp下线     │                       │</span>
<span class="line">  │                          ├──────────────────────►│                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 3. 广播玩家离开</span>
<span class="line">  │                          │                       │    ┌──────────────┐</span>
<span class="line">  │ 4. 周围玩家看到离开       │                       │    │ EntityLeave  │</span>
<span class="line">  │◄═════════════════════════════════════════════════╪═════════════════════►│</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 5. 销毁玩家实体       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │ 6. 保存玩家数据        │                       │</span>
<span class="line">  │                          ├──────────────────────────────────────────────►│</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │                       │ 7. 写入数据库</span>
<span class="line">  │                          │                       │                       │   更新Redis缓存</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │ 8. 保存完成            │                       │</span>
<span class="line">  │                          │◄──────────────────────────────────────────────┤</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │ 9. 下线成功               │                       │                       │</span>
<span class="line">  │◄─────────────────────────┤                       │                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │ 10. 断开连接              │                       │                       │</span>
<span class="line">  ├─────────────────────────✘│                       │                       │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="异常掉线处理" tabindex="-1"><a class="header-anchor" href="#异常掉线处理"><span>异常掉线处理</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">Client                    GatewayApp                CellApp                BaseApp</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │ 1. 网络断开/超时         │                       │                       │</span>
<span class="line">  ├─────────────────────────✘│                       │                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │ 2. 检测到连接断开      │                       │</span>
<span class="line">  │                          │    (心跳超时)          │                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │ 3. 启动安全下线流程     │                       │</span>
<span class="line">  │                          │    (保留实体N秒)       │                       │</span>
<span class="line">  │                          ├──────────────────────►│                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 4. 玩家进入&quot;掉线保护&quot;  │</span>
<span class="line">  │                          │                       │    - 无法移动          │</span>
<span class="line">  │                          │                       │    - 无敌状态          │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │ 5a. 如果30秒内重连        │                       │                       │</span>
<span class="line">  ├─────────────────────────►│                       │                       │</span>
<span class="line">  │                          │ 6. 验证快速重连        │                       │</span>
<span class="line">  │                          │ 7. 恢复连接            │                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │ 8. 取消掉线保护       │</span>
<span class="line">  │ ◄═════════════════════════════════════════════════╪═══════════════════════►│</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line">  │ 5b. 如果30秒后未重连      │                       │                       │</span>
<span class="line">  │                          │                       │ 9. 执行正常下线流程    │</span>
<span class="line">  │                          │                       │                       │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="下线代码示例" tabindex="-1"><a class="header-anchor" href="#下线代码示例"><span>下线代码示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// GatewayApp 处理下线</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">GatewayApp</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">handleLogout</span><span class="token punctuation">(</span>Connection<span class="token operator">*</span> conn<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Proxy<span class="token operator">*</span> proxy <span class="token operator">=</span> <span class="token function">getProxyByConnection</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>proxy<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        EntityID playerId <span class="token operator">=</span> proxy<span class="token operator">-&gt;</span><span class="token function">controlledEntity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 通知CellApp玩家下线</span></span>
<span class="line">        cellApp_<span class="token operator">-&gt;</span><span class="token function">onPlayerLogout</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 异步保存数据 (不阻塞断开连接)</span></span>
<span class="line">        baseApp_<span class="token operator">-&gt;</span><span class="token function">savePlayerDataAsync</span><span class="token punctuation">(</span>playerId<span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">,</span> conn<span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token keyword">bool</span> success<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 保存完成后的回调</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>success<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token function">logError</span><span class="token punctuation">(</span><span class="token string">&quot;保存玩家数据失败&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 立即断开连接</span></span>
<span class="line">        conn<span class="token operator">-&gt;</span><span class="token function">close</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理异常掉线</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onConnectionLost</span><span class="token punctuation">(</span>Connection<span class="token operator">*</span> conn<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Proxy<span class="token operator">*</span> proxy <span class="token operator">=</span> <span class="token function">getProxyByConnection</span><span class="token punctuation">(</span>conn<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>proxy<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        EntityID playerId <span class="token operator">=</span> proxy<span class="token operator">-&gt;</span><span class="token function">controlledEntity</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 启动掉线保护定时器</span></span>
<span class="line">        timer_<span class="token operator">-&gt;</span><span class="token function">setTimeout</span><span class="token punctuation">(</span><span class="token number">30000</span><span class="token punctuation">,</span> <span class="token punctuation">[</span><span class="token keyword">this</span><span class="token punctuation">,</span> playerId<span class="token punctuation">]</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 30秒后检查是否重连</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span><span class="token function">isPlayerReconnected</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token comment">// 未重连，执行安全下线</span></span>
<span class="line">                <span class="token function">performSafeLogout</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 通知CellApp进入掉线保护</span></span>
<span class="line">        cellApp_<span class="token operator">-&gt;</span><span class="token function">onPlayerDisconnect</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// CellApp 处理掉线</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CellApp</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onPlayerDisconnect</span><span class="token punctuation">(</span>EntityID playerId<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>entity<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 标记为掉线保护状态</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setFlag</span><span class="token punctuation">(</span>EntityFlag<span class="token double-colon punctuation">::</span>DISCONNECTED<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setInvincible</span><span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 无敌</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setMovable</span><span class="token punctuation">(</span><span class="token boolean">false</span><span class="token punctuation">)</span><span class="token punctuation">;</span>     <span class="token comment">// 不可移动</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 广播掉线状态 (可选显示&quot;掉线中&quot;)</span></span>
<span class="line">        aoi_<span class="token operator">-&gt;</span><span class="token function">broadcastToViewers</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> EntityStatusMsg<span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">.</span>entityId <span class="token operator">=</span> playerId<span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">.</span>status <span class="token operator">=</span> EntityStatus<span class="token double-colon punctuation">::</span>DISCONNECTED</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onPlayerReconnect</span><span class="token punctuation">(</span>EntityID playerId<span class="token punctuation">,</span> Proxy<span class="token operator">*</span> newProxy<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        Entity<span class="token operator">*</span> entity <span class="token operator">=</span> <span class="token function">getEntity</span><span class="token punctuation">(</span>playerId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>entity<span class="token punctuation">)</span> <span class="token keyword">return</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 1. 取消掉线保护</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">clearFlag</span><span class="token punctuation">(</span>EntityFlag<span class="token double-colon punctuation">::</span>DISCONNECTED<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setInvincible</span><span class="token punctuation">(</span><span class="token boolean">false</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        entity<span class="token operator">-&gt;</span><span class="token function">setMovable</span><span class="token punctuation">(</span><span class="token boolean">true</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 2. 广播重连状态</span></span>
<span class="line">        aoi_<span class="token operator">-&gt;</span><span class="token function">broadcastToViewers</span><span class="token punctuation">(</span>entity<span class="token punctuation">,</span> EntityStatusMsg<span class="token punctuation">{</span></span>
<span class="line">            <span class="token punctuation">.</span>entityId <span class="token operator">=</span> playerId<span class="token punctuation">,</span></span>
<span class="line">            <span class="token punctuation">.</span>status <span class="token operator">=</span> EntityStatus<span class="token double-colon punctuation">::</span>ONLINE</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 3. 同步当前状态给客户端</span></span>
<span class="line">        newProxy<span class="token operator">-&gt;</span><span class="token function">sendFullGameState</span><span class="token punctuation">(</span>entity<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="完整玩家生命周期图" tabindex="-1"><a class="header-anchor" href="#完整玩家生命周期图"><span>完整玩家生命周期图</span></a></h2><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">     注册                      登录                      游戏中                        下线</span>
<span class="line">       │                        │                         │                           │</span>
<span class="line">       ▼                        ▼                         ▼                           ▼</span>
<span class="line">┌─────────────┐          ┌─────────────┐          ┌─────────────┐             ┌─────────────┐</span>
<span class="line">│  Client注册  │          │ 连接LoginApp│          │  在CellApp   │             │ 正常/异常    │</span>
<span class="line">│  账号创建    │          │ 验证账号密码 │          │  游戏循环     │             │  下线        │</span>
<span class="line">└──────┬──────┘          └──────┬──────┘          └──────┬──────┘             └──────┬──────┘</span>
<span class="line">       │                        │                         │                           │</span>
<span class="line">       │ 数据存入               │ 返回Gateway地址          │                           │</span>
<span class="line">       ▼ Database              ▼                         ▼                           ▼</span>
<span class="line">┌─────────────────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                        BaseApp (数据管理)                                           │</span>
<span class="line">│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐        │</span>
<span class="line">│  │ 创建账号   │  │ 验证登录   │  │ 加载数据   │  │ 定时保存   │  │ 最终保存   │        │</span>
<span class="line">│  └───────────┘  └───────────┘  └───────────┘  └───────────┘  └───────────┘        │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────────────────┘</span>
<span class="line">       │                        │                         │                           │</span>
<span class="line">       │                        │ 连接Gateway              │                           │</span>
<span class="line">       │                        │ ▼                        │                           │</span>
<span class="line">       │                        │ ┌───────────────────────────────────────────────┐   │</span>
<span class="line">       │                        │ │              GatewayApp (连接管理)             │   │</span>
<span class="line">       │                        │ │  ┌─────────┐  ┌─────────┐  ┌─────────┐       │   │</span>
<span class="line">       │                        │ │  │ 验证令牌 │  │ 消息路由 │  │ 心跳检测 │       │   │</span>
<span class="line">       │                        │ │  └─────────┘  └─────────┘  └─────────┘       │   │</span>
<span class="line">       │                        │ └───────────────────────────────────────────────┘   │</span>
<span class="line">       │                        │                         │                           │</span>
<span class="line">       │                        │ 创建实体                 │                           │</span>
<span class="line">       │                        │ ▼                        │                           │</span>
<span class="line">       │                        │ ┌───────────────────────────────────────────────┐   │</span>
<span class="line">       │                        │ │              CellApp (游戏逻辑)                │   │</span>
<span class="line">       │                        │ │  ┌─────────┐  ┌─────────┐  ┌─────────┐       │   │</span>
<span class="line">       │                        │ │  │ 实体管理 │  │ AOI系统 │  │ 战斗系统 │       │   │</span>
<span class="line">       │                        │ │  │ 技能系统 │  │ NPC AI  │  │ 跨区迁移 │       │   │</span>
<span class="line">       │                        │ │  └─────────┘  └─────────┘  └─────────┘       │   │</span>
<span class="line">       │                        │ └───────────────────────────────────────────────┘   │</span>
<span class="line">       │                        │                         │                           │</span>
<span class="line">       │                        │ 聊天消息                 │                           │</span>
<span class="line">       │                        │ ────────────────────────┼───────                    │</span>
<span class="line">       │                        │                         ▼                           │</span>
<span class="line">       │                        │              ┌─────────────────────┐                │</span>
<span class="line">       │                        │              │     ChatApp         │                │</span>
<span class="line">       │                        │              │  ┌───────────────┐  │                │</span>
<span class="line">       │                        │              │  │ 频道管理       │  │                │</span>
<span class="line">       │                        │              │  │ 消息广播       │  │                │</span>
<span class="line">       │                        │              │  │ 敏感词过滤     │  │                │</span>
<span class="line">       │                        │              │  └───────────────┘  │                │</span>
<span class="line">       │                        │              └─────────────────────┘                │</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="总结" tabindex="-1"><a class="header-anchor" href="#总结"><span>总结</span></a></h2><h3 id="进程间协作模式" tabindex="-1"><a class="header-anchor" href="#进程间协作模式"><span>进程间协作模式</span></a></h3><table><thead><tr><th>场景</th><th>涉及进程</th><th>协作方式</th></tr></thead><tbody><tr><td><strong>登录</strong></td><td>Client → LoginApp → GatewayApp → BaseApp → CellApp</td><td>链式转发</td></tr><tr><td><strong>移动</strong></td><td>Client → GatewayApp → CellApp</td><td>直接路由</td></tr><tr><td><strong>战斗</strong></td><td>Client → GatewayApp → CellApp ↔ CellApp</td><td>直接路由 + 跨App同步</td></tr><tr><td><strong>聊天</strong></td><td>Client → GatewayApp → ChatApp</td><td>直接路由</td></tr><tr><td><strong>下线</strong></td><td>GatewayApp → CellApp → BaseApp</td><td>并行保存</td></tr></tbody></table><h3 id="关键设计要点" tabindex="-1"><a class="header-anchor" href="#关键设计要点"><span>关键设计要点</span></a></h3><ol><li><strong>GatewayApp是无状态的</strong> - 可以水平扩展，负载均衡器随意分配</li><li><strong>CellApp按空间分割</strong> - 玩家根据位置自动路由到对应CellApp</li><li><strong>BaseApp专注数据</strong> - 异步处理数据库IO，不阻塞游戏逻辑</li><li><strong>ChatApp独立服务</strong> - 聊天流量不影响游戏逻辑性能</li><li><strong>掉线保护机制</strong> - 给予玩家重连窗口，提升体验</li></ol>`,57)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};