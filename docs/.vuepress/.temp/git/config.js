import { GitContributors } from "C:/Users/cui/Workspaces/apollo/docs/node_modules/@vuepress/plugin-git/dist/client/components/GitContributors.js";

export default {
  enhance: ({ app }) => {
    app.component("GitContributors", GitContributors);
  },
};
