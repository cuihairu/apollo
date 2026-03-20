import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q11-transport-protocol.html","title":"Q11: TCP vs UDP vs KCP，MMO 中各自的使用场景是什么？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q11-transport-protocol.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q11-transport-protocol.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q11-tcp-vs-udp-vs-kcp-mmo-中各自的使用场景是什么" tabindex="-1"><a class="header-anchor" href="#q11-tcp-vs-udp-vs-kcp-mmo-中各自的使用场景是什么"><span>Q11: TCP vs UDP vs KCP，MMO 中各自的使用场景是什么？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对网络传输协议的理解：</p><ul><li>TCP、UDP、KCP 的本质区别</li><li>各自的优缺点和适用场景</li><li>KBEngine 为什么选择 TCP</li><li>MMO 游戏中的协议选择策略</li></ul><hr><h2 id="一、协议基础" tabindex="-1"><a class="header-anchor" href="#一、协议基础"><span>一、协议基础</span></a></h2><h3 id="_1-1-三层协议模型" tabindex="-1"><a class="header-anchor" href="#_1-1-三层协议模型"><span>1.1 三层协议模型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    传输协议层次                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │              应用层                              │       │</span>
<span class="line">│  │  (游戏逻辑: 移动、战斗、聊天等)                   │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                        │                                     │</span>
<span class="line">│        ┌───────────────┼───────────────┐                    │</span>
<span class="line">│        ▼               ▼               ▼                    │</span>
<span class="line">│  ┌──────────┐    ┌──────────┐    ┌──────────┐              │</span>
<span class="line">│  │   TCP    │    │   UDP    │    │   KCP    │              │</span>
<span class="line">│  │可靠传输  │    │不可靠    │    │可靠UDP   │              │</span>
<span class="line">│  │面向连接  │    │无连接    │    │面向连接  │              │</span>
<span class="line">│  └──────────┘    └──────────┘    └──────────┘              │</span>
<span class="line">│        │               │               │                    │</span>
<span class="line">│        └───────────────┼───────────────┘                    │</span>
<span class="line">│                        ▼                                     │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │              IP 层 (网络层)                       │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-协议特性对比" tabindex="-1"><a class="header-anchor" href="#_1-2-协议特性对比"><span>1.2 协议特性对比</span></a></h3><table><thead><tr><th>特性</th><th>TCP</th><th>UDP</th><th>KCP</th></tr></thead><tbody><tr><td><strong>连接性</strong></td><td>面向连接</td><td>无连接</td><td>面向连接</td></tr><tr><td><strong>可靠性</strong></td><td>可靠传输</td><td>不可靠</td><td>可靠传输</td></tr><tr><td><strong>顺序保证</strong></td><td>✅ 保证</td><td>❌ 不保证</td><td>✅ 保证</td></tr><tr><td><strong>流量控制</strong></td><td>✅ 滑动窗口</td><td>❌ 无</td><td>✅ 自定义窗口</td></tr><tr><td><strong>拥塞控制</strong></td><td>✅ 慢启动等</td><td>❌ 无</td><td>⚠️ 可配置</td></tr><tr><td><strong>首部开销</strong></td><td>20-60 字节</td><td>8 字节</td><td>~24 字节</td></tr><tr><td><strong>延迟</strong></td><td>较高</td><td>最低</td><td>中等</td></tr><tr><td><strong>吞吐量</strong></td><td>中等</td><td>最高</td><td>中高</td></tr></tbody></table><hr><h2 id="二、tcp-协议详解" tabindex="-1"><a class="header-anchor" href="#二、tcp-协议详解"><span>二、TCP 协议详解</span></a></h2><h3 id="_2-1-tcp-特点" tabindex="-1"><a class="header-anchor" href="#_2-1-tcp-特点"><span>2.1 TCP 特点</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                     TCP 特点                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 三次握手建立连接                                         │</span>
<span class="line">│     Client ──[SYN]──► Server                               │</span>
<span class="line">│     Client ◄──[SYN+ACK]── Server                           │</span>
<span class="line">│     Client ──[ACK]──► Server ✓ 连接建立                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 可靠传输机制                                             │</span>
<span class="line">│     ├── 序列号 (保证顺序)                                    │</span>
<span class="line">│     ├── 确认应答 (ACK)                                       │</span>
<span class="line">│     ├── 超时重传 (RTO)                                       │</span>
<span class="line">│     ├── 滑动窗口 (流量控制)                                  │</span>
<span class="line">│     └── 拥塞控制 (网络拥塞时降低发送速率)                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 四次挥手断开连接                                         │</span>
<span class="line">│     Client ──[FIN]──► Server                               │</span>
<span class="line">│     Client ◄──[ACK]── Server                               │</span>
<span class="line">│     Client ◄──[FIN]── Server                               │</span>
<span class="line">│     Client ──[ACK]──► Server ✓ 连接关闭                     │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-2-tcp-的优缺点" tabindex="-1"><a class="header-anchor" href="#_2-2-tcp-的优缺点"><span>2.2 TCP 的优缺点</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">优点：</span>
<span class="line">✅ 可靠传输 - 保证数据不丢失</span>
<span class="line">✅ 顺序保证 - 数据按序到达</span>
<span class="line">✅ 流量控制 - 避免接收方溢出</span>
<span class="line">✅ 拥塞控制 - 保护网络整体</span>
<span class="line">✅ 通用性好 - 所有网络环境支持</span>
<span class="line"></span>
<span class="line">缺点：</span>
<span class="line">❌ 延迟较高 - 握手、确认、重传</span>
<span class="line">❌ 首部开销大 - 至少 20 字节</span>
<span class="line">❌ 粘包问题 - 应用层需要处理</span>
<span class="line">❌ HEAD-OF-LINE BLOCKING - 丢包阻塞后续数据</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-3-tcp-在-mmo-中的应用" tabindex="-1"><a class="header-anchor" href="#_2-3-tcp-在-mmo-中的应用"><span>2.3 TCP 在 MMO 中的应用</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">适用场景：</span>
<span class="line">├── 登录认证       ✓ (不能丢，数据少)</span>
<span class="line">├── 聊天消息       ✓ (可靠性重要)</span>
<span class="line">├── 交易系统       ✓ (绝对不能丢)</span>
<span class="line">├── 背包操作       ✓ (数据一致性)</span>
<span class="line">├── 任务系统       ✓ (非实时)</span>
<span class="line">└── 排行榜         ✓ (批量数据)</span>
<span class="line"></span>
<span class="line">不适用场景：</span>
<span class="line">├── 实时移动       ✗ (延迟敏感)</span>
<span class="line">├── 实时战斗       ✗ (频率高，容忍丢包)</span>
<span class="line">└── 位置同步       ✗ (容忍丢包，新数据覆盖旧数据)</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、udp-协议详解" tabindex="-1"><a class="header-anchor" href="#三、udp-协议详解"><span>三、UDP 协议详解</span></a></h2><h3 id="_3-1-udp-特点" tabindex="-1"><a class="header-anchor" href="#_3-1-udp-特点"><span>3.1 UDP 特点</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                     UDP 特点                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  1. 无连接                                                   │</span>
<span class="line">│     直接发送，无需握手                                       │</span>
<span class="line">│     Client ──[数据包]──► Server                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│  2. 不可靠                                                   │</span>
<span class="line">│     ├── 不保证到达                                           │</span>
<span class="line">│     ├── 不保证顺序                                           │</span>
<span class="line">│     ├── 不重传丢失包                                         │</span>
<span class="line">│     └── 不进行流量控制                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  3. 轻量级                                                   │</span>
<span class="line">│     ├── 首部仅 8 字节                                        │</span>
<span class="line">│     ├── 无状态维护                                           │</span>
<span class="line">│     └── 发送速率无限制                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">│  UDP 首部结构：                                              │</span>
<span class="line">│  ┌──────────────────────────────────────────────────┐       │</span>
<span class="line">│  │ 源端口(16) │ 目标端口(16) │ 长度(16) │ 校验和(16) │       │</span>
<span class="line">│  └──────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-udp-的优缺点" tabindex="-1"><a class="header-anchor" href="#_3-2-udp-的优缺点"><span>3.2 UDP 的优缺点</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">优点：</span>
<span class="line">✅ 延迟最低 - 无握手，无等待</span>
<span class="line">✅ 首部开销小 - 仅 8 字节</span>
<span class="line">✅ 发送速率高 - 无拥塞控制</span>
<span class="line">✅ 支持广播/组播</span>
<span class="line">✅ 穿透性好 - NAT 穿透容易</span>
<span class="line"></span>
<span class="line">缺点：</span>
<span class="line">❌ 不可靠 - 数据可能丢失</span>
<span class="line">❌ 无序 - 可能乱序到达</span>
<span class="line">❌ 无流量控制 - 可能淹没接收方</span>
<span class="line">❌ 需要应用层实现可靠性</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-3-udp-在-mmo-中的应用" tabindex="-1"><a class="header-anchor" href="#_3-3-udp-在-mmo-中的应用"><span>3.3 UDP 在 MMO 中的应用</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">适用场景：</span>
<span class="line">├── 实时移动       ✓ (新位置覆盖旧位置)</span>
<span class="line">├── 位置同步       ✓ (容忍丢包)</span>
<span class="line">├── 战斗动作       ✓ (实时性重要)</span>
<span class="line">├── 语音聊天       ✓ (容忍丢包)</span>
<span class="line">└── 状态广播       ✓ (高频更新)</span>
<span class="line"></span>
<span class="line">不适用场景：</span>
<span class="line">├── 交易系统       ✗ (必须可靠)</span>
<span class="line">├── 道具获取       ✗ (不能丢)</span>
<span class="line">└── 账号操作       ✗ (安全性)</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kcp-协议详解" tabindex="-1"><a class="header-anchor" href="#四、kcp-协议详解"><span>四、KCP 协议详解</span></a></h2><h3 id="_4-1-kcp-简介" tabindex="-1"><a class="header-anchor" href="#_4-1-kcp-简介"><span>4.1 KCP 简介</span></a></h3><p>KCP 是一个快速可靠传输协议（ARQ，自动重传请求），以比 TCP 浪费带宽 10%-20% 的代价换取更低的延迟。</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      KCP 协议                               │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  KCP = RTO (超时重传) + 快速重传 + 独特前向纠错               │</span>
<span class="line">│                                                             │</span>
<span class="line">│  核心特性：                                                  │</span>
<span class="line">│  ├── 降低延迟：RTT 时间内发送多次                            │</span>
<span class="line">│  ├── 快速重传：不依赖超时，立即重传                          │</span>
<span class="line">│  ├── 独特 FEC：前向纠错，恢复丢失数据                        │</span>
<span class="line">│  └── 可配置：根据场景调整参数                                │</span>
<span class="line">│                                                             │</span>
<span class="line">│  与 TCP 对比：                                              │</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │  指标          │ TCP  │ KCP  │ 改善            │       │</span>
<span class="line">│  ├─────────────────────────────────────────────────┤       │</span>
<span class="line">│  │  延迟 (30%丢包) │ 200ms│ 50ms │ ↓ 75%          │       │</span>
<span class="line">│  │  延迟 (10%丢包) │ 130ms│ 30ms │ ↓ 77%          │       │</span>
<span class="line">│  │  流量 (30%丢包) │ 100% │ 120% │ ↑ 20%          │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-kcp-工作原理" tabindex="-1"><a class="header-anchor" href="#_4-2-kcp-工作原理"><span>4.2 KCP 工作原理</span></a></h3>`,32),i(d,{code:`eJwrTi0sTc1LTnXJTEwvSszlUgCCgsSikszkzILEvBKFYIXEYoWn/RNfNjQ+m7YTQzoIJP2sb+mzKdtA0mB5v/ySVIX8stQihWArmNapG571rnva06pgCFYSrGtnFwSUBAkoaADdYGuoCZcA6Zqw/mnXiqf7V79smP+yvffJngUv1i18Nn3b05krsNgRFBKiAJR8OX3L07ZWiIVPl8x6tmYhul1GELuMEHZBJYwhEsaa6KYD5eG+e7a44dnW7qcdGyDOfrJj0dMlG8Hqg3QhznZ09gbZ8WzW9OdLdiGrwjAXqPj56u6nvZshvnu/p+PJjt7naztfbGsF+uT9nk4swQRRitWJU7bBnIXhHkwRIwwRYy4AMQbOfA==`}),o[1]||=e(`<h3 id="_4-3-kcp-参数配置" tabindex="-1"><a class="header-anchor" href="#_4-3-kcp-参数配置"><span>4.3 KCP 参数配置</span></a></h3><div class="language-c line-numbers-mode" data-highlighter="prismjs" data-ext="c"><pre><code class="language-c"><span class="line"><span class="token comment">// KCP 核心参数</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 1. nodelay (是否启用 nodelay 模式)</span></span>
<span class="line"><span class="token comment">//    0: 不启用（默认，类似 TCP）</span></span>
<span class="line"><span class="token comment">//    1: 启用（RTT 内多次发送，降低延迟）</span></span>
<span class="line"><span class="token function">ikcp_nodelay</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span>          <span class="token comment">// 启用 nodelay</span></span>
<span class="line"><span class="token function">ikcp_interval</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">10</span><span class="token punctuation">)</span><span class="token punctuation">;</span>         <span class="token comment">// 内部更新时钟间隔 (ms)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 2. 快速重传限制</span></span>
<span class="line"><span class="token comment">//    0: 关闭快速重传（传统 RTO）</span></span>
<span class="line"><span class="token comment">//    1: 原始快速重传（触发后立即重传）</span></span>
<span class="line"><span class="token comment">//    2: 激进快速重传（连续触发）</span></span>
<span class="line"><span class="token function">ikcp_fastreconv</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span>        <span class="token comment">// 启用快速重传</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 3. 拥塞控制窗口</span></span>
<span class="line"><span class="token comment">//    0: 不启用拥塞控制</span></span>
<span class="line"><span class="token comment">//    1: 启用（类似 TCP）</span></span>
<span class="line"><span class="token function">ikcp_setmtu</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">1400</span><span class="token punctuation">)</span><span class="token punctuation">;</span>         <span class="token comment">// MTU 大小</span></span>
<span class="line"><span class="token function">ikcp_wndsize</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">128</span><span class="token punctuation">,</span> <span class="token number">128</span><span class="token punctuation">)</span><span class="token punctuation">;</span>   <span class="token comment">// 发送/接收窗口大小</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-4-kcp-在-mmo-中的应用" tabindex="-1"><a class="header-anchor" href="#_4-4-kcp-在-mmo-中的应用"><span>4.4 KCP 在 MMO 中的应用</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">适用场景：</span>
<span class="line">├── 实时战斗       ✓ (低延迟 + 可靠)</span>
<span class="line">├── 技能释放       ✓ (不能丢但要求低延迟)</span>
<span class="line">├── 伤害结算       ✓ (可靠性重要)</span>
<span class="line">├── 移动同步       ✓ (位置更新)</span>
<span class="line">└── 状态同步       ✓ (需要可靠但实时)</span>
<span class="line"></span>
<span class="line">不适用场景：</span>
<span class="line">├── 大文件传输     ✗ (带宽浪费)</span>
<span class="line">└── 非实时数据     ✗ (TCP 更合适)</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、kbengine-的协议选择" tabindex="-1"><a class="header-anchor" href="#五、kbengine-的协议选择"><span>五、KBEngine 的协议选择</span></a></h2><h3 id="_5-1-kbengine-为什么选择-tcp" tabindex="-1"><a class="header-anchor" href="#_5-1-kbengine-为什么选择-tcp"><span>5.1 KBEngine 为什么选择 TCP</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine 源码分析</a>：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 使用 TCP 作为传输协议</span></span>
<span class="line"><span class="token comment">// src/server/network/endpoint.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Endpoint</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 只支持 TCP</span></span>
<span class="line">    <span class="token keyword">enum</span> <span class="token class-name">Protocol</span> <span class="token punctuation">{</span></span>
<span class="line">        TCP <span class="token operator">=</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 创建 TCP socket</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">initTCP</span><span class="token punctuation">(</span><span class="token keyword">int</span> addressFamily <span class="token operator">=</span> AF_INET<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>选择 TCP 的原因</strong>：</p><ol><li><strong>可靠性优先</strong> - 账号、数据不能丢失</li><li><strong>开发简单</strong> - 无需实现可靠性层</li><li><strong>通用性好</strong> - 所有环境支持</li><li><strong>调试方便</strong> - 可用 Wireshark 直接分析</li><li><strong>延迟可接受</strong> - 本地通信延迟 &lt; 1ms</li></ol><h3 id="_5-2-kbengine-的-tcp-优化" tabindex="-1"><a class="header-anchor" href="#_5-2-kbengine-的-tcp-优化"><span>5.2 KBEngine 的 TCP 优化</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 对 TCP 的优化</span></span>
<span class="line"><span class="token comment">// src/server/network/channel.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 启用 TCP_NODELAY (禁用 Nagle 算法)</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setNoDelay</span><span class="token punctuation">(</span><span class="token keyword">bool</span> noDelay<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> flag <span class="token operator">=</span> noDelay <span class="token operator">?</span> <span class="token number">1</span> <span class="token operator">:</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">setsockopt</span><span class="token punctuation">(</span>socket_<span class="token punctuation">,</span> IPPROTO_TCP<span class="token punctuation">,</span> TCP_NODELAY<span class="token punctuation">,</span></span>
<span class="line">                   <span class="token punctuation">(</span><span class="token keyword">char</span><span class="token operator">*</span><span class="token punctuation">)</span><span class="token operator">&amp;</span>flag<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span><span class="token keyword">int</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 启用 SO_KEEPALIVE</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">setKeepAlive</span><span class="token punctuation">(</span><span class="token keyword">bool</span> keepAlive<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">int</span> flag <span class="token operator">=</span> keepAlive <span class="token operator">?</span> <span class="token number">1</span> <span class="token operator">:</span> <span class="token number">0</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token function">setsockopt</span><span class="token punctuation">(</span>socket_<span class="token punctuation">,</span> SOL_SOCKET<span class="token punctuation">,</span> SO_KEEPALIVE<span class="token punctuation">,</span></span>
<span class="line">                   <span class="token punctuation">(</span><span class="token keyword">char</span><span class="token operator">*</span><span class="token punctuation">)</span><span class="token operator">&amp;</span>flag<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span><span class="token keyword">int</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-3-kbengine-的可靠-udp-实现" tabindex="-1"><a class="header-anchor" href="#_5-3-kbengine-的可靠-udp-实现"><span>5.3 KBEngine 的可靠 UDP 实现</span></a></h3><p>KBEngine 内部有可靠 UDP 的实现（未默认启用）：</p><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 可靠 UDP (可选)</span></span>
<span class="line"><span class="token comment">// src/server/network/reliable_udp.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ReliableUDP</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 序列号</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> sequence_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 确认号</span></span>
<span class="line">    <span class="token keyword">uint16_t</span> ack_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 重传队列</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>queue<span class="token operator">&lt;</span>Packet<span class="token operator">*</span><span class="token operator">&gt;</span> retransmitQueue_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送数据（带可靠保证）</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendReliable</span><span class="token punctuation">(</span>Packet<span class="token operator">*</span> packet<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        packet<span class="token operator">-&gt;</span>sequence <span class="token operator">=</span> <span class="token operator">++</span>sequence_<span class="token punctuation">;</span></span>
<span class="line">        retransmitQueue_<span class="token punctuation">.</span><span class="token function">push</span><span class="token punctuation">(</span>packet<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment">// 发送</span></span>
<span class="line">        socket_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>packet<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 处理 ACK</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">onAck</span><span class="token punctuation">(</span><span class="token keyword">uint16_t</span> ack<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token punctuation">(</span><span class="token operator">!</span>retransmitQueue_<span class="token punctuation">.</span><span class="token function">empty</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">&amp;&amp;</span></span>
<span class="line">               retransmitQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token operator">-&gt;</span>sequence <span class="token operator">&lt;=</span> ack<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">delete</span> retransmitQueue_<span class="token punctuation">.</span><span class="token function">front</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">            retransmitQueue_<span class="token punctuation">.</span><span class="token function">pop</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、协议选择策略" tabindex="-1"><a class="header-anchor" href="#六、协议选择策略"><span>六、协议选择策略</span></a></h2><h3 id="_6-1-决策树" tabindex="-1"><a class="header-anchor" href="#_6-1-决策树"><span>6.1 决策树</span></a></h3>`,19),i(d,{code:`eJxljs9KAlEUxvc+xYXWEiMRKWmItipSyRYxSOhtbgmDIzoUooFSgQRloJZRoYuSNjWLQkSbfJhm7sy8RWf+lBe6m3PO/X7nOx8RpWN8mC3JKB33IXjbMgw8HY9ps0VHTdpQMsjvj6AUV6U9Rb8eWg91c9jQW4r1ONA+B+ZXe+3E56ymOJusAVZDqUBVn47MWZ92W/SsDwhDgEsN7cSTvHH/Qa+eNXVmdF7sj4xnFJgbbcSSvAdAm2F1xyY916H1DIB0QkONSQWSP+Ct80tDfQuhgrQviNlKmFvNlRYjJFuWSwKWCkdhzrUGD2cTaqIo/63BuLeViK9vRne9E5DWAaFGi0Ven3QgQghpqr3wXW+YyqvVf3dNmTgsS5s9enMLLL2om6fqvwAsa9xNdbULrDZ5or22F6IsV0TBiULyohhaIATjHGYk+/KvRIJ4iZHsQ66EV4RlHPT9ACoz0m0=`}),o[2]||=e(`<h3 id="_6-2-场景对照表" tabindex="-1"><a class="header-anchor" href="#_6-2-场景对照表"><span>6.2 场景对照表</span></a></h3><table><thead><tr><th>游戏系统</th><th>推荐协议</th><th>原因</th></tr></thead><tbody><tr><td><strong>登录认证</strong></td><td>TCP</td><td>安全性、可靠性</td></tr><tr><td><strong>角色创建</strong></td><td>TCP</td><td>数据一致性</td></tr><tr><td><strong>聊天系统</strong></td><td>TCP</td><td>可靠性重要</td></tr><tr><td><strong>好友系统</strong></td><td>TCP</td><td>数据不能丢</td></tr><tr><td><strong>背包操作</strong></td><td>TCP</td><td>物品不能丢</td></tr><tr><td><strong>交易系统</strong></td><td>TCP</td><td>绝对可靠</td></tr><tr><td><strong>任务系统</strong></td><td>TCP</td><td>非实时</td></tr><tr><td><strong>移动同步</strong></td><td>UDP/KCP</td><td>实时性、容错</td></tr><tr><td><strong>战斗动作</strong></td><td>KCP</td><td>低延迟 + 可靠</td></tr><tr><td><strong>技能释放</strong></td><td>KCP</td><td>低延迟 + 可靠</td></tr><tr><td><strong>伤害结算</strong></td><td>KCP/TCP</td><td>可靠性</td></tr><tr><td><strong>位置广播</strong></td><td>UDP</td><td>容错、高频</td></tr><tr><td><strong>状态同步</strong></td><td>KCP</td><td>实时可靠</td></tr><tr><td><strong>语音聊天</strong></td><td>UDP</td><td>容错、实时</td></tr></tbody></table><h3 id="_6-3-混合协议架构" tabindex="-1"><a class="header-anchor" href="#_6-3-混合协议架构"><span>6.3 混合协议架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">MMO 服务器混合协议架构：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      游戏服务器                              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│   Client                                                    │</span>
<span class="line">│     │                                                       │</span>
<span class="line">│     ├── TCP 连接 (可靠数据)                                 │</span>
<span class="line">│     │    ├── 登录/认证                                      │</span>
<span class="line">│     │    ├── 背包/交易                                      │</span>
<span class="line">│     │    ├── 聊天/社交                                      │</span>
<span class="line">│     │    └── 任务/成就                                      │</span>
<span class="line">│     │                                                       │</span>
<span class="line">│     ├── KCP 连接 (战斗数据)                                 │</span>
<span class="line">│     │    ├── 技能释放                                       │</span>
<span class="line">│     │    ├── 伤害结算                                       │</span>
<span class="line">│     │    └── 战斗状态                                       │</span>
<span class="line">│     │                                                       │</span>
<span class="line">│     └── UDP 连接 (实时数据)                                 │</span>
<span class="line">│          ├── 位置同步                                       │</span>
<span class="line">│          ├── 动作广播                                       │</span>
<span class="line">│          └── 语音聊天                                       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、性能对比" tabindex="-1"><a class="header-anchor" href="#七、性能对比"><span>七、性能对比</span></a></h2><h3 id="_7-1-延迟对比" tabindex="-1"><a class="header-anchor" href="#_7-1-延迟对比"><span>7.1 延迟对比</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">网络环境：30% 丢包率，RTT = 100ms</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  协议    │ 平均延迟 │ 最大延迟 │ 丢包恢复 │ 带宽利用率      │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│  TCP    │  200ms   │  500ms   │  1-2s   │     80%         │</span>
<span class="line">│  UDP    │  100ms   │  100ms   │   N/A   │    100%         │</span>
<span class="line">│  KCP    │   50ms   │  150ms   │  200ms  │     95%         │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-2-带宽对比" tabindex="-1"><a class="header-anchor" href="#_7-2-带宽对比"><span>7.2 带宽对比</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">发送 1000 个小包 (每包 50 字节)</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  协议    │ 首部开销 │ 总流量    │ 相比 TCP              │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│  TCP    │  20KB    │  70KB    │  100%                 │</span>
<span class="line">│  UDP    │   8KB    │  58KB    │  83%                  │</span>
<span class="line">│  KCP    │  24KB    │  74KB    │  106%                 │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、实现建议" tabindex="-1"><a class="header-anchor" href="#八、实现建议"><span>八、实现建议</span></a></h2><h3 id="_8-1-tcp-优化建议" tabindex="-1"><a class="header-anchor" href="#_8-1-tcp-优化建议"><span>8.1 TCP 优化建议</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// TCP 连接优化</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 1. 禁用 Nagle 算法 (TCP_NODELAY)</span></span>
<span class="line"><span class="token keyword">int</span> flag <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token function">setsockopt</span><span class="token punctuation">(</span>socket<span class="token punctuation">,</span> IPPROTO_TCP<span class="token punctuation">,</span> TCP_NODELAY<span class="token punctuation">,</span> <span class="token operator">&amp;</span>flag<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>flag<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 2. 启用 Keep-Alive</span></span>
<span class="line"><span class="token keyword">int</span> keepalive <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token function">setsockopt</span><span class="token punctuation">(</span>socket<span class="token punctuation">,</span> SOL_SOCKET<span class="token punctuation">,</span> SO_KEEPALIVE<span class="token punctuation">,</span> <span class="token operator">&amp;</span>keepalive<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>keepalive<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 3. 设置发送/接收缓冲区</span></span>
<span class="line"><span class="token keyword">int</span> buf_size <span class="token operator">=</span> <span class="token number">256</span> <span class="token operator">*</span> <span class="token number">1024</span><span class="token punctuation">;</span>  <span class="token comment">// 256KB</span></span>
<span class="line"><span class="token function">setsockopt</span><span class="token punctuation">(</span>socket<span class="token punctuation">,</span> SOL_SOCKET<span class="token punctuation">,</span> SO_SNDBUF<span class="token punctuation">,</span> <span class="token operator">&amp;</span>buf_size<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>buf_size<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token function">setsockopt</span><span class="token punctuation">(</span>socket<span class="token punctuation">,</span> SOL_SOCKET<span class="token punctuation">,</span> SO_RCVBUF<span class="token punctuation">,</span> <span class="token operator">&amp;</span>buf_size<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>buf_size<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 4. 设置重用地址</span></span>
<span class="line"><span class="token keyword">int</span> reuse <span class="token operator">=</span> <span class="token number">1</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token function">setsockopt</span><span class="token punctuation">(</span>socket<span class="token punctuation">,</span> SOL_SOCKET<span class="token punctuation">,</span> SO_REUSEADDR<span class="token punctuation">,</span> <span class="token operator">&amp;</span>reuse<span class="token punctuation">,</span> <span class="token keyword">sizeof</span><span class="token punctuation">(</span>reuse<span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_8-2-kcp-使用建议" tabindex="-1"><a class="header-anchor" href="#_8-2-kcp-使用建议"><span>8.2 KCP 使用建议</span></a></h3><div class="language-c line-numbers-mode" data-highlighter="prismjs" data-ext="c"><pre><code class="language-c"><span class="line"><span class="token comment">// KCP 配置建议</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// MMO 实时战斗配置</span></span>
<span class="line"><span class="token function">ikcp_nodelay</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span>        <span class="token comment">// 启用 nodelay</span></span>
<span class="line"><span class="token function">ikcp_interval</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">10</span><span class="token punctuation">)</span><span class="token punctuation">;</span>       <span class="token comment">// 10ms 更新间隔</span></span>
<span class="line"><span class="token function">ikcp_fastreconv</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span>      <span class="token comment">// 启用快速重传</span></span>
<span class="line"><span class="token function">ikcp_fastack</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span>         <span class="token comment">// 启用快速 ACK</span></span>
<span class="line"><span class="token function">ikcp_wndsize</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">256</span><span class="token punctuation">,</span> <span class="token number">256</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 大窗口</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// MMO 移动同步配置</span></span>
<span class="line"><span class="token function">ikcp_nodelay</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">1</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"><span class="token function">ikcp_interval</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">20</span><span class="token punctuation">)</span><span class="token punctuation">;</span>       <span class="token comment">// 20ms 更新间隔</span></span>
<span class="line"><span class="token function">ikcp_wndsize</span><span class="token punctuation">(</span>kcp<span class="token punctuation">,</span> <span class="token number">128</span><span class="token punctuation">,</span> <span class="token number">128</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 中等窗口</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_8-3-混合协议实现" tabindex="-1"><a class="header-anchor" href="#_8-3-混合协议实现"><span>8.3 混合协议实现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 混合协议管理器</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">HybridNetworkManager</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// TCP 连接 (可靠数据)</span></span>
<span class="line">    TCPChannel<span class="token operator">*</span> tcpChannel_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// KCP 连接 (低延迟可靠)</span></span>
<span class="line">    KCPChannel<span class="token operator">*</span> kcpChannel_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// UDP 连接 (实时数据)</span></span>
<span class="line">    UDPChannel<span class="token operator">*</span> udpChannel_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 发送消息</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">sendMessage</span><span class="token punctuation">(</span>Message<span class="token operator">*</span> msg<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">switch</span> <span class="token punctuation">(</span>msg<span class="token operator">-&gt;</span>type<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">case</span> MsgType<span class="token double-colon punctuation">::</span>LOGIN<span class="token operator">:</span></span>
<span class="line">            <span class="token keyword">case</span> MsgType<span class="token double-colon punctuation">::</span>TRADE<span class="token operator">:</span></span>
<span class="line">            <span class="token keyword">case</span> MsgType<span class="token double-colon punctuation">::</span>CHAT<span class="token operator">:</span></span>
<span class="line">                tcpChannel_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> MsgType<span class="token double-colon punctuation">::</span>SKILL<span class="token operator">:</span></span>
<span class="line">            <span class="token keyword">case</span> MsgType<span class="token double-colon punctuation">::</span>DAMAGE<span class="token operator">:</span></span>
<span class="line">                kcpChannel_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">case</span> MsgType<span class="token double-colon punctuation">::</span>MOVE<span class="token operator">:</span></span>
<span class="line">            <span class="token keyword">case</span> MsgType<span class="token double-colon punctuation">::</span>POSITION<span class="token operator">:</span></span>
<span class="line">                udpChannel_<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>msg<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">                <span class="token keyword">break</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="九、总结" tabindex="-1"><a class="header-anchor" href="#九、总结"><span>九、总结</span></a></h2><h3 id="协议选择总结" tabindex="-1"><a class="header-anchor" href="#协议选择总结"><span>协议选择总结</span></a></h3><table><thead><tr><th>场景</th><th>推荐协议</th><th>配置要点</th></tr></thead><tbody><tr><td><strong>KBEngine 默认</strong></td><td>TCP</td><td>TCP_NODELAY</td></tr><tr><td><strong>实时战斗</strong></td><td>KCP</td><td>nodelay=1, fastreconv=1</td></tr><tr><td><strong>位置同步</strong></td><td>UDP</td><td>无需可靠性</td></tr><tr><td><strong>交易系统</strong></td><td>TCP</td><td>绝对可靠</td></tr><tr><td><strong>语音聊天</strong></td><td>UDP</td><td>容错性</td></tr></tbody></table><h3 id="最佳实践" tabindex="-1"><a class="header-anchor" href="#最佳实践"><span>最佳实践</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">1. 默认使用 TCP</span>
<span class="line">   - 开发简单</span>
<span class="line">   - 调试方便</span>
<span class="line">   - 可靠性好</span>
<span class="line"></span>
<span class="line">2. 延迟敏感场景用 KCP</span>
<span class="line">   - 实时战斗</span>
<span class="line">   - 技能释放</span>
<span class="line">   - 状态同步</span>
<span class="line"></span>
<span class="line">3. 容错场景用 UDP</span>
<span class="line">   - 位置同步</span>
<span class="line">   - 状态广播</span>
<span class="line">   - 语音数据</span>
<span class="line"></span>
<span class="line">4. 混合使用</span>
<span class="line">   - 根据消息类型选择通道</span>
<span class="line">   - 互不干扰</span>
<span class="line">   - 各取所长</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine/tree/master/kbe/src/server/network" target="_blank" rel="noopener noreferrer">KBEngine GitHub - 网络层源码</a></li><li><a href="https://github.com/skywind3000/kcp" target="_blank" rel="noopener noreferrer">KCP 协议官网</a></li><li><a href="https://en.wikipedia.org/wiki/Transmission_Control_Protocol" target="_blank" rel="noopener noreferrer">TCP/IP 详解</a></li><li><a href="https://en.wikipedia.org/wiki/User_Datagram_Protocol" target="_blank" rel="noopener noreferrer">UDP 协议详解</a></li></ul>`,27)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};