import { App, Page } from "vuepress/core";
import { Plugin } from "vuepress";

//#region src/node/checkMarkdownLink.d.ts
/**
 * Check markdown links in a page and report broken links
 *
 * 检查页面中的 Markdown 链接并报告死链接
 *
 * @param page - The page to check / 要检查的页面
 * @param app - VuePress app instance / VuePress 应用实例
 * @param isIgnoreLink - Function to check if a link should be ignored / 检查链接是否应被忽略的函数
 *
 * @returns Whether any broken links were found / 是否发现了死链接
 */
declare const checkMarkdownLink: (page: Page, app: App, isIgnoreLink: (link: string) => boolean) => boolean;
//#endregion
//#region src/node/options.d.ts
interface LinksCheckPluginOptions {
  /**
   * Whether check dead links in markdown in devServer
   *
   * 是否在开发服务器检查 Markdown 中的死链
   *
   * @default true
   */
  dev?: boolean;
  /**
   * Whether check dead links in markdown during build
   *
   * 是否在构建时检查 Markdown 中的死链
   *
   * @description If set to 'error', the build will fail when dead links are found
   *
   * 如果设置为 'error'，则在发现死链时构建将失败
   *
   * @default true
   */
  build?: boolean | 'error';
  /**
   * Links to exclude from checking
   *
   * 检查时需要排除的链接
   */
  exclude?: (RegExp | string)[] | ((link: string, isDev: boolean) => boolean);
}
//#endregion
//#region src/node/linksCheckPlugin.d.ts
/**
 * VuePress plugin to check dead links in markdown files
 *
 * VuePress 插件，用于检查 Markdown 文件中的死链接
 *
 * @example
 * ```ts
 * import { linksCheckPlugin } from '@vuepress/plugin-links-check'
 *
 * export default {
 *   plugins: [
 *     // Basic usage
 *     linksCheckPlugin(),
 *
 *     // With options
 *     linksCheckPlugin({
 *       dev: true,
 *       build: 'error', // Fail build on dead links
 *       exclude: [
 *         /^https?:\/\/example\.com/,
 *         'mailto:example@example.com',
 *         (link, isDev) => isDev && link.startsWith('/api/')
 *       ]
 *     })
 *   ]
 * }
 * ```
 */
declare const linksCheckPlugin: ({
  dev,
  build,
  exclude: ignore
}: LinksCheckPluginOptions) => Plugin;
//#endregion
export { LinksCheckPluginOptions, checkMarkdownLink, linksCheckPlugin };
//# sourceMappingURL=index.d.ts.map