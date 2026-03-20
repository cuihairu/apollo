import { a as GitInjectOptions, c as GitPluginPageData, i as GitData, l as GitUrlPattern, n as GitChangelogInfo, o as GitLocaleData, r as GitContributorInfo, s as GitPluginFrontmatter, t as CoAuthorInfo, u as KnownGitProvider } from "../index-L8w9V1KM.js";
import { Logger } from "@vuepress/helper";
import { Plugin } from "vuepress/core";
import { App, LocaleConfig, Page } from "vuepress";

//#region src/node/options.d.ts
/**
 * Contributor information
 *
 * 贡献者信息
 */
interface ContributorInfo {
  /**
   * Contributor's username on the git hosting service
   *
   * 贡献者在 Git 托管服务中的用户名
   */
  username: string;
  /**
   * Contributor name displayed on the page, default is `username`
   *
   * 贡献者显示在页面上的名字， 默认为 `username`
   */
  name?: string;
  /**
   * The alias of the contributor,
   * Since contributors may have different usernames saved in their local git configuration
   * compared to their usernames on the hosting service, In this case, aliases can be used to
   * map to the actual usernames.
   *
   * 贡献者别名， 由于贡献者可能在本地 git 配置中保存的 用户名与 托管服务 用户名不一致，
   * 这时候可以通过别名映射到真实的用户名
   */
  alias?: string[] | string;
  /**
   * The primary email of the contributor
   *
   * 贡献者在 Git 托管服务中的主邮箱
   */
  email?: string;
  /**
   * The alternative emails of the contributor on the Git hosting service,
   * or emails they have used in the past.
   *
   * 贡献者在 Git 托管服务中的备用邮箱，或者曾经使用过的邮箱
   */
  emailAlias?: string[] | string;
  /**
   * The avatar url of the contributor.
   *
   * If the git hosting service is `github`, it can be ignored and left blank, as the plugin will automatically fill it in.
   *
   * 贡献者头像地址
   *
   * 如果 git 托管服务为 `github`，则可以忽略不填，由插件自动填充
   */
  avatar?: string;
  /**
   * The url of the contributor
   *
   * If the git hosting service is `github`, it can be ignored and left blank, as the plugin will automatically fill it in.
   *
   * 贡献者访问地址
   *
   * 如果 git 托管服务为 `github`，则可以忽略不填，由插件自动填充
   */
  url?: string;
}
/**
 * Options for contributors functionality
 *
 * 贡献者功能配置项
 */
interface ContributorsOptions {
  /**
   * Contributors Information
   *
   * 贡献者信息
   */
  info?: ContributorInfo[];
  /**
   * Whether to add avatar in contributor information
   *
   * 是否在贡献者信息中添加头像
   *
   * @default false
   */
  avatar?: boolean;
  /**
   * Avatar url pattern
   * - `:username` - Contributor's username
   *
   * 头像访问地址模式
   * - `:username` - 贡献者的用户名
   *
   * @example 'https://github.com/:username'
   */
  avatarPattern?: string;
  /**
   * Functions to transform contributors, e.g. remove duplicates ones and sort them
   *
   * 贡献者转换函数，例如去重和排序
   */
  transform?: (contributors: GitContributorInfo[]) => GitContributorInfo[];
}
/**
 * Options for changelog functionality
 *
 * 变更历史记录功能配置项
 */
interface ChangelogOptions {
  /**
   * Maximum number of changelog
   *
   * 最大变更记录条数
   */
  maxCount?: number;
  /**
   * The url of the git repository, e.g: https://github.com/vuepress/ecosystem
   *
   * git 仓库的访问地址，例如：https://github.com/vuepress/ecosystem
   */
  repoUrl?: string;
  /**
   * Commit url pattern
   *
   * - `:repo` - The url of the git repository
   * - `:hash` - Hash of the commit record
   *
   * 提交记录访问地址模式
   *
   * - `:repo` - git 仓库的访问地址
   * - `:hash` - 提交记录的 hash
   *
   * @default ':repo/commit/:hash'
   */
  commitUrlPattern?: string;
  /**
   * Issue url pattern
   *
   * - `:repo` - The url of the git repository
   * - `:issue` - Id of the issue
   *
   * issue 访问地址模式
   *
   * - `:repo` - git 仓库的访问地址
   * - `:issue` - issue 的 id
   *
   * @default ':repo/issues/:issue'
   */
  issueUrlPattern?: string;
  /**
   * Tag url pattern
   *
   * - `:repo` - The url of the git repository
   * - `:tag` - Name of the tag
   *
   * tag 访问地址模式,
   * 默认值：':repo/releases/tag/:tag'
   *
   * - `:repo` - git 仓库的访问地址
   * - `:tag` - tag 的名称
   *
   * @default ':repo/releases/tag/:tag'
   */
  tagUrlPattern?: string;
}
/**
 * Options for `@vuepress/plugin-git`
 *
 * `@vuepress/plugin-git` 插件配置项
 */
