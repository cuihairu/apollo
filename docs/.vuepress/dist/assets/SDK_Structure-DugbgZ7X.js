import{a as e,c as t,i as n,t as r}from"./app-B8uz0aqq.js";var i=JSON.parse(`{"path":"/sdks/unity/SDK_Structure.html","title":"Apollo SDK 目录结构","lang":"en-US","frontmatter":{},"filePathRelative":"sdks/unity/SDK_Structure.md","git":{"createdTime":1766555507000,"updatedTime":1766765466000,"contributors":[{"name":"cuihairu","username":"cuihairu","email":"chuihairu@gmail.com","commits":2,"url":"https://github.com/cuihairu"}]}}`),a={name:`SDK_Structure.md`};function o(r,i,a,o,s,c){return t(),n(`div`,null,[...i[0]||=[e(`<h1 id="apollo-sdk-目录结构" tabindex="-1"><a class="header-anchor" href="#apollo-sdk-目录结构"><span>Apollo SDK 目录结构</span></a></h1><h2 id="概述" tabindex="-1"><a class="header-anchor" href="#概述"><span>概述</span></a></h2><p><code>skds/</code> 目录存放各平台的客户端 SDK，与服务端框架保持协议和接口一致。</p><h2 id="目录结构" tabindex="-1"><a class="header-anchor" href="#目录结构"><span>目录结构</span></a></h2><div class="language-text line-numbers-mode" data-highlighter="prismjs" data-ext="text"><pre><code class="language-text"><span class="line">skds/</span>
<span class="line">├── unity/                    # Unity SDK</span>
<span class="line">│   └── ApolloSDK/</span>
<span class="line">│       ├── ApolloClient.cs         # 主客户端</span>
<span class="line">│       ├── ApolloClientConfig.cs   # 配置</span>
<span class="line">│       ├── Network/               # 网络模块</span>
<span class="line">│       │   └── NetworkManager.cs</span>
<span class="line">│       ├── Session/               # 会话管理</span>
<span class="line">│       │   └── AuthManager.cs</span>
<span class="line">│       ├── Attributes/            # 属性系统</span>
<span class="line">│       │   ├── AttributeValue.cs</span>
<span class="line">│       │   ├── AttributeContainer.cs</span>
<span class="line">│       │   ├── AttributeSyncManager.cs</span>
<span class="line">│       │   └── ...</span>
<span class="line">│       ├── Messaging/             # 消息处理</span>
<span class="line">│       └── Utilities/             # 工具类</span>
<span class="line">│</span>
<span class="line">├── cocos/                   # Cocos Creator SDK</span>
<span class="line">│   └── ApolloSDK/</span>
<span class="line">│</span>
<span class="line">├── laya/                    # LayaBox SDK</span>
<span class="line">│   └── ApolloSDK/</span>
<span class="line">│   └── ApolloSDK/</span>
<span class="line">│</span>
<span class="line">└── web/                     # Web SDK (TODO)</span>
<span class="line">    └── ApolloSDK/</span>
<span class="line"></span></code></pre><div class="line-numbers" aria-hidden="true" style="counter-reset:line-number 0;"><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div><div class="line-number"></div></div></div><h2 id="unity-sdk-模块说明" tabindex="-1"><a class="header-anchor" href="#unity-sdk-模块说明"><span>Unity SDK 模块说明</span></a></h2><table><thead><tr><th>模块</th><th>功能</th><th>状态</th></tr></thead><tbody><tr><td><code>ApolloClient.cs</code></td><td>主入口，连接、认证、心跳</td><td>✅ 已有</td></tr><tr><td><code>ApolloClientConfig.cs</code></td><td>配置类</td><td>✅ 已有</td></tr><tr><td><code>Network/NetworkManager.cs</code></td><td>TCP/Socket 连接管理</td><td>✅ 已有</td></tr><tr><td><code>Session/AuthManager.cs</code></td><td>登录认证</td><td>✅ 已有</td></tr><tr><td><code>Attributes/</code></td><td>属性同步系统</td><td>🚧 开发中</td></tr><tr><td><code>Messaging/</code></td><td>Protobuf 消息路由</td><td>📋 计划中</td></tr><tr><td><code>Utilities/</code></td><td>日志、定时器等</td><td>📋 计划中</td></tr></tbody></table><h2 id="文档规范" tabindex="-1"><a class="header-anchor" href="#文档规范"><span>文档规范</span></a></h2><ul><li>SDK 设计文档放在 <code>docs/sdks/{platform}/</code> 目录</li><li>每个模块一个 <code>.md</code> 文件</li><li>命名规范：<code>{模块名}_SDK.md</code></li></ul>`,9)]])}var s=r(a,[[`render`,o]]);export{i as _pageData,s as default};