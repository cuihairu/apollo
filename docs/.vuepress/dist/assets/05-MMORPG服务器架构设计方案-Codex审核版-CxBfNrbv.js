import{i as e,r as t,s as n,t as r}from"./app-DuH0mZgh.js";var i=JSON.parse(`{"path":"/05-MMORPG%E6%9C%8D%E5%8A%A1%E5%99%A8%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1%E6%96%B9%E6%A1%88-Codex%E5%AE%A1%E6%A0%B8%E7%89%88.html","title":"MMORPG 服务器架构设计方案 (Codex 审核版)","lang":"en-US","frontmatter":{},"filePathRelative":"05-MMORPG服务器架构设计方案-Codex审核版.md","git":{"createdTime":1764754310000,"updatedTime":1767059850000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":2,"url":"https://github.com/cuihairu"}]}}`),a={name:`05-MMORPG服务器架构设计方案-Codex审核版.md`};function o(r,i,a,o,s,c){return n(),t(`div`,null,[...i[0]||=[e(`<h1 id="mmorpg-服务器架构设计方案-codex-审核版" tabindex="-1"><a class="header-anchor" href="#mmorpg-服务器架构设计方案-codex-审核版"><span>MMORPG 服务器架构设计方案 (Codex 审核版)</span></a></h1><blockquote><p><strong>版本</strong>: 2.0.0 <strong>日期</strong>: 2024-12-03 <strong>状态</strong>: 待审核 <strong>审核方</strong>: Codex</p></blockquote><hr><h2 id="目录" tabindex="-1"><a class="header-anchor" href="#目录"><span>目录</span></a></h2><ol><li><a href="#1-%E6%9E%B6%E6%9E%84%E6%A6%82%E8%BF%B0">架构概述</a></li><li><a href="#2-%E6%9C%8D%E5%8A%A1%E6%8B%93%E6%89%91%E8%AE%BE%E8%AE%A1">服务拓扑设计</a></li><li><a href="#3-%E6%A0%B8%E5%BF%83%E6%8A%80%E6%9C%AF%E5%86%B3%E7%AD%96">核心技术决策</a></li><li><a href="#4-%E9%80%9A%E4%BF%A1%E6%9E%B6%E6%9E%84">通信架构</a></li><li><a href="#5-%E6%95%B0%E6%8D%AE%E6%9E%B6%E6%9E%84">数据架构</a></li><li><a href="#6-%E5%B1%9E%E6%80%A7%E4%B8%8E%E5%90%8C%E6%AD%A5%E7%B3%BB%E7%BB%9F">属性与同步系统</a></li><li><a href="#7-%E5%9C%BA%E6%99%AF%E4%B8%8Eaoi%E7%B3%BB%E7%BB%9F">场景与AOI系统</a></li><li><a href="#8-%E9%AB%98%E5%8F%AF%E7%94%A8%E4%B8%8E%E5%AE%B9%E7%81%BE">高可用与容灾</a></li><li><a href="#9-%E6%80%A7%E8%83%BD%E6%8C%87%E6%A0%87%E4%B8%8E%E7%BA%A6%E6%9D%9F">性能指标与约束</a></li><li><a href="#10-%E5%AE%9E%E6%96%BD%E8%B7%AF%E7%BA%BF%E5%9B%BE">实施路线图</a></li><li><a href="#11-%E5%BE%85%E5%AE%A1%E6%A0%B8%E9%97%AE%E9%A2%98%E6%B8%85%E5%8D%95">待审核问题清单</a></li></ol><hr><h2 id="_1-架构概述" tabindex="-1"><a class="header-anchor" href="#_1-架构概述"><span>1. 架构概述</span></a></h2><h3 id="_1-1-设计目标" tabindex="-1"><a class="header-anchor" href="#_1-1-设计目标"><span>1.1 设计目标</span></a></h3><table><thead><tr><th>目标</th><th>指标</th><th>说明</th></tr></thead><tbody><tr><td><strong>单服承载</strong></td><td>5000+ CCU</td><td>单 GameServer 并发在线</td></tr><tr><td><strong>响应延迟</strong></td><td>P99 &lt; 50ms</td><td>核心操作响应时间</td></tr><tr><td><strong>可用性</strong></td><td>99.9%</td><td>年度可用时间</td></tr><tr><td><strong>水平扩展</strong></td><td>线性扩展</td><td>通过增加节点提升容量</td></tr><tr><td><strong>热更新</strong></td><td>秒级生效</td><td>Lua 脚本 + 配置热更新</td></tr></tbody></table><h3 id="_1-2-架构原则" tabindex="-1"><a class="header-anchor" href="#_1-2-架构原则"><span>1.2 架构原则</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌────────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                           MMORPG 架构设计原则                               │</span>
<span class="line">├────────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                            │</span>
<span class="line">│  1. 无状态优先 (Stateless First)                                           │</span>
<span class="line">│     • 网关层完全无状态，可任意水平扩展                                       │</span>
<span class="line">│     • 游戏逻辑层状态集中在内存，定期持久化                                   │</span>
<span class="line">│                                                                            │</span>
<span class="line">│  2. 分层解耦 (Layered Architecture)                                        │</span>
<span class="line">│     • 接入层 → 逻辑层 → 数据层，职责清晰                                    │</span>
<span class="line">│     • 层间通过 Protobuf 协议通信，可独立演进                                │</span>
<span class="line">│                                                                            │</span>
<span class="line">│  3. 异步驱动 (Async-First)                                                 │</span>
<span class="line">│     • 所有 I/O 操作异步化 (IOCP/io_uring)                                  │</span>
<span class="line">│     • 避免阻塞主逻辑线程                                                    │</span>
<span class="line">│                                                                            │</span>
<span class="line">│  4. 故障隔离 (Fault Isolation)                                             │</span>
<span class="line">│     • 单服务器故障不影响其他服务器                                          │</span>
<span class="line">│     • 场景故障仅影响当前场景玩家                                            │</span>
<span class="line">│                                                                            │</span>
<span class="line">│  5. 数据最终一致 (Eventual Consistency)                                    │</span>
<span class="line">│     • 游戏内存态为准，异步落库                                              │</span>
<span class="line">│     • 关键操作 (充值/交易) 同步落库                                         │</span>
<span class="line">│                                                                            │</span>
<span class="line">└────────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-3-整体架构图" tabindex="-1"><a class="header-anchor" href="#_1-3-整体架构图"><span>1.3 整体架构图</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">                                 ┌──────────────┐</span>
<span class="line">                                 │   Client     │</span>
<span class="line">                                 │   (Unity)    │</span>
<span class="line">                                 └──────┬───────┘</span>
<span class="line">                                        │ TCP/WebSocket + Protobuf</span>
<span class="line">                         ┌──────────────┴──────────────┐</span>
<span class="line">                         ▼                             ▼</span>
<span class="line">              ┌─────────────────┐           ┌─────────────────┐</span>
<span class="line">              │   LoginGate     │           │   GameGate      │</span>
<span class="line">              │   (端口: 7100)  │           │   (端口: 7700)  │</span>
<span class="line">              │   • 认证转发    │           │   • 连接管理     │</span>
<span class="line">              │   • 版本校验    │           │   • 消息路由     │</span>
<span class="line">              │   • 限流保护    │           │   • 负载均衡     │</span>
<span class="line">              └────────┬────────┘           └────────┬────────┘</span>
<span class="line">                       │                             │</span>
<span class="line">           ┌───────────┴───────────┐     ┌───────────┴───────────┐</span>
<span class="line">           ▼                       ▼     ▼                       ▼</span>
<span class="line">    ┌─────────────┐         ┌─────────────┐               ┌─────────────┐</span>
<span class="line">    │ LoginServer │         │ GameServer  │ ◄───────────► │ GameServer  │</span>
<span class="line">    │ (端口: 7200)│         │ (端口: 7601)│   场景迁移    │ (端口: 7602)│</span>
<span class="line">    │             │         │             │               │             │</span>
<span class="line">    │ • 账号认证  │         │ • 游戏逻辑  │               │ • 游戏逻辑  │</span>
<span class="line">    │ • Token生成 │         │ • Lua脚本   │               │ • Lua脚本   │</span>
<span class="line">    │ • 排队管理  │         │ • 战斗系统  │               │ • 战斗系统  │</span>
<span class="line">    └──────┬──────┘         └──────┬──────┘               └──────┬──────┘</span>
<span class="line">           │                       │                             │</span>
<span class="line">           │               ┌───────┴───────────────────────┬─────┘</span>
<span class="line">           │               ▼                               ▼</span>
<span class="line">           │        ┌─────────────┐                 ┌─────────────┐</span>
<span class="line">           │        │ NewMServer  │                 │ WorldServer │</span>
<span class="line">           │        │ (端口: 7500)│                 │ (端口: 8866)│</span>
<span class="line">           │        │             │                 │             │</span>
<span class="line">           │        │ • 公会系统  │                 │ • 跨服协调  │</span>
<span class="line">           │        │ • 组队系统  │                 │ • 全局排行  │</span>
<span class="line">           │        │ • 交易系统  │                 │ • 匹配系统  │</span>
<span class="line">           │        │ • 邮件系统  │                 │ • 跨服活动  │</span>
<span class="line">           │        └──────┬──────┘                 └──────┬──────┘</span>
<span class="line">           │               │                               │</span>
<span class="line">           ▼               ▼                               ▼</span>
<span class="line">    ┌────────────────────────────────────────────────────────────┐</span>
<span class="line">    │                      DBServer Cluster                       │</span>
<span class="line">    │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │</span>
<span class="line">    │  │ DBServer-1  │  │ DBServer-2  │  │ DBServer-N  │         │</span>
<span class="line">    │  │ (端口: 7300)│  │ (端口: 7301)│  │ (端口: 730N)│         │</span>
<span class="line">    │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │</span>
<span class="line">    │         │                │                │                 │</span>
<span class="line">    │         └────────────────┼────────────────┘                 │</span>
<span class="line">    │                          ▼                                  │</span>
<span class="line">    │  ┌──────────────────────────────────────────────────────┐  │</span>
<span class="line">    │  │              Redis Cluster (缓存层)                   │  │</span>
<span class="line">    │  │  • 会话缓存  • 排行榜  • 分布式锁  • 消息队列         │  │</span>
<span class="line">    │  └──────────────────────────────────────────────────────┘  │</span>
<span class="line">    │                          │                                  │</span>
<span class="line">    │                          ▼                                  │</span>
<span class="line">    │  ┌──────────────────────────────────────────────────────┐  │</span>
<span class="line">    │  │              MySQL Cluster (持久层)                   │  │</span>
<span class="line">    │  │  • 主从复制  • 读写分离  • 分库分表                    │  │</span>
<span class="line">    │  └──────────────────────────────────────────────────────┘  │</span>
<span class="line">    └────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="_2-服务拓扑设计" tabindex="-1"><a class="header-anchor" href="#_2-服务拓扑设计"><span>2. 服务拓扑设计</span></a></h2><h3 id="_2-1-服务清单与职责" tabindex="-1"><a class="header-anchor" href="#_2-1-服务清单与职责"><span>2.1 服务清单与职责</span></a></h3><table><thead><tr><th>服务</th><th>数量</th><th>端口</th><th>核心职责</th><th>有状态</th></tr></thead><tbody><tr><td><strong>LoginGate</strong></td><td>2+</td><td>7100</td><td>登录连接、版本校验、限流</td><td>否</td></tr><tr><td><strong>LoginServer</strong></td><td>2+</td><td>7200</td><td>账号认证、Token 生成</td><td>否</td></tr><tr><td><strong>GameGate</strong></td><td>4+</td><td>7700+</td><td>游戏连接、消息路由、负载均衡</td><td>否</td></tr><tr><td><strong>GameServer</strong></td><td>10+</td><td>7600+</td><td>核心游戏逻辑、场景管理</td><td>是</td></tr><tr><td><strong>NewMServer</strong></td><td>2+</td><td>7500</td><td>公会/组队/交易/邮件</td><td>是</td></tr><tr><td><strong>WorldServer</strong></td><td>1</td><td>8866</td><td>跨服协调、全局状态</td><td>是</td></tr><tr><td><strong>ChatServer</strong></td><td>2+</td><td>9000+</td><td>聊天系统、消息审核</td><td>否</td></tr><tr><td><strong>DBServer</strong></td><td>4+</td><td>7300+</td><td>数据持久化、缓存管理</td><td>否</td></tr><tr><td><strong>LogServer</strong></td><td>2+</td><td>7800</td><td>日志收集、埋点统计</td><td>否</td></tr></tbody></table><h3 id="_2-2-serverid-设计-64位" tabindex="-1"><a class="header-anchor" href="#_2-2-serverid-设计-64位"><span>2.2 ServerID 设计 (64位)</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    ServerID 位段布局 (64-bit uint64_t)                    │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌────────────────┬────────────────┬────────────────┬────────────────┐ │</span>
<span class="line">│  │     Region     │      Group     │      Type      │    Instance    │ │</span>
<span class="line">│  │    (16-bit)    │    (16-bit)    │    (16-bit)    │    (16-bit)    │ │</span>
<span class="line">│  ├────────────────┼────────────────┼────────────────┼────────────────┤ │</span>
<span class="line">│  │     63-48      │     47-32      │     31-16      │      15-0      │ │</span>
<span class="line">│  └────────────────┴────────────────┴────────────────┴────────────────┘ │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Region  (0-65535): 物理区域/机房 (华东/华南/美西...)                    │</span>
<span class="line">│  Group   (0-65535): 逻辑分组/大区 (1服/2服/合服后的组...)               │</span>
<span class="line">│  Type    (0-65535): 服务类型 (见下表)                                   │</span>
<span class="line">│  Instance(0-65535): 实例编号 (同类型服务的编号)                          │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  示例: 0x0001_0002_0006_0001                                            │</span>
<span class="line">│        Region=1, Group=2, Type=6(GameServer), Instance=1                │</span>
<span class="line">│        字符串格式: &quot;1-2-6-1&quot;                                            │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>服务类型枚举</strong>:</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">ServerType</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">uint16_t</span></span> <span class="token punctuation">{</span></span>
<span class="line">    UNKNOWN         <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">    LOGIN_GATE      <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">,</span>    <span class="token comment">// 登录网关</span></span>
<span class="line">    LOGIN_SERVER    <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">,</span>    <span class="token comment">// 登录服务器</span></span>
<span class="line">    DB_SERVER       <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">,</span>    <span class="token comment">// 数据库服务器</span></span>
<span class="line">    GAME_GATE       <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">,</span>    <span class="token comment">// 游戏网关</span></span>
<span class="line">    NEWM_SERVER     <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">,</span>    <span class="token comment">// 辅助服务器</span></span>
<span class="line">    GAME_SERVER     <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">,</span>    <span class="token comment">// 游戏逻辑服务器</span></span>
<span class="line">    LOG_SERVER      <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">,</span>    <span class="token comment">// 日志服务器</span></span>
<span class="line">    WORLD_SERVER    <span class="token operator">=</span> <span class="token number">8</span><span class="token punctuation">,</span>    <span class="token comment">// 跨服世界服务器</span></span>
<span class="line">    CHAT_SERVER     <span class="token operator">=</span> <span class="token number">9</span><span class="token punctuation">,</span>    <span class="token comment">// 聊天服务器</span></span>
<span class="line">    AGENT_SERVER    <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">,</span>   <span class="token comment">// 代理服务器</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-服务发现与注册" tabindex="-1"><a class="header-anchor" href="#_2-3-服务发现与注册"><span>2.3 服务发现与注册</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                        服务注册与发现流程                                 │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌─────────────┐                      ┌─────────────────────┐          │</span>
<span class="line">│  │ GameServer  │ ──── 1. 注册 ────►   │     Registry        │          │</span>
<span class="line">│  │   启动      │                      │  (Redis / etcd)     │          │</span>
<span class="line">│  └─────────────┘                      │                     │          │</span>
<span class="line">│                                       │  存储内容:           │          │</span>
<span class="line">│  注册内容:                             │  • ServerId         │          │</span>
<span class="line">│  {                                    │  • Host:Port        │          │</span>
<span class="line">│    &quot;server_id&quot;: &quot;1-2-6-1&quot;,           │  • UnixSocketPath   │          │</span>
<span class="line">│    &quot;host&quot;: &quot;10.0.1.50&quot;,              │  • Status           │          │</span>
<span class="line">│    &quot;port&quot;: 7601,                     │  • Load             │          │</span>
<span class="line">│    &quot;unix_socket&quot;: &quot;/var/run/gs.sock&quot;,│  • LastHeartbeat    │          │</span>
<span class="line">│    &quot;status&quot;: &quot;running&quot;,              │                     │          │</span>
<span class="line">│    &quot;load&quot;: 1200,                     └──────────┬──────────┘          │</span>
<span class="line">│    &quot;version&quot;: &quot;1.2.3&quot;                           │                      │</span>
<span class="line">│  }                                              │                      │</span>
<span class="line">│                                                 │ 2. 监听变化           │</span>
<span class="line">│  ┌─────────────┐                               │                      │</span>
<span class="line">│  │  GameGate   │ ◄────────────────────────────┘                      │</span>
<span class="line">│  │             │                                                       │</span>
<span class="line">│  │ 3. 路由决策 │ ─── 根据负载选择目标 GameServer ──►                   │</span>
<span class="line">│  └─────────────┘                                                       │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  心跳机制:                                                              │</span>
<span class="line">│  • 间隔: 5秒                                                           │</span>
<span class="line">│  • 超时: 30秒无心跳标记为下线                                           │</span>
<span class="line">│  • 内容: 当前负载(在线人数)、内存使用、CPU使用                          │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="_3-核心技术决策" tabindex="-1"><a class="header-anchor" href="#_3-核心技术决策"><span>3. 核心技术决策</span></a></h2><h3 id="_3-1-技术栈选型" tabindex="-1"><a class="header-anchor" href="#_3-1-技术栈选型"><span>3.1 技术栈选型</span></a></h3><table><thead><tr><th>层次</th><th>选择</th><th>版本</th><th>理由</th></tr></thead><tbody><tr><td><strong>语言</strong></td><td>C++20</td><td>-</td><td>性能、成熟度、生态</td></tr><tr><td><strong>构建</strong></td><td>CMake + vcpkg</td><td>3.21+</td><td>跨平台、依赖管理</td></tr><tr><td><strong>序列化</strong></td><td>Protobuf</td><td>3.21+</td><td>高效、版本兼容</td></tr><tr><td><strong>日志</strong></td><td>spdlog</td><td>1.12+</td><td>高性能、异步</td></tr><tr><td><strong>脚本</strong></td><td>Lua + sol2</td><td>5.4.6 / 3.3+</td><td>热更新、性能</td></tr><tr><td><strong>数据库</strong></td><td>MySQL</td><td>8.0+</td><td>成熟稳定</td></tr><tr><td><strong>缓存</strong></td><td>Redis</td><td>7.0+</td><td>高性能、丰富数据结构</td></tr><tr><td><strong>网络</strong></td><td>IOCP/io_uring</td><td>原生</td><td>平台最优</td></tr></tbody></table><h3 id="_3-2-异步-i-o-模型选择" tabindex="-1"><a class="header-anchor" href="#_3-2-异步-i-o-模型选择"><span>3.2 异步 I/O 模型选择</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                     平台异步 I/O 模型选择策略                             │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌───────────────────────────────────────────────────────────────────┐ │</span>
<span class="line">│  │                        运行时检测流程                              │ │</span>
<span class="line">│  ├───────────────────────────────────────────────────────────────────┤ │</span>
<span class="line">│  │                                                                   │ │</span>
<span class="line">│  │  if (Windows)                                                     │ │</span>
<span class="line">│  │      → IOCP (完成端口)                                            │ │</span>
<span class="line">│  │                                                                   │ │</span>
<span class="line">│  │  else if (Linux)                                                  │ │</span>
<span class="line">│  │      kernel_version = uname()                                     │ │</span>
<span class="line">│  │      if (kernel &gt;= 5.10 &amp;&amp; io_uring_available())                 │ │</span>
<span class="line">│  │          → io_uring (提交/完成队列)                               │ │</span>
<span class="line">│  │      else                                                         │ │</span>
<span class="line">│  │          → epoll (事件驱动)                                       │ │</span>
<span class="line">│  │                                                                   │ │</span>
<span class="line">│  └───────────────────────────────────────────────────────────────────┘ │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  性能对比 (10K 并发连接):                                               │</span>
<span class="line">│  ┌─────────────┬─────────────┬─────────────┬─────────────┐             │</span>
<span class="line">│  │   指标      │    IOCP     │   epoll     │  io_uring   │             │</span>
<span class="line">│  ├─────────────┼─────────────┼─────────────┼─────────────┤             │</span>
<span class="line">│  │ 吞吐量      │   100%      │   100%      │   130-150%  │             │</span>
<span class="line">│  │ 延迟 P99    │   ~1ms      │   ~1ms      │   ~0.7ms    │             │</span>
<span class="line">│  │ CPU 占用    │   ~30%      │   ~35%      │   ~25%      │             │</span>
<span class="line">│  │ 系统调用    │   N/批      │   N/批      │   1/批      │             │</span>
<span class="line">│  └─────────────┴─────────────┴─────────────┴─────────────┘             │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-线程模型" tabindex="-1"><a class="header-anchor" href="#_3-3-线程模型"><span>3.3 线程模型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                     GameServer 线程模型                                   │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                     主线程 (Main Thread)                         │   │</span>
<span class="line">│  │  • 游戏主循环 (固定帧率 20Hz / 50ms)                              │   │</span>
<span class="line">│  │  • 消息处理                                                      │   │</span>
<span class="line">│  │  • 定时器调度                                                    │   │</span>
<span class="line">│  │  • Lua 脚本执行                                                  │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                   网络 I/O 线程池 (4-8 线程)                      │   │</span>
<span class="line">│  │  • 异步收发数据                                                   │   │</span>
<span class="line">│  │  • 数据包解析                                                    │   │</span>
<span class="line">│  │  • 投递到主线程队列                                               │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                   数据库 I/O 线程池 (4-8 线程)                    │   │</span>
<span class="line">│  │  • 异步数据库查询                                                 │   │</span>
<span class="line">│  │  • 结果回调到主线程                                               │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                   日志线程 (1 线程)                               │   │</span>
<span class="line">│  │  • 异步日志写入                                                   │   │</span>
<span class="line">│  │  • 日志轮转                                                      │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  关键约束:                                                              │</span>
<span class="line">│  • 游戏逻辑只在主线程执行 (单线程模型)                                  │</span>
<span class="line">│  • 跨线程通信使用无锁队列 (MPSC Queue)                                 │</span>
<span class="line">│  • 避免主线程阻塞 (所有 I/O 异步化)                                    │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="_4-通信架构" tabindex="-1"><a class="header-anchor" href="#_4-通信架构"><span>4. 通信架构</span></a></h2><h3 id="_4-1-协议分层" tabindex="-1"><a class="header-anchor" href="#_4-1-协议分层"><span>4.1 协议分层</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                         协议分层设计                                      │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                    应用层 (Protobuf Message)                     │   │</span>
<span class="line">│  │  • 业务消息定义                                                   │   │</span>
<span class="line">│  │  • 自动序列化/反序列化                                            │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                              │                                          │</span>
<span class="line">│                              ▼                                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                   消息层 (Packet Header)                         │   │</span>
<span class="line">│  │  ┌─────────┬─────────┬─────────┬─────────┬─────────┐            │   │</span>
<span class="line">│  │  │ Length  │  MsgID  │   Seq   │  Time   │Checksum │            │   │</span>
<span class="line">│  │  │ 2 bytes │ 2 bytes │ 4 bytes │ 4 bytes │ 2 bytes │            │   │</span>
<span class="line">│  │  └─────────┴─────────┴─────────┴─────────┴─────────┘            │   │</span>
<span class="line">│  │  Total Header: 14 bytes                                          │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                              │                                          │</span>
<span class="line">│                              ▼                                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                    传输层 (TCP / WebSocket)                      │   │</span>
<span class="line">│  │  • 可靠传输                                                      │   │</span>
<span class="line">│  │  • 粘包/拆包处理                                                 │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-消息路由" tabindex="-1"><a class="header-anchor" href="#_4-2-消息路由"><span>4.2 消息路由</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      GameGate 消息路由流程                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Client → GameGate:                                                     │</span>
<span class="line">│  ┌────────────────────────────────────────────────────────────────┐    │</span>
<span class="line">│  │ 1. 接收客户端数据包                                              │    │</span>
<span class="line">│  │ 2. 解析包头，获取 MsgID                                          │    │</span>
<span class="line">│  │ 3. 根据 MsgID 查路由表:                                          │    │</span>
<span class="line">│  │    • 0x0001-0x0FFF: 转发到 GameServer                           │    │</span>
<span class="line">│  │    • 0x1000-0x1FFF: 转发到 NewMServer                           │    │</span>
<span class="line">│  │    • 0x2000-0x2FFF: 转发到 ChatServer                           │    │</span>
<span class="line">│  │    • 0x3000-0x3FFF: 转发到 WorldServer                          │    │</span>
<span class="line">│  │ 4. 附加 SessionID + AccountID 到包头                             │    │</span>
<span class="line">│  │ 5. 转发到目标服务器                                              │    │</span>
<span class="line">│  └────────────────────────────────────────────────────────────────┘    │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  GameServer → Client:                                                   │</span>
<span class="line">│  ┌────────────────────────────────────────────────────────────────┐    │</span>
<span class="line">│  │ 1. 接收 GameServer 响应                                          │    │</span>
<span class="line">│  │ 2. 解析目标 SessionID                                            │    │</span>
<span class="line">│  │ 3. 查找对应的客户端连接                                          │    │</span>
<span class="line">│  │ 4. 转发数据到客户端                                              │    │</span>
<span class="line">│  └────────────────────────────────────────────────────────────────┘    │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  负载均衡策略 (新玩家分配):                                             │</span>
<span class="line">│  ┌────────────────────────────────────────────────────────────────┐    │</span>
<span class="line">│  │ 1. 获取所有健康的 GameServer 列表                                │    │</span>
<span class="line">│  │ 2. 过滤掉负载超过 80% 的服务器                                   │    │</span>
<span class="line">│  │ 3. 加权随机选择 (权重 = maxLoad - currentLoad)                  │    │</span>
<span class="line">│  │ 4. 返回选中的 GameServer ID                                      │    │</span>
<span class="line">│  └────────────────────────────────────────────────────────────────┘    │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-消息自动注册" tabindex="-1"><a class="header-anchor" href="#_4-3-消息自动注册"><span>4.3 消息自动注册</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 消息处理器接口</span></span>
<span class="line"><span class="token keyword">template</span><span class="token operator">&lt;</span><span class="token keyword">typename</span> <span class="token class-name">T</span><span class="token operator">&gt;</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">IMessageHandler</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">void</span> <span class="token function">Handle</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> sessionId<span class="token punctuation">,</span> <span class="token keyword">const</span> T<span class="token operator">&amp;</span> message<span class="token punctuation">)</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">virtual</span> <span class="token keyword">uint16_t</span> <span class="token function">GetMessageId</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 自动注册宏</span></span>
<span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">define</span> <span class="token macro-name function">REGISTER_MESSAGE_HANDLER</span><span class="token expression"><span class="token punctuation">(</span>MsgType<span class="token punctuation">,</span> HandlerClass<span class="token punctuation">)</span> </span><span class="token punctuation">\\</span></span>
<span class="line">    <span class="token expression"><span class="token keyword">namespace</span> <span class="token punctuation">{</span> </span><span class="token punctuation">\\</span></span>
<span class="line">        <span class="token expression"><span class="token keyword">struct</span> <span class="token class-name">HandlerClass</span></span><span class="token punctuation">##</span><span class="token expression">_Registrar <span class="token punctuation">{</span> </span><span class="token punctuation">\\</span></span>
<span class="line">            <span class="token expression">HandlerClass</span><span class="token punctuation">##</span><span class="token expression"><span class="token function">_Registrar</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span> </span><span class="token punctuation">\\</span></span>
<span class="line">                <span class="token expression"><span class="token class-name">MessageDispatcher</span><span class="token double-colon punctuation">::</span><span class="token function">Instance</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">-&gt;</span><span class="token generic-function"><span class="token function">RegisterHandler</span><span class="token generic class-name"><span class="token operator">&lt;</span>MsgType<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span> </span><span class="token punctuation">\\</span></span>
<span class="line">                    <span class="token expression">MsgType<span class="token double-colon punctuation">::</span>MSG_ID<span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span><span class="token generic-function"><span class="token function">make_unique</span><span class="token generic class-name"><span class="token operator">&lt;</span>HandlerClass<span class="token operator">&gt;</span></span></span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span> </span><span class="token punctuation">\\</span></span>
<span class="line">            <span class="token expression"><span class="token punctuation">}</span> </span><span class="token punctuation">\\</span></span>
<span class="line">        <span class="token expression"><span class="token punctuation">}</span><span class="token punctuation">;</span> </span><span class="token punctuation">\\</span></span>
<span class="line">        <span class="token expression"><span class="token keyword">static</span> HandlerClass</span><span class="token punctuation">##</span><span class="token expression">_Registrar g_</span><span class="token punctuation">##</span><span class="token expression">HandlerClass</span><span class="token punctuation">##</span><span class="token expression">_registrar<span class="token punctuation">;</span> </span><span class="token punctuation">\\</span></span>
<span class="line">    <span class="token expression"><span class="token punctuation">}</span></span></span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用示例</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">PlayerMoveHandler</span> <span class="token operator">:</span> <span class="token base-clause"><span class="token keyword">public</span> <span class="token class-name">IMessageHandler</span><span class="token operator">&lt;</span><span class="token class-name">PlayerMove</span><span class="token operator">&gt;</span></span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">Handle</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> sessionId<span class="token punctuation">,</span> <span class="token keyword">const</span> PlayerMove<span class="token operator">&amp;</span> msg<span class="token punctuation">)</span> <span class="token keyword">override</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 处理玩家移动</span></span>
<span class="line">        Player<span class="token operator">*</span> player <span class="token operator">=</span> <span class="token class-name">PlayerManager</span><span class="token double-colon punctuation">::</span><span class="token function">Get</span><span class="token punctuation">(</span>sessionId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span>player<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            player<span class="token operator">-&gt;</span><span class="token function">MoveTo</span><span class="token punctuation">(</span>msg<span class="token punctuation">.</span><span class="token function">pos_x</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> msg<span class="token punctuation">.</span><span class="token function">pos_y</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> msg<span class="token punctuation">.</span><span class="token function">pos_z</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            <span class="token function">BroadcastToAOI</span><span class="token punctuation">(</span>player<span class="token punctuation">,</span> msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> <span class="token function">GetMessageId</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token keyword">override</span> <span class="token punctuation">{</span> <span class="token keyword">return</span> MSG_PLAYER_MOVE<span class="token punctuation">;</span> <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token function">REGISTER_MESSAGE_HANDLER</span><span class="token punctuation">(</span>PlayerMove<span class="token punctuation">,</span> PlayerMoveHandler<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-4-数据包去重" tabindex="-1"><a class="header-anchor" href="#_4-4-数据包去重"><span>4.4 数据包去重</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                        三层去重机制                                       │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Layer 1: 序列号去重 (滑动窗口)                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 窗口大小: 1024                                                │   │</span>
<span class="line">│  │  • 每个连接独立窗口                                              │   │</span>
<span class="line">│  │  • 拒绝: seq &lt; base 或 已标记                                    │   │</span>
<span class="line">│  │  • 复杂度: O(1)                                                  │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                              │                                          │</span>
<span class="line">│                              ▼ 通过                                     │</span>
<span class="line">│  Layer 2: 消息指纹去重 (CRC32 + LRU)                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 仅检查关键消息 (移动/技能/道具)                               │   │</span>
<span class="line">│  │  • 计算消息体 CRC32 指纹                                         │   │</span>
<span class="line">│  │  • LRU 缓存: 10000 条目                                          │   │</span>
<span class="line">│  │  • 过期时间: 60 秒                                               │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                              │                                          │</span>
<span class="line">│                              ▼ 通过                                     │</span>
<span class="line">│  Layer 3: 业务层幂等 (由具体业务实现)                                  │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 交易: 订单号唯一                                              │   │</span>
<span class="line">│  │  • 充值: 流水号唯一                                              │   │</span>
<span class="line">│  │  • 邮件: 邮件ID唯一                                              │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="_5-数据架构" tabindex="-1"><a class="header-anchor" href="#_5-数据架构"><span>5. 数据架构</span></a></h2><h3 id="_5-1-数据分层" tabindex="-1"><a class="header-anchor" href="#_5-1-数据分层"><span>5.1 数据分层</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                         数据分层架构                                      │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                   L0: 进程内存 (最快)                             │   │</span>
<span class="line">│  │  • 在线玩家数据                                                   │   │</span>
<span class="line">│  │  • 场景实体数据                                                   │   │</span>
<span class="line">│  │  • 热点配置缓存                                                   │   │</span>
<span class="line">│  │  访问延迟: &lt; 1μs                                                 │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                              │                                          │</span>
<span class="line">│                              ▼ 未命中                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                   L1: Redis 集群 (快)                             │   │</span>
<span class="line">│  │  • 会话数据 (Token, 登录状态)                                    │   │</span>
<span class="line">│  │  • 排行榜数据                                                    │   │</span>
<span class="line">│  │  • 公会/组队数据                                                 │   │</span>
<span class="line">│  │  • 分布式锁                                                      │   │</span>
<span class="line">│  │  访问延迟: &lt; 1ms                                                 │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                              │                                          │</span>
<span class="line">│                              ▼ 未命中                                   │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                   L2: MySQL 集群 (持久)                           │   │</span>
<span class="line">│  │  • 玩家账号数据                                                   │   │</span>
<span class="line">│  │  • 角色持久数据                                                   │   │</span>
<span class="line">│  │  • 交易/充值日志                                                  │   │</span>
<span class="line">│  │  访问延迟: 5-50ms                                                │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  数据流动:                                                              │</span>
<span class="line">│  • 登录: MySQL → Redis → Memory                                        │</span>
<span class="line">│  • 运行: Memory (主) + 定期同步到 Redis                                │</span>
<span class="line">│  • 下线: Memory → Redis → MySQL (异步)                                 │</span>
<span class="line">│  • 关键操作: Memory → MySQL (同步)                                     │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-数据库分库分表" tabindex="-1"><a class="header-anchor" href="#_5-2-数据库分库分表"><span>5.2 数据库分库分表</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       MySQL 分库分表策略                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  分库策略:                                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 按玩家ID哈希: player_id % 16 → db_0 ~ db_15                  │   │</span>
<span class="line">│  │  • 每库 4 个从库 (读写分离)                                      │   │</span>
<span class="line">│  │  • 主库: 写操作                                                  │   │</span>
<span class="line">│  │  • 从库: 读操作                                                  │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  分表策略:                                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  玩家数据表 (按ID哈希):                                          │   │</span>
<span class="line">│  │  • t_player_0 ~ t_player_255                                    │   │</span>
<span class="line">│  │  • 分片键: player_id % 256                                      │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  日志表 (按时间分区):                                            │   │</span>
<span class="line">│  │  • t_log_202412, t_log_202501, ...                              │   │</span>
<span class="line">│  │  • 按月自动创建新分区                                            │   │</span>
<span class="line">│  │  • 保留 6 个月，自动归档                                         │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  关键表设计:                                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  t_player (玩家基础)                                             │   │</span>
<span class="line">│  │  ├── player_id      BIGINT PK                                   │   │</span>
<span class="line">│  │  ├── account_id     BIGINT INDEX                                │   │</span>
<span class="line">│  │  ├── name           VARCHAR(32) UNIQUE                          │   │</span>
<span class="line">│  │  ├── level          INT                                         │   │</span>
<span class="line">│  │  ├── exp            BIGINT                                      │   │</span>
<span class="line">│  │  ├── attributes     BLOB (Protobuf)                             │   │</span>
<span class="line">│  │  ├── create_time    DATETIME                                    │   │</span>
<span class="line">│  │  └── update_time    DATETIME                                    │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  t_item (道具)                                                   │   │</span>
<span class="line">│  │  ├── item_id        BIGINT PK (雪花ID)                          │   │</span>
<span class="line">│  │  ├── player_id      BIGINT INDEX                                │   │</span>
<span class="line">│  │  ├── template_id    INT                                         │   │</span>
<span class="line">│  │  ├── count          INT                                         │   │</span>
<span class="line">│  │  ├── bind_type      TINYINT                                     │   │</span>
<span class="line">│  │  └── extra_data     JSON                                        │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-3-数据持久化策略" tabindex="-1"><a class="header-anchor" href="#_5-3-数据持久化策略"><span>5.3 数据持久化策略</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       数据持久化策略                                      │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌───────────────────┬───────────────────────────────────────────────┐ │</span>
<span class="line">│  │     数据类型      │              持久化策略                        │ │</span>
<span class="line">│  ├───────────────────┼───────────────────────────────────────────────┤ │</span>
<span class="line">│  │ 关键数据          │ • 同步写入 MySQL                              │ │</span>
<span class="line">│  │ (充值/交易/等级)  │ • 事务保证                                    │ │</span>
<span class="line">│  │                   │ • 失败重试 3 次                               │ │</span>
<span class="line">│  ├───────────────────┼───────────────────────────────────────────────┤ │</span>
<span class="line">│  │ 普通数据          │ • 异步批量写入                                │ │</span>
<span class="line">│  │ (经验/道具/位置)  │ • 5 秒定期刷新                               │ │</span>
<span class="line">│  │                   │ • 优先级队列                                  │ │</span>
<span class="line">│  ├───────────────────┼───────────────────────────────────────────────┤ │</span>
<span class="line">│  │ 下线保存          │ • 玩家下线时强制保存                          │ │</span>
<span class="line">│  │                   │ • 完整属性快照                                │ │</span>
<span class="line">│  │                   │ • 超时 10 秒强制断开                          │ │</span>
<span class="line">│  ├───────────────────┼───────────────────────────────────────────────┤ │</span>
<span class="line">│  │ 定时快照          │ • 每 30 分钟全量快照                          │ │</span>
<span class="line">│  │                   │ • 用于灾难恢复                                │ │</span>
<span class="line">│  │                   │ • 保留最近 24 个                              │ │</span>
<span class="line">│  └───────────────────┴───────────────────────────────────────────────┘ │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  异步保存队列:                                                          │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  优先级 0 (最高): 关键属性 (等级/经验/货币)                      │   │</span>
<span class="line">│  │  优先级 1:        装备变更                                       │   │</span>
<span class="line">│  │  优先级 2:        道具变更                                       │   │</span>
<span class="line">│  │  优先级 3 (最低): 位置/任务状态                                  │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  处理策略: 同一玩家的保存请求合并                                │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="_6-属性与同步系统" tabindex="-1"><a class="header-anchor" href="#_6-属性与同步系统"><span>6. 属性与同步系统</span></a></h2><h3 id="_6-1-属性分类" tabindex="-1"><a class="header-anchor" href="#_6-1-属性分类"><span>6.1 属性分类</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       属性分类体系 (100+ 属性)                           │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌───────────────┬─────────────┬─────────────────────────────────────┐ │</span>
<span class="line">│  │    分类       │   ID 范围   │            属性示例                  │ │</span>
<span class="line">│  ├───────────────┼─────────────┼─────────────────────────────────────┤ │</span>
<span class="line">│  │ 基础属性      │   1 - 99    │ HP, MP, SP, 移动速度                │ │</span>
<span class="line">│  │ 战斗属性      │ 100 - 199   │ 攻击力, 防御力, 暴击率, 命中率      │ │</span>
<span class="line">│  │ 元素属性      │ 200 - 299   │ 火/水/土/风/光/暗 抗性与伤害        │ │</span>
<span class="line">│  │ 状态属性      │ 300 - 399   │ 等级, 经验, 技能点, 声望            │ │</span>
<span class="line">│  │ 特殊属性      │ 400 - 499   │ 伤害加成, 治疗加成, 生命偷取        │ │</span>
<span class="line">│  │ PvP 属性      │ 500 - 599   │ PvP伤害加成, PvP减伤, 竞技积分      │ │</span>
<span class="line">│  │ 外观属性      │ 600 - 699   │ 武器外观, 时装, 坐骑, 翅膀          │ │</span>
<span class="line">│  │ 社交属性      │ 700 - 799   │ 公会ID, 队伍ID, 好友数, 婚姻状态    │ │</span>
<span class="line">│  │ 状态标记      │ 800 - 899   │ 在线, 战斗中, 死亡, 隐身, 眩晕      │ │</span>
<span class="line">│  └───────────────┴─────────────┴─────────────────────────────────────┘ │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-属性容器设计" tabindex="-1"><a class="header-anchor" href="#_6-2-属性容器设计"><span>6.2 属性容器设计</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 属性修饰器类型</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token keyword">class</span> <span class="token class-name">ModifierType</span> <span class="token punctuation">{</span></span>
<span class="line">    Add<span class="token punctuation">,</span>        <span class="token comment">// 加法: base + value</span></span>
<span class="line">    Multiply<span class="token punctuation">,</span>   <span class="token comment">// 乘法: base * (1 + value/10000)</span></span>
<span class="line">    FinalAdd<span class="token punctuation">,</span>   <span class="token comment">// 最终加法: (base * mul) + value</span></span>
<span class="line">    Override    <span class="token comment">// 覆盖: value</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 属性修饰器</span></span>
<span class="line"><span class="token keyword">struct</span> <span class="token class-name">AttributeModifier</span> <span class="token punctuation">{</span></span>
<span class="line">    ModifierType type<span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">int64_t</span> value<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string source<span class="token punctuation">;</span>   <span class="token comment">// 来源 (装备ID/BuffID/...)</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> expireTime<span class="token punctuation">;</span>  <span class="token comment">// 过期时间 (0=永久)</span></span>
<span class="line">    <span class="token keyword">bool</span> stackable<span class="token punctuation">;</span>       <span class="token comment">// 是否可叠加</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 属性容器</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AttributeContainer</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 基本操作</span></span>
<span class="line">    <span class="token keyword">int64_t</span> <span class="token function">Get</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> attrId<span class="token punctuation">)</span> <span class="token keyword">const</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">SetBase</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> attrId<span class="token punctuation">,</span> <span class="token keyword">int64_t</span> value<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 修饰器操作</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">AddModifier</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> attrId<span class="token punctuation">,</span> <span class="token keyword">const</span> AttributeModifier<span class="token operator">&amp;</span> mod<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">RemoveModifier</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> attrId<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> source<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 变化追踪</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>AttributeChange<span class="token operator">&gt;</span> <span class="token function">FlushChanges</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> <span class="token keyword">int64_t</span><span class="token operator">&gt;</span> baseValues_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> <span class="token keyword">int64_t</span><span class="token operator">&gt;</span> finalValues_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token punctuation">,</span> std<span class="token double-colon punctuation">::</span>vector<span class="token operator">&lt;</span>AttributeModifier<span class="token operator">&gt;&gt;</span> modifiers_<span class="token punctuation">;</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>set<span class="token operator">&lt;</span><span class="token keyword">uint32_t</span><span class="token operator">&gt;</span> dirtyAttributes_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 重算最终值</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">Recalculate</span><span class="token punctuation">(</span><span class="token keyword">uint32_t</span> attrId<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-3-同步标记系统" tabindex="-1"><a class="header-anchor" href="#_6-3-同步标记系统"><span>6.3 同步标记系统</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       属性同步标记 (8 种, 可组合)                        │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌────────────────┬────────────┬─────────────────────────────────────┐ │</span>
<span class="line">│  │     标记       │    值      │              说明                    │ │</span>
<span class="line">│  ├────────────────┼────────────┼─────────────────────────────────────┤ │</span>
<span class="line">│  │ SYNC_NONE      │  0x0000    │ 不同步                              │ │</span>
<span class="line">│  │ SYNC_APPR      │  0x0001    │ 外观变化 → AOI 广播                 │ │</span>
<span class="line">│  │ SYNC_PROP      │  0x0002    │ 属性变更 → AOI 广播                 │ │</span>
<span class="line">│  │ SYNC_DB        │  0x0004    │ 需要保存数据库                      │ │</span>
<span class="line">│  │ SYNC_SELF      │  0x0008    │ 只同步给自己                        │ │</span>
<span class="line">│  │ SYNC_TEAM      │  0x0010    │ 同步给队伍                          │ │</span>
<span class="line">│  │ SYNC_GUILD     │  0x0020    │ 同步给公会                          │ │</span>
<span class="line">│  │ SYNC_WORLD     │  0x0040    │ 全服广播                            │ │</span>
<span class="line">│  │ SYNC_IMMEDIATE │  0x0080    │ 立即同步 (跳过批量)                 │ │</span>
<span class="line">│  └────────────────┴────────────┴─────────────────────────────────────┘ │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  示例配置:                                                              │</span>
<span class="line">│  ┌────────────────┬───────────────────────────────────────────────────┐ │</span>
<span class="line">│  │    属性        │              同步标记                              │ │</span>
<span class="line">│  ├────────────────┼───────────────────────────────────────────────────┤ │</span>
<span class="line">│  │ HP             │ SYNC_PROP | SYNC_DB                              │ │</span>
<span class="line">│  │ 等级           │ SYNC_PROP | SYNC_DB | SYNC_TEAM | SYNC_WORLD     │ │</span>
<span class="line">│  │ 金币           │ SYNC_SELF | SYNC_DB                              │ │</span>
<span class="line">│  │ 武器外观       │ SYNC_APPR | SYNC_DB | SYNC_IMMEDIATE             │ │</span>
<span class="line">│  │ 死亡状态       │ SYNC_PROP | SYNC_IMMEDIATE                       │ │</span>
<span class="line">│  │ 位置           │ SYNC_PROP (AOI广播, 不存库)                      │ │</span>
<span class="line">│  └────────────────┴───────────────────────────────────────────────────┘ │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-4-差分同步优化" tabindex="-1"><a class="header-anchor" href="#_6-4-差分同步优化"><span>6.4 差分同步优化</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       属性差分同步流程                                    │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  1. 变化检测                                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 属性变化时标记 dirty                                          │   │</span>
<span class="line">│  │  • 记录 (attrId, oldValue, newValue, timestamp)                 │   │</span>
<span class="line">│  │  • 存入待同步队列                                                │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                              │                                          │</span>
<span class="line">│                              ▼                                          │</span>
<span class="line">│  2. 批量处理 (50ms 周期)                                               │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 合并同一属性的多次变化                                        │   │</span>
<span class="line">│  │  • 过滤变化量过小的属性 (阈值检查)                               │   │</span>
<span class="line">│  │  • 按同步范围分组 (AOI/Team/Guild/...)                          │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                              │                                          │</span>
<span class="line">│                              ▼                                          │</span>
<span class="line">│  3. 消息构建                                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  message AttributeUpdate {                                       │   │</span>
<span class="line">│  │      uint32 player_id = 1;                                      │   │</span>
<span class="line">│  │      repeated AttrChange changes = 2;                           │   │</span>
<span class="line">│  │      uint64 snapshot_version = 3;  // 用于乱序检测               │   │</span>
<span class="line">│  │  }                                                               │   │</span>
<span class="line">│  │  message AttrChange {                                            │   │</span>
<span class="line">│  │      uint32 attr_id = 1;                                        │   │</span>
<span class="line">│  │      int64 value = 2;       // 新值 (差分则只传差值)             │   │</span>
<span class="line">│  │      uint32 change_type = 3; // 0=全量, 1=增量                   │   │</span>
<span class="line">│  │  }                                                               │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                              │                                          │</span>
<span class="line">│                              ▼                                          │</span>
<span class="line">│  4. 智能广播                                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 高优先级 (HP/死亡/外观): 立即广播                             │   │</span>
<span class="line">│  │  • 普通优先级: 合并后广播                                        │   │</span>
<span class="line">│  │  • 低优先级 (移动/非关键): 可丢弃最旧的                          │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="_7-场景与-aoi-系统" tabindex="-1"><a class="header-anchor" href="#_7-场景与-aoi-系统"><span>7. 场景与 AOI 系统</span></a></h2><h3 id="_7-1-场景架构" tabindex="-1"><a class="header-anchor" href="#_7-1-场景架构"><span>7.1 场景架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                         场景架构设计                                      │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  场景类型:                                                              │</span>
<span class="line">│  ┌───────────────┬─────────────────────────────────────────────────────┐│</span>
<span class="line">│  │ 主城场景      │ 单实例, 高并发, 全服唯一                            ││</span>
<span class="line">│  │ 野外场景      │ 单实例或多实例 (人多时分线)                         ││</span>
<span class="line">│  │ 副本场景      │ 动态创建, 玩家进入时生成                            ││</span>
<span class="line">│  │ 竞技场景      │ 动态创建, 比赛结束销毁                              ││</span>
<span class="line">│  │ 跨服场景      │ 由 WorldServer 管理                                 ││</span>
<span class="line">│  └───────────────┴─────────────────────────────────────────────────────┘│</span>
<span class="line">│                                                                         │</span>
<span class="line">│  场景分线:                                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  当场景人数 &gt; 阈值 (如 500) 时自动分线:                          │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  主城场景 #1001                                                  │   │</span>
<span class="line">│  │    ├── 1线 (人数: 450)                                          │   │</span>
<span class="line">│  │    ├── 2线 (人数: 380)                                          │   │</span>
<span class="line">│  │    └── 3线 (人数: 210)                                          │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  玩家选择:                                                        │   │</span>
<span class="line">│  │  • 默认进入人数最少的线                                          │   │</span>
<span class="line">│  │  • 可手动切换线 (CD: 30秒)                                       │   │</span>
<span class="line">│  │  • 组队/好友优先同线                                             │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-2-aoi-area-of-interest-系统" tabindex="-1"><a class="header-anchor" href="#_7-2-aoi-area-of-interest-系统"><span>7.2 AOI (Area of Interest) 系统</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                        AOI 九宫格算法                                     │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  网格划分:                                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │    地图大小: 10000 x 10000                                       │   │</span>
<span class="line">│  │    格子大小: 100 x 100                                           │   │</span>
<span class="line">│  │    格子数量: 100 x 100 = 10000 格                                │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │    ┌─────┬─────┬─────┬─────┬─────┐                              │   │</span>
<span class="line">│  │    │     │     │     │     │     │                              │   │</span>
<span class="line">│  │    ├─────┼─────┼─────┼─────┼─────┤                              │   │</span>
<span class="line">│  │    │     │ AOI │ AOI │ AOI │     │   玩家在中心格子              │   │</span>
<span class="line">│  │    ├─────┼─────┼─────┼─────┼─────┤   AOI = 周围 9 格            │   │</span>
<span class="line">│  │    │     │ AOI │ [P] │ AOI │     │   视野半径 = 1.5 * 格子大小  │   │</span>
<span class="line">│  │    ├─────┼─────┼─────┼─────┼─────┤                              │   │</span>
<span class="line">│  │    │     │ AOI │ AOI │ AOI │     │                              │   │</span>
<span class="line">│  │    ├─────┼─────┼─────┼─────┼─────┤                              │   │</span>
<span class="line">│  │    │     │     │     │     │     │                              │   │</span>
<span class="line">│  │    └─────┴─────┴─────┴─────┴─────┘                              │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  移动时 AOI 更新:                                                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  玩家从格子 A 移动到格子 B:                                       │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  1. 计算离开的格子 (A的AOI - B的AOI)                             │   │</span>
<span class="line">│  │     → 通知这些格子里的玩家: 我离开了                              │   │</span>
<span class="line">│  │     → 通知我: 这些玩家离开视野                                    │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  2. 计算进入的格子 (B的AOI - A的AOI)                             │   │</span>
<span class="line">│  │     → 通知这些格子里的玩家: 我进入了                              │   │</span>
<span class="line">│  │     → 通知我: 这些玩家进入视野                                    │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  3. 计算保持的格子 (A的AOI ∩ B的AOI)                            │   │</span>
<span class="line">│  │     → 只广播位置更新                                              │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  数据结构:                                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  class AOIGrid {                                                 │   │</span>
<span class="line">│  │      // 格子索引 → 实体列表                                      │   │</span>
<span class="line">│  │      map&lt;uint32_t, set&lt;EntityId&gt;&gt; grid_;                        │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │      // 实体 → 所在格子                                          │   │</span>
<span class="line">│  │      map&lt;EntityId, uint32_t&gt; entityGrid_;                       │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │      // 坐标 → 格子索引                                          │   │</span>
<span class="line">│  │      uint32_t GetGridIndex(float x, float y);                   │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │      // 获取周围9格的实体                                        │   │</span>
<span class="line">│  │      vector&lt;EntityId&gt; GetNearbyEntities(float x, float y);      │   │</span>
<span class="line">│  │  };                                                              │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-3-场景迁移" tabindex="-1"><a class="header-anchor" href="#_7-3-场景迁移"><span>7.3 场景迁移</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       玩家跨场景迁移流程                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  同 GameServer 内迁移 (切换地图):                                       │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  1. 从当前场景移除玩家                                           │   │</span>
<span class="line">│  │  2. 触发 AOI 离开通知                                            │   │</span>
<span class="line">│  │  3. 加入目标场景                                                 │   │</span>
<span class="line">│  │  4. 触发 AOI 进入通知                                            │   │</span>
<span class="line">│  │  5. 同步场景数据给客户端                                         │   │</span>
<span class="line">│  │  耗时: &lt; 100ms                                                   │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  跨 GameServer 迁移:                                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  GameServer A                 GameServer B                       │   │</span>
<span class="line">│  │      │                             │                              │   │</span>
<span class="line">│  │      │  1. 序列化玩家状态          │                              │   │</span>
<span class="line">│  │      │────────────────────────────►│                              │   │</span>
<span class="line">│  │      │  2. 通知 Gate 切换路由      │                              │   │</span>
<span class="line">│  │      │         │                   │                              │   │</span>
<span class="line">│  │      │         ▼                   │                              │   │</span>
<span class="line">│  │      │      GameGate               │                              │   │</span>
<span class="line">│  │      │         │                   │                              │   │</span>
<span class="line">│  │      │  3. Gate 更新会话映射       │                              │   │</span>
<span class="line">│  │      │         │                   │                              │   │</span>
<span class="line">│  │      │  4. 反序列化并加入场景      │                              │   │</span>
<span class="line">│  │      │◄────────────────────────────│                              │   │</span>
<span class="line">│  │      │  5. 确认迁移完成            │                              │   │</span>
<span class="line">│  │      │                             │                              │   │</span>
<span class="line">│  │      │  6. 清理旧数据              │                              │   │</span>
<span class="line">│  │      │                             │                              │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │  耗时: &lt; 500ms                                                   │   │</span>
<span class="line">│  │  关键: 迁移过程中消息缓存，完成后重放                             │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="_8-高可用与容灾" tabindex="-1"><a class="header-anchor" href="#_8-高可用与容灾"><span>8. 高可用与容灾</span></a></h2><h3 id="_8-1-服务高可用" tabindex="-1"><a class="header-anchor" href="#_8-1-服务高可用"><span>8.1 服务高可用</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       服务高可用设计                                      │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  无状态服务 (Gate/Login/DBServer):                                      │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 多实例部署 (N+1 冗余)                                         │   │</span>
<span class="line">│  │  • 负载均衡器 (LVS/Nginx) 自动切换                               │   │</span>
<span class="line">│  │  • 健康检查: 5s 间隔, 3 次失败剔除                               │   │</span>
<span class="line">│  │  • 故障恢复: 自动重启 + 重新注册                                 │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  有状态服务 (GameServer):                                               │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 故障检测: 心跳超时 30s                                        │   │</span>
<span class="line">│  │  • 故障处理:                                                      │   │</span>
<span class="line">│  │    1. 从 Redis 恢复玩家会话                                      │   │</span>
<span class="line">│  │    2. 通知 Gate 断开相关连接                                     │   │</span>
<span class="line">│  │    3. 玩家重连到新 GameServer                                    │   │</span>
<span class="line">│  │    4. 从 DB 加载玩家数据                                         │   │</span>
<span class="line">│  │  • 数据保护: 每 5 分钟快照到 Redis                               │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  WorldServer (单点):                                                    │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │  • 主备模式: 1 主 + 1 热备                                       │   │</span>
<span class="line">│  │  • 数据同步: 实时同步到 Redis                                    │   │</span>
<span class="line">│  │  • 故障切换: ZooKeeper 选主, &lt; 10s                              │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_8-2-数据库容灾" tabindex="-1"><a class="header-anchor" href="#_8-2-数据库容灾"><span>8.2 数据库容灾</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       数据库容灾架构                                      │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  MySQL 主从架构:                                                        │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │           ┌─────────┐                                            │   │</span>
<span class="line">│  │           │  Master │ ◄── 写操作                                 │   │</span>
<span class="line">│  │           └────┬────┘                                            │   │</span>
<span class="line">│  │                │ 半同步复制                                       │   │</span>
<span class="line">│  │       ┌────────┼────────┐                                        │   │</span>
<span class="line">│  │       ▼        ▼        ▼                                        │   │</span>
<span class="line">│  │   ┌───────┐┌───────┐┌───────┐                                   │   │</span>
<span class="line">│  │   │Slave 1││Slave 2││Slave 3│ ◄── 读操作                        │   │</span>
<span class="line">│  │   └───────┘└───────┘└───────┘                                   │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │   主库故障: Slave 1 提升为主 (&lt; 30s)                             │   │</span>
<span class="line">│  │   数据一致性: 半同步复制保证                                      │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Redis 集群:                                                            │</span>
<span class="line">│  ┌─────────────────────────────────────────────────────────────────┐   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │   ┌─────────┐  ┌─────────┐  ┌─────────┐                         │   │</span>
<span class="line">│  │   │Master 1 │  │Master 2 │  │Master 3 │  (3主3从)               │   │</span>
<span class="line">│  │   │  Slave  │  │  Slave  │  │  Slave  │                         │   │</span>
<span class="line">│  │   └─────────┘  └─────────┘  └─────────┘                         │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  │   故障切换: 自动 Failover                                        │   │</span>
<span class="line">│  │   数据分片: Hash Slot 分布                                       │   │</span>
<span class="line">│  │                                                                   │   │</span>
<span class="line">│  └─────────────────────────────────────────────────────────────────┘   │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_8-3-灾难恢复" tabindex="-1"><a class="header-anchor" href="#_8-3-灾难恢复"><span>8.3 灾难恢复</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       灾难恢复策略                                        │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  备份策略:                                                              │</span>
<span class="line">│  ┌───────────────────┬───────────────────────────────────────────────┐ │</span>
<span class="line">│  │     类型          │              策略                              │ │</span>
<span class="line">│  ├───────────────────┼───────────────────────────────────────────────┤ │</span>
<span class="line">│  │ MySQL 全量备份    │ 每日凌晨 3:00, 保留 7 天                       │ │</span>
<span class="line">│  │ MySQL 增量备份    │ 每小时, 保留 24 小时                           │ │</span>
<span class="line">│  │ MySQL Binlog      │ 实时同步, 保留 7 天                            │ │</span>
<span class="line">│  │ Redis RDB         │ 每 15 分钟, 保留 48 个                         │ │</span>
<span class="line">│  │ Redis AOF         │ 每秒刷盘                                       │ │</span>
<span class="line">│  │ 配置文件          │ Git 版本控制                                   │ │</span>
<span class="line">│  └───────────────────┴───────────────────────────────────────────────┘ │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  恢复 RPO/RTO:                                                          │</span>
<span class="line">│  ┌───────────────────┬───────────────────────────────────────────────┐ │</span>
<span class="line">│  │     场景          │     RPO          │        RTO                 │ │</span>
<span class="line">│  ├───────────────────┼──────────────────┼────────────────────────────┤ │</span>
<span class="line">│  │ 单节点故障        │    0             │     &lt; 30 秒 (自动切换)     │ │</span>
<span class="line">│  │ 机房网络故障      │    0             │     &lt; 5 分钟               │ │</span>
<span class="line">│  │ 整机房灾难        │    &lt; 1 小时      │     &lt; 2 小时               │ │</span>
<span class="line">│  │ 数据误删除        │    &lt; 15 分钟     │     &lt; 1 小时               │ │</span>
<span class="line">│  └───────────────────┴──────────────────┴────────────────────────────┘ │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="_9-性能指标与约束" tabindex="-1"><a class="header-anchor" href="#_9-性能指标与约束"><span>9. 性能指标与约束</span></a></h2><h3 id="_9-1-关键性能指标" tabindex="-1"><a class="header-anchor" href="#_9-1-关键性能指标"><span>9.1 关键性能指标</span></a></h3><table><thead><tr><th>指标</th><th>目标值</th><th>测量方法</th></tr></thead><tbody><tr><td><strong>单服 CCU</strong></td><td>5000+</td><td>压测工具模拟</td></tr><tr><td><strong>登录 TPS</strong></td><td>1000/s</td><td>登录接口压测</td></tr><tr><td><strong>消息吞吐</strong></td><td>100K msg/s</td><td>网关压测</td></tr><tr><td><strong>AOI 广播延迟</strong></td><td>P99 &lt; 20ms</td><td>埋点统计</td></tr><tr><td><strong>数据库查询</strong></td><td>P99 &lt; 10ms</td><td>慢查询日志</td></tr><tr><td><strong>内存占用</strong></td><td>&lt; 8GB / GameServer</td><td>监控采集</td></tr><tr><td><strong>CPU 占用</strong></td><td>&lt; 70% (峰值)</td><td>监控采集</td></tr></tbody></table><h3 id="_9-2-资源规划" tabindex="-1"><a class="header-anchor" href="#_9-2-资源规划"><span>9.2 资源规划</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                     单大区资源规划 (10万 CCU)                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  ┌───────────────────┬────────────┬───────────────────────────────────┐│</span>
<span class="line">│  │      服务         │    数量    │           配置                     ││</span>
<span class="line">│  ├───────────────────┼────────────┼───────────────────────────────────┤│</span>
<span class="line">│  │ GameGate          │    4 台    │ 8C16G, 万兆网卡                   ││</span>
<span class="line">│  │ GameServer        │   20 台    │ 8C32G, SSD                        ││</span>
<span class="line">│  │ LoginGate         │    2 台    │ 4C8G                              ││</span>
<span class="line">│  │ LoginServer       │    2 台    │ 4C8G                              ││</span>
<span class="line">│  │ DBServer          │    4 台    │ 8C16G                             ││</span>
<span class="line">│  │ NewMServer        │    2 台    │ 8C16G                             ││</span>
<span class="line">│  │ WorldServer       │    2 台    │ 8C16G (主备)                      ││</span>
<span class="line">│  │ ChatServer        │    2 台    │ 4C8G                              ││</span>
<span class="line">│  │ LogServer         │    2 台    │ 4C16G                             ││</span>
<span class="line">│  ├───────────────────┼────────────┼───────────────────────────────────┤│</span>
<span class="line">│  │ MySQL (主)        │    1 台    │ 16C64G, NVMe SSD                  ││</span>
<span class="line">│  │ MySQL (从)        │    3 台    │ 16C64G, NVMe SSD                  ││</span>
<span class="line">│  │ Redis 集群        │    6 台    │ 8C32G, 3主3从                     ││</span>
<span class="line">│  └───────────────────┴────────────┴───────────────────────────────────┘│</span>
<span class="line">│                                                                         │</span>
<span class="line">│  总计: ~50 台服务器                                                     │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="_10-实施路线图" tabindex="-1"><a class="header-anchor" href="#_10-实施路线图"><span>10. 实施路线图</span></a></h2><h3 id="_10-1-阶段划分" tabindex="-1"><a class="header-anchor" href="#_10-1-阶段划分"><span>10.1 阶段划分</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       实施路线图 (14 周)                                  │</span>
<span class="line">├─────────────────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Phase 1: 基础框架 (Week 1-2)                                           │</span>
<span class="line">│  ├── CMake + vcpkg 构建系统                                             │</span>
<span class="line">│  ├── 日志/线程池/内存池                                                 │</span>
<span class="line">│  ├── 基础测试框架                                                       │</span>
<span class="line">│  └── 交付: 可编译的空框架                                               │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Phase 2: 网络通信 (Week 3-6)                                           │</span>
<span class="line">│  ├── IOCP/io_uring 反应器                                              │</span>
<span class="line">│  ├── Protobuf 消息系统                                                  │</span>
<span class="line">│  ├── 消息自动注册                                                       │</span>
<span class="line">│  ├── Gate 路由转发                                                      │</span>
<span class="line">│  └── 交付: Echo 服务器示例                                              │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Phase 3: 配置系统 (Week 7-8)                                           │</span>
<span class="line">│  ├── Lua 配置加载                                                       │</span>
<span class="line">│  ├── Excel → Lua 转换工具                                               │</span>
<span class="line">│  ├── 热更新机制                                                         │</span>
<span class="line">│  └── 交付: 完整配置系统                                                 │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Phase 4: 属性系统 (Week 9-10)                                          │</span>
<span class="line">│  ├── 属性容器                                                           │</span>
<span class="line">│  ├── 修饰器系统                                                         │</span>
<span class="line">│  ├── 同步机制                                                           │</span>
<span class="line">│  ├── 持久化                                                             │</span>
<span class="line">│  └── 交付: 完整属性系统                                                 │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Phase 5: 场景系统 (Week 11-12)                                         │</span>
<span class="line">│  ├── 场景管理                                                           │</span>
<span class="line">│  ├── AOI 系统                                                           │</span>
<span class="line">│  ├── 场景迁移                                                           │</span>
<span class="line">│  └── 交付: 基础场景能力                                                 │</span>
<span class="line">│                                                                         │</span>
<span class="line">│  Phase 6: 集成测试 (Week 13-14)                                         │</span>
<span class="line">│  ├── 压力测试                                                           │</span>
<span class="line">│  ├── 性能优化                                                           │</span>
<span class="line">│  ├── 文档完善                                                           │</span>
<span class="line">│  └── 交付: v1.0.0 Release                                               │</span>
<span class="line">│                                                                         │</span>
<span class="line">└─────────────────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_10-2-里程碑" tabindex="-1"><a class="header-anchor" href="#_10-2-里程碑"><span>10.2 里程碑</span></a></h3><table><thead><tr><th>里程碑</th><th>周次</th><th>交付物</th><th>验收标准</th></tr></thead><tbody><tr><td>M1</td><td>Week 2</td><td>构建系统</td><td>跨平台编译通过</td></tr><tr><td>M2</td><td>Week 6</td><td>网络通信</td><td>Echo 性能 &gt; 50K msg/s</td></tr><tr><td>M3</td><td>Week 8</td><td>配置系统</td><td>热更新 &lt; 1s 生效</td></tr><tr><td>M4</td><td>Week 10</td><td>属性系统</td><td>100K 属性/s 吞吐</td></tr><tr><td>M5</td><td>Week 12</td><td>场景系统</td><td>5000 实体 AOI 正常</td></tr><tr><td>M6</td><td>Week 14</td><td>正式发布</td><td>全部测试通过</td></tr></tbody></table><hr><h2 id="_11-待审核问题清单" tabindex="-1"><a class="header-anchor" href="#_11-待审核问题清单"><span>11. 待审核问题清单</span></a></h2><p>以下问题请 Codex 审核并提供意见:</p><h3 id="_11-1-架构决策" tabindex="-1"><a class="header-anchor" href="#_11-1-架构决策"><span>11.1 架构决策</span></a></h3><table><thead><tr><th>#</th><th>问题</th><th>当前方案</th><th>待审核点</th></tr></thead><tbody><tr><td>1</td><td>网络模型选择</td><td>io_uring 优先</td><td>Linux 5.10 内核要求是否过高？是否需要 epoll 回退？</td></tr><tr><td>2</td><td>ServerID 设计</td><td>64位 4段式</td><td>是否需要预留更多位给未来扩展？</td></tr><tr><td>3</td><td>场景线程模型</td><td>单线程主循环</td><td>是否考虑协程方案 (C++20 coroutine)？</td></tr><tr><td>4</td><td>属性同步批次</td><td>50ms 周期</td><td>对于快节奏游戏是否过慢？建议值？</td></tr><tr><td>5</td><td>AOI 格子大小</td><td>100x100</td><td>对于不同类型游戏的建议值？</td></tr></tbody></table><h3 id="_11-2-技术选型" tabindex="-1"><a class="header-anchor" href="#_11-2-技术选型"><span>11.2 技术选型</span></a></h3><table><thead><tr><th>#</th><th>问题</th><th>当前方案</th><th>待审核点</th></tr></thead><tbody><tr><td>6</td><td>序列化</td><td>Protobuf</td><td>是否考虑 FlatBuffers 用于高频消息？</td></tr><tr><td>7</td><td>脚本引擎</td><td>Lua 5.4</td><td>是否考虑 LuaJIT 提升性能？</td></tr><tr><td>8</td><td>缓存</td><td>Redis</td><td>是否需要本地缓存 (如 memcached) 作为 L1？</td></tr><tr><td>9</td><td>注册中心</td><td>Redis</td><td>是否需要专业方案 (etcd/Consul)？</td></tr></tbody></table><h3 id="_11-3-性能约束" tabindex="-1"><a class="header-anchor" href="#_11-3-性能约束"><span>11.3 性能约束</span></a></h3><table><thead><tr><th>#</th><th>问题</th><th>当前目标</th><th>待审核点</th></tr></thead><tbody><tr><td>10</td><td>单服 CCU</td><td>5000</td><td>是否合理？能否提高？</td></tr><tr><td>11</td><td>消息吞吐</td><td>100K/s</td><td>瓶颈在哪里？如何突破？</td></tr><tr><td>12</td><td>AOI 延迟</td><td>P99 &lt; 20ms</td><td>对于大规模战斗是否足够？</td></tr></tbody></table><h3 id="_11-4-高可用" tabindex="-1"><a class="header-anchor" href="#_11-4-高可用"><span>11.4 高可用</span></a></h3><table><thead><tr><th>#</th><th>问题</th><th>当前方案</th><th>待审核点</th></tr></thead><tbody><tr><td>13</td><td>GameServer 故障</td><td>玩家重连</td><td>是否需要热备方案？</td></tr><tr><td>14</td><td>数据一致性</td><td>异步落库</td><td>关键数据同步落库的范围？</td></tr><tr><td>15</td><td>跨服迁移</td><td>消息缓存</td><td>迁移期间丢消息风险？</td></tr></tbody></table><hr><p><strong>请 Codex 针对以上问题提供专业意见和改进建议。</strong></p><hr><p><em>文档版本: 2.0.0</em><em>作者: Claude (Anthropic)</em><em>审核方: Codex (OpenAI)</em><em>创建日期: 2024-12-03</em></p>`,102)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};