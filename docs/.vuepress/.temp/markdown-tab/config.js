import { CodeTabs } from "C:/Users/cui/Workspaces/apollo/docs/node_modules/.pnpm/@vuepress+plugin-markdown-t_280cd8b1637d6f8c01846f71e1669cb5/node_modules/@vuepress/plugin-markdown-tab/dist/client/components/CodeTabs.js";
import { Tabs } from "C:/Users/cui/Workspaces/apollo/docs/node_modules/.pnpm/@vuepress+plugin-markdown-t_280cd8b1637d6f8c01846f71e1669cb5/node_modules/@vuepress/plugin-markdown-tab/dist/client/components/Tabs.js";
import "C:/Users/cui/Workspaces/apollo/docs/node_modules/.pnpm/@vuepress+plugin-markdown-t_280cd8b1637d6f8c01846f71e1669cb5/node_modules/@vuepress/plugin-markdown-tab/dist/client/styles/vars.css";

export default {
  enhance: ({ app }) => {
    app.component("CodeTabs", CodeTabs);
    app.component("Tabs", Tabs);
  },
};
