export const siteData = JSON.parse("{\"base\":\"/\",\"lang\":\"en-US\",\"title\":\"Apollo 技术文档\",\"description\":\"高性能 MMORPG 服务器框架\",\"head\":[[\"link\",{\"rel\":\"icon\",\"type\":\"image/png\",\"href\":\"/apollo.png\"}]],\"locales\":{\"/\":{\"lang\":\"en-US\",\"title\":\"Apollo 技术文档\",\"description\":\"高性能 MMORPG 服务器框架\"}}}")

if (import.meta.webpackHot) {
  import.meta.webpackHot.accept()
  __VUE_HMR_RUNTIME__.updateSiteData?.(siteData)
}

if (import.meta.hot) {
  import.meta.hot.accept((m) => {
    __VUE_HMR_RUNTIME__.updateSiteData?.(m.siteData)
  })
}
