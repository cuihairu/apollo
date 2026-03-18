import { Markdown } from "vuepress/markdown";

//#region src/node/lineNumbers/options.d.ts
/**
 * Options for markdown-it line numbers plugin
 *
 * markdown-it 行号插件选项
 */
interface MarkdownItLineNumbersOptions {
  /**
   * Show line numbers in code blocks
   *
   * 在代码块中显示行号
   *
   * - If `number`, show line numbers with code block lines not less than `number`.
   * - If `true`, show line number always
   * - If `false`, do not enable line numbers globally, but you can enable it for individual code blocks using `:line-numbers`
   * - If `'disable'`, Completely disable line number
   *
   * - 如果是 `number`，当代码块行数不少于该数字时显示行号
   * - 如果是 `true`，总是显示行号
   * - 如果是 `false`，不全局启用行号，但你可以为单个代码块使用 `:line-numbers` 启用
   * - 如果是 `'disable'`，完全禁用行号
   *
   * @default 'disable'
   */
  lineNumbers?: boolean | number | 'disable';
  /**
   * Whether to remove the last line
   *
   * 是否移除最后一行
   *
   * @default false
   */
  removeLastLine?: boolean;
  /**
   * Custom resolving function that whether to enable line numbers for a single code block
   *
   * 自定义解析函数，用于决定是否为单个代码块启用行号
   *
   * @param info - Code block info string / 代码块信息字符串
   * @returns `boolean | undefined`
   *  - `boolean` - Whether to enable line numbers / 是否启用行号
   *  - `undefined` - built-in resolving / 使用内置解析
   */
  resolveLineNumbers?: (info: string) => boolean | undefined;
}
//#endregion
//#region src/node/lineNumbers/plugin.d.ts
/**
 * Add line numbers to code blocks in markdown-it
 *
 * 为 markdown-it 中的代码块添加行号
 *
 * @param md - MarkdownIt instance / MarkdownIt 实例
 *
 * @example
 * ```ts
 * import { lineNumbers } from '@vuepress/highlighter-helper'
 *
 * md.use(lineNumbers, {
 *   lineNumbers: true,
 *   removeLastLine: false
 * })
 * ```
 */
declare const lineNumbers: (md: Markdown, {
  lineNumbers: lineNumberOptions,
  removeLastLine,
  resolveLineNumbers: customResolveLineNumbers
}?: MarkdownItLineNumbersOptions) => void;
//#endregion
//#region src/node/whitespace.d.ts
/**
 * Regular expression to match `:whitespace` directive in code block info
 *
 * 匹配代码块信息中 `:whitespace` 指令的正则表达式
 */
declare const WHITESPACE_REGEXP: RegExp;
/**
 * Regular expression to match `:no-whitespace` directive in code block info
 *
 * 匹配代码块信息中 `:no-whitespace` 指令的正则表达式
 */
declare const NO_WHITESPACE_REGEXP: RegExp;
/**
 * Whitespace position types
 *
 * 空白符位置类型
 */
type WhitespacePosition = 'all' | 'boundary' | 'leading' | 'trailing';
/**
 * Resolve whitespace position from code block info and global option
 *
 * 从代码块信息和全局选项中解析空白符位置
 *
 * @param info - Code block info string / 代码块信息字符串
 * @param globalOption - Global whitespace option / 全局空白符选项
 * @returns Resolved whitespace position or false if disabled / 解析的空白符位置，如果禁用则返回 false
 *
 * @example
 * ```ts
 * resolveWhitespacePosition('js :whitespace=all', 'boundary') // 'all'
 * resolveWhitespacePosition('js :no-whitespace', 'boundary') // false
 * resolveWhitespacePosition('js', 'boundary') // 'boundary'
 * ```
 */
declare const resolveWhitespacePosition: (info: string, globalOption: WhitespacePosition | true) => WhitespacePosition | false;
//#endregion
//#region src/node/collapsedLines/options.d.ts
/**
 * Options for markdown-it collapsed lines plugin
 *
 * markdown-it 折叠行插件选项
 */
