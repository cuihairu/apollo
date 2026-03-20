import { Plugin } from "vuepress/core";
import { LocaleConfig } from "vuepress/shared";

//#region src/shared/locales.d.ts
interface BackToTopPluginLocaleData {
  /**
   * Back to top button label text
   *
   * 返回顶部按钮标签文字
   */
  backToTop: string;
}
//#endregion
//#region src/node/options.d.ts
interface BackToTopPluginOptions {
  /**
   * Scroll threshold distance to display the back to top button (in pixels)
   *
   * 滚动距离阈值，用于显示返回顶部按钮（单位：像素）
   *
   * @default 100
   */
  threshold?: number;
  /**
   * Whether to display scroll progress
   *
   * 是否显示滚动进度
   *
   * @default true
   */
  progress?: boolean;
  /**
   * Back to top locales
   *
   * 返回顶部插件的多语言配置
   */
  locales?: LocaleConfig<BackToTopPluginLocaleData>;
}
//#endregion
//#region src/node/backToTopPlugin.d.ts
/**
 * Back to top plugin
 *
 * 返回顶部插件
 *
 * @example
 * ```ts
 * import { backToTopPlugin } from '@vuepress/plugin-back-to-top'
 *
 * export default {
 *   plugins: [backToTopPlugin()]
 * }
 * ```
 */
declare const backToTopPlugin: (options?: BackToTopPluginOptions) => Plugin;
//#endregion
export { BackToTopPluginLocaleData, BackToTopPluginOptions, backToTopPlugin };
//# sourceMappingURL=index.d.ts.map