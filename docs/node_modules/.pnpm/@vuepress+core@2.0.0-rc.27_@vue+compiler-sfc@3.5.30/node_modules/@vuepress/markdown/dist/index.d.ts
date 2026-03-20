import MarkdownIt, { Options, PluginWithOptions } from "markdown-it";
import { ComponentPluginOptions, componentPlugin } from "@mdit-vue/plugin-component";
import { FrontmatterPluginOptions, frontmatterPlugin } from "@mdit-vue/plugin-frontmatter";
import { HeadersPluginOptions, headersPlugin } from "@mdit-vue/plugin-headers";
import { MarkdownSfcBlocks, SfcPluginOptions, sfcPlugin } from "@mdit-vue/plugin-sfc";
import { titlePlugin } from "@mdit-vue/plugin-title";
import { TocPluginOptions, tocPlugin } from "@mdit-vue/plugin-toc";
import anchorPlugin from "markdown-it-anchor";
import { Options as Options$1, full as emojiPlugin } from "markdown-it-emoji";
import { PageFrontmatter, PageHeader } from "@vuepress/shared";
import { MarkdownItEnv } from "@mdit-vue/types";

//#region src/plugins/anchorPlugin.d.ts
type AnchorPluginOptions = anchorPlugin.AnchorOptions;
//#endregion
//#region src/plugins/assetsPlugin/assetsPlugin.d.ts
interface AssetsPluginOptions {
  /**
   * Whether to prepend base to absolute path
   */
  absolutePathPrependBase?: boolean;
}
/**
 * Plugin to handle assets links
 */
declare const assetsPlugin: PluginWithOptions<AssetsPluginOptions>;
//#endregion
//#region src/plugins/emojiPlugin.d.ts
type EmojiPluginOptions = Options$1;
//#endregion
//#region src/plugins/importCodePlugin/importCodePlugin.d.ts
interface ImportCodePluginOptions {
  /**
   * A function to handle the import path
   */
  handleImportPath?: (str: string) => string;
}
declare const importCodePlugin: PluginWithOptions<ImportCodePluginOptions>;
//#endregion
//#region src/plugins/linksPlugin/linksPlugin.d.ts
interface LinksPluginOptions {
  /**
   * Additional attributes for external links
   *
   * @default
   * ```js
   * ({
   *   target: '_blank',
   *   rel: 'noopener noreferrer',
   * })
   * ```
   */
  externalAttrs?: Record<string, string>;
  /**
   * Tag for internal links
   *
   * @default 'RouteLink'
   */
  internalTag?: 'a' | 'RouteLink' | 'RouterLink';
  /**
   * Method to check if a link is external
   *
   * @default import { isLinkExternal } from '@vuepress/shared'
   */
  isExternal?: (href: string, env: MarkdownEnv) => boolean;
}
/**
 * Process links in markdown file
 *
 * - internal links: convert `<a>` tag into internalTag
 * - external links: add extra attrs
 */
declare const linksPlugin: PluginWithOptions<LinksPluginOptions>;
//#endregion
//#region src/plugins/vPrePlugin/vPrePlugin.d.ts
interface VPrePluginOptions {
  /**
   * Add `v-pre` directive to `<pre>` tag of code block or not
   */
  block?: boolean;
  /**
   * Add `v-pre` directive to `<code>` tag of inline code or not
   */
  inline?: boolean;
}
/**
 * v-pre plugin
 */
declare const vPrePlugin: PluginWithOptions<VPrePluginOptions>;
//#endregion
//#region src/types.d.ts
type Markdown = MarkdownIt;
interface MarkdownOptions extends Options {
  anchor?: AnchorPluginOptions | false;
  assets?: AssetsPluginOptions | false;
  component?: false;
  emoji?: EmojiPluginOptions | false;
  frontmatter?: FrontmatterPluginOptions | false;
  headers?: HeadersPluginOptions | false;
  title?: false;
  importCode?: ImportCodePluginOptions | false;
  links?: LinksPluginOptions | false;
  sfc?: SfcPluginOptions | false;
  slugify?: MarkdownSlugifyFunction;
  toc?: TocPluginOptions | false;
  vPre?: VPrePluginOptions | false;
  /**
   * @deprecated This feature has been removed. Please use `@vuepress/plugin-prismjs` or `@vuepress/plugin-shiki` instead.
   */
  code?: never;
}
/**
 * Headers in markdown file
 */
type MarkdownHeader = PageHeader;
/**
 * Internal links in markdown file
 *
 * Used for file existence check
 */
interface MarkdownLink {
  raw: string;
  relative: string;
  absolute: string | null;
}
/**
 * The `env` object to be passed to markdown-it render function
 *
 * Input some meta data for markdown file parsing and rendering
 *
 * Output some resources from the markdown file
 */
interface MarkdownEnv extends MarkdownItEnv {
  /**
   * Base / publicPath of current site
   */
  base?: string;
  /**
   * Absolute file path of the markdown file
   */
  filePath?: string | null;
  /**
   * Relative file path of the markdown file
   */
  filePathRelative?: string | null;
  /**
   * Frontmatter of the markdown file
   */
  frontmatter?: PageFrontmatter;
  /**
   * Imported file that extracted by importCodePlugin
   */
  importedFiles?: string[];
  /**
   * Links that extracted by linksPlugin
   */
  links?: MarkdownLink[];
}
/**
 * Type of `slugify` function
 */
type MarkdownSlugifyFunction = (str: string) => string;
//#endregion
//#region src/markdown.d.ts
/**
 * Create vuepress customized markdown-it instance
 */
declare const createMarkdown: ({
  anchor,
  assets,
  code,
  vPre,
  component,
  emoji,
  frontmatter,
  headers,
  title,
  importCode,
  links,
  sfc,
  slugify,
  toc,
  ...markdownItOptions
}?: MarkdownOptions) => Markdown;
//#endregion
export { type AnchorPluginOptions, type AssetsPluginOptions, type ComponentPluginOptions, type EmojiPluginOptions, type FrontmatterPluginOptions, type HeadersPluginOptions, type ImportCodePluginOptions, type LinksPluginOptions, Markdown, MarkdownEnv, MarkdownHeader, MarkdownLink, MarkdownOptions, type MarkdownSfcBlocks, MarkdownSlugifyFunction, type SfcPluginOptions, type TocPluginOptions, type VPrePluginOptions, anchorPlugin, assetsPlugin, componentPlugin, createMarkdown, emojiPlugin, frontmatterPlugin, headersPlugin, importCodePlugin, linksPlugin, sfcPlugin, titlePlugin, tocPlugin, vPrePlugin };