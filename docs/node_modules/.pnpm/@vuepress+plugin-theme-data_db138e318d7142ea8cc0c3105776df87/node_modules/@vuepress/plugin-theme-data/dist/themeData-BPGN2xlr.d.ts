import { LocaleConfig, LocaleData } from "vuepress/shared";

//#region src/shared/themeData.d.ts
/**
 * Theme data with locale support
 *
 * 支持多语言的主题数据
 */
type ThemeData<T extends LocaleData = LocaleData> = Omit<T, 'locales'> & {
  /**
   * Locale configurations
   *
   * 多语言配置
   */
  locales?: LocaleConfig<T>;
};
//#endregion
export { ThemeData as t };
//# sourceMappingURL=themeData-BPGN2xlr.d.ts.map