import { t as ThemeData } from "../themeData-BPGN2xlr.js";
import { App, ComputedRef, InjectionKey, Ref } from "vue";
import { RouteLocale } from "vuepress/client";
import { LocaleData } from "vuepress/shared";

//#region src/client/composables/useThemeData.d.ts
/**
 * Theme data ref type
 *
 * 主题数据响应式引用类型
 */
type ThemeDataRef<T extends ThemeData = ThemeData> = Ref<T>;
declare const themeData: ThemeDataRef;
/**
 * Use theme data
 *
 * 使用主题数据
 *
 * @returns Theme data ref object / 主题数据响应式引用对象
 */
declare const useThemeData: <T extends ThemeData = ThemeData>() => ThemeDataRef<T>;
//#endregion
//#region src/client/composables/useThemeLocaleData.d.ts
/**
 * Theme locale data ref type
 *
 * 主题多语言数据响应式引用类型
 */
type ThemeLocaleDataRef<T extends LocaleData = LocaleData> = ComputedRef<T>;
declare const themeLocaleDataSymbol: InjectionKey<ThemeLocaleDataRef>;
/**
 * Use theme locale data
 *
 * 使用主题多语言数据
 *
 * @returns Theme locale data ref object / 主题多语言数据响应式引用对象
 */
declare const useThemeLocaleData: <T extends ThemeData = ThemeData>() => ThemeLocaleDataRef<T>;
/**
 * Merge the locales fields to the root fields according to the route path
 *
 * 根据路由路径将多语言字段合并到根字段
 *
 * @param theme - Theme data object / 主题数据对象
 * @param routeLocale - Route locale / 路由多语言配置
 * @returns Merged theme data / 合并后的主题数据
 */
declare const resolveThemeLocaleData: (theme: ThemeData, routeLocale: RouteLocale) => ThemeData;
//#endregion
//#region src/client/composables/setupDevtools.d.ts
/**
 * Setup Vue DevTools for theme data
 *
 * 为主题数据设置 Vue 开发者工具
 *
 * @param app - Vue app instance / Vue 应用实例
 * @param themeData - Theme data ref / 主题数据响应式引用
 * @param themeLocaleData - Theme locale data ref / 主题多语言数据响应式引用
 */
declare const setupDevTools: (app: App, themeData: ThemeDataRef, themeLocaleData: ThemeLocaleDataRef) => void;
//#endregion
export { ThemeData, ThemeDataRef, ThemeLocaleDataRef, resolveThemeLocaleData, setupDevTools, themeData, themeLocaleDataSymbol, useThemeData, useThemeLocaleData };
//# sourceMappingURL=index.d.ts.map