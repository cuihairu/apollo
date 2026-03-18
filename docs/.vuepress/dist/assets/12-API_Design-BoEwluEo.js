import{i as e,r as t,s as n,t as r}from"./app-DuH0mZgh.js";var i=JSON.parse(`{"path":"/12-API_Design.html","title":"Apollo MMORPG API 设计文档","lang":"en-US","frontmatter":{},"filePathRelative":"12-API_Design.md","git":{"createdTime":1765122408000,"updatedTime":1765122408000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"}]}}`),a={name:`12-API_Design.md`};function o(r,i,a,o,s,c){return n(),t(`div`,null,[...i[0]||=[e(`<h1 id="apollo-mmorpg-api-设计文档" tabindex="-1"><a class="header-anchor" href="#apollo-mmorpg-api-设计文档"><span>Apollo MMORPG API 设计文档</span></a></h1><blockquote><p><strong>版本</strong>: 1.0 <strong>更新日期</strong>: 2024-12-06 <strong>协议</strong>: Protocol Buffers v3 <strong>通信方式</strong>: gRPC / TCP Socket</p></blockquote><h2 id="_1-api-设计原则" tabindex="-1"><a class="header-anchor" href="#_1-api-设计原则"><span>1. API 设计原则</span></a></h2><h3 id="_1-1-设计理念" tabindex="-1"><a class="header-anchor" href="#_1-1-设计理念"><span>1.1 设计理念</span></a></h3><ol><li><strong>RESTful风格</strong>: 使用资源导向的API设计</li><li><strong>版本管理</strong>: 所有API支持版本控制</li><li><strong>向后兼容</strong>: 新版本保持对旧版本的兼容</li><li><strong>幂等性</strong>: 关键操作支持幂等调用</li><li><strong>错误处理</strong>: 统一的错误码和错误信息</li></ol><h3 id="_1-2-命名规范" tabindex="-1"><a class="header-anchor" href="#_1-2-命名规范"><span>1.2 命名规范</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 服务命名: PascalCase</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">PlayerService</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">GuildService</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 方法命名: 动词 + 名词</span></span>
<span class="line"><span class="token keyword">rpc</span> <span class="token function">CreatePlayer</span><span class="token punctuation">(</span><span class="token class-name">CreatePlayerRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">CreatePlayerResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token keyword">rpc</span> <span class="token function">GetPlayerInfo</span><span class="token punctuation">(</span><span class="token class-name">GetPlayerInfoRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetPlayerInfoResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 消息命名: PascalCase</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">PlayerInfo</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CreatePlayerRequest</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 字段命名: snake_case</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Player</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> player_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> level <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_2-通用数据结构" tabindex="-1"><a class="header-anchor" href="#_2-通用数据结构"><span>2. 通用数据结构</span></a></h2><h3 id="_2-1-基础类型定义" tabindex="-1"><a class="header-anchor" href="#_2-1-基础类型定义"><span>2.1 基础类型定义</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// base.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>base<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 通用响应状态</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">StatusCode</span> <span class="token punctuation">{</span></span>
<span class="line">  SUCCESS <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">  INVALID_REQUEST <span class="token operator">=</span> <span class="token number">1000</span><span class="token punctuation">;</span></span>
<span class="line">  UNAUTHORIZED <span class="token operator">=</span> <span class="token number">1001</span><span class="token punctuation">;</span></span>
<span class="line">  PERMISSION_DENIED <span class="token operator">=</span> <span class="token number">1002</span><span class="token punctuation">;</span></span>
<span class="line">  NOT_FOUND <span class="token operator">=</span> <span class="token number">1003</span><span class="token punctuation">;</span></span>
<span class="line">  ALREADY_EXISTS <span class="token operator">=</span> <span class="token number">1004</span><span class="token punctuation">;</span></span>
<span class="line">  INTERNAL_ERROR <span class="token operator">=</span> <span class="token number">2000</span><span class="token punctuation">;</span></span>
<span class="line">  SERVICE_UNAVAILABLE <span class="token operator">=</span> <span class="token number">2001</span><span class="token punctuation">;</span></span>
<span class="line">  TIMEOUT <span class="token operator">=</span> <span class="token number">2002</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 通用响应头</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">ResponseHeader</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token positional-class-name class-name">StatusCode</span> code <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> message <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> request_id <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> timestamp <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 分页信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Pagination</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">int32</span> page <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> page_size <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> total <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> total_pages <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 坐标信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Position</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">float</span> x <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">float</span> y <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">float</span> z <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint32</span> map_id <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 属性值</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Attribute</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">int32</span> attr_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> value <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> base_value <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 物品信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Item</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> item_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> config_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> count <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> create_time <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token map class-name">map<span class="token punctuation">&lt;</span><span class="token builtin">string</span><span class="token punctuation">,</span> <span class="token builtin">string</span><span class="token punctuation">&gt;</span></span> extra_data <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-通用请求响应" tabindex="-1"><a class="header-anchor" href="#_2-2-通用请求响应"><span>2.2 通用请求响应</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// common.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>common<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> <span class="token string">&quot;base.proto&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 通用请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Request</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">string</span> version <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> client_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> access_token <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>ResponseHeader</span> header <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 通用响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Response</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>ResponseHeader</span> header <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 批量请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">BatchRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">Request</span> requests <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 批量响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">BatchResponse</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">Response</span> responses <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_3-玩家服务-api" tabindex="-1"><a class="header-anchor" href="#_3-玩家服务-api"><span>3. 玩家服务 API</span></a></h2><h3 id="_3-1-玩家基础信息" tabindex="-1"><a class="header-anchor" href="#_3-1-玩家基础信息"><span>3.1 玩家基础信息</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// player.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>player<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> <span class="token string">&quot;base.proto&quot;</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token keyword">import</span> <span class="token string">&quot;common.proto&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 玩家服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">PlayerService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 创建角色</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">CreatePlayer</span><span class="token punctuation">(</span><span class="token class-name">CreatePlayerRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">CreatePlayerResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 获取玩家信息</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetPlayerInfo</span><span class="token punctuation">(</span><span class="token class-name">GetPlayerInfoRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetPlayerInfoResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 更新玩家信息</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">UpdatePlayerInfo</span><span class="token punctuation">(</span><span class="token class-name">UpdatePlayerInfoRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">UpdatePlayerInfoResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 删除角色</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">DeletePlayer</span><span class="token punctuation">(</span><span class="token class-name">DeletePlayerRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">DeletePlayerResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 玩家列表</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">ListPlayers</span><span class="token punctuation">(</span><span class="token class-name">ListPlayersRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">ListPlayersResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 玩家在线状态</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetPlayerOnlineStatus</span><span class="token punctuation">(</span><span class="token class-name">GetPlayerOnlineStatusRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetPlayerOnlineStatusResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 创建角色请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CreatePlayerRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> account_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> player_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> class_id <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> gender <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>Position</span> initial_pos <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 创建角色响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CreatePlayerResponse</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>ResponseHeader</span> header <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> player_name <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> create_time <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 玩家详细信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">PlayerInfo</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> player_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> level <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> exp <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> class_id <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> gender <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>Position</span> position <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> gold <span class="token operator">=</span> <span class="token number">8</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> diamond <span class="token operator">=</span> <span class="token number">9</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> guild_id <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> guild_name <span class="token operator">=</span> <span class="token number">11</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> online_time <span class="token operator">=</span> <span class="token number">12</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> last_login <span class="token operator">=</span> <span class="token number">13</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>Attribute</span> attributes <span class="token operator">=</span> <span class="token number">14</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>Item</span> items <span class="token operator">=</span> <span class="token number">15</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 获取玩家信息请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">GetPlayerInfoRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token keyword">oneof</span> identifier <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> player_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token punctuation">}</span></span>
<span class="line">  <span class="token builtin">bool</span> include_items <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">bool</span> include_attributes <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 获取玩家信息响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">GetPlayerInfoResponse</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>ResponseHeader</span> header <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">PlayerInfo</span> player_info <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-玩家属性管理" tabindex="-1"><a class="header-anchor" href="#_3-2-玩家属性管理"><span>3.2 玩家属性管理</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 属性服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">AttributeService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 获取属性</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetAttributes</span><span class="token punctuation">(</span><span class="token class-name">GetAttributesRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetAttributesResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 更新属性</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">UpdateAttributes</span><span class="token punctuation">(</span><span class="token class-name">UpdateAttributesRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">UpdateAttributesResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 批量更新</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">BatchUpdateAttributes</span><span class="token punctuation">(</span><span class="token class-name">BatchUpdateAttributesRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">BatchUpdateAttributesResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 属性更新操作</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">AttributeUpdate</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">int32</span> attr_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> delta <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>  <span class="token comment">// 变化量</span></span>
<span class="line">  <span class="token positional-class-name class-name">UpdateOperation</span> operation <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">UpdateOperation</span> <span class="token punctuation">{</span></span>
<span class="line">  SET <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>      <span class="token comment">// 设置值</span></span>
<span class="line">  ADD <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>      <span class="token comment">// 增加</span></span>
<span class="line">  SUB <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>      <span class="token comment">// 减少</span></span>
<span class="line">  MUL <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>      <span class="token comment">// 乘法</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_4-战斗系统-api" tabindex="-1"><a class="header-anchor" href="#_4-战斗系统-api"><span>4. 战斗系统 API</span></a></h2><h3 id="_4-1-技能系统" tabindex="-1"><a class="header-anchor" href="#_4-1-技能系统"><span>4.1 技能系统</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// battle.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>battle<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> <span class="token string">&quot;base.proto&quot;</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token keyword">import</span> <span class="token string">&quot;player.proto&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 战斗服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">BattleService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 使用技能</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">CastSkill</span><span class="token punctuation">(</span><span class="token class-name">CastSkillRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">CastSkillResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 造成伤害</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">DealDamage</span><span class="token punctuation">(</span><span class="token class-name">DealDamageRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">DealDamageResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 应用Buff</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">ApplyBuff</span><span class="token punctuation">(</span><span class="token class-name">ApplyBuffRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">ApplyBuffResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 移除Buff</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">RemoveBuff</span><span class="token punctuation">(</span><span class="token class-name">RemoveBuffRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">RemoveBuffResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 战斗结算</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">BattleEnd</span><span class="token punctuation">(</span><span class="token class-name">BattleEndRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">BattleEndResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用技能请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CastSkillRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> caster_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> target_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint32</span> skill_id <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>Position</span> target_pos <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token builtin">uint64</span> extra_targets <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用技能响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CastSkillResponse</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>ResponseHeader</span> header <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">bool</span> success <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> cast_time <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">SkillEffect</span> effects <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 技能效果</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">SkillEffect</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> target_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">EffectType</span> type <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> value <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint32</span> duration <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">EffectType</span> <span class="token punctuation">{</span></span>
<span class="line">  DAMAGE <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">  HEAL <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  BUFF <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  DEBUFF <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  KNOCKBACK <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-战斗匹配" tabindex="-1"><a class="header-anchor" href="#_4-2-战斗匹配"><span>4.2 战斗匹配</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 匹配服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">MatchService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 加入匹配队列</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">JoinMatch</span><span class="token punctuation">(</span><span class="token class-name">JoinMatchRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">JoinMatchResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 离开匹配队列</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">LeaveMatch</span><span class="token punctuation">(</span><span class="token class-name">LeaveMatchRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">LeaveMatchResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 获取匹配状态</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetMatchStatus</span><span class="token punctuation">(</span><span class="token class-name">GetMatchStatusRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetMatchStatusResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 接受匹配</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">AcceptMatch</span><span class="token punctuation">(</span><span class="token class-name">AcceptMatchRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">AcceptMatchResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 匹配请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">MatchRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">MatchType</span> type <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token builtin">uint64</span> team_members <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token map class-name">map<span class="token punctuation">&lt;</span><span class="token builtin">string</span><span class="token punctuation">,</span> <span class="token builtin">int32</span><span class="token punctuation">&gt;</span></span> preferences <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span>  <span class="token comment">// 匹配偏好</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">MatchType</span> <span class="token punctuation">{</span></span>
<span class="line">  SOLO <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>        <span class="token comment">// 单人</span></span>
<span class="line">  TEAM <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>        <span class="token comment">// 队伍</span></span>
<span class="line">  RANKED <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>      <span class="token comment">// 排位</span></span>
<span class="line">  CASUAL <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>      <span class="token comment">// 休闲</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_5-社交系统-api" tabindex="-1"><a class="header-anchor" href="#_5-社交系统-api"><span>5. 社交系统 API</span></a></h2><h3 id="_5-1-好友系统" tabindex="-1"><a class="header-anchor" href="#_5-1-好友系统"><span>5.1 好友系统</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// social.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>social<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> <span class="token string">&quot;base.proto&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 社交服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">SocialService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 添加好友</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">AddFriend</span><span class="token punctuation">(</span><span class="token class-name">AddFriendRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">AddFriendResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 删除好友</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">RemoveFriend</span><span class="token punctuation">(</span><span class="token class-name">RemoveFriendRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">RemoveFriendResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 好友列表</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetFriendList</span><span class="token punctuation">(</span><span class="token class-name">GetFriendListRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetFriendListResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 搜索玩家</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">SearchPlayer</span><span class="token punctuation">(</span><span class="token class-name">SearchPlayerRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">SearchPlayerResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 黑名单操作</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">BlockPlayer</span><span class="token punctuation">(</span><span class="token class-name">BlockPlayerRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">BlockPlayerResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 好友信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">FriendInfo</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> player_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> level <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> class_id <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">bool</span> online <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> status <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> last_seen <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">FriendRelationship</span> relationship <span class="token operator">=</span> <span class="token number">8</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">FriendRelationship</span> <span class="token punctuation">{</span></span>
<span class="line">  FRIEND <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">  BLOCKED <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  PENDING <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>  <span class="token comment">// 待确认</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-公会系统" tabindex="-1"><a class="header-anchor" href="#_5-2-公会系统"><span>5.2 公会系统</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 公会服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">GuildService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 创建公会</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">CreateGuild</span><span class="token punctuation">(</span><span class="token class-name">CreateGuildRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">CreateGuildResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 加入公会</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">JoinGuild</span><span class="token punctuation">(</span><span class="token class-name">JoinGuildRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">JoinGuildResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 离开公会</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">LeaveGuild</span><span class="token punctuation">(</span><span class="token class-name">LeaveGuildRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">LeaveGuildResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 公会信息</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetGuildInfo</span><span class="token punctuation">(</span><span class="token class-name">GetGuildInfoRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetGuildInfoResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 成员管理</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">ManageMember</span><span class="token punctuation">(</span><span class="token class-name">ManageMemberRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">ManageMemberResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 公会活动</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GuildActivity</span><span class="token punctuation">(</span><span class="token class-name">GuildActivityRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GuildActivityResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 公会信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">GuildInfo</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> guild_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> guild_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> description <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> level <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> exp <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> member_count <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> max_members <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> leader_id <span class="token operator">=</span> <span class="token number">8</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> leader_name <span class="token operator">=</span> <span class="token number">9</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> create_time <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">GuildMember</span> members <span class="token operator">=</span> <span class="token number">11</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 公会成员</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">GuildMember</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> player_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> level <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">GuildPosition</span> position <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> join_time <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> contribution <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">bool</span> online <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">GuildPosition</span> <span class="token punctuation">{</span></span>
<span class="line">  LEADER <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">  OFFICER <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  ELITE <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  MEMBER <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-3-聊天系统" tabindex="-1"><a class="header-anchor" href="#_5-3-聊天系统"><span>5.3 聊天系统</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 聊天服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">ChatService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 发送消息</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">SendMessage</span><span class="token punctuation">(</span><span class="token class-name">SendMessageRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">SendMessageResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 获取历史消息</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetChatHistory</span><span class="token punctuation">(</span><span class="token class-name">GetChatHistoryRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetChatHistoryResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 订阅频道</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">SubscribeChannel</span><span class="token punctuation">(</span><span class="token class-name">SubscribeChannelRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">SubscribeChannelResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 取消订阅</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">UnsubscribeChannel</span><span class="token punctuation">(</span><span class="token class-name">UnsubscribeChannelRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">UnsubscribeChannelResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 聊天消息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">ChatMessage</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> message_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> sender_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> sender_name <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">ChatChannel</span> channel <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> content <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">MessageType</span> type <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> timestamp <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token builtin">string</span> receivers <span class="token operator">=</span> <span class="token number">8</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">ChatChannel</span> <span class="token punctuation">{</span></span>
<span class="line">  WORLD <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>      <span class="token comment">// 世界</span></span>
<span class="line">  GUILD <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>      <span class="token comment">// 公会</span></span>
<span class="line">  TEAM <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>       <span class="token comment">// 队伍</span></span>
<span class="line">  PRIVATE <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>    <span class="token comment">// 私聊</span></span>
<span class="line">  SYSTEM <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span>     <span class="token comment">// 系统</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">MessageType</span> <span class="token punctuation">{</span></span>
<span class="line">  TEXT <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>       <span class="token comment">// 文本</span></span>
<span class="line">  VOICE <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>      <span class="token comment">// 语音</span></span>
<span class="line">  EMOTE <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>      <span class="token comment">// 表情</span></span>
<span class="line">  ITEM <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>       <span class="token comment">// 物品链接</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_6-场景与移动-api" tabindex="-1"><a class="header-anchor" href="#_6-场景与移动-api"><span>6. 场景与移动 API</span></a></h2><h3 id="_6-1-场景管理" tabindex="-1"><a class="header-anchor" href="#_6-1-场景管理"><span>6.1 场景管理</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// scene.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>scene<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">import</span> <span class="token string">&quot;base.proto&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 场景服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">SceneService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 进入场景</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">EnterScene</span><span class="token punctuation">(</span><span class="token class-name">EnterSceneRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">EnterSceneResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 离开场景</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">LeaveScene</span><span class="token punctuation">(</span><span class="token class-name">LeaveSceneRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">LeaveSceneResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 场景内移动</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">Move</span><span class="token punctuation">(</span><span class="token class-name">MoveRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">MoveResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 传送</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">Teleport</span><span class="token punctuation">(</span><span class="token class-name">TeleportRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">TeleportResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 场景信息</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetSceneInfo</span><span class="token punctuation">(</span><span class="token class-name">GetSceneInfoRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetSceneInfoResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 进入场景请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">EnterSceneRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint32</span> scene_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>Position</span> position <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> instance_id <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 进入场景响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">EnterSceneResponse</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>ResponseHeader</span> header <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">SceneInfo</span> scene_info <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">NearbyPlayer</span> nearby_players <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">NearbyNPC</span> nearby_npcs <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 场景信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">SceneInfo</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint32</span> scene_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> scene_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">SceneType</span> type <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>Position</span> spawn_point <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> max_players <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> current_players <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">SceneType</span> <span class="token punctuation">{</span></span>
<span class="line">  FIELD <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>      <span class="token comment">// 野外</span></span>
<span class="line">  DUNGEON <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>    <span class="token comment">// 副本</span></span>
<span class="line">  CITY <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>       <span class="token comment">// 城市</span></span>
<span class="line">  INSTANCE <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>   <span class="token comment">// 实例</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-aoi-服务" tabindex="-1"><a class="header-anchor" href="#_6-2-aoi-服务"><span>6.2 AOI 服务</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// aoi.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>aoi<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// AOI服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">AOIService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 更新位置</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">UpdatePosition</span><span class="token punctuation">(</span><span class="token class-name">UpdatePositionRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">UpdatePositionResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 订阅AOI事件</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">SubscribeAOI</span><span class="token punctuation">(</span><span class="token class-name">SubscribeAOIRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token keyword">stream</span> <span class="token class-name">AOIEvent</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 获取附近实体</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetNearbyEntities</span><span class="token punctuation">(</span><span class="token class-name">GetNearbyEntitiesRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetNearbyEntitiesResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 位置更新请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">UpdatePositionRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> entity_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>Position</span> position <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> timestamp <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// AOI事件</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">AOIEvent</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token positional-class-name class-name">EventType</span> type <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> entity_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">EntityType</span> entity_type <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">apollo<span class="token punctuation">.</span>base<span class="token punctuation">.</span>Position</span> position <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token map class-name">map<span class="token punctuation">&lt;</span><span class="token builtin">string</span><span class="token punctuation">,</span> <span class="token builtin">string</span><span class="token punctuation">&gt;</span></span> properties <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">EventType</span> <span class="token punctuation">{</span></span>
<span class="line">  ENTER <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>      <span class="token comment">// 进入视野</span></span>
<span class="line">  LEAVE <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>      <span class="token comment">// 离开视野</span></span>
<span class="line">  MOVE <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>       <span class="token comment">// 移动</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">EntityType</span> <span class="token punctuation">{</span></span>
<span class="line">  PLAYER <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">  NPC <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  MONSTER <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  ITEM <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_7-任务系统-api" tabindex="-1"><a class="header-anchor" href="#_7-任务系统-api"><span>7. 任务系统 API</span></a></h2><h3 id="_7-1-任务管理" tabindex="-1"><a class="header-anchor" href="#_7-1-任务管理"><span>7.1 任务管理</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// quest.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>quest<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 任务服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">QuestService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 获取任务列表</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetQuestList</span><span class="token punctuation">(</span><span class="token class-name">GetQuestListRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetQuestListResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 接受任务</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">AcceptQuest</span><span class="token punctuation">(</span><span class="token class-name">AcceptQuestRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">AcceptQuestResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 完成任务</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">CompleteQuest</span><span class="token punctuation">(</span><span class="token class-name">CompleteQuestRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">CompleteQuestResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 放弃任务</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">AbandonQuest</span><span class="token punctuation">(</span><span class="token class-name">AbandonQuestRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">AbandonQuestResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 更新任务进度</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">UpdateQuestProgress</span><span class="token punctuation">(</span><span class="token class-name">UpdateQuestProgressRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">UpdateQuestProgressResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 任务信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">QuestInfo</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint32</span> quest_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> quest_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> description <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">QuestStatus</span> status <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">QuestType</span> type <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> level_requirement <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">QuestObjective</span> objectives <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">QuestReward</span> rewards <span class="token operator">=</span> <span class="token number">8</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> accept_time <span class="token operator">=</span> <span class="token number">9</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> complete_time <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 任务目标</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">QuestObjective</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint32</span> objective_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">ObjectiveType</span> type <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> target_id <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> current <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> required <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> description <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">QuestStatus</span> <span class="token punctuation">{</span></span>
<span class="line">  AVAILABLE <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>    <span class="token comment">// 可接</span></span>
<span class="line">  IN_PROGRESS <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>  <span class="token comment">// 进行中</span></span>
<span class="line">  COMPLETED <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>    <span class="token comment">// 已完成</span></span>
<span class="line">  FAILED <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>       <span class="token comment">// 失败</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_8-经济系统-api" tabindex="-1"><a class="header-anchor" href="#_8-经济系统-api"><span>8. 经济系统 API</span></a></h2><h3 id="_8-1-商城系统" tabindex="-1"><a class="header-anchor" href="#_8-1-商城系统"><span>8.1 商城系统</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// shop.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>shop<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 商城服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">ShopService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 商品列表</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetShopItems</span><span class="token punctuation">(</span><span class="token class-name">GetShopItemsRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetShopItemsResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 购买商品</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">PurchaseItem</span><span class="token punctuation">(</span><span class="token class-name">PurchaseItemRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">PurchaseItemResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 出售商品</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">SellItem</span><span class="token punctuation">(</span><span class="token class-name">SellItemRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">SellItemResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 交易记录</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetTransactionHistory</span><span class="token punctuation">(</span><span class="token class-name">GetTransactionHistoryRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetTransactionHistoryResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 商品信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">ShopItem</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint32</span> item_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> item_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> price <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">CurrencyType</span> currency <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> stock <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> limit <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> sold <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">DiscountInfo</span> discount <span class="token operator">=</span> <span class="token number">8</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">CurrencyType</span> <span class="token punctuation">{</span></span>
<span class="line">  GOLD <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">  DIAMOND <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  BINDING_DIAMOND <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  HONOR <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 购买请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">PurchaseItemRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint32</span> item_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> quantity <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">CurrencyType</span> currency <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_8-2-交易系统" tabindex="-1"><a class="header-anchor" href="#_8-2-交易系统"><span>8.2 交易系统</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 交易服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">TradeService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 创建交易</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">CreateTrade</span><span class="token punctuation">(</span><span class="token class-name">CreateTradeRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">CreateTradeResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 添加物品</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">AddTradeItem</span><span class="token punctuation">(</span><span class="token class-name">AddTradeItemRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">AddTradeItemResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 确认交易</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">ConfirmTrade</span><span class="token punctuation">(</span><span class="token class-name">ConfirmTradeRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">ConfirmTradeResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 取消交易</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">CancelTrade</span><span class="token punctuation">(</span><span class="token class-name">CancelTradeRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">CancelTradeResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 交易信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Trade</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> trade_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> initiator_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">uint64</span> target_id <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">TradeStatus</span> status <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">TradeSlot</span> initiator_items <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">TradeSlot</span> target_items <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> gold_initiator <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> gold_target <span class="token operator">=</span> <span class="token number">8</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> create_time <span class="token operator">=</span> <span class="token number">9</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_9-排行榜-api" tabindex="-1"><a class="header-anchor" href="#_9-排行榜-api"><span>9. 排行榜 API</span></a></h2><h3 id="_9-1-排行榜管理" tabindex="-1"><a class="header-anchor" href="#_9-1-排行榜管理"><span>9.1 排行榜管理</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// leaderboard.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>leaderboard<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 排行榜服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">LeaderboardService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 获取排行榜</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetLeaderboard</span><span class="token punctuation">(</span><span class="token class-name">GetLeaderboardRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetLeaderboardResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 获取个人排名</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetPlayerRank</span><span class="token punctuation">(</span><span class="token class-name">GetPlayerRankRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetPlayerRankResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 更新分数</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">UpdateScore</span><span class="token punctuation">(</span><span class="token class-name">UpdateScoreRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">UpdateScoreResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 批量更新</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">BatchUpdateScores</span><span class="token punctuation">(</span><span class="token class-name">BatchUpdateScoresRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">BatchUpdateScoresResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 排行榜请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">GetLeaderboardRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token positional-class-name class-name">LeaderboardType</span> type <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> page <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> page_size <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">TimeRange</span> time_range <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 排行榜条目</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">LeaderboardEntry</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">uint64</span> player_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> player_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> rank <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> score <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token keyword">repeated</span> <span class="token builtin">string</span> extra_data <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">LeaderboardType</span> <span class="token punctuation">{</span></span>
<span class="line">  LEVEL <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span>       <span class="token comment">// 等级</span></span>
<span class="line">  POWER <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>       <span class="token comment">// 战力</span></span>
<span class="line">  ARENA <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>       <span class="token comment">// 竞技场</span></span>
<span class="line">  GUILD <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>       <span class="token comment">// 公会</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_10-系统管理-api" tabindex="-1"><a class="header-anchor" href="#_10-系统管理-api"><span>10. 系统管理 API</span></a></h2><h3 id="_10-1-服务器管理" tabindex="-1"><a class="header-anchor" href="#_10-1-服务器管理"><span>10.1 服务器管理</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// admin.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>admin<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 管理服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">AdminService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 服务器状态</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetServerStatus</span><span class="token punctuation">(</span><span class="token class-name">GetServerStatusRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetServerStatusResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 广播公告</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">BroadcastAnnouncement</span><span class="token punctuation">(</span><span class="token class-name">BroadcastAnnouncementRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">BroadcastAnnouncementResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 玩家管理</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">ManagePlayer</span><span class="token punctuation">(</span><span class="token class-name">ManagePlayerRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">ManagePlayerResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 系统配置</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">UpdateSystemConfig</span><span class="token punctuation">(</span><span class="token class-name">UpdateSystemConfigRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">UpdateSystemConfigResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 服务器状态</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">ServerStatus</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">string</span> server_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token positional-class-name class-name">ServerState</span> state <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> online_players <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> max_players <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">double</span> cpu_usage <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">double</span> memory_usage <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int64</span> uptime <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> version <span class="token operator">=</span> <span class="token number">8</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">ServerState</span> <span class="token punctuation">{</span></span>
<span class="line">  STARTING <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">  RUNNING <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  MAINTENANCE <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  SHUTTING_DOWN <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  ERROR <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_10-2-监控-api" tabindex="-1"><a class="header-anchor" href="#_10-2-监控-api"><span>10.2 监控 API</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 监控服务</span></span>
<span class="line"><span class="token keyword">service</span> <span class="token class-name">MonitoringService</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 获取指标</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetMetrics</span><span class="token punctuation">(</span><span class="token class-name">GetMetricsRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetMetricsResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 获取日志</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetLogs</span><span class="token punctuation">(</span><span class="token class-name">GetLogsRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetLogsResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 性能分析</span></span>
<span class="line">  <span class="token keyword">rpc</span> <span class="token function">GetProfilingData</span><span class="token punctuation">(</span><span class="token class-name">GetProfilingDataRequest</span><span class="token punctuation">)</span> <span class="token keyword">returns</span> <span class="token punctuation">(</span><span class="token class-name">GetProfilingDataResponse</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 性能指标</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">PerformanceMetrics</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">int64</span> timestamp <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">double</span> qps <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">double</span> latency_p50 <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">double</span> latency_p95 <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">double</span> latency_p99 <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> error_rate <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> active_connections <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_11-错误码定义" tabindex="-1"><a class="header-anchor" href="#_11-错误码定义"><span>11. 错误码定义</span></a></h2><h3 id="_11-1-全局错误码" tabindex="-1"><a class="header-anchor" href="#_11-1-全局错误码"><span>11.1 全局错误码</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// error_codes.proto</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>error<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 错误码映射表</span></span>
<span class="line"><span class="token keyword">enum</span> <span class="token class-name">ErrorCode</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token comment">// 成功</span></span>
<span class="line">  OK <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 客户端错误 1000-1999</span></span>
<span class="line">  INVALID_PARAMETER <span class="token operator">=</span> <span class="token number">1000</span><span class="token punctuation">;</span></span>
<span class="line">  MISSING_PARAMETER <span class="token operator">=</span> <span class="token number">1001</span><span class="token punctuation">;</span></span>
<span class="line">  INVALID_FORMAT <span class="token operator">=</span> <span class="token number">1002</span><span class="token punctuation">;</span></span>
<span class="line">  RATE_LIMIT_EXCEEDED <span class="token operator">=</span> <span class="token number">1003</span><span class="token punctuation">;</span></span>
<span class="line">  BANNED <span class="token operator">=</span> <span class="token number">1004</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 认证错误 1100-1199</span></span>
<span class="line">  INVALID_TOKEN <span class="token operator">=</span> <span class="token number">1100</span><span class="token punctuation">;</span></span>
<span class="line">  TOKEN_EXPIRED <span class="token operator">=</span> <span class="token number">1101</span><span class="token punctuation">;</span></span>
<span class="line">  INVALID_CREDENTIALS <span class="token operator">=</span> <span class="token number">1102</span><span class="token punctuation">;</span></span>
<span class="line">  ACCOUNT_LOCKED <span class="token operator">=</span> <span class="token number">1103</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 权限错误 1200-1299</span></span>
<span class="line">  PERMISSION_DENIED <span class="token operator">=</span> <span class="token number">1200</span><span class="token punctuation">;</span></span>
<span class="line">  INSUFFICIENT_LEVEL <span class="token operator">=</span> <span class="token number">1201</span><span class="token punctuation">;</span></span>
<span class="line">  INSUFFICIENT_RESOURCES <span class="token operator">=</span> <span class="token number">1202</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 资源错误 1300-1399</span></span>
<span class="line">  RESOURCE_NOT_FOUND <span class="token operator">=</span> <span class="token number">1300</span><span class="token punctuation">;</span></span>
<span class="line">  RESOURCE_ALREADY_EXISTS <span class="token operator">=</span> <span class="token number">1301</span><span class="token punctuation">;</span></span>
<span class="line">  RESOURCE_EXHAUSTED <span class="token operator">=</span> <span class="token number">1302</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 业务错误 2000-2999</span></span>
<span class="line">  PLAYER_NOT_FOUND <span class="token operator">=</span> <span class="token number">2000</span><span class="token punctuation">;</span></span>
<span class="line">  PLAYER_ALREADY_ONLINE <span class="token operator">=</span> <span class="token number">2001</span><span class="token punctuation">;</span></span>
<span class="line">  GUILD_NOT_FOUND <span class="token operator">=</span> <span class="token number">2002</span><span class="token punctuation">;</span></span>
<span class="line">  GUILD_FULL <span class="token operator">=</span> <span class="token number">2003</span><span class="token punctuation">;</span></span>
<span class="line">  QUEST_NOT_AVAILABLE <span class="token operator">=</span> <span class="token number">2004</span><span class="token punctuation">;</span></span>
<span class="line">  SKILL_IN_COOLDOWN <span class="token operator">=</span> <span class="token number">2005</span><span class="token punctuation">;</span></span>
<span class="line">  INSUFFICIENT_INVENTORY_SPACE <span class="token operator">=</span> <span class="token number">2006</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">  <span class="token comment">// 系统错误 3000-3999</span></span>
<span class="line">  INTERNAL_SERVER_ERROR <span class="token operator">=</span> <span class="token number">3000</span><span class="token punctuation">;</span></span>
<span class="line">  SERVICE_UNAVAILABLE <span class="token operator">=</span> <span class="token number">3001</span><span class="token punctuation">;</span></span>
<span class="line">  DATABASE_ERROR <span class="token operator">=</span> <span class="token number">3002</span><span class="token punctuation">;</span></span>
<span class="line">  NETWORK_ERROR <span class="token operator">=</span> <span class="token number">3003</span><span class="token punctuation">;</span></span>
<span class="line">  TIMEOUT <span class="token operator">=</span> <span class="token number">3004</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_12-api-版本管理" tabindex="-1"><a class="header-anchor" href="#_12-api-版本管理"><span>12. API 版本管理</span></a></h2><h3 id="_12-1-版本策略" tabindex="-1"><a class="header-anchor" href="#_12-1-版本策略"><span>12.1 版本策略</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 版本定义</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> apollo<span class="token punctuation">.</span>version<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// API版本信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">APIVersion</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">string</span> major <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>    <span class="token comment">// 主版本：不兼容的API修改</span></span>
<span class="line">  <span class="token builtin">string</span> minor <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span>    <span class="token comment">// 次版本：向下兼容的功能性新增</span></span>
<span class="line">  <span class="token builtin">string</span> patch <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>    <span class="token comment">// 修订号：向下兼容的问题修正</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 版本兼容性</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">VersionCompatibility</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">string</span> min_version <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> max_version <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> deprecated_version <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_12-2-路由策略" tabindex="-1"><a class="header-anchor" href="#_12-2-路由策略"><span>12.2 路由策略</span></a></h3><div class="language-yaml line-numbers-mode" data-highlighter="prismjs" data-ext="yml"><pre><code class="language-yaml"><span class="line"><span class="token comment"># API路由配置</span></span>
<span class="line"><span class="token key atrule">api_routes</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token key atrule">v1</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token punctuation">-</span> <span class="token key atrule">path</span><span class="token punctuation">:</span> /api/v1/player</span>
<span class="line">      <span class="token key atrule">service</span><span class="token punctuation">:</span> PlayerService</span>
<span class="line">      <span class="token key atrule">deprecated</span><span class="token punctuation">:</span> <span class="token boolean important">false</span></span>
<span class="line">    <span class="token punctuation">-</span> <span class="token key atrule">path</span><span class="token punctuation">:</span> /api/v1/battle</span>
<span class="line">      <span class="token key atrule">service</span><span class="token punctuation">:</span> BattleService</span>
<span class="line">      <span class="token key atrule">deprecated</span><span class="token punctuation">:</span> <span class="token boolean important">false</span></span>
<span class="line">  <span class="token key atrule">v2</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token punctuation">-</span> <span class="token key atrule">path</span><span class="token punctuation">:</span> /api/v2/player</span>
<span class="line">      <span class="token key atrule">service</span><span class="token punctuation">:</span> PlayerServiceV2</span>
<span class="line">      <span class="token key atrule">deprecated</span><span class="token punctuation">:</span> <span class="token boolean important">false</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_13-安全规范" tabindex="-1"><a class="header-anchor" href="#_13-安全规范"><span>13. 安全规范</span></a></h2><h3 id="_13-1-认证授权" tabindex="-1"><a class="header-anchor" href="#_13-1-认证授权"><span>13.1 认证授权</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 认证请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">AuthRequest</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">string</span> client_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> client_secret <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> grant_type <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> code <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> refresh_token <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 认证响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">AuthResponse</span> <span class="token punctuation">{</span></span>
<span class="line">  <span class="token builtin">string</span> access_token <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> refresh_token <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">int32</span> expires_in <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">  <span class="token builtin">string</span> token_type <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_13-2-签名验证" tabindex="-1"><a class="header-anchor" href="#_13-2-签名验证"><span>13.2 签名验证</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 消息签名算法</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">MessageSigner</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 生成签名</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">Sign</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> message<span class="token punctuation">,</span></span>
<span class="line">                     <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> secret_key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 验证签名</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">Verify</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> message<span class="token punctuation">,</span></span>
<span class="line">                <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> signature<span class="token punctuation">,</span></span>
<span class="line">                <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> public_key<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_14-使用示例" tabindex="-1"><a class="header-anchor" href="#_14-使用示例"><span>14. 使用示例</span></a></h2><h3 id="_14-1-客户端调用示例" tabindex="-1"><a class="header-anchor" href="#_14-1-客户端调用示例"><span>14.1 客户端调用示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// C++客户端示例</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">GameClient</span> <span class="token punctuation">{</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>PlayerService<span class="token double-colon punctuation">::</span>Stub<span class="token operator">&gt;</span> player_stub_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 创建角色</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">CreatePlayer</span><span class="token punctuation">(</span>uint64 account_id<span class="token punctuation">,</span> <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> name<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        CreatePlayerRequest request<span class="token punctuation">;</span></span>
<span class="line">        request<span class="token punctuation">.</span><span class="token function">set_account_id</span><span class="token punctuation">(</span>account_id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        request<span class="token punctuation">.</span><span class="token function">set_player_name</span><span class="token punctuation">(</span>name<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        CreatePlayerResponse response<span class="token punctuation">;</span></span>
<span class="line">        ClientContext context<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        Status status <span class="token operator">=</span> player_stub_<span class="token operator">-&gt;</span><span class="token function">CreatePlayer</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>context<span class="token punctuation">,</span></span>
<span class="line">                                                  request<span class="token punctuation">,</span></span>
<span class="line">                                                  <span class="token operator">&amp;</span>response<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> status<span class="token punctuation">.</span><span class="token function">ok</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_14-2-服务器间调用示例" tabindex="-1"><a class="header-anchor" href="#_14-2-服务器间调用示例"><span>14.2 服务器间调用示例</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 服务端调用示例</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">BattleController</span> <span class="token punctuation">{</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unique_ptr<span class="token operator">&lt;</span>BattleService<span class="token double-colon punctuation">::</span>Stub<span class="token operator">&gt;</span> battle_stub_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 使用技能</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">HandleCastSkill</span><span class="token punctuation">(</span>uint64 player_id<span class="token punctuation">,</span> uint32 skill_id<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        CastSkillRequest request<span class="token punctuation">;</span></span>
<span class="line">        request<span class="token punctuation">.</span><span class="token function">set_caster_id</span><span class="token punctuation">(</span>player_id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        request<span class="token punctuation">.</span><span class="token function">set_skill_id</span><span class="token punctuation">(</span>skill_id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 异步调用</span></span>
<span class="line">        battle_stub_<span class="token operator">-&gt;</span><span class="token function">AsyncCastSkill</span><span class="token punctuation">(</span><span class="token operator">&amp;</span>context<span class="token punctuation">,</span></span>
<span class="line">                                    request<span class="token punctuation">,</span></span>
<span class="line">                                    <span class="token operator">&amp;</span>callback<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="_15-总结" tabindex="-1"><a class="header-anchor" href="#_15-总结"><span>15. 总结</span></a></h2><p>本API设计文档定义了Apollo MMORPG服务器的完整接口规范，包括：</p><ol><li><strong>统一的API设计风格</strong>: 使用Protocol Buffers定义清晰的接口</li><li><strong>完整的功能覆盖</strong>: 涵盖玩家、战斗、社交、任务等核心系统</li><li><strong>版本管理</strong>: 支持API版本演进和向后兼容</li><li><strong>安全机制</strong>: 完整的认证授权和签名验证</li><li><strong>错误处理</strong>: 统一的错误码和错误信息</li></ol><p>这些API将作为系统间通信的契约，确保各个服务之间的协同工作。</p>`,72)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};