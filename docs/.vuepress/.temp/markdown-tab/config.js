import { CodeTabs } from "C:/Users/cui/Workspaces/apollo/docs/node_modules/.pnpm/@vuepress+plugin-markdown-t_07c84fb4644fa2c28eea2c0f8ea49148/node_modules/@vuepress/plugin-markdown-tab/dist/client/components/CodeTabs.js";
import { Tabs } from "C:/Users/cui/Workspaces/apollo/docs/node_modules/.pnpm/@vuepress+plugin-markdown-t_07c84fb4644fa2c28eea2c0f8ea49148/node_modules/@vuepress/plugin-markdown-tab/dist/client/components/Tabs.js";
import "C:/Users/cui/Workspaces/apollo/docs/node_modules/.pnpm/@vuepress+plugin-markdown-t_07c84fb4644fa2c28eea2c0f8ea49148/node_modules/@vuepress/plugin-markdown-tab/dist/client/styles/vars.css";

export default {
  enhance: ({ app }) => {
    app.component("CodeTabs", CodeTabs);
    app.component("Tabs", Tabs);
  },
};
