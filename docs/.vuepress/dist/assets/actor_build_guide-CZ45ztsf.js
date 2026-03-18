import{i as e,r as t,s as n,t as r}from"./app-DuH0mZgh.js";var i=JSON.parse(`{"path":"/actor_build_guide.html","title":"Apollo Actor 框架 - 构建和使用指南","lang":"en-US","frontmatter":{},"filePathRelative":"actor_build_guide.md","git":{"createdTime":1766741202000,"updatedTime":1766741202000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":1,"url":"https://github.com/cuihairu"}]}}`),a={name:`actor_build_guide.md`};function o(r,i,a,o,s,c){return n(),t(`div`,null,[...i[0]||=[e(`<h1 id="apollo-actor-框架-构建和使用指南" tabindex="-1"><a class="header-anchor" href="#apollo-actor-框架-构建和使用指南"><span>Apollo Actor 框架 - 构建和使用指南</span></a></h1><h2 id="目录结构" tabindex="-1"><a class="header-anchor" href="#目录结构"><span>目录结构</span></a></h2><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">apollo/</span>
<span class="line">├── include/apollo/actor/          # Actor 框架头文件</span>
<span class="line">│   ├── message.h                   # 消息定义和序列化接口</span>
<span class="line">│   ├── actor_ref.h                 # Actor 引用（包含 ActorPath）</span>
<span class="line">│   ├── actor.h                     # Actor 基类</span>
<span class="line">│   ├── actor_cell.h                # Actor 执行单元</span>
<span class="line">│   ├── actor_system.h              # Actor 系统</span>
<span class="line">│   ├── actor_manager.h             # Actor 管理器</span>
<span class="line">│   ├── dispatcher.h                # 消息调度器</span>
<span class="line">│   ├── message_bus.h               # 消息总线</span>
<span class="line">│   ├── timer_service.h             # 定时器服务</span>
<span class="line">│   └── monitoring.h                # 监控和可观测性</span>
<span class="line">│</span>
<span class="line">├── src/apollo/actor/               # Actor 框架实现</span>
<span class="line">│   ├── actor_cell.cpp</span>
<span class="line">│   ├── actor_system.cpp</span>
<span class="line">│   ├── actor_manager.cpp</span>
<span class="line">│   ├── dispatcher.cpp</span>
<span class="line">│   ├── message_bus.cpp</span>
<span class="line">│   ├── monitoring.cpp</span>
<span class="line">│   ├── timer_service.cpp</span>
<span class="line">│   ├── actor_ref.cpp</span>
<span class="line">│   └── message_serialization.cpp</span>
<span class="line">│</span>
<span class="line">├── tests/                          # 测试</span>
<span class="line">│   ├── test_actor_framework.cpp</span>
<span class="line">│   ├── test_actor_concurrency.cpp</span>
<span class="line">│   └── README.md</span>
<span class="line">│</span>
<span class="line">├── examples/                       # 示例</span>
<span class="line">│   ├── actor_demo.cpp</span>
<span class="line">│   └── actor_advanced_demo.cpp</span>
<span class="line">│</span>
<span class="line">└── docs/                           # 文档</span>
<span class="line">    └── actor_complete_guide.md</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="头文件依赖关系" tabindex="-1"><a class="header-anchor" href="#头文件依赖关系"><span>头文件依赖关系</span></a></h2><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">┌─────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                          actor_ref.h                            │</span>
<span class="line">│  ┌────────────────────────────────────────────────────────────┐ │</span>
<span class="line">│  │ ActorPath + ActorRef + ActorRefSet                          │ │</span>
<span class="line">│  │ 依赖: message.h                                             │ │</span>
<span class="line">│  └────────────────────────────────────────────────────────────┘ │</span>
<span class="line">└─────────────────────────────────────────────────────────────────┘</span>
<span class="line">                              ▲</span>
<span class="line">                              │</span>
<span class="line">┌─────────────────────────────────────────────────────────────────┐</span>
<span class="line">│                          actor.h                                │</span>
<span class="line">│  ┌────────────────────────────────────────────────────────────┐ │</span>
<span class="line">│  │ Actor + ActorContext + 内建消息                            │ │</span>
<span class="line">│  │ 依赖: actor_ref.h, message.h                               │ │</span>
<span class="line">│  └────────────────────────────────────────────────────────────┘ │</span>
<span class="line">└─────────────────────────────────────────────────────────────────┘</span>
<span class="line">                              ▲</span>
<span class="line">                              │</span>
<span class="line">        ┌───────────────────────┼───────────────────────┐</span>
<span class="line">        │                       │                       │</span>
<span class="line">┌───────────────┐     ┌─────────────────┐    ┌──────────────┐</span>
<span class="line">│ actor_cell.h  │     │ actor_system.h  │    │actor_manager.│</span>
<span class="line">│ + Envelope    │     │ + ActorSystem   │    │h            │</span>
<span class="line">└───────────────┘     └─────────────────┘    └──────────────┘</span>
<span class="line">        ▲                       ▲                       ▲</span>
<span class="line">        │                       │                       │</span>
<span class="line">┌───────────────┐     ┌─────────────────┐    ┌──────────────┐</span>
<span class="line">│ dispatcher.h  │     │ message_bus.h   │    │timer_service.│</span>
<span class="line">└───────────────┘     └─────────────────┘    │h             │</span>
<span class="line">                                               └──────────────┘</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="编译配置" tabindex="-1"><a class="header-anchor" href="#编译配置"><span>编译配置</span></a></h2><h3 id="cmake-最低要求" tabindex="-1"><a class="header-anchor" href="#cmake-最低要求"><span>CMake 最低要求</span></a></h3><div class="language-cmake line-numbers-mode" data-highlighter="prismjs" data-ext="cmake"><pre><code class="language-cmake"><span class="line"><span class="token keyword">cmake_minimum_required</span><span class="token punctuation">(</span><span class="token property">VERSION</span> <span class="token number">3.16</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div></div></div><h3 id="c-标准" tabindex="-1"><a class="header-anchor" href="#c-标准"><span>C++ 标准</span></a></h3><div class="language-cmake line-numbers-mode" data-highlighter="prismjs" data-ext="cmake"><pre><code class="language-cmake"><span class="line"><span class="token keyword">set</span><span class="token punctuation">(</span><span class="token variable">CMAKE_CXX_STANDARD</span> <span class="token number">20</span><span class="token punctuation">)</span></span>
<span class="line"><span class="token keyword">set</span><span class="token punctuation">(</span><span class="token variable">CMAKE_CXX_STANDARD_REQUIRED</span> <span class="token boolean">ON</span><span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="编译步骤" tabindex="-1"><a class="header-anchor" href="#编译步骤"><span>编译步骤</span></a></h2><h3 id="linux-macos" tabindex="-1"><a class="header-anchor" href="#linux-macos"><span>Linux/macOS</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 1. 配置</span></span>
<span class="line"><span class="token function">mkdir</span> build <span class="token operator">&amp;&amp;</span> <span class="token builtin class-name">cd</span> build</span>
<span class="line">cmake <span class="token punctuation">..</span> <span class="token parameter variable">-DCMAKE_BUILD_TYPE</span><span class="token operator">=</span>Release <span class="token parameter variable">-DAPOLLO_BUILD_TESTS</span><span class="token operator">=</span>ON</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. 编译</span></span>
<span class="line">cmake <span class="token parameter variable">--build</span> <span class="token builtin class-name">.</span> -j<span class="token variable"><span class="token variable">$(</span>nproc<span class="token variable">)</span></span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 3. 运行测试</span></span>
<span class="line">ctest --output-on-failure</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 4. 运行示例</span></span>
<span class="line">./examples/actor_demo</span>
<span class="line">./examples/actor_advanced_demo</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="windows-visual-studio" tabindex="-1"><a class="header-anchor" href="#windows-visual-studio"><span>Windows (Visual Studio)</span></a></h3><div class="language-cmd line-numbers-mode" data-highlighter="prismjs" data-ext="cmd"><pre><code class="language-cmd"><span class="line">REM 1. 配置</span>
<span class="line">mkdir build</span>
<span class="line">cd build</span>
<span class="line">cmake .. -G &quot;Visual Studio 17 2022&quot; -A x64</span>
<span class="line"></span>
<span class="line">REM 2. 编译</span>
<span class="line">cmake --build . --config Release</span>
<span class="line"></span>
<span class="line">REM 3. 运行测试</span>
<span class="line">ctest -C Release --output-on-failure</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="windows-mingw" tabindex="-1"><a class="header-anchor" href="#windows-mingw"><span>Windows (MinGW)</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 1. 配置</span></span>
<span class="line"><span class="token function">mkdir</span> build <span class="token operator">&amp;&amp;</span> <span class="token builtin class-name">cd</span> build</span>
<span class="line">cmake <span class="token punctuation">..</span> <span class="token parameter variable">-G</span> <span class="token string">&quot;MinGW Makefiles&quot;</span> <span class="token parameter variable">-DCMAKE_BUILD_TYPE</span><span class="token operator">=</span>Release</span>
<span class="line"></span>
<span class="line"><span class="token comment"># 2. 编译</span></span>
<span class="line">cmake <span class="token parameter variable">--build</span> <span class="token builtin class-name">.</span> <span class="token parameter variable">-j</span></span>
<span class="line"></span>
<span class="line"><span class="token comment"># 3. 运行</span></span>
<span class="line">./examples/actor_demo.exe</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="可选依赖" tabindex="-1"><a class="header-anchor" href="#可选依赖"><span>可选依赖</span></a></h2><h3 id="flatbuffers-推荐" tabindex="-1"><a class="header-anchor" href="#flatbuffers-推荐"><span>FlatBuffers（推荐）</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># Ubuntu/Debian</span></span>
<span class="line"><span class="token function">sudo</span> <span class="token function">apt-get</span> <span class="token function">install</span> libflatbuffers-dev</span>
<span class="line"></span>
<span class="line"><span class="token comment"># macOS (Homebrew)</span></span>
<span class="line">brew <span class="token function">install</span> flatbuffers</span>
<span class="line"></span>
<span class="line"><span class="token comment"># vcpkg</span></span>
<span class="line"><span class="token function">vcpkg</span> <span class="token function">install</span> flatbuffers</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="yaml-支持" tabindex="-1"><a class="header-anchor" href="#yaml-支持"><span>YAML 支持</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># Ubuntu/Debian</span></span>
<span class="line"><span class="token function">sudo</span> <span class="token function">apt-get</span> <span class="token function">install</span> libyaml-cpp-dev</span>
<span class="line"></span>
<span class="line"><span class="token comment"># macOS (Homebrew)</span></span>
<span class="line">brew <span class="token function">install</span> yaml-cpp</span>
<span class="line"></span>
<span class="line"><span class="token comment"># vcpkg</span></span>
<span class="line"><span class="token function">vcpkg</span> <span class="token function">install</span> yaml-cpp</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="sqlite-服务发现" tabindex="-1"><a class="header-anchor" href="#sqlite-服务发现"><span>SQLite（服务发现）</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># Ubuntu/Debian</span></span>
<span class="line"><span class="token function">sudo</span> <span class="token function">apt-get</span> <span class="token function">install</span> libsqlite3-dev</span>
<span class="line"></span>
<span class="line"><span class="token comment"># macOS</span></span>
<span class="line">brew <span class="token function">install</span> sqlite3</span>
<span class="line"></span>
<span class="line"><span class="token comment"># Windows (vcpkg)</span></span>
<span class="line"><span class="token function">vcpkg</span> <span class="token function">install</span> sqlite3</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="编译选项" tabindex="-1"><a class="header-anchor" href="#编译选项"><span>编译选项</span></a></h2><table><thead><tr><th>选项</th><th>默认值</th><th>说明</th></tr></thead><tbody><tr><td><code>APOLLO_BUILD_TESTS</code></td><td><code>ON</code></td><td>构建测试</td></tr><tr><td><code>APOLLO_BUILD_EXAMPLES</code></td><td><code>ON</code></td><td>构建示例</td></tr><tr><td><code>APOLLO_ENABLE_YAML</code></td><td><code>ON</code></td><td>启用 YAML 支持</td></tr><tr><td><code>APOLLO_ENABLE_COVERAGE</code></td><td><code>OFF</code></td><td>启用代码覆盖率</td></tr></tbody></table><h2 id="常见编译问题" tabindex="-1"><a class="header-anchor" href="#常见编译问题"><span>常见编译问题</span></a></h2><h3 id="_1-找不到-protobuf" tabindex="-1"><a class="header-anchor" href="#_1-找不到-protobuf"><span>1. 找不到 protobuf</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 使用 vcpkg</span></span>
<span class="line">cmake <span class="token punctuation">..</span> <span class="token parameter variable">-DCMAKE_TOOLCHAIN_FILE</span><span class="token operator">=</span><span class="token punctuation">[</span>vcpkg<span class="token punctuation">]</span>/scripts/buildsystems/vcpkg.cmake</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-c-20-不可用" tabindex="-1"><a class="header-anchor" href="#_2-c-20-不可用"><span>2. C++20 不可用</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 升级编译器</span></span>
<span class="line"><span class="token comment"># GCC &gt;= 10, Clang &gt;= 12, MSVC &gt;= 2019</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-链接错误" tabindex="-1"><a class="header-anchor" href="#_3-链接错误"><span>3. 链接错误</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># 确保链接了正确的库</span></span>
<span class="line">target_link_libraries<span class="token punctuation">(</span>your_target PRIVATE apollo pthread<span class="token punctuation">)</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="运行时配置" tabindex="-1"><a class="header-anchor" href="#运行时配置"><span>运行时配置</span></a></h2><h3 id="环境变量" tabindex="-1"><a class="header-anchor" href="#环境变量"><span>环境变量</span></a></h3><table><thead><tr><th>变量</th><th>说明</th><th>默认值</th></tr></thead><tbody><tr><td><code>APOLLO_SYSTEM_NAME</code></td><td>Actor 系统名称</td><td><code>apollo</code></td></tr><tr><td><code>APOLLO_ACTOR_THREADS</code></td><td>调度线程数</td><td>CPU 核心数</td></tr><tr><td><code>APOLLO_IO_THREADS</code></td><td>IO 线程数</td><td>2</td></tr><tr><td><code>APOLLO_MAILBOX_SIZE</code></td><td>邮箱大小</td><td>256</td></tr></tbody></table><h3 id="配置文件" tabindex="-1"><a class="header-anchor" href="#配置文件"><span>配置文件</span></a></h3><div class="language-yaml line-numbers-mode" data-highlighter="prismjs" data-ext="yml"><pre><code class="language-yaml"><span class="line"><span class="token comment"># actor-config.yaml</span></span>
<span class="line"><span class="token key atrule">system</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token key atrule">name</span><span class="token punctuation">:</span> <span class="token string">&quot;my-game-server&quot;</span></span>
<span class="line">  <span class="token key atrule">address</span><span class="token punctuation">:</span> <span class="token string">&quot;127.0.0.1&quot;</span></span>
<span class="line">  <span class="token key atrule">dataCenter</span><span class="token punctuation">:</span> <span class="token string">&quot;shanghai&quot;</span></span>
<span class="line">  <span class="token key atrule">hostId</span><span class="token punctuation">:</span> <span class="token string">&quot;server-001&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token key atrule">threads</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token key atrule">dispatcher</span><span class="token punctuation">:</span> <span class="token number">4</span></span>
<span class="line">  <span class="token key atrule">io</span><span class="token punctuation">:</span> <span class="token number">2</span></span>
<span class="line">  <span class="token key atrule">background</span><span class="token punctuation">:</span> <span class="token number">2</span></span>
<span class="line"></span>
<span class="line"><span class="token key atrule">mailbox</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token key atrule">capacity</span><span class="token punctuation">:</span> <span class="token number">256</span></span>
<span class="line">  <span class="token key atrule">highWatermark</span><span class="token punctuation">:</span> <span class="token number">192</span></span>
<span class="line">  <span class="token key atrule">lowWatermark</span><span class="token punctuation">:</span> <span class="token number">64</span></span>
<span class="line"></span>
<span class="line"><span class="token key atrule">discovery</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token key atrule">backend</span><span class="token punctuation">:</span> <span class="token string">&quot;sqlite&quot;</span>  <span class="token comment"># sqlite, redis, memory</span></span>
<span class="line">  <span class="token key atrule">dbPath</span><span class="token punctuation">:</span> <span class="token string">&quot;:memory:&quot;</span></span>
<span class="line"></span>
<span class="line"><span class="token key atrule">monitoring</span><span class="token punctuation">:</span></span>
<span class="line">  <span class="token key atrule">enableHttpApi</span><span class="token punctuation">:</span> <span class="token boolean important">true</span></span>
<span class="line">  <span class="token key atrule">httpApiPort</span><span class="token punctuation">:</span> <span class="token number">8080</span></span>
<span class="line">  <span class="token key atrule">enablePrometheus</span><span class="token punctuation">:</span> <span class="token boolean important">false</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="性能调优" tabindex="-1"><a class="header-anchor" href="#性能调优"><span>性能调优</span></a></h2><h3 id="_1-线程池配置" tabindex="-1"><a class="header-anchor" href="#_1-线程池配置"><span>1. 线程池配置</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line">ThreadPoolConfig config<span class="token punctuation">;</span></span>
<span class="line">config<span class="token punctuation">.</span>dispatcherThreads <span class="token operator">=</span> std<span class="token double-colon punctuation">::</span>thread<span class="token double-colon punctuation">::</span><span class="token function">hardware_concurrency</span><span class="token punctuation">(</span><span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line">config<span class="token punctuation">.</span>ioThreads <span class="token operator">=</span> <span class="token number">4</span><span class="token punctuation">;</span></span>
<span class="line">config<span class="token punctuation">.</span>dispatchStrategy <span class="token operator">=</span> DispatchStrategy<span class="token double-colon punctuation">::</span>LeastLoaded<span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_2-邮箱大小" tabindex="-1"><a class="header-anchor" href="#_2-邮箱大小"><span>2. 邮箱大小</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line">MailboxConfig mailboxConfig<span class="token punctuation">;</span></span>
<span class="line">mailboxConfig<span class="token punctuation">.</span>capacity <span class="token operator">=</span> <span class="token number">1024</span><span class="token punctuation">;</span>  <span class="token comment">// 增加容量</span></span>
<span class="line">mailboxConfig<span class="token punctuation">.</span>strategy <span class="token operator">=</span> ipc<span class="token double-colon punctuation">::</span>BackpressureStrategy<span class="token double-colon punctuation">::</span>Block<span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_3-消息序列化" tabindex="-1"><a class="header-anchor" href="#_3-消息序列化"><span>3. 消息序列化</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 使用 Binary 格式（最快）</span></span>
<span class="line">Message <span class="token function">msg</span><span class="token punctuation">(</span>data<span class="token punctuation">,</span> MessageFormat<span class="token double-colon punctuation">::</span>Binary<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span>
<span class="line"><span class="token comment">// 使用 FlatBuffers（零拷贝）</span></span>
<span class="line">Message <span class="token function">msg</span><span class="token punctuation">(</span>data<span class="token punctuation">,</span> MessageFormat<span class="token double-colon punctuation">::</span>FlatBuffers<span class="token punctuation">)</span><span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="_4-本地通信优先" tabindex="-1"><a class="header-anchor" href="#_4-本地通信优先"><span>4. 本地通信优先</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token comment">// 自动选择最优传输</span></span>
<span class="line">endpoint<span class="token punctuation">.</span>transportMask <span class="token operator">=</span> TransportMask<span class="token double-colon punctuation">::</span>SharedMemory <span class="token operator">|</span></span>
<span class="line">                         TransportMask<span class="token double-colon punctuation">::</span>UnixSocket <span class="token operator">|</span></span>
<span class="line">                         TransportMask<span class="token double-colon punctuation">::</span>Tcp<span class="token punctuation">;</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="调试" tabindex="-1"><a class="header-anchor" href="#调试"><span>调试</span></a></h2><h3 id="启用调试日志" tabindex="-1"><a class="header-anchor" href="#启用调试日志"><span>启用调试日志</span></a></h3><div class="language-cpp line-numbers-mode" data-highlighter="prismjs" data-ext="cpp"><pre><code class="language-cpp"><span class="line"><span class="token macro property"><span class="token directive-hash">#</span><span class="token directive keyword">define</span> <span class="token macro-name">APOLLO_DEBUG_LOG</span> <span class="token expression"><span class="token number">1</span></span></span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div></div></div><h3 id="内存检测" tabindex="-1"><a class="header-anchor" href="#内存检测"><span>内存检测</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># Valgrind</span></span>
<span class="line">valgrind --leak-check<span class="token operator">=</span>full --show-leak-kinds<span class="token operator">=</span>all ./your_app</span>
<span class="line"></span>
<span class="line"><span class="token comment"># AddressSanitizer</span></span>
<span class="line">cmake <span class="token punctuation">..</span> <span class="token parameter variable">-DSANITIZE_ADDRESS</span><span class="token operator">=</span>ON</span>
<span class="line">cmake <span class="token parameter variable">--build</span> <span class="token builtin class-name">.</span></span>
<span class="line">./your_app</span>
<span class="line"></span>
<span class="line"><span class="token comment"># ThreadSanitizer</span></span>
<span class="line">cmake <span class="token punctuation">..</span> <span class="token parameter variable">-DSANITIZE_THREAD</span><span class="token operator">=</span>ON</span>
<span class="line">cmake <span class="token parameter variable">--build</span> <span class="token builtin class-name">.</span></span>
<span class="line">./your_app</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="性能分析" tabindex="-1"><a class="header-anchor" href="#性能分析"><span>性能分析</span></a></h3><div class="language-bash line-numbers-mode" data-highlighter="prismjs" data-ext="sh"><pre><code class="language-bash"><span class="line"><span class="token comment"># perf (Linux)</span></span>
<span class="line">perf record <span class="token parameter variable">-g</span> ./your_app</span>
<span class="line">perf report</span>
<span class="line"></span>
<span class="line"><span class="token comment"># gprof</span></span>
<span class="line">cmake <span class="token punctuation">..</span> <span class="token parameter variable">-DCMAKE_BUILD_TYPE</span><span class="token operator">=</span>Debug <span class="token parameter variable">-DPROFILE</span><span class="token operator">=</span>ON</span>
<span class="line">cmake <span class="token parameter variable">--build</span> <span class="token builtin class-name">.</span></span>
<span class="line">./your_app</span>
<span class="line">gprof your_app gmon.out <span class="token operator">&gt;</span> analysis.txt</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="部署" tabindex="-1"><a class="header-anchor" href="#部署"><span>部署</span></a></h2><h3 id="docker" tabindex="-1"><a class="header-anchor" href="#docker"><span>Docker</span></a></h3><div class="language-docker line-numbers-mode" data-highlighter="prismjs" data-ext="docker"><pre><code class="language-docker"><span class="line"><span class="token instruction"><span class="token keyword">FROM</span> ubuntu:22.04</span></span>
<span class="line"></span>
<span class="line"><span class="token instruction"><span class="token keyword">RUN</span> apt-get update &amp;&amp; apt-get install -y <span class="token operator">\\</span></span>
<span class="line">    g++ <span class="token operator">\\</span></span>
<span class="line">    cmake <span class="token operator">\\</span></span>
<span class="line">    libprotobuf-dev <span class="token operator">\\</span></span>
<span class="line">    libyaml-cpp-dev <span class="token operator">\\</span></span>
<span class="line">    libsqlite3-dev <span class="token operator">\\</span></span>
<span class="line">    &amp;&amp; rm -rf /var/lib/apt/lists/*</span></span>
<span class="line"></span>
<span class="line"><span class="token instruction"><span class="token keyword">WORKDIR</span> /app</span></span>
<span class="line"><span class="token instruction"><span class="token keyword">COPY</span> . .</span></span>
<span class="line"><span class="token instruction"><span class="token keyword">RUN</span> mkdir build &amp;&amp; cd build &amp;&amp; <span class="token operator">\\</span></span>
<span class="line">    cmake .. -DCMAKE_BUILD_TYPE=Release &amp;&amp; <span class="token operator">\\</span></span>
<span class="line">    cmake --build . -j$(nproc)</span></span>
<span class="line"></span>
<span class="line"><span class="token instruction"><span class="token keyword">CMD</span> [<span class="token string">&quot;./build/examples/actor_advanced_demo&quot;</span>]</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h3 id="系统服务" tabindex="-1"><a class="header-anchor" href="#系统服务"><span>系统服务</span></a></h3><div class="language-ini line-numbers-mode" data-highlighter="prismjs" data-ext="ini"><pre><code class="language-ini"><span class="line"><span class="token comment"># /etc/systemd/system/apollo-game-server.service</span></span>
<span class="line"><span class="token section"><span class="token punctuation">[</span><span class="token section-name selector">Unit</span><span class="token punctuation">]</span></span></span>
<span class="line"><span class="token key attr-name">Description</span><span class="token punctuation">=</span><span class="token value attr-value">Apollo Game Server</span></span>
<span class="line"><span class="token key attr-name">After</span><span class="token punctuation">=</span><span class="token value attr-value">network.target</span></span>
<span class="line"></span>
<span class="line"><span class="token section"><span class="token punctuation">[</span><span class="token section-name selector">Service</span><span class="token punctuation">]</span></span></span>
<span class="line"><span class="token key attr-name">Type</span><span class="token punctuation">=</span><span class="token value attr-value">simple</span></span>
<span class="line"><span class="token key attr-name">User</span><span class="token punctuation">=</span><span class="token value attr-value">apollo</span></span>
<span class="line"><span class="token key attr-name">WorkingDirectory</span><span class="token punctuation">=</span><span class="token value attr-value">/opt/apollo</span></span>
<span class="line"><span class="token key attr-name">ExecStart</span><span class="token punctuation">=</span><span class="token value attr-value">/opt/apollo/bin/game-server --config /etc/apollo/config.yaml</span></span>
<span class="line"><span class="token key attr-name">Restart</span><span class="token punctuation">=</span><span class="token value attr-value">on-failure</span></span>
<span class="line"></span>
<span class="line"><span class="token section"><span class="token punctuation">[</span><span class="token section-name selector">Install</span><span class="token punctuation">]</span></span></span>
<span class="line"><span class="token key attr-name">WantedBy</span><span class="token punctuation">=</span><span class="token value attr-value">multi-user.target</span></span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="故障排查" tabindex="-1"><a class="header-anchor" href="#故障排查"><span>故障排查</span></a></h2><h3 id="问题-actor-消息丢失" tabindex="-1"><a class="header-anchor" href="#问题-actor-消息丢失"><span>问题：Actor 消息丢失</span></a></h3><ol><li>检查邮箱水位线</li><li>检查背压策略</li><li>启用调试日志</li></ol><h3 id="问题-性能下降" tabindex="-1"><a class="header-anchor" href="#问题-性能下降"><span>问题：性能下降</span></a></h3><ol><li>使用 profiler 分析瓶颈</li><li>调整线程池大小</li><li>检查序列化格式</li></ol><h3 id="问题-死锁" tabindex="-1"><a class="header-anchor" href="#问题-死锁"><span>问题：死锁</span></a></h3><ol><li>使用 ThreadSanitizer 检测</li><li>检查 ask() 模式的超时设置</li><li>避免 Actor 循环等待</li></ol>`,66)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};