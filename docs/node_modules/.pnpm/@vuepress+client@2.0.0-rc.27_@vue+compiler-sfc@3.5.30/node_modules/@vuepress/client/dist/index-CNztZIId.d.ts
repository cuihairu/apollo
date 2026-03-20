import { App, Component, ComputedRef, Ref } from "vue";
import { HeadConfig, PageData, PageFrontmatter, PageHeader, SiteData, SiteLocaleData } from "@vuepress/shared";
import { Router } from "vue-router";

//#region src/constants.d.ts
/**
 * Name of the default layout
 */
declare const LAYOUT_NAME_DEFAULT = "Layout";
/**
 * Name of the 404 page layout
 */
declare const LAYOUT_NAME_NOT_FOUND = "NotFound";
//#endregion
//#region src/types/routes.d.ts
interface PageChunk {
  default: Component;
  _pageData: PageData;
}
type RouteMeta = Record<string, unknown>;
interface Route<T extends RouteMeta = RouteMeta> {
  loader: () => Promise<PageChunk>;
  meta: T;
}
type Redirects = Record<string, string>;
type Routes = Record<string, Route>;
//#endregion
//#region src/types/clientData.d.ts
interface Layouts {
  [LAYOUT_NAME_DEFAULT]: Component;
  [LAYOUT_NAME_NOT_FOUND]: Component;
  [key: string]: Component;
}
type PageComponent = Component;
type PageHead = HeadConfig[];
type PageHeadTitle = string;
type PageLang = string;
type PageLayout = Component;
type RoutePath = string;
type RouteLocale = string;
type LayoutsRef = ComputedRef<Layouts>;
type PageComponentRef = ComputedRef<PageComponent>;
type PageDataRef<T extends Record<string, unknown> = Record<string, unknown>> = ComputedRef<PageData<T>>;
type PageFrontmatterRef<T extends Record<string, unknown> = Record<string, unknown>> = ComputedRef<PageFrontmatter<T>>;
type PageHeadRef = ComputedRef<PageHead>;
type PageHeadTitleRef = ComputedRef<PageHeadTitle>;
type PageLangRef = ComputedRef<PageLang>;
type PageLayoutRef = ComputedRef<PageLayout>;
type RedirectsRef = Ref<Redirects>;
type RoutePathRef = ComputedRef<RoutePath>;
type RouteLocaleRef = ComputedRef<RouteLocale>;
type RoutesRef = Ref<Routes>;
type SiteDataRef = Ref<SiteData>;
type SiteLocaleDataRef = ComputedRef<SiteLocaleData>;
interface ClientData<Frontmatter extends Record<string, unknown> = Record<string, unknown>, Data extends Record<string, unknown> = Record<string, unknown>> {
  layouts: LayoutsRef;
  pageComponent: PageComponentRef;
  pageData: PageDataRef<Data>;
  pageFrontmatter: PageFrontmatterRef<Frontmatter>;
  pageHead: PageHeadRef;
  pageHeadTitle: PageHeadTitleRef;
  pageLang: PageLangRef;
  pageLayout: PageLayoutRef;
  redirects: RedirectsRef;
  routePath: RoutePathRef;
  routeLocale: RouteLocaleRef;
  routes: RoutesRef;
  siteData: SiteDataRef;
  siteLocaleData: SiteLocaleDataRef;
  frontmatter: PageFrontmatterRef<Frontmatter>;
  head: PageHeadRef;
  headTitle: PageHeadTitleRef;
  lang: PageLangRef;
  page: PageDataRef<Data>;
  site: SiteDataRef;
  siteLocale: SiteLocaleDataRef;
}
//#endregion
//#region src/types/clientConfig.d.ts
/**
 * Configure vuepress client
 */
interface ClientConfig {
  /**
   * An enhance function to be called after vue app instance and
   * vue-router instance has been created
   */
  enhance?: (context: {
    app: App;
    router: Router;
    siteData: SiteDataRef;
  }) => Promise<void> | void;
  /**
   * A function to be called inside the setup function of vue app
   */
  setup?: () => void;
  /**
   * Layout components
   */
  layouts?: Partial<Layouts>;
  /**
   * Components to be placed directly into the root node of vue app
   */
  rootComponents?: Component[];
}
//#endregion
//#region src/types/createVueAppFunction.d.ts
type CreateVueAppFunction = () => Promise<{
  app: App;
  router: Router;
}>;
//#endregion
//#region src/types/onContentUpdated.d.ts
type ContentUpdatedReason = 'beforeUnmount' | 'mounted' | 'updated';
type ContentUpdatedCallback = (reason: ContentUpdatedReason) => unknown;
//#endregion
export { SiteData as A, PageLayoutRef as C, RoutePath as D, RouteLocaleRef as E, Redirects as F, Route as I, RouteMeta as L, SiteLocaleData as M, SiteLocaleDataRef as N, RoutePathRef as O, PageChunk as P, Routes as R, PageLayout as S, RouteLocale as T, PageHeadTitle as _, ClientData as a, PageLang as b, LayoutsRef as c, PageData as d, PageDataRef as f, PageHeadRef as g, PageHead as h, ClientConfig as i, SiteDataRef as j, RoutesRef as k, PageComponent as l, PageFrontmatterRef as m, ContentUpdatedReason as n, HeadConfig as o, PageFrontmatter as p, CreateVueAppFunction as r, Layouts as s, ContentUpdatedCallback as t, PageComponentRef as u, PageHeadTitleRef as v, RedirectsRef as w, PageLangRef as x, PageHeader as y };