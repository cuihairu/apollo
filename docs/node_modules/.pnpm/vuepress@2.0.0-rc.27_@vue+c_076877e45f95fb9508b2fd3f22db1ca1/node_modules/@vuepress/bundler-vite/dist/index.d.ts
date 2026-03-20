import { InlineConfig } from "vite";
import { Bundler, BundlerOptions } from "@vuepress/core";
import { Options } from "@vitejs/plugin-vue";

//#region src/types.d.ts
/**
 * Options for bundler-vite
 */
interface ViteBundlerOptions extends BundlerOptions {
  viteOptions?: InlineConfig;
  vuePluginOptions?: Options;
}
//#endregion
//#region src/viteBundler.d.ts
declare const viteBundler: (options?: ViteBundlerOptions) => Bundler;
//#endregion
export { ViteBundlerOptions, viteBundler as default, viteBundler };