//#region src/client/composables/setupCollapsedLines.d.ts
/**
 * Setup collapsed lines functionality for code blocks
 *
 * 为代码块设置折叠行功能
 *
 * @param options - Setup options / 设置选项
 * @param options.selector - CSS selector for collapsed lines elements / 折叠行元素的 CSS 选择器
 *
 * @example
 * ```ts
 * import { setupCollapsedLines } from '@vuepress/highlighter-helper/client'
 *
 * // Use default selector
 * setupCollapsedLines()
 *
 * // Use custom selector
 * setupCollapsedLines({
 *   selector: '.my-collapsed-lines'
 * })
 * ```
 */
declare const setupCollapsedLines: ({
  selector
}?: {
  selector?: string;
}) => void;
//#endregion
export { setupCollapsedLines };
//# sourceMappingURL=index.d.ts.map