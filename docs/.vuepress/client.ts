import { defineClientConfig } from '@vuepress/client'
import type { Router } from 'vuepress'

export default defineClientConfig({
  enhance({ app, router, siteData }) {
    // 注册全局组件
    // app.component('MyComponent', MyComponent)

    // 路由守卫
    if (typeof window !== 'undefined') {
      router.beforeEach((to, from, next) => {
        // 页面跳转前的逻辑
        next()
      })

      router.afterEach((to) => {
        // 页面跳转后的逻辑
        console.log(`Navigated to: ${to.path}`)
      })
    }
  },

  setup() {
    // 客户端 setup
  },

  rootComponents: [],
})
