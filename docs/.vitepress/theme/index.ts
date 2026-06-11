import DefaultTheme from 'vitepress/theme'
import { inBrowser, useRoute } from 'vitepress'
import { nextTick, watch } from 'vue'
import type { Theme } from 'vitepress'
import './custom.css'

const theme: Theme = {
  extends: DefaultTheme,
  setup() {
    const route = useRoute()

    if (!inBrowser) {
      return
    }

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
