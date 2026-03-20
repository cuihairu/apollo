import { a as INSPECTOR_STATE_SECTION_NAME, i as INSPECTOR_NODES, n as INSPECTOR_ID, o as PLUGIN_ID, r as INSPECTOR_LABEL, s as PLUGIN_LABEL, t as COMPONENT_STATE_TYPE } from "./constants-COBTj3zj.js";
import { watch } from "vue";
import { setupDevtoolsPlugin } from "@vue/devtools-api";
//#region src/devtools/setupDevtools.ts
const setupDevtools = (app, clientData) => {
	setupDevtoolsPlugin({
		app,
		id: PLUGIN_ID,
		label: PLUGIN_LABEL,
		packageName: "@vuepress/client",
		homepage: "https://vuepress.vuejs.org",
		logo: "https://vuepress.vuejs.org/images/hero.png",
		componentStateTypes: [COMPONENT_STATE_TYPE]
	}, (api) => {
		const clientDataEntries = Object.entries(clientData);
		const clientDataKeys = Object.keys(clientData);
		const clientDataValues = Object.values(clientData);
		api.on.inspectComponent((payload) => {
			payload.instanceData.state.push(...clientDataEntries.map(([name, item]) => ({
				type: COMPONENT_STATE_TYPE,
				editable: false,
				key: name,
				value: item.value
			})));
		});
		api.addInspector({
			id: INSPECTOR_ID,
			label: INSPECTOR_LABEL,
			icon: "article"
		});
		api.on.getInspectorTree((payload) => {
			if (payload.inspectorId !== "org.vuejs.vuepress") return;
			payload.rootNodes = Object.values(INSPECTOR_NODES).map((node) => ({
				id: node.id,
				label: node.label,
				children: node.keys.map((key) => ({
					id: key,
					label: key
				}))
			}));
		});
		api.on.getInspectorState((payload) => {
			if (payload.inspectorId !== "org.vuejs.vuepress") return;
			const inspectorNode = INSPECTOR_NODES[payload.nodeId];
			if (inspectorNode) {
				payload.state = { [inspectorNode.label]: inspectorNode.keys.map((key) => ({
					key,
					value: clientData[key].value
				})) };
				return;
			}
			if (clientDataKeys.includes(payload.nodeId)) payload.state = { [INSPECTOR_STATE_SECTION_NAME]: [{
				key: payload.nodeId,
				value: clientData[payload.nodeId].value
			}] };
		});
		watch(clientDataValues, () => {
			api.notifyComponentUpdate();
			api.sendInspectorState(INSPECTOR_ID);
		});
	});
};
//#endregion
export { setupDevtools };