interface GitPluginOptions {
  /**
   * Page filter, if it returns `true`, the page will collect git information.
   *
   * 页面过滤器，如果返回 `true` ，该页面将收集 git 信息
   */
  filter?: (page: Page) => boolean;
  /**
   * Whether to get the created time of a page
   *
   * 是否收集页面创建时间
   *
   * @default true
   */
  createdTime?: boolean;
  /**
   * Whether to get the updated time of a page
   *
   * 是否收集页面更新时间
   *
   * @default true
   */
  updatedTime?: boolean;
  /**
   * Whether to get the contributors of a page
   *
   * 是否收集页面的贡献者
   *
   * @default true
   */
  contributors?: ContributorsOptions | boolean;
  /**
   * Whether to get the changelog of a page
   *
   * 是否收集页面的变更历史记录
   *
   * @default false
   */
  changelog?: ChangelogOptions | boolean;
  /**
   * @deprecated use `contributors.transform` instead
   * Functions to transform contributors, e.g. remove duplicates ones and sort them
   */
  transformContributors?: (contributors: GitContributorInfo[]) => GitContributorInfo[];
  /**
   * Localization config
   *
   * 本地化配置
   */
  locales?: LocaleConfig<GitLocaleData>;
}
//#endregion
//#region src/node/gitPlugin.d.ts
/**
 * Git plugin
 *
 * Git 插件
 *
 * @example
 * ```ts
 * import { gitPlugin } from '@vuepress/plugin-git'
 *
 * export default {
 *   plugins: [
 *     gitPlugin({
 *       createdTime: true,
 *       updatedTime: true,
 *       contributors: true,
 *       changelog: false
 *     })
 *   ]
 * }
 * ```
 */
declare const gitPlugin: ({
  createdTime,
  updatedTime,
  contributors,
  changelog,
  filter,
  transformContributors,
  locales
}?: GitPluginOptions) => Plugin;
//#endregion
//#region src/node/typings.d.ts
interface RawCommit {
  /**
   * File path
   */
  filepath: string;
  /**
   * Commit hash
   */
  hash: string;
  /**
   * Unix timestamp in milliseconds
   */
  time: number;
  /**
   * Commit message
   */
  message: string;
  /**
   * Commit message body
   */
  body: string;
  /**
   * Commit refs
   */
  refs: string;
  /**
   * Commit author name
   */
  author: string;
  /**
   * Commit author email
   */
  email: string;
  /**
   * The co-authors of the commit
   */
  coAuthors: CoAuthorInfo[];
}
interface MergedRawCommit extends Omit<RawCommit, 'filepath'> {
  filepaths: string[];
}
//#endregion
//#region src/node/resolveChangelog.d.ts
/**
 * Resolve changelog
 *
 * 解析变更日志
 *
 * @param app - VuePress app instance / VuePress 应用实例
 * @param commits - Git commits / Git 提交记录
 * @param options - Changelog options / 变更日志选项
 * @param contributors - Contributor info / 贡献者信息
 * @returns Resolved changelog info / 解析后的变更日志信息
 */
declare const resolveChangelog: (app: App, commits: MergedRawCommit[], options: ChangelogOptions, contributors: ContributorInfo[]) => GitChangelogInfo[];
//#endregion
//#region src/node/resolveContributors.d.ts
/**
 * Get raw contributors from commits
 *
 * 从提交记录中获取原始贡献者信息
 *
 * @param commits - Git commits / Git 提交记录
 * @param options - Contributors options / 贡献者选项
 * @param gitProvider - Git provider / Git 提供商
 *
 * @returns Raw contributors / 原始贡献者信息
 */
declare const getRawContributors: (commits: MergedRawCommit[], options: ContributorsOptions, gitProvider: KnownGitProvider | null) => GitContributorInfo[];
/**
 * Resolve contributors
 *
 * 解析贡献者
 *
 * @param commits - Git commits / Git 提交记录
 * @param gitProvider - Git provider / Git 提供商
 * @param options - Contributors options / 贡献者选项
 * @param extraContributors - Extra contributors / 额外贡献者
 *
 * @returns Resolved contributors / 解析后的贡献者
 */
declare const resolveContributors: (commits: MergedRawCommit[], gitProvider: KnownGitProvider | null, options: ContributorsOptions, extraContributors?: string[]) => GitContributorInfo[];
//#endregion
//#region src/node/utils/checkGithubUsername.d.ts
/**
 * Check if the username is a valid github username
 *
 * - length <= 39
 * - starts with a letter
 * - contains only letters, numbers, and hyphens
 * - does not start or end with a hyphen
 * - does not contain consecutive hyphens
 *
 * @param username - The username to check
 * @returns Whether the username is a valid github username
 */
