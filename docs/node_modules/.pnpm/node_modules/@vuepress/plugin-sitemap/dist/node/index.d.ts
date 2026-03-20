import { App, Page, PageFrontmatter, PluginFunction } from "vuepress/core";

//#region src/typings/sitemap.d.ts
type SitemapChangeFrequency = 'always' | 'daily' | 'hourly' | 'monthly' | 'never' | 'weekly' | 'yearly';
/**
 * None standard sitemap options used by google
 */
interface SitemapImageOption {
  /**
   * The URL of the image.
   */
  url: string;
  /**
   * The caption of the image.
   */
  caption?: string;
  /**
   * The title of the image.
   */
  title?: string;
  /**
   * The geographic location of the image.
   */
  geoLocation?: string;
  /**
   * A URL to the license of the image.
   */
  license?: string;
}
/**
 * None standard sitemap options used by google
 */
interface SitemapVideoOption {
  /**
   * A URL pointing to the video thumbnail image file.
   */
  'thumbnail_loc': string;
  /**
   * The title of the video.
   */
  'title': string;
  /**
   * A description of the video. Maximum 2048 characters.
   */
  'description': string;
  /**
   * A URL pointing to the actual video media file. Should be one of the supported formats.
   * HTML is not a supported format. Flash is allowed, but no longer supported on most mobile platforms,
   * and so may be indexed less well. Must not be the same as the <loc> URL.
   */
  'content_loc'?: string;
  /**
   * A URL pointing to a player for a specific video. Usually this is the information
   * in the src element of an <embed> tag. Must not be the same as the <loc> URL
   */
  'player_loc'?: string;
  /**
   * A string the search engine can append as a query param to enable automatic playback
   */
  'player_loc:autoplay'?: string;
  /**
   * Duration of video in seconds
   */
  'duration'?: number;
  /**
   * The date after which the video will no longer be available.
   */
  'expiration_date'?: string;
  /**
   * The number of times the video has been viewed.
   */
  'view_count': number;
  /**
   * The date the video was first published, in W3C format.
   */
  'publication_date'?: string;
  /**
   * A short description of the broad category that the video belongs to.
   * This is a string no longer than 256 characters.
   */
  'category'?: string;
  /**
   * Whether to show or hide your video in search results from specific countries.
   */
  'restriction'?: string;
  'restriction:relationship'?: string;
  /**
   * The price to download or view the video. Omit this tag for free videos.
   */
  'price'?: string;
  /**
   * Specifies the resolution of the purchased version. Supported values are hd and sd.
   */
  'price:resolution'?: string;
  /**
   * Specifies the currency in ISO 4217 format.
   */
  'price:currency'?: string;
  /**
   * Specifies the purchase option. Supported values are rent and own.
   */
  'price:type'?: string;
  /**
   * The video uploader's name. Only one video:uploader is allowed per video.
   * max 255 characters.
   */
  'uploader'?: string;
  /**
   * Whether to show or hide your video in search results on specified platform types.
   * This is a list of space-delimited platform types.
   *
   * @see https://support.google.com/webmasters/answer/80471?hl=en&ref_topic=4581190
   */
  'platform'?: string;
  'platform:relationship'?: 'Allow' | 'Deny';
  'id'?: string;
  /**
   * An arbitrary string tag describing the video.
   * Tags are generally very short descriptions of key concepts associated with a video or piece of content.
   */
  'tag'?: string[];
  /**
   * The rating of the video. Must be float numbers
   */
  'rating'?: number;
  'family_friendly'?: 'NO' | 'YES';
  /**
   * Indicates whether a subscription (either paid or free) is required to view the video.
   */
  'requires_subscription'?: 'NO' | 'YES';
  /**
   * Indicates whether the video is a live stream.
   */
  'live'?: 'NO' | 'YES';
}
/**
 * None standard sitemap options used by google
 */
interface SitemapLinkOption {
  lang: string;
  url: string;
}
/**
 * None standard sitemap options used by google
 */
interface SitemapNewsOption {
  access: 'Registration' | 'Subscription';
  publication: {
    /**
     * Name of the news publication.
     *
     * It must exactly match the name as it appears on your articles on news.google.com,
     * except for anything in parentheses.
     */
    name: string;
    /**
     * Language of your publication. Use an ISO 639 language code (2 or 3 letters).
     */
    language: string;
    genres?: string;
    /**
     * Article publication date in W3C format,
     * using either the "complete date" (YYYY-MM-DD) format or the "complete date plus hours, minutes, and seconds"
     */
    publication_date: string;
    /**
     * The title of the news article.
     */
    title: string;
    keywords?: string;
    stock_tickers?: string;
  };
}
//#endregion
//#region src/typings/frontmatter.d.ts
interface SitemapFrontmatterOption {
  /**
   * Update Frequency
   *
   * 更新频率
   */
  changefreq?: SitemapChangeFrequency;
  /**
   * Priority of this page, supports 0.0 -1.0
   *
   * 页面优先级，0.0 - 1.0
   */
  priority?: number;
  /**
   * Image config
   *
   * 图片配置
   */
  img?: SitemapImageOption[];
  /**
   * Video config
   *
   * 视频配置
   */
  video?: SitemapVideoOption[];
  /**
   * News config
   *
   * 新闻配置
   */
  news?: SitemapNewsOption[];
}
interface SitemapPluginFrontmatter extends PageFrontmatter {
  sitemap?: SitemapFrontmatterOption | false;
}
//#endregion
//#region ../../development/plugin-git/dist/index-L8w9V1KM.d.ts
/**
 * Co-author information
 *
 * 合作作者信息
 */
