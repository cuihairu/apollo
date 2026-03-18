const isProd = process.env.NODE_ENV === 'production'

module.exports = {
  title: 'Apollo 技术文档',
  description: '高性能 MMORPG 服务器框架',

  // GitHub Pages base URL (修改为你的仓库名)
  base: isProd ? '/apollo/' : '/',

  // 端口配置
  port: 8080,
  host: '0.0.0.0',

  // 主题配置
  theme: '@vuepress/theme-default',

  themeConfig: {
    logo: '/logo.png',

    // 导航栏
    navbar: [
      {
        text: '指南',
        link: '/guide/',
      },
      {
        text: '架构',
        children: [
          '/architecture/overview.md',
          '/architecture/bigworld.md',
          '/architecture/bigworld-lifecycle.md',
          '/architecture/aoi.md',
          '/architecture/aoi-broadcast.md',
        ]
      },
      {
        text: '模块',
        children: [
          '/modules/base.md',
          '/modules/core.md',
          '/modules/runtime.md',
          '/modules/data.md',
          '/modules/net.md',
          '/modules/actor.md',
          '/modules/game.md',
          '/modules/bigworld.md',
        ]
      },
      {
        text: 'API 参考',
        link: '/api/',
      },
      {
        text: 'GitHub',
        link: 'https://github.com/your-org/apollo',
      },
    ],

    // 侧边栏
    sidebar: {
      '/guide/': [
        {
          text: '开始',
          children: [
            '/guide/README.md',
            '/guide/installation.md',
            '/guide/quick-start.md',
          ],
        },
        {
          text: '基础',
          children: [
            '/guide/concepts.md',
            '/guide/module-system.md',
            '/guide/configuration.md',
          ],
        },
      ],

      '/architecture/': [
        {
          text: '架构设计',
          children: [
            '/architecture/overview.md',
            '/architecture/bigworld.md',
            '/architecture/bigworld-lifecycle.md',
          ],
        },
        {
          text: 'AOI 系统',
          children: [
            '/architecture/aoi.md',
            '/architecture/aoi-broadcast.md',
          ],
        },
      ],

      '/modules/': [
        '/modules/base.md',
        '/modules/core.md',
        '/modules/runtime.md',
        '/modules/data.md',
        '/modules/net.md',
        '/modules/actor.md',
        '/modules/game.md',
        '/modules/bigworld.md',
      ],

      '/api/': [
        '/api/README.md',
        '/api/base.md',
        '/api/core.md',
        '/api/runtime.md',
      ],
    },

    // 社交链接
    socialLinks: [
      { icon: 'github', link: 'https://github.com/cuijw/apollo' },
    ],

    // 仓库配置 (用于编辑链接和贡献者)
    repo: 'cuijw/apollo',

    // 编辑链接
    editLink: true,
    editLinkText: '在 GitHub 上编辑此页',
    editLinkPattern: ':repo/edit/:branch/docs/:path',

    // 最后更新时间
    lastUpdated: true,
    lastUpdatedText: '最后更新',

    // 贡献者
    contributors: true,
    contributorsText: '贡献者',

    // 页脚
    footer: {
      message: '基于 MIT 许可发布',
      copyright: 'Copyright © 2024-present Apollo Team',
    },

    // 搜索
    search: true,
    searchMaxSuggestions: 10,
    searchPlaceholder: '搜索文档...',
  },

  // Markdown 配置
  markdown: {
    importCode: {
      handleImportPath: (str) => str.replace(/^@/, '/path/to/src'),
    },
  },

  // 插件
  plugins: [
    '@vuepress/plugin-search',
    '@vuepress/plugin-medium-zoom',
    ['@vuepress/plugin-git', {
      createdTime: true,
      updatedTime: true,
      contributors: true,
    }],
  ],

  // 构建配置
  bundler: {
    title: 'Apollo 技术文档',
    description: '高性能 MMORPG 服务器框架',
  },
}
