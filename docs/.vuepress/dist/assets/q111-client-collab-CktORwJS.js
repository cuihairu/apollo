import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q111-client-collab.html","title":"Q111: 如何与客户端同学协作？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q111-client-collab.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q111-client-collab.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q111-如何与客户端同学协作" tabindex="-1"><a class="header-anchor" href="#q111-如何与客户端同学协作"><span>Q111: 如何与客户端同学协作？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察跨团队协作能力：</p><ul><li>接口设计</li><li>协议定义</li><li>联调流程</li><li>问题排查</li></ul><hr><h2 id="一、协作模式" tabindex="-1"><a class="header-anchor" href="#一、协作模式"><span>一、协作模式</span></a></h2><h3 id="_1-1-协作流程" tabindex="-1"><a class="header-anchor" href="#_1-1-协作流程"><span>1.1 协作流程</span></a></h3>`,7),i(d,{code:`eJxLy8kvT85ILCpR8AniUgACx+jna6c97Zj0ck7Ds41NsQq6unYKTtHP5vQ+7Vr4fPX6F+v2vVi3MBaiFCzpHP103aJnHduRJMGyTmBZl+invf0v1q17um7Wk52dEH3OEBmIMhcwxzX6Wd/Sp/2Ln01rf7ZwMUSZK1jGDWH30z0NT/snIku6I+yGSYJl3cCyHtEvGqe82ND8bGv3i/VTIfrcITIQZR5gjmf0y+nrXi6a8WT/uqdLeiHKPKHKAFBwdhI=`}),o[1]||=e(`<hr><h2 id="二、协议设计" tabindex="-1"><a class="header-anchor" href="#二、协议设计"><span>二、协议设计</span></a></h2><h3 id="_2-1-protobuf-定义" tabindex="-1"><a class="header-anchor" href="#_2-1-protobuf-定义"><span>2.1 Protobuf 定义</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line"><span class="token comment">// 游戏协议定义</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 基础定义</span></span>
<span class="line"><span class="token keyword">syntax</span> <span class="token operator">=</span> <span class="token string">&quot;proto3&quot;</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">package</span> game<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ========== 通用消息 ==========</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Empty</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 错误信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Error</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int32</span> code <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> message <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 位置信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">Vector3</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">float</span> x <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">float</span> y <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">float</span> z <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ========== 登录协议 ==========</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 登录请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">LoginRequest</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">string</span> account <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> password <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> device_id <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> client_version <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 登录响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">LoginResponse</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int32</span> code <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> message <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> token <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">ServerInfo</span> servers <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">ServerInfo</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int32</span> server_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> server_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> address <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> port <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> load <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">bool</span> is_recommend <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ========== 角色协议 ==========</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 创建角色请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CreateCharacterRequest</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">string</span> name <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> class_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> gender <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 角色信息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CharacterInfo</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int64</span> character_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> level <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> class_id <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> exp <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token positional-class-name class-name">Vector3</span> position <span class="token operator">=</span> <span class="token number">6</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int64</span> gold <span class="token operator">=</span> <span class="token number">7</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 创建角色响应</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CreateCharacterResponse</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int32</span> code <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> message <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token positional-class-name class-name">CharacterInfo</span> character <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ========== 游戏消息 ==========</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 位置同步</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">PositionSync</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int64</span> entity_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token positional-class-name class-name">Vector3</span> position <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">float</span> yaw <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">uint32</span> timestamp <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 技能释放</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CastSkillRequest</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int32</span> skill_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int64</span> target_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token positional-class-name class-name">Vector3</span> target_position <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 技能释放通知</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">CastSkillNotify</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int64</span> caster_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> skill_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int64</span> target_id <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token positional-class-name class-name">Vector3</span> position <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">uint32</span> timestamp <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 伤害通知</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">DamageNotify</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int64</span> target_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int64</span> attacker_id <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> damage <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> current_hp <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">bool</span> is_critical <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// ========== 聊天协议 ==========</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 聊天请求</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">ChatRequest</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int32</span> channel <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span>  <span class="token comment">// 1:世界 2:公会 3:私聊</span></span>
<span class="line">    <span class="token builtin">string</span> content <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int64</span> target_id <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span>  <span class="token comment">// 私聊时使用</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 聊天消息</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">ChatMessage</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int64</span> sender_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> sender_name <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">int32</span> channel <span class="token operator">=</span> <span class="token number">3</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">string</span> content <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">uint32</span> timestamp <span class="token operator">=</span> <span class="token number">5</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 服务端消息定义</span></span>
<span class="line"><span class="token keyword">message</span> <span class="token class-name">ServerMessage</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">uint32</span> msg_id <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token builtin">uint64</span> sequence <span class="token operator">=</span> <span class="token number">2</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token keyword">oneof</span> body <span class="token punctuation">{</span></span>
<span class="line">        <span class="token positional-class-name class-name">LoginResponse</span> login_response <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token positional-class-name class-name">CreateCharacterResponse</span> create_character_response <span class="token operator">=</span> <span class="token number">11</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token positional-class-name class-name">CharacterInfo</span> character_info <span class="token operator">=</span> <span class="token number">12</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token positional-class-name class-name">PositionSync</span> position_sync <span class="token operator">=</span> <span class="token number">20</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token positional-class-name class-name">CastSkillNotify</span> cast_skill_notify <span class="token operator">=</span> <span class="token number">21</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token positional-class-name class-name">DamageNotify</span> damage_notify <span class="token operator">=</span> <span class="token number">22</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token positional-class-name class-name">ChatMessage</span> chat_message <span class="token operator">=</span> <span class="token number">30</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token positional-class-name class-name">Error</span> error <span class="token operator">=</span> <span class="token number">100</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-接口文档" tabindex="-1"><a class="header-anchor" href="#_2-2-接口文档"><span>2.2 接口文档</span></a></h3><div class="language-markdown line-numbers-mode" data-highlighter="prismjs" data-ext="md"><pre><code class="language-markdown"><span class="line"><span class="token title important"><span class="token punctuation">#</span> 接口文档模板</span></span>
<span class="line"></span>
<span class="line"><span class="token title important"><span class="token punctuation">##</span> 消息: 登录</span></span>
<span class="line"></span>
<span class="line"><span class="token title important"><span class="token punctuation">###</span> 请求 (Client -&gt; Server)</span></span>
<span class="line">\`\`\`protobuf</span>
<span class="line">LoginRequest {</span>
<span class="line">    string account;      // 账号</span>
<span class="line">    string password;     // 密码 (MD5)</span>
<span class="line">    string device_id;    // 设备 ID</span>
<span class="line">    string client_version; // 客户端版本</span>
<span class="line">}</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="响应-server-client" tabindex="-1"><a class="header-anchor" href="#响应-server-client"><span>响应 (Server -&gt; Client)</span></a></h3><div class="language-protobuf line-numbers-mode" data-highlighter="prismjs" data-ext="protobuf"><pre><code class="language-protobuf"><span class="line">LoginResponse <span class="token punctuation">{</span></span>
<span class="line">    <span class="token builtin">int32</span> code<span class="token punctuation">;</span>         <span class="token comment">// 0:成功, 其他:失败</span></span>
<span class="line">    <span class="token builtin">string</span> <span class="token keyword">message</span><span class="token punctuation">;</span>     <span class="token comment">// 提示信息</span></span>
<span class="line">    <span class="token builtin">string</span> token<span class="token punctuation">;</span>       <span class="token comment">// 登录令牌</span></span>
<span class="line">    <span class="token keyword">repeated</span> <span class="token positional-class-name class-name">ServerInfo</span> servers<span class="token punctuation">;</span> <span class="token comment">// 可选服务器列表</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="错误码" tabindex="-1"><a class="header-anchor" href="#错误码"><span>错误码</span></a></h3><table><thead><tr><th>Code</th><th>说明</th></tr></thead><tbody><tr><td>0</td><td>成功</td></tr><tr><td>1001</td><td>账号不存在</td></tr><tr><td>1002</td><td>密码错误</td></tr><tr><td>1003</td><td>账号被封禁</td></tr><tr><td>1004</td><td>版本不匹配</td></tr><tr><td>1005</td><td>服务器已满</td></tr></tbody></table><h3 id="注意事项" tabindex="-1"><a class="header-anchor" href="#注意事项"><span>注意事项</span></a></h3><ol><li>密码需要 MD5 加密后传输</li><li>token 有效期 24 小时</li><li>建议客户端缓存最近登录的服务器</li></ol><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line"></span>
<span class="line">---</span>
<span class="line"></span>
<span class="line">## 三、联调工具</span>
<span class="line"></span>
<span class="line">### 3.1 测试工具</span>
<span class="line"></span>
<span class="line">\`\`\`python</span>
<span class="line"># 联调测试工具</span>
<span class="line"></span>
<span class="line">import socket</span>
<span class="line">import struct</span>
<span class="line">import json</span>
<span class="line">import time</span>
<span class="line"></span>
<span class="line">class GameClientTester:</span>
<span class="line">    &quot;&quot;&quot;游戏客户端测试器&quot;&quot;&quot;</span>
<span class="line"></span>
<span class="line">    def __init__(self, host, port):</span>
<span class="line">        self.host = host</span>
<span class="line">        self.port = port</span>
<span class="line">        self.socket = None</span>
<span class="line">        self.sequence = 0</span>
<span class="line"></span>
<span class="line">    def connect(self):</span>
<span class="line">        &quot;&quot;&quot;连接服务器&quot;&quot;&quot;</span>
<span class="line">        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)</span>
<span class="line">        self.socket.connect((self.host, self.port))</span>
<span class="line">        print(f&quot;Connected to {self.host}:{self.port}&quot;)</span>
<span class="line"></span>
<span class="line">    def disconnect(self):</span>
<span class="line">        &quot;&quot;&quot;断开连接&quot;&quot;&quot;</span>
<span class="line">        if self.socket:</span>
<span class="line">            self.socket.close()</span>
<span class="line"></span>
<span class="line">    def send_message(self, msg_id, body):</span>
<span class="line">        &quot;&quot;&quot;发送消息&quot;&quot;&quot;</span>
<span class="line">        self.sequence += 1</span>
<span class="line"></span>
<span class="line">        # 序列化消息体</span>
<span class="line">        if isinstance(body, str):</span>
<span class="line">            body_bytes = body.encode(&#39;utf-8&#39;)</span>
<span class="line">        else:</span>
<span class="line">            body_bytes = json.dumps(body).encode(&#39;utf-8&#39;)</span>
<span class="line"></span>
<span class="line">        # 构造消息头</span>
<span class="line">        header = struct.pack(&#39;&gt;IHH&#39;,</span>
<span class="line">            len(body_bytes),  # 消息长度</span>
<span class="line">            msg_id,           # 消息 ID</span>
<span class="line">            self.sequence     # 序列号</span>
<span class="line">        )</span>
<span class="line"></span>
<span class="line">        # 发送</span>
<span class="line">        self.socket.sendall(header + body_bytes)</span>
<span class="line">        print(f&quot;Sent: msg_id={msg_id}, seq={self.sequence}&quot;)</span>
<span class="line"></span>
<span class="line">    def receive_message(self, timeout=5):</span>
<span class="line">        &quot;&quot;&quot;接收消息&quot;&quot;&quot;</span>
<span class="line">        self.socket.settimeout(timeout)</span>
<span class="line"></span>
<span class="line">        try:</span>
<span class="line">            # 接收消息头 (8 字节)</span>
<span class="line">            header = self._recv_all(8)</span>
<span class="line">            if not header:</span>
<span class="line">                return None</span>
<span class="line"></span>
<span class="line">            length, msg_id, sequence = struct.unpack(&#39;&gt;IHH&#39;, header)</span>
<span class="line"></span>
<span class="line">            # 接收消息体</span>
<span class="line">            body = self._recv_all(length)</span>
<span class="line">            if body:</span>
<span class="line">                body_data = json.loads(body.decode(&#39;utf-8&#39;))</span>
<span class="line">                print(f&quot;Recv: msg_id={msg_id}, seq={sequence}&quot;)</span>
<span class="line">                return {&#39;msg_id&#39;: msg_id, &#39;sequence&#39;: sequence, &#39;body&#39;: body_data}</span>
<span class="line"></span>
<span class="line">        except socket.timeout:</span>
<span class="line">            print(&quot;Receive timeout&quot;)</span>
<span class="line">            return None</span>
<span class="line"></span>
<span class="line">    def _recv_all(self, size):</span>
<span class="line">        &quot;&quot;&quot;接收指定大小数据&quot;&quot;&quot;</span>
<span class="line">        data = b&#39;&#39;</span>
<span class="line">        while len(data) &lt; size:</span>
<span class="line">            packet = self.socket.recv(size - len(data))</span>
<span class="line">            if not packet:</span>
<span class="line">                return None</span>
<span class="line">            data += packet</span>
<span class="line">        return data</span>
<span class="line"></span>
<span class="line"></span>
<span class="line"># 使用示例</span>
<span class="line">def test_login():</span>
<span class="line">    &quot;&quot;&quot;测试登录&quot;&quot;&quot;</span>
<span class="line">    client = GameClientTester(&#39;localhost&#39;, 9999)</span>
<span class="line">    client.connect()</span>
<span class="line"></span>
<span class="line">    # 发送登录请求</span>
<span class="line">    login_request = {</span>
<span class="line">        &#39;account&#39;: &#39;test_user&#39;,</span>
<span class="line">        &#39;password&#39;: &#39;md5_hash_here&#39;,</span>
<span class="line">        &#39;device_id&#39;: &#39;test_device&#39;,</span>
<span class="line">        &#39;client_version&#39;: &#39;1.0.0&#39;</span>
<span class="line">    }</span>
<span class="line">    client.send_message(1001, login_request)</span>
<span class="line"></span>
<span class="line">    # 接收响应</span>
<span class="line">    response = client.receive_message()</span>
<span class="line">    if response:</span>
<span class="line">        print(f&quot;Response: {response[&#39;body&#39;]}&quot;)</span>
<span class="line"></span>
<span class="line">    client.disconnect()</span>
<span class="line"></span>
<span class="line"></span>
<span class="line">if __name__ == &#39;__main__&#39;:</span>
<span class="line">    test_login()</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、问题排查" tabindex="-1"><a class="header-anchor" href="#四、问题排查"><span>四、问题排查</span></a></h2><h3 id="_4-1-常见问题" tabindex="-1"><a class="header-anchor" href="#_4-1-常见问题"><span>4.1 常见问题</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 联调常见问题</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">CollaborationIssues</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;协作问题&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    issues <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token string">&#39;Protocol_Mismatch&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;现象&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;消息解析失败&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;原因&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;Protobuf 版本不一致&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;字段定义不同&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;大小端问题&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;解决&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;统一 Protobuf 定义文件&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;版本号校验&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;添加日志打印原始数据&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">        <span class="token string">&#39;State_Inconsistency&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;现象&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;客户端显示和服务端不一致&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;原因&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;客户端预测错误&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;服务端校验失败&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;网络延迟导致&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;解决&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;增加服务端校正&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;客户端显示服务端状态&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;添加偏差日志&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">        <span class="token string">&#39;Timing_Issues&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;现象&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;操作顺序不对&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;原因&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;异步处理时序&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;消息乱序&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;并发竞争&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;解决&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;添加序列号&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;请求-响应匹配&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;状态机检查&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span></span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">,</span></span>
<span class="line"></span>
<span class="line">        <span class="token string">&#39;Performance_Issues&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;现象&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;卡顿/掉帧&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;原因&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;消息量过大&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;处理不及时&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;主线程阻塞&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;解决&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span></span>
<span class="line">                <span class="token string">&#39;消息合并&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;异步处理&#39;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;性能分析定位&#39;</span></span>
<span class="line">            <span class="token punctuation">]</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、最佳实践" tabindex="-1"><a class="header-anchor" href="#五、最佳实践"><span>五、最佳实践</span></a></h2><h3 id="_5-1-协作建议" tabindex="-1"><a class="header-anchor" href="#_5-1-协作建议"><span>5.1 协作建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>协议优先</strong></td><td>先定义协议再开发</td></tr><tr><td><strong>文档同步</strong></td><td>自动生成文档</td></tr><tr><td><strong>版本兼容</strong></td><td>支持多版本协议</td></tr><tr><td><strong>联调环境</strong></td><td>独立的测试环境</td></tr><tr><td><strong>日志规范</strong></td><td>统一的日志格式</td></tr><tr><td><strong>定期同步</strong></td><td>每周例会同步进度</td></tr></tbody></table><hr><h2 id="六、总结" tabindex="-1"><a class="header-anchor" href="#六、总结"><span>六、总结</span></a></h2><h3 id="协作核心" tabindex="-1"><a class="header-anchor" href="#协作核心"><span>协作核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">客户端协作 = 协议统一 + 文档完善 + 联调工具 + 问题快速响应</span>
<span class="line">- Protobuf 统一定义</span>
<span class="line">- 自动生成文档和代码</span>
<span class="line">- 测试工具辅助开发</span>
<span class="line">- 及时沟通解决问题</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://developers.google.com/protocol-buffers" target="_blank" rel="noopener noreferrer">Protocol Buffers Guide</a></li><li><a href="https://www.gabrielgambetta.com/client-server-game-architecture/" target="_blank" rel="noopener noreferrer">Game Client-Server Communication</a></li></ul>`,28)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};