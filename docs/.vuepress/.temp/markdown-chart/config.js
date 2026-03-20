import { defineClientConfig } from "vuepress/client";
import Mermaid from "C:/Users/cui/Workspaces/apollo/docs/node_modules/.pnpm/@vuepress+plugin-markdown-c_733a8d7df032fc3e7adefe5239ee268b/node_modules/@vuepress/plugin-markdown-chart/lib/client/components/Mermaid.js";

export default defineClientConfig({
  enhance: ({ app }) => {
    app.component("Mermaid", Mermaid);
  },
});
