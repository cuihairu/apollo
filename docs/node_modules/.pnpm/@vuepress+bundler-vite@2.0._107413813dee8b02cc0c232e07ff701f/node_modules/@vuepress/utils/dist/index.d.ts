import debug from "debug";
import fs from "fs-extra";
import hash from "hash-sum";
import ora, { Ora } from "ora";
import colors from "picocolors";
import picomatch from "picomatch";
import * as tinyglobby from "tinyglobby";
import path from "upath";
import { HeadAttrsConfig, HeadConfig } from "@vuepress/shared";

//#region src/console/formatMs.d.ts
/**
 * Format millisecond
 */
declare const formatMs: (ms: number) => string;
//#endregion
//#region src/console/logger.d.ts
declare const info: (...args: unknown[]) => void;
declare const tip: (...args: unknown[]) => void;
declare const success: (...args: unknown[]) => void;
declare const warn: (...args: unknown[]) => void;
declare const error: (...args: unknown[]) => void;
declare const createError: (message?: string) => Error;
declare const logger: {
  info: (...args: unknown[]) => void;
  tip: (...args: unknown[]) => void;
  success: (...args: unknown[]) => void;
  warn: (...args: unknown[]) => void;
  error: (...args: unknown[]) => void;
  createError: (message?: string) => Error;
};
//#endregion
//#region src/console/withSpinner.d.ts
declare const withSpinner: (msg: string) => <T>(target: (spinner?: Ora) => Promise<T>) => Promise<T>;
//#endregion
//#region src/module/getDirname.d.ts
declare const getDirname: (importMetaUrl: string) => string;
//#endregion
//#region src/module/importFile.d.ts
/**
 * A helper for dynamically importing a file path
 *
 * We need to use `pathToFileURL` to transform file path wo compat with windows
 */
declare const importFile: <T>(filePath: string) => Promise<T>;
/**
 * A wrapper of `importFile` and returns the default export
 */
declare const importFileDefault: <T>(filePath: string) => Promise<T>;
//#endregion
//#region src/module/isChildPath.d.ts
/**
 * Check if `child` is a sub path of `parent` or not. Return `true` if
 * they are the same path
 */
declare const isChildPath: (child: string, parent: string) => boolean;
//#endregion
//#region src/module/sanitizeFileName.d.ts
declare const sanitizeFileName: (name: string) => string;
//#endregion
//#region src/module/transformPathToFileName.d.ts
/**
 * Transforms a path to a file name, replacing slashes with underscores
 */
declare const transformPathToFileName: (rawPath: string) => string;
//#endregion
//#region src/ssr/renderHead.d.ts
/**
 * Render head config to string
 */
declare const renderHead: ([tag, attrs, innerHTML]: HeadConfig) => string;
//#endregion
//#region src/ssr/renderHeadAttrs.d.ts
/**
 * Render head attrs config to string
 */
declare const renderHeadAttrs: (attrs: HeadAttrsConfig) => string;
//#endregion
//#region src/ssr/templateRenderer.d.ts
/**
 * HTML outlets of the template renderer
 */
declare enum TemplateRendererOutlet {
  Content = "<!--vuepress-ssr-content-->",
  Head = "<!--vuepress-ssr-head-->",
  Lang = "{{ lang }}",
  Prefetch = "<!--vuepress-ssr-prefetch-->",
  Preload = "<!--vuepress-ssr-preload-->",
  Scripts = "<!--vuepress-ssr-scripts-->",
  Styles = "<!--vuepress-ssr-styles-->",
  Version = "{{ version }}"
}
/**
 * Context type of the template renderer
 */
interface TemplateRendererContext {
  /**
   * The rendered page content. Typically to be put inside `<div id="app"></div>`
   */
  content: string;
  /**
   * The rendered page head. Typically to be put inside `<head></head>`
   */
  head: string;
  /**
   * The language of the page. Typically to be put inside `<html lang="{{ lang }}">`
   */
  lang: string;
  /**
   * The rendered prefetch links. Typically to be put inside `<head></head>`
   */
  prefetch: string;
  /**
   * The rendered preload links. Typically to be put inside `<head></head>`
   */
  preload: string;
  /**
   * The rendered scripts. Typically to be put before `</body>`
   */
  scripts: string;
  /**
   * The rendered styles. Typically to be put inside `<head></head>`
   */
  styles: string;
  /**
   * The version of VuePress
   */
  version: string;
}
/**
 * Type of the template renderer function
 */
type TemplateRenderer = (template: string, context: TemplateRendererContext) => Promise<string> | string;
/**
 * The default template renderer implementation
 */
declare const templateRenderer: TemplateRenderer;
//#endregion
export { TemplateRenderer, TemplateRendererContext, TemplateRendererOutlet, colors, createError, debug, error, formatMs, fs, getDirname, hash, importFile, importFileDefault, info, isChildPath, logger, ora, path, picomatch, renderHead, renderHeadAttrs, sanitizeFileName, success, templateRenderer, tinyglobby, tip, transformPathToFileName, warn, withSpinner };