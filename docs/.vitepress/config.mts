import { defineConfig } from 'vitepress'

const isProd = process.env.NODE_ENV === 'production'

const architectureSidebar = [
  {
    text: '架构入口',
    items: [
      { text: '架构概述', link: '/architecture/overview' },
      { text: '轻量 MMO 与塔防适配判断', link: '/architecture/lightweight-mmo-and-tower-defense-fit' },
      { text: 'Apollo 最终蓝图', link: '/architecture/apollo-final-blueprint' },
      { text: '渐进式游戏框架', link: '/architecture/apollo-progressive-game-framework' },
      { text: '分层设计', link: '/architecture/apollo-layering-design' },
    ],
  },
  {
    text: '目标 Profile',
    items: [
      { text: 'MMO Topology 范围与组合', link: '/architecture/mmo-topology-scope-and-composition-design' },
      { text: 'Shard / Zone / Instance / Match', link: '/architecture/shard-zone-instance-match-topology-design' },
      { text: 'Compact GameServer', link: '/30-Compact_GameServer_Design' },
      { text: 'MMO 组件装配目录', link: '/architecture/mmo-component-assembly-catalog' },
      { text: 'Standard MMO 任务清单', link: '/architecture/standard-mmo-task-checklist' },
      { text: 'MMO 模块落地清单', link: '/architecture/mmo-module-rollout-plan' },
      { text: 'MMO 代码任务映射', link: '/architecture/mmo-code-task-mapping' },
    ],
  },
  {
    text: '核心链路',
    items: [
      { text: 'Gateway 会话', link: '/architecture/gateway-session-design' },
      { text: 'PlayerAnchor', link: '/architecture/player-anchor-design' },
      { text: '玩家在线主链', link: '/architecture/player-online-flow' },
      { text: 'WorldHost', link: '/architecture/world-host-design' },
      { text: 'World 进入与切图', link: '/architecture/world-entry-transfer-design' },
      { text: 'AOI Pipeline', link: '/architecture/interest-management-and-aoi-pipeline-design' },
      { text: 'Combat Runtime 与 ECS 边界', link: '/architecture/combat-runtime-and-ecs-boundary-design' },
    ],
  },
  {
    text: '可选大世界增强',
    items: [
      { text: 'Distributed Space', link: '/architecture/distributed-space-design' },
      { text: 'Space Partition', link: '/architecture/space-partition-topology-design' },
      { text: 'Witness 与 Ghost', link: '/architecture/witness-ghost-design' },
      { text: 'Authority Transfer', link: '/architecture/authority-transfer-design' },
      { text: 'BigWorld 架构', link: '/architecture/bigworld' },
      { text: 'BigWorld 生命周期', link: '/architecture/bigworld-lifecycle' },
    ],
  },
  {
    text: '工程治理',
    items: [
      { text: 'Starter 与模块装配', link: '/architecture/starter-and-module-assembly-design' },
      { text: '模块重组设计', link: '/architecture/module-reorganization-design' },
      { text: 'App Bootstrap 生命周期', link: '/architecture/app-bootstrap-lifecycle-design' },
      { text: 'Host Builder 与 DI', link: '/architecture/host-builder-and-di-design' },
      { text: 'Testing 与 Verification', link: '/architecture/testing-and-verification-strategy' },
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
      { text: '架构判断', link: '/architecture/lightweight-mmo-and-tower-defense-fit' },
      { text: '架构', link: '/architecture/overview' },
      { text: '模块', link: '/modules/' },
      { text: '应用', link: '/apps/' },
      { text: 'API', link: '/api/' },
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
          ],
        },
        {
          text: '基础',
          items: [
            { text: '核心概念', link: '/guide/concepts' },
            { text: '模块系统', link: '/guide/module-system' },
            { text: '配置', link: '/guide/configuration' },
          ],
        },
      ],

      '/architecture/': architectureSidebar,

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
