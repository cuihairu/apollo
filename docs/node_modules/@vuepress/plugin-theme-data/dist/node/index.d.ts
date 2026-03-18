import { t as ThemeData } from "../themeData-BPGN2xlr.js";
import { App, Plugin } from "vuepress/core";

//#region src/node/prepareThemeData.d.ts
/**
 * Prepare theme data
 *
 * 准备主题数据
 *
 * @param app - VuePress app instance / VuePress 应用实例
 * @param themeData - Theme data object / 主题数据对象
 */
declare const prepareThemeData: (app: App, themeData: ThemeData) => Promise<void>;
//#endregion
//#region src/node/themeDataPlugin.d.ts
/**
 * Options of `@vuepress/plugin-theme-data`
 *
 * `@vuepress/plugin-theme-data` 的选项
 */
interface ThemeDataPluginOptions {
  /**
   * Theme data to be used in client side
   *
   * 要在客户端使用的主题数据
   */
  themeData: ThemeData;
}
/**
 * Theme data plugin
 *
 * 主题数据插件
 */
declare const themeDataPlugin: ({
  themeData
}: ThemeDataPluginOptions) => Plugin;
//#endregion
export { ThemeData, ThemeDataPluginOptions, prepareThemeData, themeDataPlugin };
//# sourceMappingURL=index.d.ts.map