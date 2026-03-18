import { D as usePageHead, O as usePageLang, R as clientDataSymbol, a as ClientOnly, d as resolveRoute, h as updateHeadSymbol, i as Content, k as usePageLayout, m as routes, p as redirects, r as resolvers, s as RouteLink, t as withBase } from "./utils-_b2e7bpk.js";
import { clientConfigs } from "@internal/clientConfigs";
import { computed, createApp, createSSRApp, customRef, h, onMounted, provide, shallowRef, useSSRContext, watch } from "vue";
import { siteData } from "@internal/siteData";
import { isPlainObject, isString, removeEndingSlash } from "@vuepress/shared";
import { START_LOCATION, createMemoryHistory, createRouter, createWebHistory } from "vue-router";
//#region src/internal/siteData.ts
/**
* Global site data ref
*/
const siteData$1 = shallowRef(siteData);
if (__VUEPRESS_DEV__ && (import.meta.webpackHot || import.meta.hot)) __VUE_HMR_RUNTIME__.updateSiteData = (data) => {
	siteData$1.value = data;
};
//#endregion
//#region src/router/createVueRouter.ts
/**
* - use `createWebHistory` in dev mode and build mode client bundle
* - use `createMemoryHistory` in build mode server bundle
*/
const historyCreator = __VUEPRESS_SSR__ ? createMemoryHistory : createWebHistory;
/**
* Create router instance
*/
const createVueRouter = () => {
	const router = createRouter({
		history: historyCreator(removeEndingSlash(__VUEPRESS_BASE__)),
		routes: [{
			name: "vuepress-route",
			path: "/:catchAll(.*)",
			components: {}
		}],
		scrollBehavior: (to, _from, savedPosition) => {
			if (savedPosition) return savedPosition;
			if (to.hash) return { el: to.hash };
			return { top: 0 };
		}
	});
	router.beforeResolve(async (to, from) => {
		if (to.path !== from.path || from === START_LOCATION) {
			const route = resolveRoute(to.fullPath);
			if (route.path !== to.fullPath) return route.path;
			const pageChunk = await route.loader();
			to.meta = {
				...route.meta,
				_pageChunk: pageChunk
			};
		} else if (to.path === from.path) to.meta = from.meta;
	});
	return router;
};
//#endregion
//#region src/setupGlobalComponents.ts
/**
* Register global built-in components
*/
const setupGlobalComponents = (app) => {
	app.component("ClientOnly", ClientOnly);
	app.component("Content", Content);
	app.component("RouteLink", RouteLink);
};
//#endregion
//#region src/setupGlobalComputed.ts
/**
* Create and provide global computed
*/
const setupGlobalComputed = (app, router, clientConfigs) => {
	const routePath = computed(() => router.currentRoute.value.path);
	const pageChunk = customRef((track, trigger) => ({
		get() {
			track();
			return router.currentRoute.value.meta._pageChunk;
		},
		set(value) {
			router.currentRoute.value.meta._pageChunk = value;
			trigger();
		}
	}));
	if (__VUEPRESS_DEV__ && (import.meta.webpackHot || import.meta.hot)) __VUE_HMR_RUNTIME__.updatePageData = async (newPageData) => {
		const newPageChunk = {
			default: (await routes.value[newPageData.path].loader()).default,
			_pageData: newPageData
		};
		routes.value[newPageData.path].loader = async () => Promise.resolve(newPageChunk);
		if (newPageData.path === router.currentRoute.value.meta._pageChunk?._pageData.path) pageChunk.value = newPageChunk;
	};
	const layouts = computed(() => resolvers.resolveLayouts(clientConfigs));
	const routeLocale = computed(() => resolvers.resolveRouteLocale(siteData$1.value.locales, routePath.value));
	const siteLocaleData = computed(() => resolvers.resolveSiteLocaleData(siteData$1.value, routeLocale.value));
	const pageComponent = computed(() => pageChunk.value.default);
	const pageData = computed(() => pageChunk.value._pageData);
	const pageFrontmatter = computed(() => pageData.value.frontmatter);
	const pageHeadTitle = computed(() => resolvers.resolvePageHeadTitle(pageData.value, siteLocaleData.value));
	const pageHead = computed(() => resolvers.resolvePageHead(pageHeadTitle.value, pageFrontmatter.value, siteLocaleData.value));
	const pageLang = computed(() => resolvers.resolvePageLang(pageData.value, siteLocaleData.value));
	const clientData = {
		layouts,
		pageData,
		pageComponent,
		pageFrontmatter,
		pageHead,
		pageHeadTitle,
		pageLang,
		pageLayout: computed(() => resolvers.resolvePageLayout(pageData.value, layouts.value)),
		redirects,
		routeLocale,
		routePath,
		routes,
		siteData: siteData$1,
		siteLocaleData,
		frontmatter: pageFrontmatter,
		head: pageHead,
		headTitle: pageHeadTitle,
		lang: pageLang,
		page: pageData,
		site: siteData$1,
		siteLocale: siteLocaleData
	};
	app.provide(clientDataSymbol, clientData);
	Object.defineProperties(app.config.globalProperties, {
		$pageFrontmatter: { get: () => pageFrontmatter.value },
		$pageHead: { get: () => pageHead.value },
		$pageHeadTitle: { get: () => pageHeadTitle.value },
		$pageLang: { get: () => pageLang.value },
		$pageData: { get: () => pageData.value },
		$routeLocale: { get: () => routeLocale.value },
		$withBase: { get: () => withBase },
		$frontmatter: { get: () => pageFrontmatter.value },
		$head: { get: () => pageHead.value },
		$headTitle: { get: () => pageHeadTitle.value },
		$lang: { get: () => pageLang.value },
		$page: { get: () => pageData.value },
		$site: { get: () => siteData$1.value },
		$siteLocale: { get: () => siteLocaleData.value }
	});
	return clientData;
};
//#endregion
//#region src/setupUpdateHead.ts
/**
* Query the matched head element of head config
*/
const queryHeadElement = ([tagName, attrs, content = ""]) => {
	const selector = `head > ${tagName}${Object.entries(attrs).map(([key, value]) => {
		if (isString(value)) return `[${key}=${JSON.stringify(value)}]`;
		if (value) return `[${key}]`;
		return "";
	}).join("")}`;
	return Array.from(document.querySelectorAll(selector)).find((item) => item.innerText === content) ?? null;
};
/**
* Create head element from head config
*/
const createHeadElement = ([tagName, attrs, content]) => {
	if (!isString(tagName)) return null;
	const headElement = document.createElement(tagName);
	if (isPlainObject(attrs)) Object.entries(attrs).forEach(([key, value]) => {
		if (isString(value)) headElement.setAttribute(key, value);
		else if (value) headElement.setAttribute(key, "");
	});
	if (isString(content)) headElement.appendChild(document.createTextNode(content));
	return headElement;
};
/**
* Auto update head and provide as global util
*/
const setupUpdateHead = () => {
	const head = usePageHead();
	const lang = usePageLang();
	if (__VUEPRESS_SSR__) {
		const ssrContext = useSSRContext();
		if (ssrContext) {
			ssrContext.head = head.value;
			ssrContext.lang = lang.value;
		}
		return;
	}
	let managedHeadElements = [];
	/**
	* Take over the existing head elements
	*/
	const takeOverHeadElements = () => {
		head.value.forEach((item) => {
			const headElement = queryHeadElement(item);
			if (headElement) managedHeadElements.push(headElement);
		});
	};
	/**
	* Generate head elements from current head config
	*/
	const generateHeadElements = () => {
		const result = [];
		head.value.forEach((item) => {
			const headElement = createHeadElement(item);
			if (headElement) result.push(headElement);
		});
		return result;
	};
	/**
	* Update head elements
	*/
	const updateHead = () => {
		document.documentElement.lang = lang.value;
		const newHeadElements = generateHeadElements();
		managedHeadElements.forEach((oldEl, oldIndex) => {
			const matchedIndex = newHeadElements.findIndex((newEl) => oldEl.isEqualNode(newEl));
			if (matchedIndex === -1) {
				oldEl.remove();
				delete managedHeadElements[oldIndex];
			} else newHeadElements.splice(matchedIndex, 1);
		});
		newHeadElements.forEach((el) => document.head.appendChild(el));
		managedHeadElements = [...managedHeadElements.filter((item) => !!item), ...newHeadElements];
	};
	provide(updateHeadSymbol, updateHead);
	onMounted(() => {
		if (!__VUEPRESS_DEV__) takeOverHeadElements();
		watch(head, updateHead, { immediate: __VUEPRESS_DEV__ });
	});
};
//#endregion
//#region src/app.ts
/**
* - use `createApp` in dev mode
* - use `createSSRApp` in build mode
*/
const appCreator = __VUEPRESS_DEV__ ? createApp : createSSRApp;
const createVueApp = async () => {
	const app = appCreator({
		name: "Vuepress",
		setup() {
			setupUpdateHead();
			for (const clientConfig of clientConfigs) clientConfig.setup?.();
			const clientRootComponents = clientConfigs.flatMap(({ rootComponents = [] }) => rootComponents.map((component) => h(component)));
			const pageLayout = usePageLayout();
			return () => [h(pageLayout.value), clientRootComponents];
		}
	});
	const router = createVueRouter();
	setupGlobalComponents(app);
	const globalComputed = setupGlobalComputed(app, router, clientConfigs);
	if (__VUEPRESS_DEV__ || __VUE_PROD_DEVTOOLS__) {
		const { setupDevtools } = await import("./setupDevtools-B3cgI7XH.js");
		setupDevtools(app, globalComputed);
	}
	for (const clientConfig of clientConfigs) await clientConfig.enhance?.({
		app,
		router,
		siteData: siteData$1
	});
	app.use(router);
	return {
		app,
		router
	};
};
if (!__VUEPRESS_SSR__) createVueApp().then(({ app, router }) => {
	router.isReady().then(() => {
		app.mount("#app");
	});
});
//#endregion
export { createVueApp };
