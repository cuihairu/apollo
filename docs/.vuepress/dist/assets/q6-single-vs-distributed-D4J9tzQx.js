import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q6-single-vs-distributed.html","title":"Q6: 单服架构 vs 分布式架构，如何选择？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q6-single-vs-distributed.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q6-single-vs-distributed.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q6-单服架构-vs-分布式架构-如何选择" tabindex="-1"><a class="header-anchor" href="#q6-单服架构-vs-分布式架构-如何选择"><span>Q6: 单服架构 vs 分布式架构，如何选择？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对服务器架构选择的决策能力：</p><ul><li>单服架构和分布式架构的本质区别</li><li>KBEngine 的分布式架构设计思路</li><li>在什么场景下选择哪种架构</li><li>分布式架构的挑战和解决方案</li></ul><hr><h2 id="一、架构定义" tabindex="-1"><a class="header-anchor" href="#一、架构定义"><span>一、架构定义</span></a></h2><h3 id="_1-1-单服架构-single-server-architecture" tabindex="-1"><a class="header-anchor" href="#_1-1-单服架构-single-server-architecture"><span>1.1 单服架构（Single-Server Architecture）</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                       单服架构                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │              单一服务器进程                       │       │</span>
<span class="line">│   │  ┌───────────────────────────────────────────┐  │       │</span>
<span class="line">│   │  │  网络层 │ 业务逻辑 │ 数据访问 │ 数据库   │  │       │</span>
<span class="line">│   │  └───────────────────────────────────────────┘  │       │</span>
<span class="line">│   └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                        │                                     │</span>
<span class="line">│                        ▼                                     │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │              客户端连接池                        │       │</span>
<span class="line">│   │  Player1, Player2, ..., PlayerN                 │       │</span>
<span class="line">│   └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">特点：</span>
<span class="line">- 所有功能在一个进程内完成</span>
<span class="line">- 共享内存，无需序列化</span>
<span class="line">- 部署简单，调试方便</span>
<span class="line">- 扩展受限于单机资源</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-分布式架构-distributed-architecture" tabindex="-1"><a class="header-anchor" href="#_1-2-分布式架构-distributed-architecture"><span>1.2 分布式架构（Distributed Architecture）</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                   KBEngine 分布式架构                        │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│   Client ──► Loginapp ──► Baseapp ──► Cellapp              │</span>
<span class="line">│      │          │            │           │                  │</span>
<span class="line">│      │          ▼            ▼           ▼                  │</span>
<span class="line">│      │     BaseappMgr   CellappMgr     Space               │</span>
<span class="line">│      │          │            │                              │</span>
<span class="line">│      └──────────┴────────────┴──────────────► DBMgr        │</span>
<span class="line">│                                    │                        │</span>
<span class="line">│                                    ▼                        │</span>
<span class="line">│                                 Database                    │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">特点：</span>
<span class="line">- 功能按职责分离到不同进程</span>
<span class="line">- 进程间通过网络通信</span>
<span class="line">- 可独立扩展各组件</span>
<span class="line">- 动态负载均衡</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、kbengine-的分布式架构" tabindex="-1"><a class="header-anchor" href="#二、kbengine-的分布式架构"><span>二、KBEngine 的分布式架构</span></a></h2><h3 id="_2-1-架构图" tabindex="-1"><a class="header-anchor" href="#_2-1-架构图"><span>2.1 架构图</span></a></h3><p>根据 <a href="https://www.kbelab.com/manual/engine-overview.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 引擎概览</a>：</p>`,14),i(d,{code:`eJyNk8tq20AUhvd+CuFCaRdBSG43JQQkeVOwet+JLpRGcgzCFrJLa9AiCcT1IikJbZqa0BgHm96o1dBAwMb2y3Sk0VtkbgojR4JoIZjzfzpn/nOObKfx7s2m6bWEV2pBQE/z7XrVM91NQXNqVr3VNIpgfBZ2L6PfATjfKb4mEH40yaCIIHFBOQnKXLCUBJ/QoFXfKKSLVRrVWt0oRr0pmB2l61QUg6im666ue+IaZeJfezDYFqPZIdj9B7qdeHc/L7dqNi3koj+JBlvp1KqiG1hFqfWqR7Ljo+K60XgQHXRA7wcPSwksSIR95jXet8Xw6G+4Pxaj4fz/ZMjj8jUu3wYvXeO5XdIsx0FN+jmJjy/irSmcHy6NBPnBDOcHH7P9aMgPg5kfmlgSlaePxbD7NfxyzNNosoymdigt59Bo5IzOdVM2W6ZRpP1I+yjrRllNHDBg8inDQ1k17untl88r4gtro9ZM8/FJJ5oP7y+V1yThLlpT/CoJKytrPt0nGFyG5zs+2jZCVRSihf0RDM7gRR/OZj5eF16Ei8/g5JRuoJ/Km10ILk7DjyOcBitoP8i7RK+1FKQfkLrg2wc4GKACrHoWST3DYAo6PR+1LxNcxSQ42Av/jES42I6+T3FSckvCoKGxFik67QyZMP25bqJ0UBhEY2CjbbUdC7fHrjnOozuWZD+0bU5BDWSSbdsPLImTcM0cCRVJFJyxcAULxqlM`}),o[1]||=e(`<h3 id="_2-2-组件职责对比" tabindex="-1"><a class="header-anchor" href="#_2-2-组件职责对比"><span>2.2 组件职责对比</span></a></h3><table><thead><tr><th>组件</th><th>职责</th><th>可多实例</th><th>单点故障</th></tr></thead><tbody><tr><td><strong>Loginapp</strong></td><td>登录验证、网关分配</td><td>✅</td><td>❌</td></tr><tr><td><strong>Baseapp</strong></td><td>Proxy管理、数据缓存、社交系统</td><td>✅</td><td>❌ (有备份)</td></tr><tr><td><strong>Cellapp</strong></td><td>空间逻辑、AOI、战斗系统</td><td>✅</td><td>❌</td></tr><tr><td><strong>BaseappMgr</strong></td><td>BaseApp负载均衡</td><td>❌</td><td>⚠️</td></tr><tr><td><strong>CellappMgr</strong></td><td>CellApp负载均衡</td><td>❌</td><td>⚠️</td></tr><tr><td><strong>DBMgr</strong></td><td>数据库访问管理</td><td>❌</td><td>⚠️</td></tr></tbody></table><blockquote><p><strong>关键设计</strong>：只有 Manager 和 DBMgr 是单点，但可以通过进程监控快速重启。</p></blockquote><h3 id="_2-3-完整通信流程" tabindex="-1"><a class="header-anchor" href="#_2-3-完整通信流程"><span>2.3 完整通信流程</span></a></h3>`,4),i(d,{code:`eJx1kstO20AUhvc8xSxhQaJeVlkg1famUtL2FUZ0FFkKtmuHXnZp1apuBEkphECThrYSbaHggBRIwDK8TGbGeQvm4piJDFnF5/vPOf9/bA+9WkXWMjJMWHbhyhxgPwe6VXPZdKBVBTqAHtArJrKqGVbkrGiXTQs6ToZqJY416CFGS2U3K1B4dq/o1lGlck83R4okww0ODY23CvbMriJgv0Yu0AvgQQ7Q3RBHrcnOOQnOhEBfXFoqFpJ63B+S0w/zqx5yLbiC8g70vDe2+3JBSItMahTA5HAt7r+PB39wcyjqxqKcIUukdULWg7RBKzEgxpJubRw14sFeHEX0+0f1BFppOuJ6C3d6CcLdE/yjlkxinCWQNon/Fdf35mdkC3fkfZhjA3uksa/u4oE1vokD6Ux6kHXsd3B4+cK1377DQW8cbaaQRcf1n9x84wAH50pOcQDWmwFaYlsuk7bv8PmI++zgT/tkNCJ+czzapq21GbPCptTQg8tJe5C60tP7qp+ELg4qYvKDJkg5qGhl1Wlg/l/Jyx+TCZIn3mdDcTvS7L3RHueADIWvDmmjn4aSu+nfENf/5clWiD+H9PgL2VwfR91bB1Pdk+dP4+AXDdp54u+Q7fakFsZXG6qOG724Jt+OabgrXt6s1egINzcYwsFv4g/p//7cDVACrj4=`}),o[2]||=e(`<hr><h2 id="三、架构对比分析" tabindex="-1"><a class="header-anchor" href="#三、架构对比分析"><span>三、架构对比分析</span></a></h2><h3 id="_3-1-对比表格" tabindex="-1"><a class="header-anchor" href="#_3-1-对比表格"><span>3.1 对比表格</span></a></h3><table><thead><tr><th>维度</th><th>单服架构</th><th>分布式架构 (KBEngine)</th></tr></thead><tbody><tr><td><strong>开发复杂度</strong></td><td>低，一个工程完成</td><td>中高，多个组件协同</td></tr><tr><td><strong>部署复杂度</strong></td><td>低，单机部署</td><td>中高，多机协同</td></tr><tr><td><strong>调试难度</strong></td><td>低，单进程调试</td><td>高，跨进程调试</td></tr><tr><td><strong>横向扩展</strong></td><td>❌ 受限于单机</td><td>✅ 可无限扩展</td></tr><tr><td><strong>纵向扩展</strong></td><td>✅ 升级硬件即可</td><td>⚠️ 需配合架构调整</td></tr><tr><td><strong>容错能力</strong></td><td>❌ 单点故障</td><td>✅ 组件隔离</td></tr><tr><td><strong>资源隔离</strong></td><td>❌ 共享资源</td><td>✅ 独立资源</td></tr><tr><td><strong>通信开销</strong></td><td>❌ 无(内存访问)</td><td>⚠️ 序列化+网络传输</td></tr><tr><td><strong>数据一致性</strong></td><td>✅ 简单(事务)</td><td>⚠️ 需分布式事务</td></tr><tr><td><strong>运维成本</strong></td><td>低</td><td>高</td></tr></tbody></table><h3 id="_3-2-性能对比" tabindex="-1"><a class="header-anchor" href="#_3-2-性能对比"><span>3.2 性能对比</span></a></h3><h4 id="cpu-利用率" tabindex="-1"><a class="header-anchor" href="#cpu-利用率"><span>CPU 利用率</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">单服架构：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    CPU 利用率分布                            │</span>
<span class="line">│  ████████████████████████████████████ 100%                 │</span>
<span class="line">│                                                             │</span>
<span class="line">│  问题：CPU 密集型任务会阻塞其他任务                          │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">分布式架构：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    CPU 利用率分布                            │</span>
<span class="line">│  Baseapp:    ████████████████████░░░░ 60%                  │</span>
<span class="line">│  Cellapp 1:  ██████████████████████░░ 70%                  │</span>
<span class="line">│  Cellapp 2:  ████████████████░░░░░░░ 50%                  │</span>
<span class="line">│                                                             │</span>
<span class="line">│  优势：任务隔离，互不影响                                    │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="内存使用" tabindex="-1"><a class="header-anchor" href="#内存使用"><span>内存使用</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">单服架构：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      内存使用                                │</span>
<span class="line">│  所有数据共享同一内存空间                                    │</span>
<span class="line">│  - 优点：无序列化开销                                        │</span>
<span class="line">│  - 缺点：内存泄漏影响全局                                    │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">分布式架构：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      内存使用                                │</span>
<span class="line">│  每个进程独立内存空间                                        │</span>
<span class="line">│  - 优点：隔离性好，单进程崩溃不影响其他                      │</span>
<span class="line">│  - 缺点：需要序列化传输数据                                  │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、选择标准" tabindex="-1"><a class="header-anchor" href="#四、选择标准"><span>四、选择标准</span></a></h2><h3 id="_4-1-决策树" tabindex="-1"><a class="header-anchor" href="#_4-1-决策树"><span>4.1 决策树</span></a></h3>`,12),i(d,{code:`eJxtkVtLwlAcwN/9FAd6Fq9FihiE1kNImr1JD3mZDcRAFxEaDC+hzLn1IBUspZsZQXYhkWX4ZTpn7qmv0NnOHBMaDM7h//v//pdD5Q6PUwf7BQbshmwAf3EGXxJwwsJHTmWbiHtC3RHq1vaA3R4EMVdJva0hqQelgSJPf2QZdV7XTm16asylMeUAWHY6AQ6VQZzOZ3OZBOQ7SOINjxXFoB3/Bh1zl9B4jBqC8vYFuxzWWtCgZjXAEF1kCnTyiMmkE7BxBsdVOBHmepLk1pMgf6M0Ww50x2HlvB0rEIls70Q3HRvROC7vKWljC+ezzxqSRXMqD1HV67PRx0JtaxhJTfVKxBJvCQpD1H6AwgW859F1Bcp9U+Ul7OXwHw+JQbFv9kneQz/ryyfHcD6dQO3BjBf9wLrYQLLgCMLps8r2yBi/k5bywmJErQ6U73eyeEtZ3andF4yL6yTSfmVrPZzP0vmMsd4ic4J7MvsBFJ3L+ZdSq5mVlM8CGHIjTFGUL+W1/QFFEwmj`}),o[3]||=e(`<h3 id="_4-2-详细选择标准" tabindex="-1"><a class="header-anchor" href="#_4-2-详细选择标准"><span>4.2 详细选择标准</span></a></h3><h4 id="选择单服架构的场景" tabindex="-1"><a class="header-anchor" href="#选择单服架构的场景"><span>选择单服架构的场景</span></a></h4><table><thead><tr><th>条件</th><th>说明</th></tr></thead><tbody><tr><td><strong>在线规模 &lt; 500</strong></td><td>单机可承载</td></tr><tr><td><strong>游戏类型简单</strong></td><td>卡牌、棋类、回合制</td></tr><tr><td><strong>开发周期短</strong></td><td>快速原型验证</td></tr><tr><td><strong>团队规模小</strong></td><td>&lt; 5 人</td></tr><tr><td><strong>预算有限</strong></td><td>服务器成本敏感</td></tr><tr><td><strong>无需跨服</strong></td><td>单服即可满足需求</td></tr></tbody></table><h4 id="选择分布式架构的场景" tabindex="-1"><a class="header-anchor" href="#选择分布式架构的场景"><span>选择分布式架构的场景</span></a></h4><table><thead><tr><th>条件</th><th>说明</th></tr></thead><tbody><tr><td><strong>在线规模 &gt; 1000</strong></td><td>需要多机负载</td></tr><tr><td><strong>空间型游戏</strong></td><td>MMORPG、FPS、大世界</td></tr><tr><td><strong>需要动态扩容</strong></td><td>活动期间流量波动大</td></tr><tr><td><strong>需要跨服功能</strong></td><td>跨服战场、聊天</td></tr><tr><td><strong>团队有运维能力</strong></td><td>有专门的运维人员</td></tr><tr><td><strong>追求高可用</strong></td><td>商业化运营</td></tr></tbody></table><hr><h2 id="五、kbengine-的设计权衡" tabindex="-1"><a class="header-anchor" href="#五、kbengine-的设计权衡"><span>五、KBEngine 的设计权衡</span></a></h2><h3 id="_5-1-为什么选择分布式" tabindex="-1"><a class="header-anchor" href="#_5-1-为什么选择分布式"><span>5.1 为什么选择分布式？</span></a></h3><p>根据 <a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine GitHub</a>：</p><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">KBEngine 底层架构被设计为多进程分布式动态负载均衡方案，</span>
<span class="line">理论上只需要不断扩展硬件就能够不断增加承载上限，</span>
<span class="line">单台机器的承载上限取决于游戏逻辑本身的复杂度。</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>设计目标</strong>：</p><ol><li><strong>无上限扩展</strong> - 通过增加机器提升承载</li><li><strong>组件解耦</strong> - 各组件独立开发和升级</li><li><strong>故障隔离</strong> - 单个组件崩溃不影响全局</li><li><strong>灵活部署</strong> - 可根据负载动态调整</li></ol><h3 id="_5-2-分布式的代价" tabindex="-1"><a class="header-anchor" href="#_5-2-分布式的代价"><span>5.2 分布式的代价</span></a></h3><h4 id="通信开销" tabindex="-1"><a class="header-anchor" href="#通信开销"><span>通信开销</span></a></h4><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 单服架构：内存访问</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">updateEntityPosition</span><span class="token punctuation">(</span>Entity<span class="token operator">*</span> entity<span class="token punctuation">,</span> Position pos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    entity<span class="token operator">-&gt;</span>position <span class="token operator">=</span> pos<span class="token punctuation">;</span>  <span class="token comment">// 直接内存访问，~10ns</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 分布式架构：网络通信</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">updateEntityPosition</span><span class="token punctuation">(</span>EntityID id<span class="token punctuation">,</span> Position pos<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// 1. 序列化</span></span>
<span class="line">    MemoryStream stream<span class="token punctuation">;</span></span>
<span class="line">    stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>id<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>pos<span class="token punctuation">.</span>x<span class="token punctuation">)</span><span class="token punctuation">;</span> stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>pos<span class="token punctuation">.</span>y<span class="token punctuation">)</span><span class="token punctuation">;</span> stream<span class="token punctuation">.</span><span class="token function">pack</span><span class="token punctuation">(</span>pos<span class="token punctuation">.</span>z<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 2. 网络传输</span></span>
<span class="line">    channel<span class="token operator">-&gt;</span><span class="token function">send</span><span class="token punctuation">(</span>stream<span class="token punctuation">.</span><span class="token function">data</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> stream<span class="token punctuation">.</span><span class="token function">size</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// ~100μs</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 3. 接收端反序列化</span></span>
<span class="line">    <span class="token comment">// ...</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><p><strong>开销对比</strong>：内存访问 ~10ns vs 网络通信 ~100μs（相差 10000 倍）</p><h4 id="一致性挑战" tabindex="-1"><a class="header-anchor" href="#一致性挑战"><span>一致性挑战</span></a></h4><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 单服架构：事务保证</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">transferItem</span><span class="token punctuation">(</span>PlayerID from<span class="token punctuation">,</span> PlayerID to<span class="token punctuation">,</span> ItemID item<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    db<span class="token operator">-&gt;</span><span class="token function">begin</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    db<span class="token operator">-&gt;</span><span class="token function">removeItem</span><span class="token punctuation">(</span>from<span class="token punctuation">,</span> item<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    db<span class="token operator">-&gt;</span><span class="token function">addItem</span><span class="token punctuation">(</span>to<span class="token punctuation">,</span> item<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    db<span class="token operator">-&gt;</span><span class="token function">commit</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span>  <span class="token comment">// 原子操作</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 分布式架构：跨进程</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">transferItem</span><span class="token punctuation">(</span>PlayerID from<span class="token punctuation">,</span> PlayerID to<span class="token punctuation">,</span> ItemID item<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token comment">// from 和 to 可能在不同 BaseApp</span></span>
<span class="line">    <span class="token comment">// 需要分布式事务或两阶段提交</span></span>
<span class="line">    BaseApp<span class="token operator">*</span> fromApp <span class="token operator">=</span> <span class="token function">findBaseApp</span><span class="token punctuation">(</span>from<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    BaseApp<span class="token operator">*</span> toApp <span class="token operator">=</span> <span class="token function">findBaseApp</span><span class="token punctuation">(</span>to<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> <span class="token punctuation">(</span>fromApp <span class="token operator">!=</span> toApp<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token comment">// 跨 BaseApp 交易，更复杂</span></span>
<span class="line">        <span class="token function">distributedTransfer</span><span class="token punctuation">(</span>fromApp<span class="token punctuation">,</span> toApp<span class="token punctuation">,</span> from<span class="token punctuation">,</span> to<span class="token punctuation">,</span> item<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="六、混合架构方案" tabindex="-1"><a class="header-anchor" href="#六、混合架构方案"><span>六、混合架构方案</span></a></h2><h3 id="_6-1-渐进式架构" tabindex="-1"><a class="header-anchor" href="#_6-1-渐进式架构"><span>6.1 渐进式架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">阶段 1：单服起步</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│  │           单一服务器进程                        │       │</span>
<span class="line">│  │  所有逻辑在一起，快速开发                        │       │</span>
<span class="line">│  └─────────────────────────────────────────────────┘       │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line">         │ 游戏上线，玩家增长</span>
<span class="line">         ▼</span>
<span class="line">阶段 2：数据库分离</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  ┌─────────────────┐              ┌─────────────────┐       │</span>
<span class="line">│  │   游戏服务器     │ ──────────►  │    数据库       │       │</span>
<span class="line">│  │  (逻辑+缓存)     │              │  (MySQL/Redis)  │       │</span>
<span class="line">│  └─────────────────┘              └─────────────────┘       │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line">         │ 继续增长</span>
<span class="line">         ▼</span>
<span class="line">阶段 3：逻辑分离</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │</span>
<span class="line">│  │ 网关服务器 │  │逻辑服务器 │  │空间服务器 │                  │</span>
<span class="line">│  │ (Gateway) │  │(BaseApp) │  │(CellApp) │                  │</span>
<span class="line">│  └──────────┘  └──────────┘  └──────────┘                  │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-业务分离架构" tabindex="-1"><a class="header-anchor" href="#_6-2-业务分离架构"><span>6.2 业务分离架构</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">根据业务特性选择架构：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      游戏系统分类                            │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  单服即可：                                                  │</span>
<span class="line">│  ├── 聊天系统      → 可独立为 ChatApp                       │</span>
<span class="line">│  ├── 好友系统      → 可在 BaseApp 实现                       │</span>
<span class="line">│  ├── 排行榜        → 可独立使用 Redis Sorted Set             │</span>
<span class="line">│  ├── 邮件系统      → 可独立为 MailApp                       │</span>
<span class="line">│  └── GM 系统       → 可独立为 GMApp                         │</span>
<span class="line">│                                                             │</span>
<span class="line">│  需要分布式：                                                │</span>
<span class="line">│  ├── 空间逻辑      → 必须用 CellApp                         │</span>
<span class="line">│  ├── 战斗系统      → 必须用 CellApp                         │</span>
<span class="line">│  ├── AOI 系统      → 必须用 CellApp                         │</span>
<span class="line">│  └── 大量玩家交互  → 必须分布式                              │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、实战建议" tabindex="-1"><a class="header-anchor" href="#七、实战建议"><span>七、实战建议</span></a></h2><h3 id="_7-1-小团队建议" tabindex="-1"><a class="header-anchor" href="#_7-1-小团队建议"><span>7.1 小团队建议</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">如果你是 &lt; 10 人的小团队：</span>
<span class="line"></span>
<span class="line">1. 第一款游戏：单服架构</span>
<span class="line">   - 使用成熟的框架（如 KBEngine）</span>
<span class="line">   - 专注游戏逻辑，而非底层架构</span>
<span class="line">   - 快速上线验证</span>
<span class="line"></span>
<span class="line">2. 第二款游戏：考虑分布式</span>
<span class="line">   - 如果第一款成功，有架构经验</span>
<span class="line">   - 可以使用 KBEngine 的分布式特性</span>
<span class="line">   - 逐步学习运维知识</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-2-架构迁移建议" tabindex="-1"><a class="header-anchor" href="#_7-2-架构迁移建议"><span>7.2 架构迁移建议</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">从单服迁移到分布式：</span>
<span class="line"></span>
<span class="line">1. 识别瓶颈</span>
<span class="line">   ├── CPU 密集型 → 分离为独立服务</span>
<span class="line">   ├── IO 密集型  → 使用异步+缓存</span>
<span class="line">   └── 内存密集型 → 分离数据服务</span>
<span class="line"></span>
<span class="line">2. 逐步迁移</span>
<span class="line">   ├── 先分离数据库（最容易）</span>
<span class="line">   ├── 再分离缓存层</span>
<span class="line">   ├── 最后分离业务逻辑</span>
<span class="line"></span>
<span class="line">3. 保持兼容</span>
<span class="line">   ├── 使用 API Gateway 统一入口</span>
<span class="line">   ├── 保持客户端接口不变</span>
<span class="line">   └── 灰度切换</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-3-kbengine-使用建议" tabindex="-1"><a class="header-anchor" href="#_7-3-kbengine-使用建议"><span>7.3 KBEngine 使用建议</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 中可以根据负载动态调整</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># kbengine_defs.xml</span></span>
<span class="line"><span class="token operator">&lt;</span>Baseapp<span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 单机最大承载 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>maxConnections<span class="token operator">&gt;</span><span class="token number">1000</span><span class="token operator">&lt;</span><span class="token operator">/</span>maxConnections<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 自动备份间隔 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>autoArchiveTime<span class="token operator">&gt;</span><span class="token number">300</span><span class="token operator">&lt;</span><span class="token operator">/</span>autoArchiveTime<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line">    <span class="token operator">&lt;</span>!<span class="token operator">-</span><span class="token operator">-</span> 负载均衡权重 <span class="token operator">-</span><span class="token operator">-</span><span class="token operator">&gt;</span></span>
<span class="line">    <span class="token operator">&lt;</span>loadBalanceWeight<span class="token operator">&gt;</span><span class="token number">1.0</span><span class="token operator">&lt;</span><span class="token operator">/</span>loadBalanceWeight<span class="token operator">&gt;</span></span>
<span class="line"><span class="token operator">&lt;</span><span class="token operator">/</span>Baseapp<span class="token operator">&gt;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 当负载过高时，增加 BaseApp 进程</span></span>
<span class="line"><span class="token comment"># BaseappMgr 会自动将新玩家分配到负载低的 BaseApp</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="核心观点" tabindex="-1"><a class="header-anchor" href="#核心观点"><span>核心观点</span></a></h3><table><thead><tr><th>场景</th><th>推荐架构</th><th>原因</th></tr></thead><tbody><tr><td><strong>独立游戏/原型验证</strong></td><td>单服</td><td>快速开发，成本低</td></tr><tr><td><strong>卡牌/回合制游戏</strong></td><td>单服</td><td>逻辑简单，无需复杂架构</td></tr><tr><td><strong>MMORPG</strong></td><td>分布式(KBEngine)</td><td>空间大，玩家多</td></tr><tr><td><strong>FPS/MOBA</strong></td><td>分布式</td><td>低延迟，高并发</td></tr><tr><td><strong>商业运营项目</strong></td><td>分布式</td><td>高可用，可扩展</td></tr></tbody></table><h3 id="决策检查清单" tabindex="-1"><a class="header-anchor" href="#决策检查清单"><span>决策检查清单</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">架构决策检查清单：</span>
<span class="line"></span>
<span class="line">□ 预期在线规模是多少？</span>
<span class="line">□ 游戏类型是什么？</span>
<span class="line">□ 开发周期有多长？</span>
<span class="line">□ 团队规模和经验如何？</span>
<span class="line">□ 运维能力如何？</span>
<span class="line">□ 预算是否充足？</span>
<span class="line">□ 是否需要跨服功能？</span>
<span class="line">□ 是否需要热更新？</span>
<span class="line">□ 对可用性要求多高？</span>
<span class="line">□ 未来扩展性如何考虑？</span>
<span class="line"></span>
<span class="line">如果 7+ 个答案指向同一方向，就可以做出决策。</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine GitHub - 架构说明</a></li><li><a href="https://www.kbelab.com/manual/engine-overview.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 引擎概览</a></li><li><a href="https://www.bigworldtech.com/" target="_blank" rel="noopener noreferrer">BigWorld Technology - Architecture</a></li><li><a href="https://www.gdcvault.com/play/1022800/Scalable-Game-Server-Architecture-in" target="_blank" rel="noopener noreferrer">Scalable Game Server Architecture</a></li></ul>`,41)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};