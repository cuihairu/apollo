import { fs, importFile, importFileDefault } from "@vuepress/utils";
import { ssrContextKey } from "vue";
//#region src/build/createVueServerApp.ts
/**
* Create vue app and router for server side rendering
*/
const createVueServerApp = async (serverAppPath) => {
	const { createVueApp } = await (serverAppPath.endsWith(".cjs") ? importFileDefault : importFile)(serverAppPath);
	const { app, router } = await createVueApp();
	return {
		vueApp: app,
		vueRouter: router
	};
};
//#endregion
//#region src/build/getSsrTemplate.ts
/**
* Util to read the ssr template file
*/
const getSsrTemplate = async (app) => fs.readFile(app.options.templateBuild, { encoding: "utf8" });
//#endregion
//#region src/build/renderPageToString.ts
/**
* Render a vuepress page to string
*/
const renderPageToString = async ({ page, vueApp, vueRouter, ssrContextInit }) => {
	await vueRouter.push(page.path);
	await vueRouter.isReady();
	delete vueApp._context.provides[ssrContextKey];
	const ssrContext = {
		lang: "en",
		head: [],
		...ssrContextInit
	};
	const { renderToString } = await import("vue/server-renderer");
	return {
		ssrContext,
		ssrString: await renderToString(vueApp, ssrContext)
	};
};
//#endregion
export { createVueServerApp, getSsrTemplate, renderPageToString };