interface CoAuthorInfo {
  /**
   * Co-author name
   *
   * 协同作者名称
   */
  name: string;
  /**
   * Co-author email
   *
   * 协同作者邮箱
   */
  email: string;
}
/**
 * Git contributor information
 *
 * Git 贡献者信息
 */
interface GitContributorInfo {
  /**
   * Contributor display name
   *
   * 贡献者显示名称
   */
  name: string;
  /**
   * Contributor email
   *
   * 贡献者邮箱
   */
  email: string;
  /**
   * Contributor username on the git hosting service
   *
   * 贡献者在 git 托管服务中的用户名
   */
  username: string;
  /**
   * Number of commits
   *
   * 提交次数
   */
  commits: number;
  /**
   * Contributor avatar
   *
   * 贡献者头像
   */
  avatar?: string;
  /**
   * The url of the contributor
   *
   * 贡献者访问地址
   */
  url?: string;
}
/**
 * Git changelog information
 *
 * Git 变更日志信息
 */
interface GitChangelogInfo {
  /**
   * Commit hash
   *
   * 提交哈希
   */
  hash: string;
  /**
   * Unix timestamp in milliseconds
   *
   * Unix 时间戳，单位毫秒
   */
  time: number;
  /**
   * Commit message
   *
   * 提交信息
   */
  message: string;
  /**
   * The url of the commit
   *
   * 提交访问地址
   */
  commitUrl?: string;
  /**
   * release tag
   *
   * 发布标签
   */
  tag?: string;
  /**
   * The url of the release tag
   *
   * 标签访问地址
   */
  tagUrl?: string;
  /**
   * Commit author name
   *
   * 提交作者名称
   */
  author: string;
  /**
   * Commit author email
   *
   * 提交作者邮箱
   */
  email: string;
  /**
   * The co-authors of the commit
   *
   * 提交协同作者列表
   */
  coAuthors?: CoAuthorInfo[];
}
/**
 * Git plugin frontmatter
 *
 * Git 插件 frontmatter
 */
/**
 * Git data
 *
 * Git 数据
 */
interface GitData {
  /**
   * Unix timestamp in milliseconds of the first commit
   */
  createdTime?: number;
  /**
   * Unix timestamp in milliseconds of the last commit
   */
  updatedTime?: number;
  /**
   * Contributors of all commits
   */
  contributors?: GitContributorInfo[];
  /**
   * Changelog of a page
   */
  changelog?: GitChangelogInfo[];
}
/**
 * Git locale data
 *
 * Git 多语言数据
 */
//#endregion
//#region src/typings/options.d.ts
type PageModifyTimeGetter = <ExtraPageData extends Record<string, unknown> & {
  git?: GitData;
} = {
  git?: GitData;
}, ExtraPageFrontmatter extends Record<string, unknown> = Record<never, never>, ExtraPageFields extends Record<string, unknown> = Record<never, never>>(page: Page<ExtraPageData, ExtraPageFrontmatter, ExtraPageFields>, app: App) => string;
interface SitemapPluginOptions {
  /**
   * domain which to be deployed to
   *
   * 部署的网站域名
   */
  hostname: string;
  /**
   * Extra urls to be included
   *
   * 需要额外包含的网址
   */
  extraUrls?: string[];
  /**
   * Page paths to be excluded
   *
   * 不被收录的页面路径
   */
  excludePaths?: string[];
  /**
   * Output filename, relative to dest folder
   *
   * 输出的文件名，相对于输出目录
   *
   * @default "sitemap.xml"
   */
  sitemapFilename?: string;
  /**
   * Output xsl filename, relative to dest folder
   *
   * 输出的 xsl 文件名，相对于输出目录
   *
   * @default "sitemap.xsl"
   */
  sitemapXSLFilename?: string;
  /**
   * XSL file content used as template
   *
   * 用作模板的 XSL 文件内容
   *
   * @default Content of "@vuepress/plugin-sitemap/templates/sitemap.xsl"
   */
  sitemapXSLTemplate?: string;
  /**
   * Page default update frequency
   *
   * 页面默认更新频率
   *
   * @default "daily"
   */
  changefreq?: SitemapChangeFrequency;
  /**
   * Date format function
   *
   * 时间格式化器
   */
  modifyTimeGetter?: PageModifyTimeGetter;
  /**
   * Whether enabled in devServer
   *
   * @description For performance reasons, we do not provide hot reload. Reboot your devServer to sync your changes.
   *
   * 是否在开发服务器中启用
   *
   * @description 由于性能原因，我们不提供热更新。重启开发服务器以同步你的变更。
   *
   * @default false
   */
  devServer?: boolean;
  /**
   * Hostname to use in devServer
   *
   * 开发服务器使用的主机名
   *
   * @default 'http://localhost:${port}'
   */
  devHostname?: string;
  /**
   * XML namespaces to turn on - all by default
   */
  xmlNameSpace?: {
    news: boolean;
    video: boolean;
    xhtml: boolean;
    image: boolean;
    custom?: string[];
  };
}
//#endregion
//#region src/node/sitemapPlugin.d.ts
declare const sitemapPlugin: (options: SitemapPluginOptions) => PluginFunction;
//#endregion
export { PageModifyTimeGetter, SitemapChangeFrequency, SitemapFrontmatterOption, SitemapImageOption, SitemapLinkOption, SitemapNewsOption, SitemapPluginFrontmatter, SitemapPluginOptions, SitemapVideoOption, sitemapPlugin };
//# sourceMappingURL=index.d.ts.map