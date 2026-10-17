import { defineConfig } from 'vitepress'

const isProd = process.env.NODE_ENV === 'production'

const architectureSidebar = [
  {
    text: 'Apollo 架构',
    items: [
      { text: '架构总览', link: '/architecture/overview' },
      { text: '当前架构决策', link: '/architecture/adr' },
      { text: 'Remote Entity Call 语义层', link: '/architecture/remote-entity-call-design' },
      { text: '观测与运行时内省', link: '/architecture/observability-watcher-and-runtime-introspection-design' },
      { text: 'Host Builder 与 DI', link: '/architecture/host-builder-and-di-design' },
      { text: 'Starter 与模块装配', link: '/architecture/starter-and-module-assembly-design' },
      { text: '房间制 GameServer 设计', link: '/30-Room-Based_GameServer_Design' },
    ],
  },
  {
    text: '外部引擎参考',
    items: [
      { text: 'BigWorld 架构', link: '/architecture/bigworld' },
      { text: 'BigWorld 生命周期', link: '/architecture/bigworld-lifecycle' },
      { text: 'KBE 源码分析', link: '/architecture/kbe-source-analysis' },
      { text: 'KBE 参考原则', link: '/architecture/kbe-reference-principles' },
      { text: 'KBE EntityDef 分析', link: '/architecture/kbengine-entitydef-analysis' },
      { text: 'Base / Cell / Proxy 模型', link: '/architecture/base-cell-proxy-model' },
      { text: 'Witness 与 Ghost', link: '/architecture/witness-ghost-design' },
      { text: 'AOI', link: '/architecture/aoi' },
      { text: 'AOI 广播', link: '/architecture/aoi-broadcast' },
    ],
  },
]

