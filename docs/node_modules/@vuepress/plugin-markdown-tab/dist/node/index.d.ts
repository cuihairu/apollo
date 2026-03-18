import { PluginSimple } from "markdown-it";
import { Plugin } from "vuepress/core";

//#region src/node/codeTabs.d.ts
/**
 * Markdown-it plugin for code tabs
 *
 * 用于代码选项卡的 Markdown-it 插件
 *
 * @param md - MarkdownIt instance / MarkdownIt 实例
 */
declare const codeTabs: PluginSimple;
//#endregion
//#region src/node/options.d.ts
/**
 * Markdown tab plugin configuration
 *
 * Markdown 选项卡插件配置
 */
interface MarkdownTabPluginOptions {
  /**
   * Whether to enable code tabs
   *
   * 是否启用代码选项卡
   *
   * @default false
   */
  codeTabs?: boolean;
  /**
   * Whether to enable tabs
   *
   * 是否启用选项卡
   *
   * @default false
   */
  tabs?: boolean;
}
//#endregion
//#region src/node/markdownTabPlugin.d.ts
declare module 'vuepress/markdown' {
  interface MarkdownOptions {
    tab?: MarkdownTabPluginOptions;
  }
}
/**
 * Markdown tab plugin
 *
 * Markdown 选项卡插件
 *
 * @example
 * ```ts
 * import { markdownTabPlugin } from '@vuepress/plugin-markdown-tab'
 *
 * export default {
 *   plugins: [
 *     markdownTabPlugin({
 *       codeTabs: true,
 *       tabs: true,
 *     }),
 *   ],
 * }
 * ```
 */
declare const markdownTabPlugin: (options: MarkdownTabPluginOptions) => Plugin;
//#endregion
//#region src/node/tabs.d.ts
/**
 * Markdown-it plugin for tabs
 *
 * 用于选项卡的 Markdown-it 插件
 *
 * @param md - MarkdownIt instance / MarkdownIt 实例
 */
declare const tabs: PluginSimple;
//#endregion
export { MarkdownTabPluginOptions, codeTabs, markdownTabPlugin, tabs };
//# sourceMappingURL=index.d.ts.map