declare const checkGithubUsername: (username: string) => boolean;
//#endregion
//#region src/node/utils/checkGitRepo.d.ts
/**
 * Check if the directory is likely a valid Git repository root or subdirectory
 *
 * 检查目录是否为有效的 Git 仓库根目录或子目录
 *
 * @param cwd The directory to check / 要检查的目录
 * @returns True if it's a Git repository, false otherwise / 如果是 Git 仓库则返回 true，否则返回 false
 */
declare const checkGitRepo: (cwd: string) => boolean;
//#endregion
//#region src/node/utils/digestSHA256.d.ts
declare const digestSHA256: (message: string) => string;
//#endregion
//#region src/node/utils/getCommits.d.ts
/**
 * Get raw commits for a specific file
 *
 * ${commit_hash} ${author_name} ${author_email} ${author_date} ${subject} ${ref} ${body}
 *
 * @see {@link https://git-scm.com/docs/pretty-formats | documentation} for details.
 *
 * @param filepath - The file path to get commits for / 要获取提交记录的文件路径
 * @param cwd - The working directory to run git command / 运行 git 命令的工作目录
 * @param options - Git plugin options / Git 插件选项
 *
 * @returns Raw commits for the specified file / 指定文件的原始提交记录
 */
declare const getRawCommits: (filepath: string, cwd: string, options: GitPluginOptions) => Promise<RawCommit[]>;
/**
 * Merge raw commits by hash
 *
 * 按哈希值合并原始提交记录
 *
 * @param commits - Raw commits / 原始提交记录
 * @returns Merged raw commits / 合并后的原始提交记录
 */
declare const mergeRawCommits: (commits: RawCommit[]) => MergedRawCommit[];
/**
 * Get merged commits for multiple file paths
 *
 * 获取多个文件路径的合并提交记录
 *
 * @param filepaths - File paths to get commits for / 要获取提交记录的文件路径
 * @param cwd - Working directory / 工作目录
 * @param options - Git plugin options / Git 插件选项
 * @returns Merged commits for the specified file paths / 指定文件路径的合并提交记录
 */
declare const getCommits: (filepaths: string[], cwd: string, options: GitPluginOptions) => Promise<MergedRawCommit[]>;
//#endregion
//#region src/node/utils/getContributorInfo.d.ts
declare const getContributorInfo: (contributor: {
  name: string;
  email?: string;
}, infos?: ContributorInfo[]) => ContributorInfo | null;
//#endregion
//#region src/node/utils/getUserNameWithNoreplyEmail.d.ts
declare const getUserNameWithNoreplyEmail: (email: string) => string | null;
//#endregion
//#region src/node/utils/inferGitProvider.d.ts
/**
 * Gets the URL of a Git remote.
 *
 * 获取 Git 远程仓库的 URL
 *
 * @param cwd - The directory where the git commands should be executed. / 执行 git 命令的目录
 *
 * @returns The URL of the Git remote, or null if it cannot be determined. / Git 远程仓库的 URL，如果无法确定则返回 null
 */
declare const getRemoteUrl: (cwd: string) => string | null;
/**
 * Infer git provider from remote URL
 *
 * 从远程 URL 推断 Git 提供商
 *
 * @param cwd - The directory where the git commands should be executed / 执行 git 命令的目录
 *
 * @returns The inferred git provider, or null if it cannot be determined / 推断出的 Git 提供商，如果无法确定则返回 null
 */
declare const inferGitProvider: (cwd: string) => KnownGitProvider | null;
//#endregion
//#region src/node/utils/injectGitOptions.d.ts
declare const injectGitOptions: (provider: KnownGitProvider | null, changelog: ChangelogOptions | boolean) => GitInjectOptions;
//#endregion
//#region src/node/utils/logger.d.ts
declare const PLUGIN_NAME = "@vuepress/plugin-git";
declare const logger: Logger;
//#endregion
//#region src/node/utils/sanitizeHTML.d.ts
/**
 * Sanitize HTML
 *
 * 清理 HTML
 *
 * @see https://github.com/rehypejs/rehype-sanitize
 *
 * @param html - HTML string / HTML 字符串
 * @returns Sanitized HTML string / 清理后的 HTML 字符串
 */
declare const sanitizeHTML: (html: string) => string;
//#endregion
export { ChangelogOptions, CoAuthorInfo, ContributorInfo, ContributorsOptions, GitChangelogInfo, GitContributorInfo, GitData, GitInjectOptions, GitLocaleData, GitPluginFrontmatter, GitPluginOptions, GitPluginPageData, GitUrlPattern, KnownGitProvider, MergedRawCommit, PLUGIN_NAME, RawCommit, checkGitRepo, checkGithubUsername, digestSHA256, getCommits, getContributorInfo, getRawCommits, getRawContributors, getRemoteUrl, getUserNameWithNoreplyEmail, gitPlugin, inferGitProvider, injectGitOptions, logger, mergeRawCommits, resolveChangelog, resolveContributors, sanitizeHTML };
//# sourceMappingURL=index.d.ts.map