export default defineConfig({
  title: 'Apollo',
  description: '轻量房间制多人游戏服务器引擎',
  base: isProd ? '/apollo/' : '/',
  cleanUrls: true,
  ignoreDeadLinks: false,

  head: [
    ['link', { rel: 'icon', type: 'image/png', href: '/apollo.png' }],
  ],

  locales: {
    root: {
      label: '简体中文',
      lang: 'zh-CN',
      themeConfig: {
        nav: [
          { text: '指南', link: '/guide/' },
          { text: '架构', link: '/analysis/architecture-review' },
          { text: '模块', link: '/modules/' },
          { text: '应用', link: '/apps/' },
          { text: 'API', link: '/api/' },
          { text: 'SDK', link: '/sdks/' },
          { text: 'QA', link: '/QA' },
        ],
        sidebar: {
          '/guide/': [
            { text: '开始', items: [
              { text: '指南概览', link: '/guide/' }, { text: '安装', link: '/guide/installation' },
              { text: '快速开始', link: '/guide/quick-start' }, { text: '上手教程', link: '/guide/tutorial' },
            ] },
            { text: '基础', items: [
              { text: '核心概念', link: '/guide/concepts' }, { text: '模块系统', link: '/guide/module-system' },
              { text: '配置', link: '/guide/configuration' }, { text: '工具脚本', link: '/guide/tooling' },
              { text: '依赖安全与升级', link: '/guide/dependency-security' }, { text: '架构总览', link: '/guide/architecture' },
            ] },
          ],
          '/architecture/': architectureSidebar,
          '/design/': [{ text: '设计文档', items: [
            { text: '设计总览', link: '/design/' }, { text: '概念词汇表', link: '/design/concept-glossary' },
            { text: '玩家对象模型', link: '/design/player-object-model' }, { text: '会话与在线目录', link: '/design/session-and-online-directory' },
            { text: '登录流程', link: '/design/login-flow' }, { text: '网关拓扑', link: '/design/gateway-topology-survey' },
            { text: 'RoomServer 单写者', link: '/design/room-server-single-writer' }, { text: 'Lua 脚本', link: '/design/scripting-lua' },
            { text: 'SDK 契约', link: '/design/sdk-contract' }, { text: 'XML 契约生成', link: '/design/xml-generation' },
          ] }],
          '/modules/': [{ text: '模块', items: [
            { text: 'Base', link: '/modules/base' }, { text: 'Core', link: '/modules/core' },
            { text: 'Runtime', link: '/modules/runtime' }, { text: 'Data', link: '/modules/data' },
            { text: 'Net', link: '/modules/net' }, { text: 'Game', link: '/modules/game' }, { text: 'BigWorld 参考', link: '/modules/bigworld' },
          ] }],
          '/apps/': [{ text: '服务器应用', items: [{ text: '应用概览', link: '/apps/' }] }],
          '/api/': [{ text: 'API 参考', items: [
            { text: '概览', link: '/api/' }, { text: 'Base', link: '/api/base' }, { text: 'Core', link: '/api/core' },
            { text: 'Runtime', link: '/api/runtime' }, { text: 'Data', link: '/api/data' }, { text: 'Net', link: '/api/net' }, { text: 'Game', link: '/api/game' },
          ] }],
          '/sdks/': [{ text: 'SDK 文档', items: [
            { text: '概览', link: '/sdks/' }, { text: 'Unity SDK 目录结构', link: '/sdks/unity/SDK_Structure' },
            { text: 'Unity 属性同步 SDK', link: '/sdks/unity/Attribute_SDK' },
          ] }],
        },
        editLink: { pattern: 'https://github.com/cuihairu/apollo/edit/main/docs/:path', text: '在 GitHub 上编辑此页' },
        lastUpdated: { text: '最后更新', formatOptions: { dateStyle: 'short', timeStyle: 'medium' } },
        footer: { message: '基于 MIT 许可发布', copyright: 'Copyright (c) 2024-present Apollo Team' },
        search: { provider: 'local', options: { translations: { button: { buttonText: '搜索文档', buttonAriaLabel: '搜索文档' }, modal: { noResultsText: '没有找到结果', resetButtonTitle: '清除查询', footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' } } } } },
      },
    },
    en: {
      label: 'English',
      lang: 'en-US',
      link: '/en/',
      themeConfig: {
        nav: [
          { text: 'Guide', link: '/en/guide/' }, { text: 'Architecture', link: '/en/analysis/architecture-review' },
          { text: 'Modules', link: '/en/modules/' }, { text: 'Applications', link: '/en/apps/' },
          { text: 'API', link: '/en/api/' }, { text: 'SDK', link: '/en/sdks/' }, { text: 'QA', link: '/en/QA' },
        ],
        sidebar: {
          '/en/guide/': [{ text: 'Getting Started', items: [
            { text: 'Guide Overview', link: '/en/guide/' }, { text: 'Installation', link: '/en/guide/installation' },
            { text: 'Quick Start', link: '/en/guide/quick-start' }, { text: 'Tutorial', link: '/en/guide/tutorial' },
          ] }, { text: 'Fundamentals', items: [
            { text: 'Core Concepts', link: '/en/guide/concepts' }, { text: 'Modules', link: '/en/guide/module-system' },
            { text: 'Configuration', link: '/en/guide/configuration' }, { text: 'Tooling', link: '/en/guide/tooling' },
            { text: 'Dependency Security', link: '/en/guide/dependency-security' }, { text: 'Architecture', link: '/en/guide/architecture' },
          ] }],
          '/en/architecture/': [
            { text: 'Apollo Architecture', items: [
              { text: 'Overview', link: '/en/architecture/overview' }, { text: 'Active Decisions', link: '/en/architecture/adr' },
              { text: 'Remote Entity Call', link: '/en/architecture/remote-entity-call-design' },
              { text: 'Observability', link: '/en/architecture/observability-watcher-and-runtime-introspection-design' },
              { text: 'Host Builder and DI', link: '/en/architecture/host-builder-and-di-design' },
              { text: 'Room-Based GameServer', link: '/en/30-Room-Based_GameServer_Design' },
            ] },
            { text: 'External Engine References', items: [
              { text: 'BigWorld', link: '/en/architecture/bigworld' }, { text: 'BigWorld Lifecycle', link: '/en/architecture/bigworld-lifecycle' },
              { text: 'KBEngine Analysis', link: '/en/architecture/kbe-source-analysis' }, { text: 'Reference Principles', link: '/en/architecture/kbe-reference-principles' },
              { text: 'Base / Cell / Proxy', link: '/en/architecture/base-cell-proxy-model' }, { text: 'Witness and Ghost', link: '/en/architecture/witness-ghost-design' },
            ] },
          ],
          '/en/design/': [{ text: 'Design Documents', items: [
            { text: 'Overview', link: '/en/design/' }, { text: 'Glossary', link: '/en/design/concept-glossary' },
            { text: 'Player Object Model', link: '/en/design/player-object-model' }, { text: 'Sessions and Online Directory', link: '/en/design/session-and-online-directory' },
            { text: 'Login Flow', link: '/en/design/login-flow' }, { text: 'Gateway Topology', link: '/en/design/gateway-topology-survey' },
            { text: 'RoomServer Single Writer', link: '/en/design/room-server-single-writer' }, { text: 'Lua Scripting', link: '/en/design/scripting-lua' },
            { text: 'SDK Contract', link: '/en/design/sdk-contract' }, { text: 'XML Contract Generation', link: '/en/design/xml-generation' },
          ] }],
          '/en/modules/': [{ text: 'Modules', items: [
            { text: 'Base', link: '/en/modules/base' }, { text: 'Core', link: '/en/modules/core' }, { text: 'Runtime', link: '/en/modules/runtime' },
            { text: 'Data', link: '/en/modules/data' }, { text: 'Net', link: '/en/modules/net' }, { text: 'Game', link: '/en/modules/game' },
          ] }],
          '/en/apps/': [{ text: 'Server Applications', items: [{ text: 'Overview', link: '/en/apps/' }] }],
          '/en/api/': [{ text: 'API Reference', items: [
            { text: 'Overview', link: '/en/api/' }, { text: 'Base', link: '/en/api/base' }, { text: 'Core', link: '/en/api/core' },
            { text: 'Runtime', link: '/en/api/runtime' }, { text: 'Data', link: '/en/api/data' }, { text: 'Net', link: '/en/api/net' }, { text: 'Game', link: '/en/api/game' },
          ] }],
          '/en/sdks/': [{ text: 'SDK Documentation', items: [
            { text: 'Overview', link: '/en/sdks/' }, { text: 'Unity SDK Structure', link: '/en/sdks/unity/SDK_Structure' },
            { text: 'Unity Attribute Sync SDK', link: '/en/sdks/unity/Attribute_SDK' },
          ] }],
        },
        editLink: { pattern: 'https://github.com/cuihairu/apollo/edit/main/docs/:path', text: 'Edit this page on GitHub' },
        lastUpdated: { text: 'Last updated', formatOptions: { dateStyle: 'short', timeStyle: 'medium' } },
        footer: { message: 'Released under the MIT License', copyright: 'Copyright (c) 2024-present Apollo Team' },
        search: { provider: 'local', options: { translations: { button: { buttonText: 'Search', buttonAriaLabel: 'Search docs' }, modal: { noResultsText: 'No results found', resetButtonTitle: 'Clear search', footer: { selectText: 'Select', navigateText: 'Navigate', closeText: 'Close' } } } } },
      },
    },
  },

  themeConfig: {
    logo: '/apollo.png',
    socialLinks: [{ icon: 'github', link: 'https://github.com/cuihairu/apollo' }],
  },

  markdown: {
    config(md) {
      const defaultFence = md.renderer.rules.fence

      md.renderer.rules.fence = (tokens, idx, options, env, self) => {
        const token = tokens[idx]
        const language = token.info.trim().split(/\s+/)[0]

        if (language === 'mermaid') {
          return `<pre class="mermaid">${md.utils.escapeHtml(token.content)}</pre>`
        }

        return defaultFence
          ? defaultFence(tokens, idx, options, env, self)
          : self.renderToken(tokens, idx, options)
      }
    },
  },
})
