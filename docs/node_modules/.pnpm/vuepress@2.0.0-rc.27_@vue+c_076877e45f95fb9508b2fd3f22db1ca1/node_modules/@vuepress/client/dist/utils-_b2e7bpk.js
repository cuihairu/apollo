import { computed, defineAsyncComponent, defineComponent, h, inject, onMounted, onUnmounted, reactive, ref, shallowRef, toRef } from "vue";
import { dedupeHead, isLinkHttp, isLinkWithProtocol, isString, normalizeRoutePath, removeLeadingSlash, resolveLocalePath, splitPath } from "@vuepress/shared";
import { useRoute, useRoute as useRoute$1, useRouter, useRouter as useRouter$1 } from "vue-router";
import { redirects, routes } from "@internal/routes";
//#region src/composables/clientData.ts
/**
* Injection key for client data
*/
const clientDataSymbol = Symbol(__VUEPRESS_DEV__ ? "clientData" : "");
/**
* Returns client data
*/
const useClientData = () => {
	const clientData = inject(clientDataSymbol);
	if (!clientData) throw new Error("useClientData() is called without provider.");
	return clientData;
};
//#endregion
//#region src/composables/clientDataUtils.ts
const useLayouts = () => useClientData().layouts;
const usePageComponent = () => useClientData().pageComponent;
const usePageData = () => useClientData().pageData;
const usePageFrontmatter = () => useClientData().pageFrontmatter;
const usePageHead = () => useClientData().pageHead;
const usePageLang = () => useClientData().pageLang;
const usePageLayout = () => useClientData().pageLayout;
const useRedirects = () => useClientData().redirects;
const useRouteLocale = () => useClientData().routeLocale;
const useRoutePath = () => useClientData().routePath;
const useRoutes = () => useClientData().routes;
const useSiteData = () => useClientData().siteData;
const useSiteLocaleData = () => useClientData().siteLocaleData;
const useData = useClientData;
const useFrontmatter = usePageFrontmatter;
const useHead = usePageHead;
const useLang = usePageLang;
const usePage = usePageData;
const useSite = useSiteData;
const useSiteLocale = useSiteLocaleData;
//#endregion
//#region src/internal/contentUpdatedCallbacks.ts
/**
* Global content updated callbacks
*/
const contentUpdatedCallbacks = /* @__PURE__ */ new Set();
//#endregion
//#region src/composables/onContentUpdated.ts
/**
* Register callback that is called every time the markdown content is updated
* in the DOM.
*/
const onContentUpdated = (fn) => {
	contentUpdatedCallbacks.add(fn);
	onUnmounted(() => {
		contentUpdatedCallbacks.delete(fn);
	});
};
//#endregion
//#region src/composables/updateHead.ts
/**
* Injection key for `updateHead` util
*/
const updateHeadSymbol = Symbol(__VUEPRESS_DEV__ ? "updateHead" : "");
/**
* Returns the `updateHead` util
*/
const useUpdateHead = () => {
	const updateHead = inject(updateHeadSymbol);
	if (!updateHead) throw new Error("useUpdateHead() is called without provider.");
	return updateHead;
};
//#endregion
//#region src/internal/routes.ts
/**
* Global redirects ref
*/
const redirects$1 = shallowRef(redirects);
/**
* Global routes ref
*/
const routes$1 = shallowRef(routes);
if (__VUEPRESS_DEV__ && (import.meta.webpackHot || import.meta.hot)) {
	__VUE_HMR_RUNTIME__.updateRedirects = (data) => {
		redirects$1.value = data;
	};
	__VUE_HMR_RUNTIME__.updateRoutes = (data) => {
		routes$1.value = data;
	};
}
//#endregion
//#region src/router/resolveRoutePath.ts
/**
* Resolve route path with given raw path
*/
const resolveRoutePath = (pathname, currentPath) => {
	const normalizedRoutePath = normalizeRoutePath(pathname, currentPath);
	if (routes$1.value[normalizedRoutePath]) return normalizedRoutePath;
	const encodedRoutePath = encodeURI(normalizedRoutePath);
	if (routes$1.value[encodedRoutePath]) return encodedRoutePath;
	const redirectedRoutePath = redirects$1.value[normalizedRoutePath] || redirects$1.value[encodedRoutePath];
	if (redirectedRoutePath) return redirectedRoutePath;
	return normalizedRoutePath;
};
//#endregion
//#region src/router/resolveRoute.ts
/**
* Resolve route with given path
*/
const resolveRoute = (path, currentPath) => {
	const { pathname, hashAndQueries } = splitPath(path);
	const routePath = resolveRoutePath(pathname, currentPath);
	const routeFullPath = routePath + hashAndQueries;
	if (!routes$1.value[routePath]) return {
		...routes$1.value["/404.html"],
		path: routeFullPath,
		notFound: true
	};
	return {
		...routes$1.value[routePath],
		path: routeFullPath,
		notFound: false
	};
};
//#endregion
//#region src/router/resolveRouteFullPath.ts
/**
* Resolve route full path with given raw path
*/
const resolveRouteFullPath = (path, currentPath) => {
	const { pathname, hashAndQueries } = splitPath(path);
	return resolveRoutePath(pathname, currentPath) + hashAndQueries;
};
//#endregion
//#region src/components/RouteLink.ts
/**
* Forked from https://github.com/vuejs/router/blob/941b2131e80550009e5221d4db9f366b1fea3fd5/packages/router/src/RouterLink.ts#L293
*/
const guardEvent = (event) => {
	if (event.metaKey || event.altKey || event.ctrlKey || event.shiftKey) return;
	if (event.defaultPrevented) return;
	if (event.button !== void 0 && event.button !== 0) return;
	if (event.currentTarget) {
		if (event.currentTarget.getAttribute("target")?.match(/\b_blank\b/i)) return;
	}
	event.preventDefault();
	return true;
};
/**
* Component to render a link to another route.
*
* It's similar to `RouterLink` in `vue-router`, but more lightweight.
*
* It's recommended to use `RouteLink` in VuePress.
*/
const RouteLink = defineComponent({
	name: "RouteLink",
	props: {
		to: {
			type: String,
			required: true
		},
		active: Boolean,
		activeClass: {
			type: String,
			default: "route-link-active"
		}
	},
	slots: Object,
	setup(props, { slots }) {
		const router = useRouter();
		const route = useRoute();
		const path = computed(() => props.to.startsWith("#") || props.to.startsWith("?") ? props.to : `${__VUEPRESS_BASE__}${resolveRouteFullPath(props.to, route.path).substring(1)}`);
		return () => h("a", {
			class: ["route-link", { [props.activeClass]: props.active }],
			href: path.value,
			onClick: (event = {}) => {
				if (guardEvent(event)) router.push(props.to).catch();
			}
		}, slots.default());
	}
});
//#endregion
//#region src/components/AutoLink.ts
/**
* Component to render a link automatically according to the link type
*
* - If the link is internal, it will be rendered as a `<RouteLink>`
* - If the link is external, it will be rendered as a normal `<a>` tag
*/
const AutoLink = defineComponent({
	name: "AutoLink",
	props: { config: {
		type: Object,
		required: true
	} },
	slots: Object,
	setup(props, { slots }) {
		const config = toRef(props, "config");
		const route = useRoute();
		const siteData = useSiteData();
		const withProtocol = computed(() => isLinkWithProtocol(config.value.link));
		const linkTarget = computed(() => config.value.target || (withProtocol.value ? "_blank" : void 0));
		const isBlankTarget = computed(() => linkTarget.value === "_blank");
		const isInternal = computed(() => !withProtocol.value && !isBlankTarget.value);
		const linkRel = computed(() => config.value.rel || (isBlankTarget.value ? "noopener noreferrer" : null));
		const linkAriaLabel = computed(() => config.value.ariaLabel ?? config.value.text);
		const shouldBeActiveInSubpath = computed(() => {
			if (config.value.exact) return false;
			const localePaths = Object.keys(siteData.value.locales);
			return localePaths.length ? localePaths.every((key) => key !== config.value.link) : config.value.link !== "/";
		});
		const isActive = computed(() => {
			if (!isInternal.value) return false;
			if (config.value.activeMatch) return (config.value.activeMatch instanceof RegExp ? config.value.activeMatch : new RegExp(config.value.activeMatch, "u")).test(route.path);
			if (shouldBeActiveInSubpath.value) return route.path.startsWith(config.value.link);
			return route.path === config.value.link;
		});
		return () => {
			const { before, after, default: defaultSlot } = slots;
			const content = defaultSlot?.(config.value) ?? [
				before?.(config.value),
				config.value.text,
				after?.(config.value)
			];
			return isInternal.value ? h(RouteLink, {
				"class": "auto-link",
				"to": config.value.link,
				"active": isActive.value,
				"aria-label": linkAriaLabel.value
			}, () => content) : h("a", {
				"class": "auto-link external-link",
				"href": config.value.link,
				"aria-label": linkAriaLabel.value,
				"rel": linkRel.value,
				"target": linkTarget.value
			}, content);
		};
	}
});
//#endregion
//#region src/components/ClientOnly.ts
/**
* Wrapper component that only renders its content on the client side and skips server side rendering
*
* Since vue 3.5, you can try the new `data-allow-mismatch` attribute instead of `<ClientOnly>` component in some cases to avoid hydration mismatch.
*
* @see https://blog.vuejs.org/posts/vue-3-5#data-allow-mismatch
*/
const ClientOnly = defineComponent({
	name: "ClientOnly",
	setup(_, ctx) {
		const isMounted = ref(false);
		onMounted(() => {
			isMounted.value = true;
		});
		return () => isMounted.value ? ctx.slots.default?.() : null;
	}
});
//#endregion
//#region src/components/Content.ts
/**
* Execute all callbacks registered via `onContentUpdated`.
*
* @internal
*/
const runContentUpdatedCallbacks = (reason) => {
	contentUpdatedCallbacks.forEach((fn) => fn(reason));
};
/**
* Markdown rendered content
*/
const Content = defineComponent({
	name: "Content",
	props: { path: {
		type: String,
		required: false,
		default: ""
	} },
	setup(props) {
		const pageComponent = usePageComponent();
		const ContentComponent = computed(() => {
			if (!props.path) return pageComponent.value;
			const route = resolveRoute(props.path);
			return defineAsyncComponent(async () => route.loader().then((m) => m.default));
		});
		return () => h(ContentComponent.value, {
			onVnodeMounted: () => {
				runContentUpdatedCallbacks("mounted");
			},
			onVnodeUpdated: () => {
				runContentUpdatedCallbacks("updated");
			},
			onVnodeBeforeUnmount: () => {
				runContentUpdatedCallbacks("beforeUnmount");
			}
		});
	}
});
//#endregion
//#region src/constants.ts
/**
* Name of the default layout
*/
const LAYOUT_NAME_DEFAULT = "Layout";
//#endregion
//#region src/resolvers.ts
/**
* Resolver methods to get global computed
*
* Users can override corresponding method for advanced customization
*
* @experimental - This is an experimental API and may be changed in minor versions
*/
const resolvers = reactive({
	resolveLayouts: (clientConfigs) => clientConfigs.reduce((prev, item) => ({
		...prev,
		...item.layouts
	}), {}),
	resolvePageHead: (pageHeadTitle, pageFrontmatter, siteLocaleDate) => {
		const description = isString(pageFrontmatter.description) ? pageFrontmatter.description : siteLocaleDate.description;
		return dedupeHead([
			...Array.isArray(pageFrontmatter.head) ? pageFrontmatter.head : [],
			...siteLocaleDate.head,
			[
				"title",
				{},
				pageHeadTitle
			],
			["meta", {
				name: "description",
				content: description
			}]
		]);
	},
	resolvePageHeadTitle: (pageData, siteLocaleDate) => [pageData.title, siteLocaleDate.title].filter((item) => !!item).join(" | "),
	resolvePageLang: (pageData, siteLocaleData) => pageData.lang || siteLocaleData.lang || "en-US",
	resolvePageLayout: (pageData, layouts) => {
		const layoutName = isString(pageData.frontmatter.layout) ? pageData.frontmatter.layout : LAYOUT_NAME_DEFAULT;
		if (!layouts[layoutName]) throw new Error(`[vuepress] Cannot resolve layout: ${layoutName}`);
		return layouts[layoutName];
	},
	resolveRouteLocale: (locales, routePath) => resolveLocalePath(locales, decodeURI(routePath)),
	resolveSiteLocaleData: ({ base, locales, ...siteData }, routeLocale) => ({
		...siteData,
		...locales[routeLocale],
		head: [...locales[routeLocale]?.head ?? [], ...siteData.head]
	})
});
//#endregion
//#region src/utils/defineClientConfig.ts
/**
* A helper function to help you define vuepress client config file
*/
const defineClientConfig = (clientConfig = {}) => clientConfig;
//#endregion
//#region src/utils/withBase.ts
/**
* Prefix url with site base
*/
const withBase = (url) => {
	if (isLinkHttp(url)) return url;
	return `${__VUEPRESS_BASE__}${removeLeadingSlash(url)}`;
};
//#endregion
export { useRedirects as A, usePage as C, usePageHead as D, usePageFrontmatter as E, useSiteData as F, useSiteLocale as I, useSiteLocaleData as L, useRoutePath as M, useRoutes as N, usePageLang as O, useSite as P, clientDataSymbol as R, useLayouts as S, usePageData as T, onContentUpdated as _, ClientOnly as a, useHead as b, useRoute$1 as c, resolveRoute as d, resolveRoutePath as f, useUpdateHead as g, updateHeadSymbol as h, Content as i, useRouteLocale as j, usePageLayout as k, useRouter$1 as l, routes$1 as m, defineClientConfig as n, AutoLink as o, redirects$1 as p, resolvers as r, RouteLink as s, withBase as t, resolveRouteFullPath as u, useData as v, usePageComponent as w, useLang as x, useFrontmatter as y, useClientData as z };
