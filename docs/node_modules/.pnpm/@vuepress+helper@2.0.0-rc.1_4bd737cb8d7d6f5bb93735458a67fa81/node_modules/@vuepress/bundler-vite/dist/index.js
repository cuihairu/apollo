import { createRequire } from "node:module";
import { createVueServerApp, getSsrTemplate, renderPageToString } from "@vuepress/bundlerutils";
import { colors, debug, fs, renderHead, sanitizeFileName, withSpinner } from "@vuepress/utils";
import { build, createServer, mergeConfig } from "vite";
import autoprefixer from "autoprefixer";
import postcssrc from "postcss-load-config";
import history from "connect-history-api-fallback";
import { createPage, renderPageToVue } from "@vuepress/core";
import vuePlugin from "@vitejs/plugin-vue";
//#region src/plugins/vuepressBuildPlugin.ts
/**
* Configure build command for vuepress
*/
const vuepressBuildPlugin = ({ isServer }) => ({
	name: "vuepress:build",
	generateBundle(_, bundle) {
		if (isServer) Object.keys(bundle).forEach((key) => {
			if (bundle[key].type === "asset") delete bundle[key];
		});
	}
});
//#endregion
//#region src/plugins/vuepressConfigPlugin.ts
/**
* Resolve vite config `resolve.alias`
*/
const resolveAlias = async ({ app, isServer }) => {
	const alias = {
		"@internal": app.dir.temp("internal"),
		"@temp": app.dir.temp(),
		"@source": app.dir.source()
	};
	(await app.pluginApi.hooks.alias.process(app, isServer)).forEach((aliasObject) => {
		Object.assign(alias, aliasObject);
	});
	return [...Object.keys(alias).sort((a, b) => b.length - a.length).map((item) => ({
		find: item,
		replacement: alias[item]
	}))];
};
/**
* Resolve vite config `define`
*/
const resolveDefine = async ({ app, isBuild, isServer }) => {
	const define = {
		__VUEPRESS_VERSION__: JSON.stringify(app.version),
		__VUEPRESS_BASE__: JSON.stringify(app.options.base),
		__VUEPRESS_DEV__: JSON.stringify(!isBuild),
		__VUEPRESS_SSR__: JSON.stringify(isServer),
		__VUE_OPTIONS_API__: JSON.stringify(true),
		__VUE_PROD_DEVTOOLS__: JSON.stringify(app.env.isDebug),
		__VUE_PROD_HYDRATION_MISMATCH_DETAILS__: JSON.stringify(app.env.isDebug)
	};
	if (app.env.isDebug) define["process.env.NODE_ENV"] = JSON.stringify("development");
	(await app.pluginApi.hooks.define.process(app, isServer)).forEach((defineObject) => {
		Object.entries(defineObject).forEach(([key, value]) => {
			define[key] = JSON.stringify(value);
		});
	});
	return define;
};
/**
* Resolve and setup vite config
*/
const vuepressConfigPlugin = ({ app, isBuild, isServer }) => ({
	name: "vuepress:config",
	enforce: "pre",
	async config() {
		const clientPackages = [
			"@vuepress/client",
			"vuepress",
			...app.pluginApi.plugins.filter(({ name }) => name !== "user-config").map(({ name }) => name)
		];
		let postcssPlugins;
		try {
			postcssPlugins = (await postcssrc()).plugins;
		} catch {
			postcssPlugins = [autoprefixer];
		}
		return {
			root: app.dir.source(),
			base: app.options.base,
			mode: !isBuild || app.env.isDebug ? "development" : "production",
			define: await resolveDefine({
				app,
				isBuild,
				isServer
			}),
			publicDir: app.dir.public(),
			cacheDir: app.dir.cache(),
			resolve: { alias: await resolveAlias({
				app,
				isServer
			}) },
			css: {
				postcss: { plugins: isServer ? [] : postcssPlugins },
				preprocessorOptions: { scss: { charset: false } }
			},
			server: {
				host: app.options.host,
				port: app.options.port,
				open: app.options.open
			},
			build: {
				ssr: isServer,
				outDir: isServer ? app.dir.temp(".server") : app.dir.dest(),
				emptyOutDir: false,
				cssCodeSplit: false,
				rolldownOptions: {
					input: app.dir.client(fs.readJsonSync(app.dir.client("package.json")).exports["./app"]),
					output: {
						sanitizeFileName,
						...isServer ? { entryFileNames: `[name].[hash].mjs` } : {}
					},
					preserveEntrySignatures: "allow-extension"
				},
				minify: isServer ? false : !app.env.isDebug
			},
			optimizeDeps: { exclude: clientPackages },
			ssr: {
				format: "esm",
				noExternal: clientPackages
			}
		};
	}
});
//#endregion
//#region src/plugins/vuepressDevPlugin.ts
/**
* Configure dev command for vuepress
*/
const vuepressDevPlugin = ({ app }) => ({
	name: "vuepress:dev",
	async configureServer(server) {
		const indexHtml = (await fs.readFile(app.options.templateDev, { encoding: "utf-8" })).replace(/<\/body>/, `\
<script type="module">
import 'vuepress/client-app'
<\/script>
</body>`);
		return () => {
			server.middlewares.use(history({ rewrites: [{
				from: /\.html$/,
				to: "/index.html"
			}] })).use((req, res, next) => {
				if (!req.url?.endsWith(".html")) {
					next();
					return;
				}
				res.statusCode = 200;
				res.setHeader("Content-Type", "text/html");
				server.transformIndexHtml(req.url, indexHtml, req.originalUrl).then((result) => {
					res.end(result);
				});
			});
		};
	}
});
//#endregion
//#region src/plugins/vuepressMarkdownPlugin.ts
/**
* Handle markdown transformation
*/
const vuepressMarkdownPlugin = ({ app }) => ({
	name: "vuepress:markdown",
	enforce: "pre",
	async transform(code, id) {
		if (!id.endsWith(".md")) return;
		const page = app.pagesMap[id];
		if (page?.content === code) return renderPageToVue(app, page);
		return renderPageToVue(app, await createPage(app, {
			content: code,
			filePath: id
		}));
	},
	async handleHotUpdate(ctx) {
		if (!ctx.file.endsWith(".md")) return;
		const newPage = await createPage(app, {
			content: await ctx.read(),
			filePath: ctx.file
		});
		ctx.read = () => renderPageToVue(app, newPage);
	}
});
//#endregion
//#region src/plugins/vuepressUserConfigPlugin.ts
/**
* A plugin to allow user config to override vite config
*/
const vuepressUserConfigPlugin = ({ options }) => ({
	name: "vuepress:user-config",
	enforce: "post",
	config: () => options.viteOptions ?? {}
});
//#endregion
//#region src/plugins/vuepressVuePlugin.ts
/**
* Wrapper of the official vue plugin
*/
const vuepressVuePlugin = ({ options }) => vuePlugin({
	include: [/\.vue$/, /\.md$/],
	...options.vuePluginOptions
});
//#endregion
//#region src/resolveViteConfig.ts
const resolveViteConfig = ({ app, options, isBuild, isServer }) => mergeConfig({
	clearScreen: false,
	configFile: false,
	logLevel: !isBuild || app.env.isDebug ? "info" : "warn",
	esbuild: { charset: "utf8" },
	plugins: [
		vuepressConfigPlugin({
			app,
			isBuild,
			isServer
		}),
		vuepressMarkdownPlugin({ app }),
		vuepressDevPlugin({ app }),
		vuepressBuildPlugin({ isServer }),
		vuepressVuePlugin({ options }),
		vuepressUserConfigPlugin({ options })
	]
}, options.viteOptions ?? {});
//#endregion
//#region src/build/renderPagePrefetchLinks.ts
/**
* Render prefetch links of current page
*/
const renderPagePrefetchLinks = ({ app, outputEntryChunk, pageChunkFiles }) => {
	const { shouldPrefetch } = app.options;
	if (shouldPrefetch === false) return "";
	return outputEntryChunk.dynamicImports.filter((item) => !pageChunkFiles.some((file) => file === item)).map((item) => {
		const type = item.endsWith(".js") ? "script" : item.endsWith(".css") ? "style" : "";
		if (shouldPrefetch !== true && !shouldPrefetch(item, type)) return "";
		return `<link rel="prefetch" href="${app.options.base}${item}" as="${type}">`;
	}).join("");
};
//#endregion
//#region src/build/renderPagePreloadLinks.ts
/**
* Render preload links of current page
*/
const renderPagePreloadLinks = ({ app, outputEntryChunk, pageChunkFiles }) => {
	const { shouldPreload } = app.options;
	if (shouldPreload === false) return "";
	return Array.from(new Set([
		outputEntryChunk.fileName,
		...outputEntryChunk.imports,
		...pageChunkFiles
	])).map((item) => {
		const type = item.endsWith(".js") ? "script" : item.endsWith(".css") ? "style" : "";
		if (shouldPreload === true && type !== "script" && type !== "style") return "";
		if (shouldPreload !== true && !shouldPreload(item, type)) return "";
		if (type === "script") return `<link rel="modulepreload" href="${app.options.base}${item}">`;
		return `<link rel="preload" href="${app.options.base}${item}"${type !== "" ? ` as="${type}"` : ""}>`;
	}).join("");
};
//#endregion
//#region src/build/renderPageScripts.ts
/**
* Render scripts of current page
*/
const renderPageScripts = ({ app, outputEntryChunk }) => `<script type="module" src="${app.options.base}${outputEntryChunk.fileName}" defer><\/script>`;
//#endregion
//#region src/build/renderPageStyles.ts
/**
* Render styles of current page
*/
const renderPageStyles = ({ app, outputCssAsset }) => outputCssAsset ? [`<link rel="preload" href="${app.options.base}${outputCssAsset.fileName}" as="style">`, `<link rel="stylesheet" href="${app.options.base}${outputCssAsset.fileName}">`].join("") : "";
//#endregion
//#region src/build/resolvePageChunkFiles.ts
const resolvePageChunkFiles = ({ page, output }) => output.filter((item) => item.type === "chunk" && item.facadeModuleId === page.chunkFilePath).flatMap(({ fileName, imports, dynamicImports }) => [
	fileName,
	...imports,
	...dynamicImports
]);
//#endregion
//#region src/build/renderPage.ts
const renderPage = async ({ app, page, vueApp, vueRouter, ssrTemplate, output, outputEntryChunk, outputCssAsset }) => {
	const { ssrContext, ssrString } = await renderPageToString({
		page,
		vueApp,
		vueRouter
	});
	const pageChunkFiles = resolvePageChunkFiles({
		page,
		output
	});
	const html = await app.options.templateBuildRenderer(ssrTemplate, {
		content: ssrString,
		head: ssrContext.head.map(renderHead).join(""),
		lang: ssrContext.lang,
		prefetch: renderPagePrefetchLinks({
			app,
			outputEntryChunk,
			pageChunkFiles
		}),
		preload: renderPagePreloadLinks({
			app,
			outputEntryChunk,
			pageChunkFiles
		}),
		scripts: renderPageScripts({
			app,
			outputEntryChunk
		}),
		styles: renderPageStyles({
			app,
			outputCssAsset
		}),
		version: app.version
	});
	await fs.outputFile(page.htmlFilePath, html);
};
//#endregion
//#region src/build/build.ts
const log = debug("vuepress:bundler-vite/build");
const build$1 = async (options, app) => {
	await app.pluginApi.hooks.extendsBundlerOptions.process(options, app);
	log("compiling start");
	let clientOutput;
	let serverOutput;
	await withSpinner("Compiling with vite")(async () => {
		const clientConfig = resolveViteConfig({
			app,
			options,
			isBuild: true,
			isServer: false
		});
		const serverConfig = resolveViteConfig({
			app,
			options,
			isBuild: true,
			isServer: true
		});
		[clientOutput, serverOutput] = await Promise.all([build(clientConfig), build(serverConfig)]);
	});
	log("compiling finish");
	await withSpinner(`Rendering ${app.pages.length} pages`)(async (spinner) => {
		const clientEntryChunk = clientOutput.output.find((item) => item.type === "chunk" && item.isEntry);
		const clientCssAsset = clientOutput.output.find((item) => item.type === "asset" && item.fileName.endsWith(".css"));
		const serverEntryChunk = serverOutput.output.find((item) => item.type === "chunk" && item.isEntry);
		const { vueApp, vueRouter } = await createVueServerApp(app.dir.temp(".server", serverEntryChunk.fileName));
		const ssrTemplate = await getSsrTemplate(app);
		for (const page of app.pages) {
			if (spinner) spinner.text = `Rendering pages ${colors.magenta(page.path)}`;
			await renderPage({
				app,
				page,
				vueApp,
				vueRouter,
				ssrTemplate,
				output: clientOutput.output,
				outputEntryChunk: clientEntryChunk,
				outputCssAsset: clientCssAsset
			});
		}
	});
	if (!app.env.isDebug) await fs.remove(app.dir.temp(".server"));
};
//#endregion
//#region src/dev.ts
const require = createRequire(import.meta.url);
const dev = async (options, app) => {
	await app.pluginApi.hooks.extendsBundlerOptions.process(options, app);
	const server = await createServer(resolveViteConfig({
		app,
		options,
		isBuild: false,
		isServer: false
	}));
	await server.listen();
	const viteVersion = fs.readJsonSync(require.resolve("vite/package.json")).version;
	server.config.logger.info(colors.cyan(`\n  vite v${viteVersion}`) + colors.green(` dev server running at:\n`), { clear: !server.config.logger.hasWarned });
	server.printUrls();
	return server.close.bind(server);
};
//#endregion
//#region src/viteBundler.ts
const viteBundler = (options = {}) => ({
	name: "@vuepress/bundler-vite",
	dev: async (app) => dev(options, app),
	build: async (app) => build$1(options, app)
});
//#endregion
//#region src/index.ts
var src_default = viteBundler;
//#endregion
export { src_default as default, viteBundler };
