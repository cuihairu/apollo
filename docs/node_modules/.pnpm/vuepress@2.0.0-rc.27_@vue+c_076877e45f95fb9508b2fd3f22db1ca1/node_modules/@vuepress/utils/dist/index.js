import debug from "debug";
import fs from "fs-extra";
import hash from "hash-sum";
import ora, { default as ora$1 } from "ora";
import colors, { default as colors$1 } from "picocolors";
import picomatch from "picomatch";
import * as tinyglobby from "tinyglobby";
import path, { default as path$1 } from "upath";
import process from "node:process";
import { fileURLToPath, pathToFileURL } from "node:url";
//#region src/console/formatMs.ts
/**
* Format millisecond
*/
const formatMs = (ms) => {
	if (ms < 1e3) return `${ms}ms`;
	return `${(ms / 1e3).toFixed(2)}s`;
};
//#endregion
//#region src/console/logger.ts
const info = (...args) => {
	console.log(colors$1.cyan("info"), ...args);
};
const tip = (...args) => {
	console.log(colors$1.blue("tip"), ...args);
};
const success = (...args) => {
	console.log(colors$1.green("success"), ...args);
};
const warn = (...args) => {
	console.warn(colors$1.yellow("warning"), ...args);
};
const error = (...args) => {
	console.error(colors$1.red("error"), ...args);
};
const createError = (message) => {
	error(message);
	return new Error(message);
};
const logger = {
	info,
	tip,
	success,
	warn,
	error,
	createError
};
//#endregion
//#region src/console/withSpinner.ts
const withSpinner = (msg) => async (target) => {
	if (process.env.DEBUG) return target();
	const start = Date.now();
	const spinner = ora$1();
	try {
		spinner.start(msg);
		const result = await target(spinner);
		spinner.succeed(`${msg} - done in ${formatMs(Date.now() - start)}`);
		return result;
	} catch (e) {
		spinner.fail(`${msg} - failed in ${formatMs(Date.now() - start)}`);
		throw e;
	}
};
//#endregion
//#region src/module/getDirname.ts
const getDirname = (importMetaUrl) => path$1.dirname(fileURLToPath(importMetaUrl));
//#endregion
//#region src/module/importFile.ts
/**
* A helper for dynamically importing a file path
*
* We need to use `pathToFileURL` to transform file path wo compat with windows
*/
const importFile = async (filePath) => import(pathToFileURL(filePath).toString());
/**
* A wrapper of `importFile` and returns the default export
*/
const importFileDefault = async (filePath) => importFile(filePath).then((m) => m.default);
//#endregion
//#region src/module/isChildPath.ts
/**
* Check if `child` is a sub path of `parent` or not. Return `true` if
* they are the same path
*/
const isChildPath = (child, parent) => {
	const childPath = path$1.normalize(child);
	const parentPath = path$1.normalize(parent);
	if (!path$1.win32.isAbsolute(childPath) || !path$1.win32.isAbsolute(parentPath)) return false;
	const relativePath = path$1.relative(parentPath, childPath);
	return relativePath === "" || !relativePath.startsWith("..");
};
//#endregion
//#region src/module/sanitizeFileName.ts
const INVALID_CHAR_REGEX = /[\u0000-\u001F"#$%&*+,:;<=>?[\]^`{|}\u007F]/g;
const DRIVE_LETTER_REGEX = /^[a-z]:/i;
const sanitizeFileName = (name) => {
	const driveLetter = DRIVE_LETTER_REGEX.exec(name)?.[0] || "";
	return driveLetter + name.slice(driveLetter.length).replace(INVALID_CHAR_REGEX, "_").replace(/^_+/, "");
};
//#endregion
//#region src/module/transformPathToFileName.ts
/**
* Transforms a path to a file name, replacing slashes with underscores
*/
const transformPathToFileName = (rawPath) => sanitizeFileName(rawPath.replace(/\//g, "_"));
//#endregion
//#region src/ssr/renderHeadAttrs.ts
/**
* Render head attrs config to string
*/
const renderHeadAttrs = (attrs) => Object.entries(attrs).filter((item) => item[1] !== false).map(([key, value]) => value === true ? ` ${key}` : ` ${key}="${attrs[key]}"`).join("");
//#endregion
//#region src/ssr/renderHead.ts
/**
* Render head config to string
*/
const renderHead = ([tag, attrs, innerHTML = ""]) => {
	const openTag = `<${tag}${renderHeadAttrs(attrs)}>`;
	if (tag === "link" || tag === "meta" || tag === "base") return openTag;
	return `${openTag}${innerHTML}</${tag}>`;
};
//#endregion
//#region src/ssr/templateRenderer.ts
/**
* HTML outlets of the template renderer
*/
let TemplateRendererOutlet = /* @__PURE__ */ function(TemplateRendererOutlet) {
	TemplateRendererOutlet["Content"] = "<!--vuepress-ssr-content-->";
	TemplateRendererOutlet["Head"] = "<!--vuepress-ssr-head-->";
	TemplateRendererOutlet["Lang"] = "{{ lang }}";
	TemplateRendererOutlet["Prefetch"] = "<!--vuepress-ssr-prefetch-->";
	TemplateRendererOutlet["Preload"] = "<!--vuepress-ssr-preload-->";
	TemplateRendererOutlet["Scripts"] = "<!--vuepress-ssr-scripts-->";
	TemplateRendererOutlet["Styles"] = "<!--vuepress-ssr-styles-->";
	TemplateRendererOutlet["Version"] = "{{ version }}";
	return TemplateRendererOutlet;
}({});
/**
* The default template renderer implementation
*/
const templateRenderer = (template, { content, head, lang, prefetch, preload, scripts, styles, version }) => template.replace(TemplateRendererOutlet.Content, () => content).replace(TemplateRendererOutlet.Head, head).replace(TemplateRendererOutlet.Lang, lang).replace(TemplateRendererOutlet.Prefetch, prefetch).replace(TemplateRendererOutlet.Preload, preload).replace(TemplateRendererOutlet.Scripts, scripts).replace(TemplateRendererOutlet.Styles, styles).replace(TemplateRendererOutlet.Version, version);
//#endregion
export { TemplateRendererOutlet, colors, createError, debug, error, formatMs, fs, getDirname, hash, importFile, importFileDefault, info, isChildPath, logger, ora, path, picomatch, renderHead, renderHeadAttrs, sanitizeFileName, success, templateRenderer, tinyglobby, tip, transformPathToFileName, warn, withSpinner };
