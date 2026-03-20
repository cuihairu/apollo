import{getDirname as e,path as t}from"vuepress/utils";const n=async(e,t)=>{let n=`\
export const themeData = JSON.parse(${JSON.stringify(JSON.stringify(t))})
`;e.env.isDev&&(n+=`
if (import.meta.webpackHot) {
  import.meta.webpackHot.accept()
  if (__VUE_HMR_RUNTIME__.updateThemeData) {
    __VUE_HMR_RUNTIME__.updateThemeData(themeData)
  }
}

if (import.meta.hot) {
  import.meta.hot.accept(({ themeData }) => {
    __VUE_HMR_RUNTIME__.updateThemeData(themeData)
  })
}
`),await e.writeTemp(`internal/themeData.js`,n)},r=import.meta.dirname||e(import.meta.url),i=({themeData:e})=>({name:`@vuepress/plugin-theme-data`,clientConfigFile:t.resolve(r,`../client/config.js`),onPrepared:t=>n(t,e)});export{n as prepareThemeData,i as themeDataPlugin};
//# sourceMappingURL=index.js.map