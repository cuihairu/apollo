import{a as e,c as t,i as n,l as r,s as i,t as a}from"./app-B8uz0aqq.js";var o=JSON.parse(`{"path":"/qa/q107-production-incident.html","title":"Q107: 线上出过什么严重事故？如何处理的？","lang":"en-US","frontmatter":{},"filePathRelative":"qa/q107-production-incident.md","git":{"createdTime":1773965296000,"updatedTime":1773965296000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"},{"name":"Claude Opus 4.6 (1M context)","username":"","email":"noreply@anthropic.com","commits":1}]}}`),s={name:`q107-production-incident.md`};function c(a,o,s,c,l,u){let d=r(`Mermaid`);return t(),n(`div`,null,[o[0]||=e(`<h1 id="q107-线上出过什么严重事故-如何处理的" tabindex="-1"><a class="header-anchor" href="#q107-线上出过什么严重事故-如何处理的"><span>Q107: 线上出过什么严重事故？如何处理的？</span></a></h1><h2 id="问题分析" tabindex="-1"><a class="header-anchor" href="#问题分析"><span>问题分析</span></a></h2><p>本题考察生产事故处理能力：</p><ul><li>事故案例</li><li>应急响应</li><li>根因分析</li><li>改进措施</li></ul><hr><h2 id="一、事故案例库" tabindex="-1"><a class="header-anchor" href="#一、事故案例库"><span>一、事故案例库</span></a></h2><h3 id="_1-1-常见事故类型" tabindex="-1"><a class="header-anchor" href="#_1-1-常见事故类型"><span>1.1 常见事故类型</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────┐</span>
<span class="line">│                    生产事故分类                                │</span>
<span class="line">├─────────────────────────────────────────────────────────────┤</span>
<span class="line">│                                                             │</span>
<span class="line">│  数据事故:                                                   │</span>
<span class="line">│  ├── 数据丢失                                               │</span>
<span class="line">│  ├── 数据错乱                                               │</span>
<span class="line">│  ├── 回滚失败                                               │</span>
<span class="line">│  └── 影响: 玩家损失巨大                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  服务事故:                                                   │</span>
<span class="line">│  ├── 服务宕机                                               │</span>
<span class="line">│  ├── 响应超时                                               │</span>
<span class="line">│  ├── 连接断开                                               │</span>
<span class="line">│  └── 影响: 无法登录/游戏                                     │</span>
<span class="line">│                                                             │</span>
<span class="line">│  安全事故:                                                   │</span>
<span class="line">│  ├── 刷物品                                                 │</span>
<span class="line">│  ├── 外挂泛滥                                               │</span>
<span class="line">│  ├── 数据泄露                                               │</span>
<span class="line">│  └── 影响: 经济系统崩溃                                      │</span>
<span class="line">│                                                             │</span>
<span class="line">│  性能事故:                                                   │</span>
<span class="line">│  ├── 负载过高                                               │</span>
<span class="line">│  ├── 内存溢出                                               │</span>
<span class="line">│  ├── 数据库锁                                               │</span>
<span class="line">│  └── 影响: 服务不可用                                        │</span>
<span class="line">│                                                             │</span>
<span class="line">└─────────────────────────────────────────────────────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="二、事故处理流程" tabindex="-1"><a class="header-anchor" href="#二、事故处理流程"><span>二、事故处理流程</span></a></h2><h3 id="_2-1-应急响应" tabindex="-1"><a class="header-anchor" href="#_2-1-应急响应"><span>2.1 应急响应</span></a></h3>`,11),i(d,{code:`eJxLy8kvT85ILCpRCHHhUgACx+in/ROf9214sqv72dTWWAVdXTsFp+inHXOfrV36Yn3Lkz0bYsHqnMAyztVPdix92d77fNfypx2ra8EyziCZmgCDGgWX6Odblj9rWPp0cu/TXVMg+qCyhjUKrtFP969+2TAfi6xRjYJb9LO1i5/u2AGTBUu7gC11B0otetY7/8meGU9bOyAaXSEyYLYbmO0BdHTbs3kTYKrAUu5gKc/oZ42Lni7pfTan92nXQogBHhAZiDJPMMcr+tmCnU9nL4CYA1HmBZbxjn42ZeeL/bOf9a16Nm1vLBcASKyF6w==`}),o[1]||=e(`<h3 id="_2-2-事故分级" tabindex="-1"><a class="header-anchor" href="#_2-2-事故分级"><span>2.2 事故分级</span></a></h3><table><thead><tr><th>级别</th><th>描述</th><th>响应时间</th><th>示例</th></tr></thead><tbody><tr><td>P0</td><td>核心功能完全不可用</td><td>5分钟</td><td>所有玩家无法登录</td></tr><tr><td>P1</td><td>严重影响部分玩家</td><td>15分钟</td><td>某个服掉线</td></tr><tr><td>P2</td><td>轻微影响</td><td>1小时</td><td>个别功能异常</td></tr><tr><td>P3</td><td>观察性问题</td><td>4小时</td><td>偶发卡顿</td></tr></tbody></table><hr><h2 id="三、案例研究-数据库误删" tabindex="-1"><a class="header-anchor" href="#三、案例研究-数据库误删"><span>三、案例研究：数据库误删</span></a></h2><h3 id="_3-1-事故描述" tabindex="-1"><a class="header-anchor" href="#_3-1-事故描述"><span>3.1 事故描述</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 生产事故案例</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">事故描述:</span>
<span class="line">时间: 周六晚 20:00 (高峰期)</span>
<span class="line">影响: 全服玩家数据丢失</span>
<span class="line">持续时间: 2 小时</span>
<span class="line"></span>
<span class="line">事故经过:</span>
<span class="line">1. 执行数据库清理脚本</span>
<span class="line">2. 误将 WHERE 条件写错</span>
<span class="line">3. 清空了玩家数据表</span>
<span class="line">4. 发现后立即停止服务</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 问题脚本 (错误示例)</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">cleanup_old_players</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">    错误: 注释掉的条件导致全表删除</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    <span class="token comment"># WHERE last_login &lt; DATE_SUB(NOW(), INTERVAL 180 DAY)</span></span>
<span class="line">    sql <span class="token operator">=</span> <span class="token string">&quot;DELETE FROM players&quot;</span>  <span class="token comment"># 忘记 WHERE 条件!</span></span>
<span class="line">    db<span class="token punctuation">.</span>execute<span class="token punctuation">(</span>sql<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 正确脚本</span></span>
<span class="line"><span class="token keyword">def</span> <span class="token function">cleanup_old_players_safe</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;安全的清理脚本&quot;&quot;&quot;</span></span>
<span class="line">    <span class="token comment"># 1. 先查询确认</span></span>
<span class="line">    check_sql <span class="token operator">=</span> <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">        SELECT COUNT(*)</span>
<span class="line">        FROM players</span>
<span class="line">        WHERE last_login &lt; DATE_SUB(NOW(), INTERVAL 180 DAY)</span>
<span class="line">    &quot;&quot;&quot;</span></span>
<span class="line">    count <span class="token operator">=</span> db<span class="token punctuation">.</span>query_one<span class="token punctuation">(</span>check_sql<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Will delete </span><span class="token interpolation"><span class="token punctuation">{</span>count<span class="token punctuation">}</span></span><span class="token string"> old players&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">if</span> count <span class="token operator">&gt;</span> <span class="token number">10000</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token comment"># 异常数量检查</span></span>
<span class="line">        <span class="token keyword">raise</span> Exception<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Too many rows to delete: </span><span class="token interpolation"><span class="token punctuation">{</span>count<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token comment"># 2. 分批删除</span></span>
<span class="line">    batch_size <span class="token operator">=</span> <span class="token number">1000</span></span>
<span class="line">    offset <span class="token operator">=</span> <span class="token number">0</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">while</span> <span class="token boolean">True</span><span class="token punctuation">:</span></span>
<span class="line">        delete_sql <span class="token operator">=</span> <span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">            DELETE FROM players</span>
<span class="line">            WHERE last_login &lt; DATE_SUB(NOW(), INTERVAL 180 DAY)</span>
<span class="line">            LIMIT %s</span>
<span class="line">        &quot;&quot;&quot;</span></span>
<span class="line">        affected <span class="token operator">=</span> db<span class="token punctuation">.</span>execute<span class="token punctuation">(</span>delete_sql<span class="token punctuation">,</span> <span class="token punctuation">(</span>batch_size<span class="token punctuation">,</span><span class="token punctuation">)</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">if</span> affected <span class="token operator">==</span> <span class="token number">0</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token keyword">break</span></span>
<span class="line"></span>
<span class="line">        offset <span class="token operator">+=</span> affected</span>
<span class="line">        <span class="token keyword">print</span><span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Deleted </span><span class="token interpolation"><span class="token punctuation">{</span>offset<span class="token punctuation">}</span></span><span class="token string"> rows&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-2-应急处理" tabindex="-1"><a class="header-anchor" href="#_3-2-应急处理"><span>3.2 应急处理</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 应急恢复流程</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">IncidentHandler</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;事故处理器&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">handle_data_loss</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;处理数据丢失&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 1. 立即停止服务</span></span>
<span class="line">        self<span class="token punctuation">.</span>stop_all_services<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 2. 评估损失</span></span>
<span class="line">        loss_assessment <span class="token operator">=</span> self<span class="token punctuation">.</span>assess_data_loss<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Data loss assessment: </span><span class="token interpolation"><span class="token punctuation">{</span>loss_assessment<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 3. 恢复数据</span></span>
<span class="line">        recovery_result <span class="token operator">=</span> self<span class="token punctuation">.</span>recover_data<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 4. 验证数据</span></span>
<span class="line">        validation_result <span class="token operator">=</span> self<span class="token punctuation">.</span>validate_recovery<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 5. 恢复服务</span></span>
<span class="line">        <span class="token keyword">if</span> validation_result<span class="token punctuation">[</span><span class="token string">&#39;success&#39;</span><span class="token punctuation">]</span><span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>restore_services<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token keyword">else</span><span class="token punctuation">:</span></span>
<span class="line">            CRITICAL_MSG<span class="token punctuation">(</span><span class="token string">&quot;Recovery validation failed!&quot;</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token comment"># 启动备份方案</span></span>
<span class="line">            self<span class="token punctuation">.</span>fallback_recovery<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">assess_data_loss</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;评估数据损失&quot;&quot;&quot;</span></span>
<span class="line">        assessment <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;affected_tables&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;affected_players&#39;</span><span class="token punctuation">:</span> <span class="token number">0</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;data_range&#39;</span><span class="token punctuation">:</span> <span class="token boolean">None</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;recovery_options&#39;</span><span class="token punctuation">:</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 检查每个表</span></span>
<span class="line">        <span class="token keyword">for</span> table <span class="token keyword">in</span> <span class="token punctuation">[</span><span class="token string">&#39;players&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;inventory&#39;</span><span class="token punctuation">,</span> <span class="token string">&#39;guilds&#39;</span><span class="token punctuation">]</span><span class="token punctuation">:</span></span>
<span class="line">            count <span class="token operator">=</span> self<span class="token punctuation">.</span>query<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;SELECT COUNT(*) FROM </span><span class="token interpolation"><span class="token punctuation">{</span>table<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            assessment<span class="token punctuation">[</span><span class="token string">&#39;affected_tables&#39;</span><span class="token punctuation">]</span><span class="token punctuation">.</span>append<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&#39;table&#39;</span><span class="token punctuation">:</span> table<span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;count&#39;</span><span class="token punctuation">:</span> count</span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> assessment</span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">recover_data</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;恢复数据&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 选项 1: 从最新备份恢复</span></span>
<span class="line">        backup_time <span class="token operator">=</span> self<span class="token punctuation">.</span>get_latest_backup_time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Latest backup from: </span><span class="token interpolation"><span class="token punctuation">{</span>backup_time<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 选项 2: 从 binlog 恢复</span></span>
<span class="line">        binlog_position <span class="token operator">=</span> self<span class="token punctuation">.</span>get_binlog_position<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Binlog position: </span><span class="token interpolation"><span class="token punctuation">{</span>binlog_position<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 执行恢复</span></span>
<span class="line">        <span class="token keyword">try</span><span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 停止应用</span></span>
<span class="line">            self<span class="token punctuation">.</span>stop_applications<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 恢复备份</span></span>
<span class="line">            self<span class="token punctuation">.</span>restore_from_backup<span class="token punctuation">(</span>backup_time<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 应用 binlog</span></span>
<span class="line">            self<span class="token punctuation">.</span>apply_binlog<span class="token punctuation">(</span>binlog_position<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token string">&#39;success&#39;</span><span class="token punctuation">:</span> <span class="token boolean">True</span><span class="token punctuation">,</span> <span class="token string">&#39;method&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;backup+binlog&#39;</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">except</span> Exception <span class="token keyword">as</span> e<span class="token punctuation">:</span></span>
<span class="line">            ERROR_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Recovery failed: </span><span class="token interpolation"><span class="token punctuation">{</span>e<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token punctuation">{</span><span class="token string">&#39;success&#39;</span><span class="token punctuation">:</span> <span class="token boolean">False</span><span class="token punctuation">,</span> <span class="token string">&#39;error&#39;</span><span class="token punctuation">:</span> <span class="token builtin">str</span><span class="token punctuation">(</span>e<span class="token punctuation">)</span><span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">validate_recovery</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;验证恢复结果&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 检查数据完整性</span></span>
<span class="line">        checks <span class="token operator">=</span> <span class="token punctuation">[</span></span>
<span class="line">            self<span class="token punctuation">.</span>check_player_count<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            self<span class="token punctuation">.</span>check_data_consistency<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">,</span></span>
<span class="line">            self<span class="token punctuation">.</span>check_critical_accounts<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        <span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        all_passed <span class="token operator">=</span> <span class="token builtin">all</span><span class="token punctuation">(</span>check<span class="token punctuation">[</span><span class="token string">&#39;passed&#39;</span><span class="token punctuation">]</span> <span class="token keyword">for</span> check <span class="token keyword">in</span> checks<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;success&#39;</span><span class="token punctuation">:</span> all_passed<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;checks&#39;</span><span class="token punctuation">:</span> checks</span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="四、案例研究-刷物品漏洞" tabindex="-1"><a class="header-anchor" href="#四、案例研究-刷物品漏洞"><span>四、案例研究：刷物品漏洞</span></a></h2><h3 id="_4-1-事故描述" tabindex="-1"><a class="header-anchor" href="#_4-1-事故描述"><span>4.1 事故描述</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">事故描述:</span>
<span class="line">时间: 活动上线后 10 分钟</span>
<span class="line">影响: 游戏经济崩溃</span>
<span class="line">损失: 数百亿游戏币</span>
<span class="line"></span>
<span class="line">事故经过:</span>
<span class="line">1. 新增交易功能</span>
<span class="line">2. 未校验余额</span>
<span class="line">3. 玩家发现可无限刷钱</span>
<span class="line">4. 短时间内大量传播</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-2-漏洞修复" tabindex="-1"><a class="header-anchor" href="#_4-2-漏洞修复"><span>4.2 漏洞修复</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 刷物品漏洞修复</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">TradingSystem</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;交易系统 (修复版)&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">trade_item</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">,</span> price<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;交易物品&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 1. 开启事务</span></span>
<span class="line">        <span class="token keyword">with</span> self<span class="token punctuation">.</span>db<span class="token punctuation">.</span>transaction<span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 2. 加锁查询 (悲观锁)</span></span>
<span class="line">            player <span class="token operator">=</span> self<span class="token punctuation">.</span>db<span class="token punctuation">.</span>query_for_update<span class="token punctuation">(</span></span>
<span class="line">                <span class="token string">&quot;SELECT * FROM players WHERE id = ? FOR UPDATE&quot;</span><span class="token punctuation">,</span></span>
<span class="line">                <span class="token punctuation">(</span>player_id<span class="token punctuation">,</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> <span class="token keyword">not</span> player<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">raise</span> Exception<span class="token punctuation">(</span><span class="token string">&quot;Player not found&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 3. 校验物品数量</span></span>
<span class="line">            inventory <span class="token operator">=</span> self<span class="token punctuation">.</span>get_inventory<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token keyword">if</span> inventory<span class="token punctuation">[</span><span class="token string">&#39;count&#39;</span><span class="token punctuation">]</span> <span class="token operator">&lt;</span> count<span class="token punctuation">:</span></span>
<span class="line">                <span class="token keyword">raise</span> Exception<span class="token punctuation">(</span><span class="token string">&quot;Insufficient items&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 4. 校验余额 (如果购买)</span></span>
<span class="line">            <span class="token keyword">if</span> price <span class="token operator">&lt;</span> <span class="token number">0</span><span class="token punctuation">:</span>  <span class="token comment"># 购买</span></span>
<span class="line">                <span class="token keyword">if</span> player<span class="token punctuation">[</span><span class="token string">&#39;gold&#39;</span><span class="token punctuation">]</span> <span class="token operator">&lt;</span> <span class="token builtin">abs</span><span class="token punctuation">(</span>price<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">                    <span class="token keyword">raise</span> Exception<span class="token punctuation">(</span><span class="token string">&quot;Insufficient gold&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 5. 扣除物品</span></span>
<span class="line">            self<span class="token punctuation">.</span>remove_item<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> item_id<span class="token punctuation">,</span> count<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 6. 更新余额</span></span>
<span class="line">            self<span class="token punctuation">.</span>update_gold<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> player<span class="token punctuation">[</span><span class="token string">&#39;gold&#39;</span><span class="token punctuation">]</span> <span class="token operator">+</span> price<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 7. 记录日志</span></span>
<span class="line">            self<span class="token punctuation">.</span>log_transaction<span class="token punctuation">(</span><span class="token punctuation">{</span></span>
<span class="line">                <span class="token string">&#39;player_id&#39;</span><span class="token punctuation">:</span> player_id<span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;item_id&#39;</span><span class="token punctuation">:</span> item_id<span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;count&#39;</span><span class="token punctuation">:</span> count<span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;price&#39;</span><span class="token punctuation">:</span> price<span class="token punctuation">,</span></span>
<span class="line">                <span class="token string">&#39;timestamp&#39;</span><span class="token punctuation">:</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 8. 通知客户端</span></span>
<span class="line">        self<span class="token punctuation">.</span>notify_client<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;action&#39;</span><span class="token punctuation">:</span> <span class="token string">&#39;trade_complete&#39;</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;item_id&#39;</span><span class="token punctuation">:</span> item_id<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;count&#39;</span><span class="token punctuation">:</span> count<span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;new_gold&#39;</span><span class="token punctuation">:</span> player<span class="token punctuation">[</span><span class="token string">&#39;gold&#39;</span><span class="token punctuation">]</span> <span class="token operator">+</span> price</span>
<span class="line">        <span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">AntiAbuse</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;防刷系统&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">__init__</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        self<span class="token punctuation">.</span>player_limits <span class="token operator">=</span> <span class="token punctuation">{</span><span class="token punctuation">}</span></span>
<span class="line">        self<span class="token punctuation">.</span>global_limits <span class="token operator">=</span> <span class="token punctuation">{</span></span>
<span class="line">            <span class="token string">&#39;transactions_per_minute&#39;</span><span class="token punctuation">:</span> <span class="token number">1000</span><span class="token punctuation">,</span></span>
<span class="line">            <span class="token string">&#39;gold_change_per_minute&#39;</span><span class="token punctuation">:</span> <span class="token number">1000000</span></span>
<span class="line">        <span class="token punctuation">}</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">check_transaction_limit</span><span class="token punctuation">(</span>self<span class="token punctuation">,</span> player_id<span class="token punctuation">,</span> action<span class="token punctuation">,</span> amount<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;检查交易限制&quot;&quot;&quot;</span></span>
<span class="line">        current_time <span class="token operator">=</span> time<span class="token punctuation">.</span>time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        key <span class="token operator">=</span> <span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> action<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 获取玩家历史</span></span>
<span class="line">        <span class="token keyword">if</span> key <span class="token keyword">not</span> <span class="token keyword">in</span> self<span class="token punctuation">.</span>player_limits<span class="token punctuation">:</span></span>
<span class="line">            self<span class="token punctuation">.</span>player_limits<span class="token punctuation">[</span>key<span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">[</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        history <span class="token operator">=</span> self<span class="token punctuation">.</span>player_limits<span class="token punctuation">[</span>key<span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 清理 1 分钟前的记录</span></span>
<span class="line">        history<span class="token punctuation">[</span><span class="token punctuation">:</span><span class="token punctuation">]</span> <span class="token operator">=</span> <span class="token punctuation">[</span>t <span class="token keyword">for</span> t <span class="token keyword">in</span> history <span class="token keyword">if</span> current_time <span class="token operator">-</span> t <span class="token operator">&lt;</span> <span class="token number">60</span><span class="token punctuation">]</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 检查频率</span></span>
<span class="line">        <span class="token keyword">if</span> <span class="token builtin">len</span><span class="token punctuation">(</span>history<span class="token punctuation">)</span> <span class="token operator">&gt;</span> <span class="token number">100</span><span class="token punctuation">:</span>  <span class="token comment"># 每分钟最多 100 次</span></span>
<span class="line">            WARNING_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Player </span><span class="token interpolation"><span class="token punctuation">{</span>player_id<span class="token punctuation">}</span></span><span class="token string"> exceeded transaction limit&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 检查金额</span></span>
<span class="line">        <span class="token keyword">if</span> action <span class="token operator">==</span> <span class="token string">&#39;gold_change&#39;</span><span class="token punctuation">:</span></span>
<span class="line">            total <span class="token operator">=</span> <span class="token builtin">sum</span><span class="token punctuation">(</span>h<span class="token punctuation">[</span><span class="token string">&#39;amount&#39;</span><span class="token punctuation">]</span> <span class="token keyword">for</span> h <span class="token keyword">in</span> history<span class="token punctuation">)</span></span>
<span class="line">            <span class="token keyword">if</span> total <span class="token operator">+</span> amount <span class="token operator">&gt;</span> <span class="token number">100000</span><span class="token punctuation">:</span>  <span class="token comment"># 每分钟最多 10 万</span></span>
<span class="line">                WARNING_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Player </span><span class="token interpolation"><span class="token punctuation">{</span>player_id<span class="token punctuation">}</span></span><span class="token string"> exceeded gold limit&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line">                <span class="token keyword">return</span> <span class="token boolean">False</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 记录</span></span>
<span class="line">        history<span class="token punctuation">.</span>append<span class="token punctuation">(</span><span class="token punctuation">{</span><span class="token string">&#39;time&#39;</span><span class="token punctuation">:</span> current_time<span class="token punctuation">,</span> <span class="token string">&#39;amount&#39;</span><span class="token punctuation">:</span> amount<span class="token punctuation">}</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">return</span> <span class="token boolean">True</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-3-数据回滚" tabindex="-1"><a class="header-anchor" href="#_4-3-数据回滚"><span>4.3 数据回滚</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 数据回滚方案</span></span>
<span class="line"></span>
<span class="line"><span class="token keyword">class</span> <span class="token class-name">DataRollback</span><span class="token punctuation">:</span></span>
<span class="line">    <span class="token triple-quoted-string string">&quot;&quot;&quot;数据回滚&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">rollback_economy</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;回滚经济数据&quot;&quot;&quot;</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 1. 确定回滚点</span></span>
<span class="line">        exploit_detected_time <span class="token operator">=</span> self<span class="token punctuation">.</span>get_exploit_detected_time<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line">        rollback_time <span class="token operator">=</span> exploit_detected_time <span class="token operator">-</span> <span class="token number">300</span>  <span class="token comment"># 前5分钟</span></span>
<span class="line"></span>
<span class="line">        INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Rolling back to </span><span class="token interpolation"><span class="token punctuation">{</span>rollback_time<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 2. 备份当前数据</span></span>
<span class="line">        self<span class="token punctuation">.</span>backup_current_state<span class="token punctuation">(</span><span class="token string">&quot;before_rollback&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 3. 回滚玩家金币</span></span>
<span class="line">        self<span class="token punctuation">.</span>rollback_player_gold<span class="token punctuation">(</span>rollback_time<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 4. 回滚物品交易</span></span>
<span class="line">        self<span class="token punctuation">.</span>rollback_item_transactions<span class="token punctuation">(</span>rollback_time<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 5. 追缴非法所得</span></span>
<span class="line">        self<span class="token punctuation">.</span>confiscate_illegal_assets<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token comment"># 6. 补偿正常玩家</span></span>
<span class="line">        self<span class="token punctuation">.</span>compensate_players<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">    <span class="token keyword">def</span> <span class="token function">confiscate_illegal_assets</span><span class="token punctuation">(</span>self<span class="token punctuation">)</span><span class="token punctuation">:</span></span>
<span class="line">        <span class="token triple-quoted-string string">&quot;&quot;&quot;追缴非法所得&quot;&quot;&quot;</span></span>
<span class="line">        <span class="token comment"># 查找异常获取物品的玩家</span></span>
<span class="line">        suspicious_players <span class="token operator">=</span> self<span class="token punctuation">.</span>find_suspicious_players<span class="token punctuation">(</span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">        <span class="token keyword">for</span> player_id <span class="token keyword">in</span> suspicious_players<span class="token punctuation">:</span></span>
<span class="line">            <span class="token comment"># 计算非法所得</span></span>
<span class="line">            illegal_amount <span class="token operator">=</span> self<span class="token punctuation">.</span>calculate_illegal_amount<span class="token punctuation">(</span>player_id<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 扣除</span></span>
<span class="line">            self<span class="token punctuation">.</span>deduct_gold<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> illegal_amount<span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 记录</span></span>
<span class="line">            INFO_MSG<span class="token punctuation">(</span><span class="token string-interpolation"><span class="token string">f&quot;Confiscated </span><span class="token interpolation"><span class="token punctuation">{</span>illegal_amount<span class="token punctuation">}</span></span><span class="token string"> gold from player </span><span class="token interpolation"><span class="token punctuation">{</span>player_id<span class="token punctuation">}</span></span><span class="token string">&quot;</span></span><span class="token punctuation">)</span></span>
<span class="line"></span>
<span class="line">            <span class="token comment"># 封禁</span></span>
<span class="line">            <span class="token keyword">if</span> illegal_amount <span class="token operator">&gt;</span> <span class="token number">100000</span><span class="token punctuation">:</span></span>
<span class="line">                self<span class="token punctuation">.</span>ban_player<span class="token punctuation">(</span>player_id<span class="token punctuation">,</span> reason<span class="token operator">=</span><span class="token string">&quot;exploit_abuse&quot;</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="五、事后分析" tabindex="-1"><a class="header-anchor" href="#五、事后分析"><span>五、事后分析</span></a></h2><h3 id="_5-1-根因分析" tabindex="-1"><a class="header-anchor" href="#_5-1-根因分析"><span>5.1 根因分析</span></a></h3><div class="language-python line-numbers-mode" data-highlighter="prismjs" data-ext="py"><pre><code class="language-python"><span class="line"><span class="token comment"># 5 Whys 分析法</span></span>
<span class="line"></span>
<span class="line"><span class="token triple-quoted-string string">&quot;&quot;&quot;</span>
<span class="line">事故: 数据库误删玩家数据</span>
<span class="line"></span>
<span class="line">Why 1: 为什么数据被删除?</span>
<span class="line">答: 清理脚本误删</span>
<span class="line"></span>
<span class="line">Why 2: 为什么清理脚本会误删?</span>
<span class="line">答: WHERE 条件被注释掉</span>
<span class="line"></span>
<span class="line">Why 3: 为什么 WHERE 条件被注释?</span>
<span class="line">答: 测试时临时注释，忘记恢复</span>
<span class="line"></span>
<span class="line">Why 4: 为什么没有测试就上线?</span>
<span class="line">答: 缺少代码审查流程</span>
<span class="line"></span>
<span class="line">Why 5: 为什么缺少流程?</span>
<span class="line">答: 没有建立完善的上线规范</span>
<span class="line"></span>
<span class="line">根本原因: 缺少完善的代码审查和上线流程</span>
<span class="line">&quot;&quot;&quot;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_5-2-改进措施" tabindex="-1"><a class="header-anchor" href="#_5-2-改进措施"><span>5.2 改进措施</span></a></h3><table><thead><tr><th>措施</th><th>说明</th></tr></thead><tbody><tr><td>代码审查</td><td>所有数据库操作必须审查</td></tr><tr><td>脚本验证</td><td>脚本执行前预览</td></tr><tr><td>备份测试</td><td>定期测试备份恢复</td></tr><tr><td>权限控制</td><td>限制生产环境操作</td></tr><tr><td>监控告警</td><td>异常操作立即告警</td></tr></tbody></table><hr><h2 id="六、最佳实践" tabindex="-1"><a class="header-anchor" href="#六、最佳实践"><span>六、最佳实践</span></a></h2><h3 id="_6-1-事故处理建议" tabindex="-1"><a class="header-anchor" href="#_6-1-事故处理建议"><span>6.1 事故处理建议</span></a></h3><table><thead><tr><th>实践</th><th>说明</th></tr></thead><tbody><tr><td><strong>快速止损</strong></td><td>优先恢复服务</td></tr><tr><td><strong>保持冷静</strong></td><td>按流程处理</td></tr><tr><td><strong>记录详细</strong></td><td>保留所有日志</td></tr><tr><td><strong>及时沟通</strong></td><td>通知相关人员</td></tr><tr><td><strong>事后总结</strong></td><td>输出事故报告</td></tr><tr><td><strong>预防为主</strong></td><td>建立防护机制</td></tr></tbody></table><hr><h2 id="七、总结" tabindex="-1"><a class="header-anchor" href="#七、总结"><span>七、总结</span></a></h2><h3 id="生产事故处理核心" tabindex="-1"><a class="header-anchor" href="#生产事故处理核心"><span>生产事故处理核心</span></a></h3><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">事故处理 = 快速响应 + 止损恢复 + 根因分析 + 改进预防</span>
<span class="line">- 分级响应</span>
<span class="line">- 优先恢复服务</span>
<span class="line">- 详细记录过程</span>
<span class="line">- 建立改进机制</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><hr><h2 id="参考资料" tabindex="-1"><a class="header-anchor" href="#参考资料"><span>参考资料</span></a></h2><ul><li><a href="https://www.us-cert.gov/sites/default/files/publications/incident_response_guidance.pdf" target="_blank" rel="noopener noreferrer">Incident Response Guide</a></li><li><a href="https://www.amazon.com/Google-SRE-Book-Site-Reliability/dp/1491950358" target="_blank" rel="noopener noreferrer">Post-Mortem Culture</a></li></ul>`,33)])}var l=a(s,[[`render`,c]]);export{o as _pageData,l as default};