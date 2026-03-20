import { defineClientConfig } from "vuepress/client";
import Mermaid from "C:/Users/cui/Workspaces/apollo/docs/node_modules/.pnpm/@vuepress+plugin-markdown-c_f8ff9df3ddc3e1ff1fd9b959a4770b4d/node_modules/@vuepress/plugin-markdown-chart/lib/client/components/Mermaid.js";

export default defineClientConfig({
  enhance: ({ app }) => {
    app.component("Mermaid", Mermaid);
  },
});
