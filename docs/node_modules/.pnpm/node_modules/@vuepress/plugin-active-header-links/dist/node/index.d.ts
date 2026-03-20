import { Plugin } from "vuepress/core";

//#region src/node/activeHeaderLinksPlugin.d.ts
/**
 * Options for `@vuepress/plugin-active-header-links`
 *
 * `@vuepress/plugin-active-header-links` 的选项
 */
interface ActiveHeaderLinksPluginOptions {
  /**
   * Selector of header link
   *
   * If a header anchor does not have a corresponding header link,
   * this plugin won't change the route hash to that anchor when
   * scrolling to it.
   *
   * @default 'a.sidebar-item'
   */
  headerLinkSelector?: string;
  /**
   * Selector of header anchor
   *
   * @default '.header-anchor'
   */
  headerAnchorSelector?: string;
  /**
   * The delay of the debounced scroll event listener (in millisecond)
   *
   * @default 200
   */
  delay?: number;
  /**
   * Pixel offset when a header anchor to be determined as active
   *
   * @default 5
   */
  offset?: number;
}
declare const activeHeaderLinksPlugin: ({
  headerLinkSelector,
  headerAnchorSelector,
  delay,
  offset
}?: ActiveHeaderLinksPluginOptions) => Plugin;
//#endregion
export { ActiveHeaderLinksPluginOptions, activeHeaderLinksPlugin };
//# sourceMappingURL=index.d.ts.map