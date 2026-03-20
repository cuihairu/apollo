import { slugify } from "@mdit-vue/shared";
import { colors, fs, logger, path } from "@vuepress/utils";
import MarkdownIt from "markdown-it";
import { componentPlugin } from "@mdit-vue/plugin-component";
import { frontmatterPlugin } from "@mdit-vue/plugin-frontmatter";
import { headersPlugin } from "@mdit-vue/plugin-headers";
import { sfcPlugin } from "@mdit-vue/plugin-sfc";
import { titlePlugin } from "@mdit-vue/plugin-title";
import { tocPlugin } from "@mdit-vue/plugin-toc";
import anchorPlugin from "markdown-it-anchor";
import { decode } from "mdurl";
import { full as emojiPlugin } from "markdown-it-emoji";
import { inferRoutePath, isLinkExternal, removeLeadingSlash } from "@vuepress/shared";
//#region src/plugins/assetsPlugin/resolveLink.ts
const resolveLink = (link, { env, absolutePathPrependBase = false }) => {
	if (link.startsWith("data:")) return link;
	let resolvedLink = decode(link);
	if (absolutePathPrependBase && env.base && link.startsWith("/")) resolvedLink = path.join(env.base, resolvedLink);
	return resolvedLink;
};
//#endregion
//#region src/plugins/assetsPlugin/assetsPlugin.ts
/**
* Plugin to handle assets links
*/
const assetsPlugin = (md, { absolutePathPrependBase = false } = {}) => {
	const rawImageRule = md.renderer.rules.image;
	md.renderer.rules.image = (tokens, idx, options, env, self) => {
		const token = tokens[idx];
		const link = token.attrGet("src");
		if (link) token.attrSet("src", resolveLink(link, {
			env,
			absolutePathPrependBase
		}));
		return rawImageRule(tokens, idx, options, env, self);
	};
	const createHtmlRule = (rawHtmlRule) => (tokens, idx, options, env, self) => {
		tokens[idx].content = tokens[idx].content.replace(/(<img\b.*?src=)(['"])(.*?)\2/gs, (_, prefix, quote, src) => `${prefix}${quote}${resolveLink(src.trim(), {
			env,
			absolutePathPrependBase
		})}${quote}`).replace(/(<img\b.*?srcset=)(['"])(.*?)\2/gs, (_, prefix, quote, srcset) => `${prefix}${quote}${srcset.split(",").map((item) => item.trim().replace(/^([^ ]*?)([ \n].*)?$/, (__, url, descriptor = "") => `${resolveLink(url.trim(), {
			env,
			absolutePathPrependBase
		})}${descriptor.replace(/[ \n]+/g, " ").trimEnd()}`)).join(", ")}${quote}`);
		return rawHtmlRule(tokens, idx, options, env, self);
	};
	const rawHtmlBlockRule = md.renderer.rules.html_block;
	const rawHtmlInlineRule = md.renderer.rules.html_inline;
	md.renderer.rules.html_block = createHtmlRule(rawHtmlBlockRule);
	md.renderer.rules.html_inline = createHtmlRule(rawHtmlInlineRule);
};
//#endregion
//#region src/plugins/importCodePlugin/createImportCodeBlockRule.ts
const MIN_LENGTH = 9;
const START_CODES = [
	64,
	91,
	99,
	111,
	100,
	101
];
const SYNTAX_RE = /^@\[code(?:{(?:(?:(?<lineStart>\d+)?-(?<lineEnd>\d+)?)|(?<lineSingle>\d+))})?(?: (?<info>[^\]]+))?\]\((?<importPath>[^)]*)\)/;
/**
* Utility function to parse line number from line string that matched by SYNTAX_RE
*/
const parseLineNumber = (line) => line ? Number.parseInt(line, 10) : void 0;
const createImportCodeBlockRule = ({ handleImportPath = (str) => str }) => (state, startLine, endLine, silent) => {
	/* istanbul ignore if */
	if (state.sCount[startLine] - state.blkIndent >= 4) return false;
	const pos = state.bMarks[startLine] + state.tShift[startLine];
	const max = state.eMarks[startLine];
	if (pos + MIN_LENGTH > max) return false;
	for (let i = 0; i < START_CODES.length; i += 1) if (state.src.charCodeAt(pos + i) !== START_CODES[i]) return false;
	const match = state.src.slice(pos, max).match(SYNTAX_RE);
	if (!match?.groups) return false;
	if (silent) return true;
	const { info, importPath } = match.groups;
	const lineSingle = parseLineNumber(match.groups.lineSingle);
	const lineStart = lineSingle ?? parseLineNumber(match.groups.lineStart) ?? 0;
	const lineEnd = lineSingle ?? parseLineNumber(match.groups.lineEnd);
	const meta = {
		importPath: handleImportPath(importPath),
		lineStart,
		lineEnd
	};
	const token = state.push("import_code", "code", 0);
	token.info = info ?? path.extname(meta.importPath).slice(1);
	token.markup = "```";
	token.map = [startLine, startLine + 1];
	token.meta = meta;
	state.line = startLine + 1;
	return true;
};
//#endregion
//#region src/plugins/importCodePlugin/resolveImportCode.ts
const resolveImportCode = ({ importPath, lineStart, lineEnd }, { filePath }) => {
	let importFilePath = importPath;
	if (!path.isAbsolute(importPath)) {
		if (!filePath) {
			logger.error(`Import file ${colors.magenta(importPath)} can not be resolved`);
			return {
				importFilePath: null,
				importCode: "Error when resolving path"
			};
		}
		importFilePath = path.resolve(filePath, "..", importPath);
	}
	if (!fs.existsSync(importFilePath)) {
		logger.error(`Import file ${colors.magenta(importPath)} not found`);
		return {
			importFilePath,
			importCode: "File not found"
		};
	}
	const fileContent = fs.readFileSync(importFilePath).toString();
	return {
		importFilePath,
		importCode: fileContent.split("\n").slice(lineStart ? lineStart - 1 : lineStart, lineEnd).join("\n").replace(/\n?$/, "\n")
	};
};
//#endregion
//#region src/plugins/importCodePlugin/importCodePlugin.ts
const importCodePlugin = (md, options = {}) => {
	md.block.ruler.before("fence", "import_code", createImportCodeBlockRule(options), { alt: [
		"paragraph",
		"reference",
		"blockquote",
		"list"
	] });
	md.renderer.rules.import_code = (tokens, idx, opts, env, slf) => {
		const token = tokens[idx];
		const { importFilePath, importCode } = resolveImportCode(token.meta, env);
		token.content = importCode;
		if (importFilePath) (env.importedFiles ??= []).push(importFilePath);
		return md.renderer.rules.fence(tokens, idx, opts, env, slf);
	};
};
//#endregion
//#region src/plugins/linksPlugin/resolvePaths.ts
/**
* Resolve relative and absolute paths according to the `base` and `filePathRelative`
*/
const resolvePaths = (rawPath, base, filePathRelative) => {
	let absolutePath;
	let relativePath;
	if (rawPath.startsWith("/")) if (rawPath.endsWith(".md")) {
		absolutePath = path.join(base, rawPath);
		relativePath = removeLeadingSlash(rawPath);
	} else {
		absolutePath = rawPath;
		relativePath = path.relative(base, absolutePath);
	}
	else if (filePathRelative) {
		relativePath = path.join(path.dirname(encodeURI(filePathRelative)), rawPath);
		absolutePath = path.join(base, relativePath);
	} else {
		relativePath = rawPath.replace(/^(?:\.\/)?(.*)$/, "$1");
		absolutePath = null;
	}
	return {
		absolutePath,
		relativePath
	};
};
//#endregion
//#region src/plugins/linksPlugin/linksPlugin.ts
/**
* Process links in markdown file
*
* - internal links: convert `<a>` tag into internalTag
* - external links: add extra attrs
*/
const linksPlugin = (md, options = {}) => {
	const internalTag = options.internalTag || "RouteLink";
	const isExternal = options.isExternal ?? ((href, env) => isLinkExternal(href, env.base));
	const externalAttrs = {
		target: "_blank",
		rel: "noopener noreferrer",
		...options.externalAttrs
	};
	let hasOpenInternalLink = false;
	const handleLinkOpen = (tokens, idx, env) => {
		const token = tokens[idx];
		const hrefIndex = token.attrIndex("href");
		/* istanbul ignore if */
		if (hrefIndex < 0) return;
		const hrefAttr = token.attrs[hrefIndex];
		const hrefLink = hrefAttr[1];
		const { base = "/", filePathRelative = null } = env;
		if (isExternal(hrefLink, env)) {
			Object.entries(externalAttrs).forEach(([key, val]) => {
				token.attrSet(key, val);
			});
			return;
		}
		const internalLinkMatch = hrefLink.match(/^([^#?]*?(?:\/|\.md|\.html))([#?].*)?$/);
		if (!internalLinkMatch) return;
		const rawPath = internalLinkMatch[1];
		const rawHashAndQueries = (internalLinkMatch[2] || "").replace(/^#(\d)/, "#_$1");
		const { relativePath, absolutePath } = resolvePaths(rawPath, base, filePathRelative);
		if (["RouterLink", "RouteLink"].includes(internalTag)) {
			token.tag = internalTag;
			hrefAttr[0] = "to";
			hrefAttr[1] = `${inferRoutePath(absolutePath ? absolutePath.replace(new RegExp(`^${base}`), "/") : relativePath)}${rawHashAndQueries}`;
			hasOpenInternalLink = true;
		} else hrefAttr[1] = `${inferRoutePath(absolutePath ?? relativePath)}${rawHashAndQueries}`;
		(env.links ??= []).push({
			raw: hrefLink,
			relative: relativePath,
			absolute: absolutePath
		});
	};
	md.renderer.rules.link_open = (tokens, idx, opts, env, self) => {
		handleLinkOpen(tokens, idx, env);
		return self.renderToken(tokens, idx, opts);
	};
	md.renderer.rules.link_close = (tokens, idx, opts, _env, self) => {
		if (hasOpenInternalLink) {
			hasOpenInternalLink = false;
			tokens[idx].tag = internalTag;
		}
		return self.renderToken(tokens, idx, opts);
	};
};
//#endregion
//#region src/plugins/vPrePlugin/resolveVPre.ts
/**
* Resolve the `:v-pre` / `:no-v-pre` mark from token info
*/
const resolveVPre = (info) => {
	if (/:v-pre\b/.test(info)) return true;
	if (/:no-v-pre\b/.test(info)) return false;
	return null;
};
//#endregion
//#region src/plugins/vPrePlugin/vPrePlugin.ts
/**
* v-pre plugin
*/
const vPrePlugin = (md, { inline = true, block = true } = {}) => {
	const rawFence = md.renderer.rules.fence;
	md.renderer.rules.fence = (...args) => {
		const [tokens, idx] = args;
		const token = tokens[idx];
		const info = token.info ? md.utils.unescapeAll(token.info).trim() : "";
		let result = rawFence(...args);
		if (resolveVPre(info) ?? block) result = `<pre v-pre${result.slice(4)}`;
		return result;
	};
	if (inline) {
		const rawInlineCodeRule = md.renderer.rules.code_inline;
		md.renderer.rules.code_inline = (...args) => {
			return `<code v-pre${rawInlineCodeRule(...args).slice(5)}`;
		};
	}
};
//#endregion
//#region src/markdown.ts
/**
* Create vuepress customized markdown-it instance
*/
const createMarkdown = ({ anchor, assets, code, vPre, component, emoji, frontmatter, headers, title, importCode, links, sfc, slugify: slugify$1 = slugify, toc, ...markdownItOptions } = {}) => {
	const md = MarkdownIt({
		...markdownItOptions,
		html: true
	});
	if (anchor !== false) md.use(anchorPlugin, {
		level: [
			1,
			2,
			3,
			4,
			5,
			6
		],
		slugify: slugify$1,
		permalink: anchorPlugin.permalink.headerLink({
			class: "header-anchor",
			safariReaderFix: true
		}),
		...anchor
	});
	if (assets !== false) md.use(assetsPlugin, assets);
	if (code) logger.warn(`\`markdown.code\` option has been removed, please use '@vuepress/plugin-shiki' or '@vuepress/plugin-prismjs' instead.\n See https://vuepress.vuejs.org/reference/config.html#markdown-code`);
	if (component !== false) md.use(componentPlugin);
	if (emoji !== false) md.use(emojiPlugin, emoji);
	if (frontmatter !== false) md.use(frontmatterPlugin, {
		...frontmatter,
		grayMatterOptions: {
			excerpt: false,
			...frontmatter?.grayMatterOptions
		}
	});
	if (headers !== false) md.use(headersPlugin, {
		level: [2, 3],
		slugify: slugify$1,
		...headers
	});
	if (importCode !== false) md.use(importCodePlugin, importCode);
	if (links !== false) md.use(linksPlugin, links);
	if (sfc !== false) md.use(sfcPlugin, sfc);
	if (toc !== false) md.use(tocPlugin, {
		level: [2, 3],
		slugify: slugify$1,
		linkTag: "router-link",
		...toc
	});
	if (title !== false) md.use(titlePlugin);
	if (vPre !== false) md.use(vPrePlugin, vPre);
	return md;
};
//#endregion
export { anchorPlugin, assetsPlugin, componentPlugin, createMarkdown, emojiPlugin, frontmatterPlugin, headersPlugin, importCodePlugin, linksPlugin, sfcPlugin, titlePlugin, tocPlugin, vPrePlugin };
