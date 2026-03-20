import { t as CopyCodePluginLocaleData } from "../locales-Db__yjXP.js";
import { DefaultLocaleInfo } from "@vuepress/helper";
import { PluginFunction } from "vuepress/core";
import { LocaleConfig } from "vuepress/shared";

//#region src/node/options.d.ts
interface CopyCodePluginOptions {
  /**
   * Code block selector
   *
   * 代码块选择器
   *
   * @default '[vp-content] div[class*="language-"] pre'
   */
  selector?: string[] | string;
  /**
   * Prompt message display time
   *
   * @description setting it to `0` will disable the hint.
   *
   * 提示消息显示时间
   *
   * @description 设置为 `0` 会禁用提示。
   *
   * @default 2000
   */
  duration?: number;
  /**
   * Whether to display on the mobile side
   *
   * 是否展示在移动端
   *
   * @default false
   */
  showInMobile?: boolean;
  /**
   * When copying code, selecting to ignore nodes in the code block will result in the text content of the selected nodes not being copied.
   *
   * 复制代码时，选择忽略代码块中的节点，被选中的节点文本内容将不会被复制
   *
   * @default ""
   */
  ignoreSelector?: string[] | string;
  /**
   * Locale config
   *
   * 国际化配置
   */
  locales?: LocaleConfig<CopyCodePluginLocaleData>;
  /**
   * Whether to copy inline code content when double click.
   *
   * 是否在双击时复制内联代码内容
   *
   * @description
   * - boolean: Whether to copy inline code content when double click.
   * - string | string[]: The selector of inline code.
   *
   * - boolean: 是否在双击时复制内联代码内容
   * - string | string[]: 内联代码的选择器
   *
   * @default false
   */
  inline?: string[] | boolean | string;
}
//#endregion
//#region src/node/copyCodePlugin.d.ts
/**
 * Copy code plugin for VuePress
 *
 * VuePress 复制代码插件
 *
 * @example
 * ```ts
 * import { copyCodePlugin } from '@vuepress/plugin-copy-code'
 *
 * export default {
 *   plugins: [
 *     copyCodePlugin({
 *       // options
 *     }),
 *   ],
 * }
 * ```
 */
declare const copyCodePlugin: (options?: CopyCodePluginOptions) => PluginFunction;
//#endregion
//#region src/node/locales.d.ts
/**
 * Default locale info for `@vuepress/plugin-copy-code`
 */
declare const copyCodeLocaleInfo: DefaultLocaleInfo<CopyCodePluginLocaleData>;
//#endregion
export { CopyCodePluginLocaleData, CopyCodePluginOptions, copyCodeLocaleInfo, copyCodePlugin };
//# sourceMappingURL=index.d.ts.map