import { ExactLocaleConfig } from "@vuepress/helper";
import { LocaleConfig } from "vuepress/shared";
import { PluginWithOptions } from "markdown-it";
import { Plugin } from "vuepress/core";

//#region src/node/options.d.ts
interface MarkdownHintPluginLocaleData {
  /**
   * Default title text for important block
   *
   * 重要块的默认标题
   */
  important: string;
  /**
   * Default title text for note block
   *
   * 注释块的默认标题
   */
  note: string;
  /**
   * Default title text for tip block
   *
   * 提示块的默认标题
   */
  tip: string;
  /**
   * Default title text for warning block
   *
   * 注意块的默认标题
   */
  warning: string;
  /**
   * Default title text for caution block
   *
   * 警告块的默认标题
   */
  caution: string;
  /**
   * Default title text for info block
   *
   * 信息块的默认标题
   */
  info: string;
  /**
   * Default title text for details block
   *
   * 详情块的默认标题
   */
  details: string;
}
type MarkdownHintPluginLocaleConfig = ExactLocaleConfig<MarkdownHintPluginLocaleData>;
/**
 * Markdown hint plugin configuration
 *
 * Markdown 提示插件配置
 */
interface MarkdownHintPluginOptions {
  /**
   * Whether to enable GFM alerts
   *
   * 是否启用 GFM 警告
   *
   * @default false
   */
  alert?: boolean;
  /**
   * Whether to enable hint containers including important, info, note, tip, warning, caution, details
   *
   * 是否启用提示容器，包括 important、info、note、tip、warning、caution、details
   *
   * @default true
   */
  hint?: boolean;
  /**
   * Whether to inject default styles
   *
   * 是否注入默认样式
   *
   * @default true
   */
  injectStyles?: boolean;
  /**
   * Locale config
   *
   * 国际化配置选项
   */
  locales?: LocaleConfig<MarkdownHintPluginLocaleData>;
}
//#endregion
//#region src/node/alert.d.ts
type MarkdownItAlertOptions = ExactLocaleConfig<MarkdownHintPluginLocaleData>;
/**
 * GFM alert markdown-it plugin
 *
 * GFM 警告 markdown-it 插件
 *
 * @param md - MarkdownIt instance / MarkdownIt 实例
 * @param options - plugin options / 插件选项
 */
declare const alert: PluginWithOptions<MarkdownItAlertOptions>;
//#endregion
//#region src/node/hint.d.ts
type MarkdownItHintOptions = ExactLocaleConfig<MarkdownHintPluginLocaleData>;
type MarkdownHintContainerName = keyof MarkdownHintPluginLocaleData;
/**
 * Hint container markdown-it plugin
 *
 * 提示容器 markdown-it 插件
 *
 * @param md - MarkdownIt instance / MarkdownIt 实例
 * @param options - plugin options / 插件选项
 */
declare const hint: PluginWithOptions<MarkdownItHintOptions>;
//#endregion
//#region src/node/markdownHintPlugin.d.ts
declare module 'vuepress/markdown' {
  interface MarkdownOptions {
    hint?: MarkdownHintPluginOptions;
  }
}
/**
 * Markdown hint plugin
 *
 * Markdown 提示插件
 *
 * @example
 * ```ts
 * import { markdownHintPlugin } from '@vuepress/plugin-markdown-hint'
 *
 * export default {
 *   plugins: [
 *     markdownHintPlugin({
 *       hint: true,
 *       alert: true,
 *     }),
 *   ],
 * }
 * ```
 */
declare const markdownHintPlugin: (options: MarkdownHintPluginOptions) => Plugin;
//#endregion
export { MarkdownHintContainerName, MarkdownHintPluginLocaleConfig, MarkdownHintPluginLocaleData, MarkdownHintPluginOptions, MarkdownItAlertOptions, MarkdownItHintOptions, alert, hint, markdownHintPlugin };
//# sourceMappingURL=index.d.ts.map