import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q8-avoid-spof.html","title":"Q8: 如何设计才能避免单点故障？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q8-avoid-spof.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q8-avoid-spof.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q8-如何设计才能避免单点故障" tabindex="-1"><a class="header-anchor" href="#q8-如何设计才能避免单点故障"><span>Q8: 如何设计才能避免单点故障？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察对高可用架构设计的理解：</p><ul><li>什么是单点故障（SPOF）</li><li>KBEngine 中哪些组件是单点</li><li>如何消除单点故障</li><li>高可用架构的设计模式</li></ul><hr><h2 id="一、单点故障概述" tabindex="-1"><a class="header-anchor" href="#一、单点故障概述"><span>一、单点故障概述</span></a></h2><h3 id="_1-1-什么是单点故障" tabindex="-1"><a class="header-anchor" href="#_1-1-什么是单点故障"><span>1.1 什么是单点故障</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                      单点故障示意图                          │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌──────────┐                                              │</span>
<span class="line">│   │ Clients  │                                              │</span>
<span class="line">│   └────┬─────┘                                              │</span>
<span class="line">│        │                                                    │</span>
<span class="line">│        ▼                                                    │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │              单一登录服务器                       │       │</span>
<span class="line">│   │            (Single Point of Failure)            │       │</span>
<span class="line">│   │                                                 │       │</span>
<span class="line">│   │            ❌ 崩溃 ❌                            │       │</span>
<span class="line">│   │                                                 │       │</span>
<span class="line">│   │              所有玩家无法登录！                   │       │</span>
<span class="line">│   └─────────────────────────────────────────────────┘       │</span>
<span class="line">│        │                                                    │</span>
<span class="line">│        ▼                                                    │</span>
<span class="line">│   ┌──────────┐                                              │</span>
<span class="line">│   │ Database │                                              │</span>
<span class="line">│   └──────────┘                                              │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">单点故障（Single Point of Failure）定义：</span>
<span class="line">系统中的某个组件，如果发生故障会导致整个系统不可用。</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_1-2-常见的单点故障" tabindex="-1"><a class="header-anchor" href="#_1-2-常见的单点故障"><span>1.2 常见的单点故障</span></a></h3><table><thead><tr><th>组件类型</th><th>单点故障表现</th><th>影响</th></tr></thead><tbody><tr><td><strong>单一服务器</strong></td><td>服务器宕机</td><td>完全不可用</td></tr><tr><td><strong>单一网络设备</strong></td><td>交换机故障</td><td>网络隔离</td></tr><tr><td><strong>单一数据库</strong></td><td>数据库宕机</td><td>数据无法读写</td></tr><tr><td><strong>单一进程</strong></td><td>进程崩溃</td><td>功能不可用</td></tr><tr><td><strong>单一电源</strong></td><td>电源故障</td><td>整机断电</td></tr></tbody></table><hr><h2 id="二、kbengine-中的单点分析" tabindex="-1"><a class="header-anchor" href="#二、kbengine-中的单点分析"><span>二、KBEngine 中的单点分析</span></a></h2><h3 id="_2-1-kbengine-架构中的单点" tabindex="-1"><a class="header-anchor" href="#_2-1-kbengine-架构中的单点"><span>2.1 KBEngine 架构中的单点</span></a></h3><p>根据 <a href="https://www.kbelab.com/manual/engine-overview.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 引擎概览</a>：</p>`,14),i(d,{code:`eJxLy8kvT85ILCpRCHHiUgCC4tKk9KLEggwFpadLZj1dN+/Jvu7nu1ue7N72fk/Hs+kLnvZOfd6089nU1pez5rzf06kE1gMCPo7RPvnpmXmJBQWxcEEnx2inxOJUFDFnx2jn1JwcuFhqXgoXus1gS+DWPl074+mcFThtdnL0hdnim16EbJEvzCYUcRffaBcnuAgW659N3fCsd93TjU0IO1ycojVcEksSk4D2aKJp9HFU0NXTtat5OafhxbLGGpB7wOJORIq7QISd0YSBzoeYD9SnoKtrB3QDmOsC50GcXVKZkwpWk5aZk2OlnJaWnJJihCQFNAeXFNAoFBkANayzQA==`}),o[1]||=e(`<h3 id="_2-2-各组件单点分析" tabindex="-1"><a class="header-anchor" href="#_2-2-各组件单点分析"><span>2.2 各组件单点分析</span></a></h3><table><thead><tr><th>组件</th><th>是否单点</th><th>故障影响</th><th>KBEngine 的应对</th></tr></thead><tbody><tr><td><strong>Loginapp</strong></td><td>❌ 可多实例</td><td>无影响</td><td>可部署多个，负载均衡</td></tr><tr><td><strong>Baseapp</strong></td><td>❌ 可多实例</td><td>自动备份</td><td>Baseapp 间互相备份</td></tr><tr><td><strong>Cellapp</strong></td><td>❌ 可多实例</td><td>空间不可用</td><td>其他 Cellapp 可接管</td></tr><tr><td><strong>BaseappMgr</strong></td><td>✅ 单点</td><td>无法分配新玩家</td><td>Machine 进程监控重启</td></tr><tr><td><strong>CellappMgr</strong></td><td>✅ 单点</td><td>无法创建新空间</td><td>Machine 进程监控重启</td></tr><tr><td><strong>DBMgr</strong></td><td>✅ 单点</td><td>无法访问数据库</td><td>Machine 进程监控重启</td></tr><tr><td><strong>Database</strong></td><td>✅ 单点</td><td>完全不可用</td><td>需要 MySQL 集群</td></tr></tbody></table><h3 id="_2-3-kbengine-的容错机制" tabindex="-1"><a class="header-anchor" href="#_2-3-kbengine-的容错机制"><span>2.3 KBEngine 的容错机制</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine 的备份机制</span></span>
<span class="line"><span class="token comment"># scripts/kbe_scripts/base/baseapp.py</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Baseapp</span><span class="token punctuation">(</span>KBEngine<span class="token punctuation">.</span>Base<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">onReady</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        Baseapp 准备就绪时注册备份</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 向其他 Baseapp 发送备份请求</span></span>
<span class="line">        self<span class="token punctuation">.</span>startBackupToOtherBaseapps<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">backupData</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        定期备份玩家数据到其他 Baseapp</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">for</span> entity <span class="token keyword">in</span> self<span class="token punctuation">.</span>entities<span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 将实体数据序列化发送给备份 Baseapp</span></span>
<span class="line">            self<span class="token punctuation">.</span>sendBackupData<span class="token punctuation">(</span>entity<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="三、消除单点故障的方法" tabindex="-1"><a class="header-anchor" href="#三、消除单点故障的方法"><span>三、消除单点故障的方法</span></a></h2><h3 id="_3-1-冗余设计" tabindex="-1"><a class="header-anchor" href="#_3-1-冗余设计"><span>3.1 冗余设计</span></a></h3><h4 id="服务冗余" tabindex="-1"><a class="header-anchor" href="#服务冗余"><span>服务冗余</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">无冗余：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   Clients ──► Loginapp ──► Baseapp                          │</span>
<span class="line">│                  │          │                               │</span>
<span class="line">│                  ▼          ▼                               │</span>
<span class="line">│                崩溃❌      崩溃❌                              │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">服务冗余：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   Clients ──►┌────────────┐                                │</span>
<span class="line">│              │  Loginapp 1│  ◄── 一个崩溃，其他继续服务      │</span>
<span class="line">│              ├────────────┤                                │</span>
<span class="line">│              │  Loginapp 2│                                │</span>
<span class="line">│              ├────────────┤                                │</span>
<span class="line">│              │  Loginapp 3│                                │</span>
<span class="line">│              └────────────┘                                │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h4 id="数据冗余" tabindex="-1"><a class="header-anchor" href="#数据冗余"><span>数据冗余</span></a></h4><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">主从复制：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌──────────┐          写入          ┌──────────┐         │</span>
<span class="line">│   │  Client  │ ─────────────────────►│  Master  │         │</span>
<span class="line">│   └──────────┘                        └────┬─────┘         │</span>
<span class="line">│        ▲                                    │               │</span>
<span class="line">│        │           同步复制                  │               │</span>
<span class="line">│        │                                    ▼               │</span>
<span class="line">│        │                            ┌──────────┐         │</span>
<span class="line">│        │          读取 ◄───────────────│  Slave   │         │</span>
<span class="line">│        └──────────────────────────────┴──────────┘         │</span>
<span class="line">│                      主库崩溃时提升从库                      │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-负载均衡" tabindex="-1"><a class="header-anchor" href="#_3-2-负载均衡"><span>3.2 负载均衡</span></a></h3>`,12),i(d,{code:`eJxLy8kvT85ILCpR8AniUgCC4tKk9KLEggwF55zM1LyS4milp+sWPevY/nz1eqVYsAoQcDaMhsgrGCIJGsEEjZAEjWGCfhDB1LwULjDDxyn6xZb5L/bufTq3/cXChU9nrrBJKtK380vPzKvQ9wkLjoUogzsoOLWoLLUI6KBnc3qfdoHUv5zd9nzfEiRnBRtGQ1QhOyvYCCaI5KxgY5igMZqznA0V1IBeARHGCrq6dkB3Qp0L4tU87Wh72dpbA7QKqCIYpCzYGOrQksqcVJCytMycHCvltLQ0y2QTLgAbIG15`}),o[2]||=e(`<h3 id="_3-3-故障检测与自动恢复" tabindex="-1"><a class="header-anchor" href="#_3-3-故障检测与自动恢复"><span>3.3 故障检测与自动恢复</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">监控与恢复流程：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │              监控系统                            │       │</span>
<span class="line">│   │  ├── 心跳检测 (每秒)                              │       │</span>
<span class="line">│   │  ├── 健康检查 (每 5 秒)                           │       │</span>
<span class="line">│   │  └── 资源监控 (每秒)                              │       │</span>
<span class="line">│   └────────────┬────────────────────────────────────┘       │</span>
<span class="line">│                │                                            │</span>
<span class="line">│                ▼                                            │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │            检测到故障 ❌                         │       │</span>
<span class="line">│   │  ┌─────────────────────────────────────────┐    │       │</span>
<span class="line">│   │  │  1. 确认故障 (连续 3 次心跳失败)          │    │       │</span>
<span class="line">│   │  │  2. 触发告警 (通知运维)                   │    │       │</span>
<span class="line">│   │  │  3. 自动重启 (尝试恢复服务)               │    │       │</span>
<span class="line">│   │  │  4. 流量切换 (切换到备用实例)              │    │       │</span>
<span class="line">│   │  └─────────────────────────────────────────┘    │       │</span>
<span class="line">│   └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、kbengine-单点消除方案" tabindex="-1"><a class="header-anchor" href="#四、kbengine-单点消除方案"><span>四、KBEngine 单点消除方案</span></a></h2><h3 id="_4-1-baseappmgr-消除方案" tabindex="-1"><a class="header-anchor" href="#_4-1-baseappmgr-消除方案"><span>4.1 BaseappMgr 消除方案</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">方案 1：进程监控快速重启</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │              Machine 进程                        │       │</span>
<span class="line">│   │  ┌─────────────────────────────────────────┐    │       │</span>
<span class="line">│   │  │  监控 BaseappMgr 状态                    │    │       │</span>
<span class="line">│   │  │  - 心跳检测 (每秒)                        │    │       │</span>
<span class="line">│   │  │  - 崩溃检测 (进程不存在)                 │    │       │</span>
<span class="line">│   │  └─────────────────────────────────────────┘    │       │</span>
<span class="line">│   │                        │                        │       │</span>
<span class="line">│   │                        ▼                        │       │</span>
<span class="line">│   │  ┌─────────────────────────────────────────┐    │       │</span>
<span class="line">│   │  │  检测到崩溃 → 立即重启                   │    │       │</span>
<span class="line">│   │  │  - 保存崩溃现场 (日志)                   │    │       │</span>
<span class="line">│   │  │  - 重新启动进程                          │    │       │</span>
<span class="line">│   │  │  - 恢复时间: ~5-10 秒                    │    │       │</span>
<span class="line">│   │  └─────────────────────────────────────────┘    │       │</span>
<span class="line">│   └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># KBEngine Machine 进程的监控逻辑（伪代码）</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Machine</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">monitor_processes</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        监控所有子进程状态</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token boolean">True</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">for</span> process <span class="token keyword">in</span> self<span class="token punctuation">.</span>processes<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">if</span> <span class="token keyword">not</span> process<span class="token punctuation">.</span>is_alive<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                    self<span class="token punctuation">.</span>log<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;</span><span class="token interpolation"><span class="token punctuation">{</span>process<span class="token punctuation">.</span>name<span class="token punctuation">}</span></span><span class="token string"> 崩溃，正在重启...&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">                    self<span class="token punctuation">.</span>restart_process<span class="token punctuation">(</span>process<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            sleep<span class="token punctuation">(</span><span class="token number">1</span><span class="token punctuation">)</span>  <span class="token comment"># 每秒检查一次</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-dbmgr-消除方案" tabindex="-1"><a class="header-anchor" href="#_4-2-dbmgr-消除方案"><span>4.2 DBMgr 消除方案</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">方案 1：DBMgr 集群（高可用）</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │               DBMgr 集群                         │       │</span>
<span class="line">│   │                                                  │       │</span>
<span class="line">│   │  ┌────────────┐  ┌────────────┐                │       │</span>
<span class="line">│   │  │  DBMgr 1   │◄─┼────────────┤                │       │</span>
<span class="line">│   │  │  (Active)  │  │  DBMgr 2   │                │       │</span>
<span class="line">│   │  └──────┬─────┘  │ (Standby)  │                │       │</span>
<span class="line">│   │         │         └──────┬─────┘                │       │</span>
<span class="line">│   │         │                │                      │       │</span>
<span class="line">│   │         │    心跳同步     │                      │       │</span>
<span class="line">│   │         │◄───────────────►│                      │       │</span>
<span class="line">│   │         │                                        │       │</span>
<span class="line">│   └─────────┼────────────────────────────────────────┘       │</span>
<span class="line">│             │                                               │</span>
<span class="line">│             ▼                                               │</span>
<span class="line">│   ┌─────────────────────────────────────────────────┐       │</span>
<span class="line">│   │            MySQL 主从集群                        │       │</span>
<span class="line">│   │  ┌────────────┐         ┌────────────┐          │       │</span>
<span class="line">│   │  │   Master   │◄────────┤   Slave    │          │       │</span>
<span class="line">│   │  │  (读写)    │  同步   │   (只读)   │          │       │</span>
<span class="line">│   │  └────────────┘         └────────────┘          │       │</span>
<span class="line">│   └─────────────────────────────────────────────────┘       │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-database-消除方案" tabindex="-1"><a class="header-anchor" href="#_4-3-database-消除方案"><span>4.3 Database 消除方案</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">方案：MySQL 主从 + 故障自动切换</span>
<span class="line"></span>
<span class="line">正常状态：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   Baseapp ──► DBMgr ──► MySQL Master (写入)                │</span>
<span class="line">│                  │                                         │</span>
<span class="line">│                  └─────────────► MySQL Slave (只读)         │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">Master 故障切换：</span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                                                             │</span>
<span class="line">│   1. 检测到 Master 宕机                                      │</span>
<span class="line">│   2. 提升 Slave 为新的 Master                               │</span>
<span class="line">│   3. DBMgr 连接到新的 Master                                │</span>
<span class="line">│   4. 旧 Master 修复后成为 Slave                             │</span>
<span class="line">│                                                             │</span>
<span class="line">│   Baseapp ──► DBMgr ──► MySQL Slave (新 Master)            │</span>
<span class="line">│                  └─────────────► MySQL Master (修复后)      │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、完整的高可用架构" tabindex="-1"><a class="header-anchor" href="#五、完整的高可用架构"><span>五、完整的高可用架构</span></a></h2><h3 id="_5-1-理想的高可用架构" tabindex="-1"><a class="header-anchor" href="#_5-1-理想的高可用架构"><span>5.1 理想的高可用架构</span></a></h3>`,14),i(d,{code:`eJyFU01Lw0AQvfsrlgiihypJQVREyMcxEWzFS/AQ220thFqa+HUTPehBwYuIB5GKHkT8wkKlpfhnTFv/hbO7zXY3JZhDYN+bmfdmdrbk7+wXtr16iNaNCQRfsLtVrnu1bWT6FVwNA1eJXu97Z63+81v0caxs0iDymarLQpAqgFoMagKYjcFVBuJqcUIWsw1XGTTvBt1udHs6aDRkKdtQ3dVypXowZ2/k0c9XR6Q0gYoeTqUslJmNvk8Grc/o8rz38jhLwtMc7EAVV+nfdKLuVUJeV11Ke7Wa2KytayNc6NfWsyM8m9ay4QUYhnvX7jeOZD0D9AibkDNALoYFNQPUYngoxmCHV3HKdXlqQPJahORzG7dpYt+HsTy1f6+biQUAm4RN2DTBZgyLSwA2Y1iwaRKbQ3zMJpC81j82LS/0XKV39d67eJVtWiBgGWO1LSjNYGlpLFi1aecwv2YjxwtCXJ8ROS3m8r63hwUq507ncLESwKvZFbK4T1NFU/A4yC+LMpkVupxTfB35iXE6PekkHHaJRcgYjYM9gROsBf2zuAQ2jHMY6DC1ZMwyCYLbJO4oCHfFXMsYLUYujB5TipEYGDmcYMI0gp8YR1uFUaZm54ZXGx76mE6mVPH9pcnCAp4vLEqUlkbRjhlXKhWKRSbGuThvjKPNpXOpeaTFVCqR9QeniIB6`}),o[3]||=e(`<h3 id="_5-2-容错能力对比" tabindex="-1"><a class="header-anchor" href="#_5-2-容错能力对比"><span>5.2 容错能力对比</span></a></h3><table><thead><tr><th>架构</th><th>单点数</th><th>容错能力</th><th>恢复时间</th></tr></thead><tbody><tr><td><strong>基础架构</strong></td><td>6 个</td><td>❌ 无</td><td>需人工介入</td></tr><tr><td><strong>+ 进程监控</strong></td><td>6 个</td><td>⚠️ 进程级</td><td>~5-10 秒</td></tr><tr><td><strong>+ Manager 备份</strong></td><td>3 个</td><td>✅ 进程级</td><td>~5-10 秒</td></tr><tr><td><strong>+ 数据库集群</strong></td><td>1 个</td><td>✅ 进程级</td><td>~30-60 秒</td></tr><tr><td><strong>+ 负载均衡</strong></td><td>0 个</td><td>✅ 完全容错</td><td>&lt; 5 秒</td></tr></tbody></table><hr><h2 id="六、实现方案" tabindex="-1"><a class="header-anchor" href="#六、实现方案"><span>六、实现方案</span></a></h2><h3 id="_6-1-heartbeat-心跳检测" tabindex="-1"><a class="header-anchor" href="#_6-1-heartbeat-心跳检测"><span>6.1 Heartbeat 心跳检测</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// KBEngine 中的心跳机制</span></span>
<span class="line"><span class="token comment">// src/server/network/channel.h</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">Channel</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 心跳超时时间</span></span>
<span class="line">    <span class="token keyword">static</span> <span class="token keyword">constexpr</span> <span class="token keyword">int</span> HEARTBEAT_TIMEOUT <span class="token operator">=</span> <span class="token number">10</span><span class="token punctuation">;</span>  <span class="token comment">// 秒</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 最后心跳时间</span></span>
<span class="line">    <span class="token keyword">uint64_t</span> lastHeartbeatTime_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 检查心跳</span></span>
<span class="line">    <span class="token keyword">bool</span> <span class="token function">isAlive</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token keyword">const</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> now <span class="token operator">=</span> <span class="token function">getTimeInSeconds</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">(</span>now <span class="token operator">-</span> lastHeartbeatTime_<span class="token punctuation">)</span> <span class="token operator">&lt;</span> HEARTBEAT_TIMEOUT<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 检测进程心跳</span></span>
<span class="line"><span class="token keyword">void</span> <span class="token function">checkProcessHeartbeat</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">    <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> process <span class="token operator">:</span> monitoredProcesses<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token punctuation">(</span><span class="token operator">!</span>process<span class="token punctuation">.</span>channel<span class="token operator">-&gt;</span><span class="token function">isAlive</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token comment">// 进程可能挂了</span></span>
<span class="line">            <span class="token function">onProcessDead</span><span class="token punctuation">(</span>process<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-2-服务发现" tabindex="-1"><a class="header-anchor" href="#_6-2-服务发现"><span>6.2 服务发现</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 服务发现机制</span></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">ServiceRegistry</span> <span class="token punctuation">{</span></span>
<span class="line"><span class="token keyword">private</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 服务注册表</span></span>
<span class="line">    <span class="token keyword">struct</span> <span class="token class-name">ServiceInfo</span> <span class="token punctuation">{</span></span>
<span class="line">        std<span class="token double-colon punctuation">::</span>string address<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">int</span> port<span class="token punctuation">;</span></span>
<span class="line">        <span class="token keyword">uint64_t</span> lastHeartbeat<span class="token punctuation">;</span></span>
<span class="line">        ServiceStatus status<span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>unordered_map<span class="token operator">&lt;</span>std<span class="token double-colon punctuation">::</span>string<span class="token punctuation">,</span> ServiceInfo<span class="token operator">&gt;</span> services_<span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">public</span><span class="token operator">:</span></span>
<span class="line">    <span class="token comment">// 注册服务</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">registerService</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> name<span class="token punctuation">,</span></span>
<span class="line">                         <span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> address<span class="token punctuation">,</span></span>
<span class="line">                         <span class="token keyword">int</span> port<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        services_<span class="token punctuation">[</span>name<span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">{</span>address<span class="token punctuation">,</span> port<span class="token punctuation">,</span> <span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span> ALIVE<span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 获取可用服务</span></span>
<span class="line">    std<span class="token double-colon punctuation">::</span>string <span class="token function">getAvailableService</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> name<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        <span class="token keyword">for</span> <span class="token punctuation">(</span><span class="token keyword">auto</span><span class="token operator">&amp;</span> <span class="token punctuation">[</span>key<span class="token punctuation">,</span> info<span class="token punctuation">]</span> <span class="token operator">:</span> services_<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token keyword">if</span> <span class="token punctuation">(</span>info<span class="token punctuation">.</span>status <span class="token operator">==</span> ALIVE <span class="token operator">&amp;&amp;</span></span>
<span class="line">                <span class="token punctuation">(</span><span class="token function">now</span><span class="token punctuation">(</span><span class="token punctuation">)</span> <span class="token operator">-</span> info<span class="token punctuation">.</span>lastHeartbeat<span class="token punctuation">)</span> <span class="token operator">&lt;</span> HEARTBEAT_TIMEOUT<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">                <span class="token keyword">return</span> info<span class="token punctuation">.</span>address<span class="token punctuation">;</span></span>
<span class="line">            <span class="token punctuation">}</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line">        <span class="token keyword">return</span> <span class="token string">&quot;&quot;</span><span class="token punctuation">;</span>  <span class="token comment">// 无可用服务</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment">// 下线服务</span></span>
<span class="line">    <span class="token keyword">void</span> <span class="token function">offlineService</span><span class="token punctuation">(</span><span class="token keyword">const</span> std<span class="token double-colon punctuation">::</span>string<span class="token operator">&amp;</span> name<span class="token punctuation">)</span> <span class="token punctuation">{</span></span>
<span class="line">        services_<span class="token punctuation">[</span>name<span class="token punctuation">]</span><span class="token punctuation">.</span>status <span class="token operator">=</span> DEAD<span class="token punctuation">;</span></span>
<span class="line">        <span class="token comment">// 触发告警</span></span>
<span class="line">        <span class="token function">alert</span><span class="token punctuation">(</span>name <span class="token operator">+</span> <span class="token string">&quot; is down!&quot;</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">    <span class="token punctuation">}</span></span>
<span class="line"><span class="token punctuation">}</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_6-3-自动故障切换" tabindex="-1"><a class="header-anchor" href="#_6-3-自动故障切换"><span>6.3 自动故障切换</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># Python 实现的故障切换</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">FailoverManager</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>primary <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line">        self<span class="token punctuation">.</span>secondary <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line">        self<span class="token punctuation">.</span>current <span class="token operator">=</span> <span class="token boolean">None</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_primary</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        检查主服务是否可用</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> self<span class="token punctuation">.</span>primary<span class="token punctuation">.</span>ping<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">except</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">get_connection</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        获取可用连接（自动切换）</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token keyword">if</span> self<span class="token punctuation">.</span>current <span class="token keyword">is</span> <span class="token boolean">None</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>current <span class="token operator">=</span> self<span class="token punctuation">.</span>primary</span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 检查当前连接</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token keyword">not</span> self<span class="token punctuation">.</span>check_primary<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>log<span class="token punctuation">(</span><span class="token string">&quot;主服务不可用，切换到备用&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            self<span class="token punctuation">.</span>current <span class="token operator">=</span> self<span class="token punctuation">.</span>secondary</span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 尝试恢复主服务</span></span>
<span class="line">            self<span class="token punctuation">.</span>recover_primary<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> self<span class="token punctuation">.</span>current<span class="token punctuation">.</span>connect<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">recover_primary</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        尝试恢复主服务</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 后台线程尝试连接</span></span>
<span class="line">        Thread<span class="token punctuation">(</span>target<span class="token operator">=</span>self<span class="token punctuation">.</span>_try_recover<span class="token punctuation">)</span><span class="token punctuation">.</span>start<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">_try_recover</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token keyword">while</span> <span class="token boolean">True</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">if</span> self<span class="token punctuation">.</span>primary<span class="token punctuation">.</span>ping<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                self<span class="token punctuation">.</span>log<span class="token punctuation">(</span><span class="token string">&quot;主服务已恢复，切换回来&quot;</span><span class="token punctuation">)</span></span>
<span class="line">                self<span class="token punctuation">.</span>current <span class="token operator">=</span> self<span class="token punctuation">.</span>primary</span>
<span class="line">                <span class="token keyword">break</span></span>
<span class="line">            sleep<span class="token punctuation">(</span><span class="token number">5</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="七、实战建议" tabindex="-1"><a class="header-anchor" href="#七、实战建议"><span>七、实战建议</span></a></h2><h3 id="_7-1-渐进式高可用方案" tabindex="-1"><a class="header-anchor" href="#_7-1-渐进式高可用方案"><span>7.1 渐进式高可用方案</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">阶段 1：基础监控</span>
<span class="line">├── 进程监控（自动重启）</span>
<span class="line">├── 日志收集</span>
<span class="line">└── 告警通知</span>
<span class="line"></span>
<span class="line">阶段 2：服务冗余</span>
<span class="line">├── 关键服务多实例</span>
<span class="line">├── 负载均衡</span>
<span class="line">└── 故障自动切换</span>
<span class="line"></span>
<span class="line">阶段 3：数据冗余</span>
<span class="line">├── 数据库主从</span>
<span class="line">├── 数据自动备份</span>
<span class="line">└── 灾难恢复计划</span>
<span class="line"></span>
<span class="line">阶段 4：完全高可用</span>
<span class="line">├── 多机房部署</span>
<span class="line">├── 数据多活</span>
<span class="line">└── 自动故障转移</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-2-成本与收益" tabindex="-1"><a class="header-anchor" href="#_7-2-成本与收益"><span>7.2 成本与收益</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">高可用方案的成本收益分析：</span>
<span class="line"></span>
<span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│  方案              │ 成本  │ 可用性 │ 适用场景             │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│  单服务器           │ 1x    │ 99%   │ 开发测试环境         │</span>
<span class="line">│  + 进程监控         │ 1x    │ 99.5% │ 小型游戏             │</span>
<span class="line">│  + 服务冗余         │ 2x    │ 99.9% │ 中型游戏             │</span>
<span class="line">│  + 数据库主从       │ 2-3x  │ 99.95%│ 大型游戏             │</span>
<span class="line">│  + 多机房           │ 4x+   │ 99.99%│ 商业运营游戏         │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span>
<span class="line">可用性计算：</span>
<span class="line">- 99%   = 年宕机时间 3.65 天</span>
<span class="line">- 99.5% = 年宕机时间 1.83 天</span>
<span class="line">- 99.9% = 年宕机时间 8.76 小时</span>
<span class="line">- 99.95% = 年宕机时间 4.38 小时</span>
<span class="line">- 99.99% = 年宕机时间 52.56 分钟</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_7-3-kbengine-部署建议" tabindex="-1"><a class="header-anchor" href="#_7-3-kbengine-部署建议"><span>7.3 KBEngine 部署建议</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># KBEngine 高可用部署配置</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 1. Loginapp（多实例）</span></span>
<span class="line">machine:</span>
<span class="line">  - loginapp <span class="token punctuation">(</span>port <span class="token number">20013</span><span class="token punctuation">)</span></span>
<span class="line">  - loginapp <span class="token punctuation">(</span>port <span class="token number">20014</span><span class="token punctuation">)</span></span>
<span class="line">  - loginapp <span class="token punctuation">(</span>port <span class="token number">20015</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. Baseapp（多实例 + 互相备份）</span></span>
<span class="line">machine:</span>
<span class="line">  - baseapp <span class="token punctuation">(</span>port <span class="token number">20015</span><span class="token punctuation">)</span></span>
<span class="line">  - baseapp <span class="token punctuation">(</span>port <span class="token number">20016</span><span class="token punctuation">)</span></span>
<span class="line">  - baseapp <span class="token punctuation">(</span>port <span class="token number">20017</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 3. Cellapp（多实例）</span></span>
<span class="line">machine:</span>
<span class="line">  - cellapp <span class="token punctuation">(</span>port <span class="token number">20019</span><span class="token punctuation">)</span></span>
<span class="line">  - cellapp <span class="token punctuation">(</span>port <span class="token number">20020</span><span class="token punctuation">)</span></span>
<span class="line">  - cellapp <span class="token punctuation">(</span>port <span class="token number">20021</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 4. 数据库（MySQL 主从）</span></span>
<span class="line">MySQL:</span>
<span class="line">  - Master: <span class="token number">192.168</span>.1.10</span>
<span class="line">  - Slave:  <span class="token number">192.168</span>.1.11</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 5. 监控（Machine 进程）</span></span>
<span class="line">machine:</span>
<span class="line">  - 所有进程由 Machine 监控</span>
<span class="line">  - 崩溃自动重启</span>
<span class="line">  - 日志记录到 Logger</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="八、总结" tabindex="-1"><a class="header-anchor" href="#八、总结"><span>八、总结</span></a></h2><h3 id="核心原则" tabindex="-1"><a class="header-anchor" href="#核心原则"><span>核心原则</span></a></h3><table><thead><tr><th>原则</th><th>说明</th><th>实现方式</th></tr></thead><tbody><tr><td><strong>消除单点</strong></td><td>任何组件都不能是唯一的</td><td>冗余部署</td></tr><tr><td><strong>故障隔离</strong></td><td>单个故障不影响整体</td><td>进程隔离</td></tr><tr><td><strong>快速检测</strong></td><td>及时发现故障</td><td>心跳监控</td></tr><tr><td><strong>自动恢复</strong></td><td>无需人工介入</td><td>自动重启/切换</td></tr><tr><td><strong>优雅降级</strong></td><td>部分功能降级保证核心</td><td>熔断机制</td></tr></tbody></table><h3 id="检查清单" tabindex="-1"><a class="header-anchor" href="#检查清单"><span>检查清单</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">高可用设计检查清单：</span>
<span class="line"></span>
<span class="line">□ 所有关键组件是否有冗余？</span>
<span class="line">□ 是否有心跳检测机制？</span>
<span class="line">□ 进程崩溃是否能自动恢复？</span>
<span class="line">□ 数据库是否有主从复制？</span>
<span class="line">□ 是否有负载均衡？</span>
<span class="line">□ 是否有监控告警？</span>
<span class="line">□ 是否有灾难恢复计划？</span>
<span class="line">□ 是否定期演练故障切换？</span>
<span class="line"></span>
<span class="line">建议：</span>
<span class="line">- 生产环境至少达到 99.9% 可用性</span>
<span class="line">- 关键组件必须有冗余</span>
<span class="line">- 定期进行故障演练</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.kbelab.com/manual/disaster.html" target="_blank" rel="noopener noreferrer">KBEngine Lab - 灾难恢复</a></li><li><a href="https://github.com/kbengine/kbengine" target="_blank" rel="noopener noreferrer">KBEngine GitHub - 源码分析</a></li><li><a href="https://dev.mysql.com/doc/refman/8.0/en/replication.html" target="_blank" rel="noopener noreferrer">MySQL 主从复制配置</a></li><li><a href="https://sre.google/sre-book/eliminating-toil/" target="_blank" rel="noopener noreferrer">高可用架构设计模式</a></li></ul>`,27)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};