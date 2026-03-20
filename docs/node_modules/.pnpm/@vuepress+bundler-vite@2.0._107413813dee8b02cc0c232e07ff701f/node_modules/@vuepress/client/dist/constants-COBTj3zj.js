import { t as __exportAll } from "./chunk-pbuEa-1d.js";
//#region src/devtools/constants.ts
var constants_exports = /* @__PURE__ */ __exportAll({
	COMPONENT_STATE_TYPE: () => COMPONENT_STATE_TYPE,
	INSPECTOR_ID: () => INSPECTOR_ID,
	INSPECTOR_LABEL: () => INSPECTOR_LABEL,
	INSPECTOR_NODES: () => INSPECTOR_NODES,
	INSPECTOR_STATE_SECTION_NAME: () => INSPECTOR_STATE_SECTION_NAME,
	PLUGIN_ID: () => PLUGIN_ID,
	PLUGIN_LABEL: () => PLUGIN_LABEL
});
const PLUGIN_ID = "org.vuejs.vuepress";
const PLUGIN_LABEL = "VuePress";
const COMPONENT_STATE_TYPE = PLUGIN_LABEL;
const INSPECTOR_ID = PLUGIN_ID;
const INSPECTOR_LABEL = PLUGIN_LABEL;
const INSPECTOR_NODE_INTERNAL = {
	id: "INTERNAL",
	label: "Internal",
	keys: [
		"layouts",
		"routes",
		"redirects"
	]
};
const INSPECTOR_NODE_SITE = {
	id: "SITE",
	label: "Site",
	keys: ["siteData", "siteLocaleData"]
};
const INSPECTOR_NODE_ROUTE = {
	id: "ROUTE",
	label: "Route",
	keys: ["routePath", "routeLocale"]
};
const INSPECTOR_NODE_PAGE = {
	id: "PAGE",
	label: "Page",
	keys: [
		"pageData",
		"pageFrontmatter",
		"pageLang",
		"pageHead",
		"pageHeadTitle",
		"pageLayout",
		"pageComponent"
	]
};
const INSPECTOR_NODES = {
	[INSPECTOR_NODE_INTERNAL.id]: INSPECTOR_NODE_INTERNAL,
	[INSPECTOR_NODE_SITE.id]: INSPECTOR_NODE_SITE,
	[INSPECTOR_NODE_ROUTE.id]: INSPECTOR_NODE_ROUTE,
	[INSPECTOR_NODE_PAGE.id]: INSPECTOR_NODE_PAGE
};
const INSPECTOR_STATE_SECTION_NAME = "State";
//#endregion
export { INSPECTOR_STATE_SECTION_NAME as a, constants_exports as c, INSPECTOR_NODES as i, INSPECTOR_ID as n, PLUGIN_ID as o, INSPECTOR_LABEL as r, PLUGIN_LABEL as s, COMPONENT_STATE_TYPE as t };
