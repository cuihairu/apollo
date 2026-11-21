import DefaultTheme from 'vitepress/theme'
import { inBrowser, useRoute, useData } from 'vitepress'
import { nextTick, watch } from 'vue'
import type { Theme } from 'vitepress'
import './custom.css'
import ArchitectureDiagram from './components/ArchitectureDiagram.vue'

const theme: Theme = {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('ArchitectureDiagram', ArchitectureDiagram)
  },
  setup() {
    const route = useRoute()

    if (!inBrowser) {
      return
    }

    const { lang } = useData()
    // Remember an explicit in-app locale switch so the pre-hydration
    // auto-switch (config.mts head script) never bounces the visitor again.
    watch(lang, (value) => {
      try {
        localStorage.setItem('apollo-docs-lang', value.startsWith('en') ? 'en' : 'zh')
      }
      catch {
        /* storage unavailable: the next visit re-runs the auto-switch */
      }
    })

    const renderMermaid = async () => {
      const { default: mermaid } = await import('mermaid')
      mermaid.initialize({
        startOnLoad: false,
        securityLevel: 'loose',
        theme: document.documentElement.classList.contains('dark') ? 'dark' : 'default',
      })
      await mermaid.run({ querySelector: '.mermaid' })
    }

    watch(
      () => route.path,
      () => {
        nextTick(() => {
          void renderMermaid()
        })
      },
      { immediate: true },
    )
  },
}

export default theme