interface MarkdownItCollapsedLinesOptions {
  /**
   * Whether to collapse code blocks when they exceed a certain number of lines
   *
   * 当代码块超过一定行数时是否折叠
   *
   * - If `number`, collapse starts from line `number`.
   * - If `true`, collapse starts from line 15 by default.
   * - If `false`, do not enable code block collapsing globally, but you can enable it for individual code blocks using `:collapsed-lines`
   * - If `'disable'`, Completely disable code block collapsing
   *
   * - 如果是 `number`，从第 `number` 行开始折叠
   * - 如果是 `true`，默认从第 15 行开始折叠
   * - 如果是 `false`，不全局启用代码块折叠，但你可以为单个代码块使用 `:collapsed-lines` 启用
   * - 如果是 `'disable'`，完全禁用代码块折叠
   *
   * @default 'disable'
   */
  collapsedLines?: boolean | number | 'disable';
  /**
   * Whether to remove the last line
   *
   * 是否移除最后一行
   *
   * @default false
   */
  removeLastLine?: boolean;
}
//#endregion
//#region src/node/collapsedLines/plugin.d.ts
/**
 * Add collapsed lines functionality to code blocks in markdown-it
 *
 * 为 markdown-it 中的代码块添加折叠行功能
 *
 * @param md - MarkdownIt instance / MarkdownIt 实例
 * @param options - Plugin options / 插件选项
 *
 * @example
 * ```ts
 * import { collapsedLines } from '@vuepress/highlighter-helper'
 *
 * md.use(collapsedLines, {
 *   collapsedLines: 15,
 *   removeLastLine: false
 * })
 * ```
 */
declare const collapsedLines: (md: Markdown, {
  collapsedLines: collapsedLinesOptions,
  removeLastLine
}?: MarkdownItCollapsedLinesOptions) => void;
//#endregion
//#region src/node/codeBlockTitle/options.d.ts
/**
 * Code block title render function
 *
 * 代码块标题渲染函数
 *
 * @param title - The title to render / 要渲染的标题
 * @param code - The code block HTML / 代码块 HTML
 * @returns The rendered HTML / 渲染后的 HTML
 */
type CodeBlockTitleRender = (title: string, code: string) => string;
/**
 * Options for markdown-it code block title plugin
 *
 * markdown-it 代码块标题插件选项
 */
interface MarkdownItCodeBlockTitleOptions {
  /**
   * Whether to render the title of the code block
   *
   * 是否渲染代码块的标题
   *
   * - If `true`, enable the title render of the code block
   * - If `false`, disable the title render of the code block
   * - If `Function`, custom title render
   *
   * - 如果是 `true`，启用代码块标题渲染
   * - 如果是 `false`，禁用代码块标题渲染
   * - 如果是 `Function`，自定义标题渲染
   *
   * @default true
   */
  codeBlockTitle?: CodeBlockTitleRender | boolean;
}
//#endregion
//#region src/node/codeBlockTitle/plugin.d.ts
/**
 * Add code block title functionality to markdown-it
 *
 * 为 markdown-it 添加代码块标题功能
 *
 * @param md - MarkdownIt instance / MarkdownIt 实例
 * @param options - Plugin options / 插件选项
 *
 * @example
 * ```ts
 * import { codeBlockTitle } from '@vuepress/highlighter-helper'
 *
 * md.use(codeBlockTitle, {
 *   codeBlockTitle: true
 * })
 * ```
 */
declare const codeBlockTitle: (md: Markdown, {
  codeBlockTitle: codeBlockTitleOptions
}?: MarkdownItCodeBlockTitleOptions) => void;
//#endregion
//#region src/node/utils/resolveAttr.d.ts
/**
 * Resolve attribute value from code block info string
 *
 * 从代码块信息字符串中解析属性值
 *
 * @param info - Code block info string / 代码块信息字符串
 * @param attr - Attribute name to resolve / 要解析的属性名
 * @returns The attribute value or null if not found / 属性值，如果未找到则返回 null
 *
 * @example
 * ```ts
 * resolveAttr('js title="example.js"', 'title') // 'example.js'
 * resolveAttr('js title=\'example.js\'', 'title') // 'example.js'
 * resolveAttr('js', 'title') // null
 * ```
 */
declare const resolveAttr: (info: string, attr: string) => string | null;
//#endregion
export { CodeBlockTitleRender, MarkdownItCodeBlockTitleOptions, MarkdownItCollapsedLinesOptions, MarkdownItLineNumbersOptions, NO_WHITESPACE_REGEXP, WHITESPACE_REGEXP, WhitespacePosition, codeBlockTitle, collapsedLines, lineNumbers, resolveAttr, resolveWhitespacePosition };
//# sourceMappingURL=index.d.ts.map