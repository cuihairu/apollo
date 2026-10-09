import { defineConfig } from 'vitepress'

const isProd = process.env.NODE_ENV === 'production'

const architectureSidebar = [
  {
    text: '语义层与定位参考件（A 档）',
    items: [
      { text: '架构总览', link: '/architecture/overview' },
      { text: '架构决策记录 ADR-001..014', link: '/architecture/adr' },
      { text: 'Remote Entity Call 语义层', link: '/architecture/remote-entity-call-design' },
      { text: '观测与运行时内省', link: '/architecture/observability-watcher-and-runtime-introspection-design' },
      { text: 'Host Builder 与 DI', link: '/architecture/host-builder-and-di-design' },
      { text: 'Starter 与模块装配', link: '/architecture/starter-and-module-assembly-design' },
      { text: 'Compact GameServer 定位', link: '/30-Compact_GameServer_Design' },
    ],
  },
  {
    text: '引擎源码分析参考（B 档）',
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
  description: '轻量 MMO 与塔防游戏服务端引擎',
  base: isProd ? '/apollo/' : '/',
  cleanUrls: true,
  ignoreDeadLinks: true,

  head: [
    ['link', { rel: 'icon', type: 'image/png', href: '/apollo.png' }],
  ],

  themeConfig: {
    logo: '/apollo.png',

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
        {
          text: '开始',
          items: [
            { text: '指南概览', link: '/guide/' },
            { text: '安装', link: '/guide/installation' },
            { text: '快速开始', link: '/guide/quick-start' },
            { text: '上手教程', link: '/guide/tutorial' },
          ],
        },
        {
          text: '基础',
          items: [
            { text: '核心概念', link: '/guide/concepts' },
            { text: '模块系统', link: '/guide/module-system' },
            { text: '配置', link: '/guide/configuration' },
            { text: '工具脚本', link: '/guide/tooling' },
            { text: '架构总览', link: '/guide/architecture' },
          ],
        },
      ],

      '/architecture/': architectureSidebar,

      '/design/': [
        {
          text: '设计件（docs/design）',
          items: [
            { text: '设计件总览', link: '/design/' },
            { text: '术语契约', link: '/design/term-contract' },
            { text: '概念词汇表', link: '/design/concept-glossary' },
            { text: '玩家对象模型', link: '/design/player-object-model' },
            { text: '属性系统与同步', link: '/design/attribute-sync' },
            { text: '会话与在线目录', link: '/design/session-and-online-directory' },
            { text: '登录流程', link: '/design/login-flow' },
            { text: '战斗确定性', link: '/design/battle-determinism' },
            { text: '战斗实例卸载', link: '/design/battle-instance-offload' },
            { text: '战斗验证服务', link: '/design/battle-verification-service' },
            { text: '网络抽象与传输内核', link: '/design/net-abstraction' },
            { text: '日志系统', link: '/design/logging' },
            { text: '崩溃采集', link: '/design/crash-capture' },
            { text: '时钟与时间', link: '/design/clock-and-time' },
            { text: '容量与基准', link: '/design/capacity-and-benchmark' },
            { text: '备份容灾与宕机接管', link: '/design/backup-revive' },
            { text: '网关拓扑调研', link: '/design/gateway-topology-survey' },
            { text: '入站第三方对接面', link: '/design/inbound-interfaces' },
            { text: '脚本系统（Lua）', link: '/design/scripting-lua' },
            { text: 'SDK 契约', link: '/design/sdk-contract' },
            { text: 'XML 契约生成', link: '/design/xml-generation' },
          ],
        },
      ],

      '/modules/': [
        {
          text: '模块',
          items: [
            { text: 'Base', link: '/modules/base' },
            { text: 'Core', link: '/modules/core' },
            { text: 'Runtime', link: '/modules/runtime' },
            { text: 'Data', link: '/modules/data' },
            { text: 'Net', link: '/modules/net' },
            { text: 'Game', link: '/modules/game' },
            { text: 'BigWorld', link: '/modules/bigworld' },
          ],
        },
      ],

      '/apps/': [
        {
          text: '服务器应用',
          items: [
            { text: '应用概览', link: '/apps/' },
            { text: 'BigWorld 服务器应用', link: '/apps/BigWorld服务器应用实现' },
          ],
        },
      ],

      '/api/': [
        {
          text: 'API 参考',
          items: [
            { text: 'API 概览', link: '/api/' },
            { text: 'Base', link: '/api/base' },
            { text: 'Core', link: '/api/core' },
            { text: 'Runtime', link: '/api/runtime' },
            { text: 'Data', link: '/api/data' },
            { text: 'Net', link: '/api/net' },
            { text: 'Game', link: '/api/game' },
          ],
        },
      ],

      '/sdks/': [
        {
          text: 'SDK 文档',
          items: [
            { text: 'SDK 概览', link: '/sdks/' },
            { text: 'Unity SDK 目录结构', link: '/sdks/unity/SDK_Structure' },
            { text: 'Unity 属性同步 SDK', link: '/sdks/unity/Attribute_SDK' },
          ],
        },
      ],
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/cuihairu/apollo' },
    ],

    editLink: {
      pattern: 'https://github.com/cuihairu/apollo/edit/main/docs/:path',
      text: '在 GitHub 上编辑此页',
    },

    lastUpdated: {
      text: '最后更新',
      formatOptions: {
        dateStyle: 'short',
        timeStyle: 'medium',
      },
    },

    footer: {
      message: '基于 MIT 许可发布',
      copyright: 'Copyright (c) 2024-present Apollo Team',
    },

    search: {
      provider: 'local',
      options: {
        translations: {
          button: {
            buttonText: '搜索文档',
            buttonAriaLabel: '搜索文档',
          },
          modal: {
            noResultsText: '没有找到结果',
            resetButtonTitle: '清除查询',
            footer: {
              selectText: '选择',
              navigateText: '切换',
              closeText: '关闭',
            },
          },
        },
      },
    },
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
