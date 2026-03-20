import { LocaleData, ensureEndingSlash, ensureLeadingSlash, isFunction, isLinkExternal, isLinkHttp, isLinkWithProtocol, isPlainObject, isString, removeEndingSlash, removeLeadingSlash } from "vuepress/shared";

//#region src/shared/deepAssign.d.ts
type IAnyObject = Record<string, any>;
/**
 * Deep merge objects to the first one
 *
 * 深度合并对象到第一个对象
 *
 * @param originObject - The target object to merge into / 要合并到的目标对象
 * @param overrideObjects - Objects to merge from / 要合并的对象
 *
 * @returns Merged object / 合并后的对象
 *
 * @example
 * ```ts
 * const obj1 = { a: 1, b: { c: 2 } }
 * const obj2 = { b: { d: 3 }, e: 4 }
 * deepAssign(obj1, obj2) // { a: 1, b: { c: 2, d: 3 }, e: 4 }
 * ```
 */
declare const deepAssign: <OriginType extends IAnyObject, MergeType extends IAnyObject = OriginType, FinalType extends Partial<OriginType> & Partial<MergeType> = OriginType & MergeType>(originObject: OriginType, ...overrideObjects: (MergeType | null | undefined)[]) => FinalType;
//#endregion
//#region src/shared/date.d.ts
/**
 * Get Date info from user input
 *
 * 从用户输入获取日期信息
 *
 * @param input - The input to parse / 要解析的输入
 *
 * @returns Date object or null if input is invalid / 日期对象，如果输入无效则返回 null
 *
 * @example
 * ```ts
 * getDate('2023-01-01') // Date object
 * getDate(1640995200000) // Date object
 * getDate('invalid') // null
 * ```
 */
declare const getDate: (input: unknown) => Date | null;
/**
 * Date sorter from latest to oldest
 *
 * 从最新到最旧的日期排序器
 *
 * @param valueA - First date value / 第一个日期值
 * @param valueB - Second date value / 第二个日期值
 *
 * @returns Comparison result / 比较结果
 *
 * @example
 * ```ts
 * const dates = ['2023-01-01', '2023-12-31', '2023-06-15']
 * dates.sort(dateSorter) // ['2023-12-31', '2023-06-15', '2023-01-01']
 * ```
 */
declare const dateSorter: (valueA: Date | number | string | undefined, valueB: Date | number | string | undefined) => number;
//#endregion
//#region src/shared/header.d.ts
type HeaderLevels = number | 'deep' | false | [number, number];
interface GetHeadersOptions {
  /**
   * The selector of the headers.
   *
   * It will be passed as an argument to `document.querySelectorAll(selector)`,
   * so you should pass a `CSS Selector` string.
   *
   * @default '[vp-content] h1, [vp-content] h2, [vp-content] h3, [vp-content] h4, [vp-content] h5, [vp-content] h6'
   */
  selector?: string;
  /**
   * Ignore specific elements within the header.
   *
   * The Array of `CSS Selector`
   *
   * @default []
   */
  ignore?: string[];
  /**
   * The levels of the headers.
   *
   * `1` to `6` for `<h1>` to `<h6>`
   *
   * - `false`: No headers.
   * - `number`: only headings of that level will be displayed.
   * - `[number, number]: headings level tuple, where the first number should be less than the second number, for example, `[2, 4]` which means all headings from `<h2>` to `<h4>` will be displayed.
   * - `deep`: same as `[2, 6]`, which means all headings from `<h2>` to `<h6>` will be displayed.
   *
   * @default 2
   */
  levels?: HeaderLevels;
}
//#endregion
//#region src/shared/helper.d.ts
/**
 * Check if a value is defined
 *
 * 检查值是否已定义
 *
 * @param val - The value to check / 要检查的值
 *
 * @returns Whether the value is defined / 值是否已定义
 *
 * @example
 * ```ts
 * isDef(undefined) // false
 * isDef(null) // true
 * isDef(0) // true
 * ```
 */
declare const isDef: <T = any>(val?: T) => val is T;
/**
 * Check if a value is boolean
 *
 * 检查值是否为布尔值
 *
 * @param val - The value to check / 要检查的值
 *
 * @returns Whether the value is boolean / 值是否为布尔值
 *
 * @example
 * ```ts
 * isBoolean(true) // true
 * isBoolean(false) // true
 * isBoolean('true') // false
 * ```
 */
declare const isBoolean: (val: any) => val is boolean;
/**
 * Check if a value is number
 *
 * 检查值是否为数字
 *
 * @param val - The value to check / 要检查的值
 *
 * @returns Whether the value is number / 值是否为数字
 *
 * @example
 * ```ts
 * isNumber(42) // true
 * isNumber('42') // false
 * isNumber(NaN) // true
 * ```
 */
declare const isNumber: (val: any) => val is number;
/**
 * Check if a value is array
 *
 * 检查值是否为数组
 *
 * @param val - The value to check / 要检查的值
 * @returns Whether the value is array / 值是否为数组
 */
declare const isArray: <ItemType>(val: unknown) => val is ItemType[];
/**
 * Check if a value is regexp
 *
 * 检查值是否为正则表达式
 *
 * @param val - The value to check / 要检查的值
 *
 * @returns Whether the value is regexp / 值是否为正则表达式
 *
 * @example
 * ```ts
 * isRegExp(/test/) // true
 * isRegExp('test') // false
 * ```
 */
declare const isRegExp: (val: unknown) => val is RegExp;
/**
 * Check if a value is starting with the given prefix
 *
 * 检查值是否以给定前缀开头
 *
 * @param str - The string to check / 要检查的字符串
 * @param prefix - The prefix to match / 要匹配的前缀
 *
 * @returns Whether the string starts with the prefix / 字符串是否以前缀开头
 *
 * @example
 * ```ts
 * startsWith('hello world', 'hello') // true
 * startsWith('hello world', 'world') // false
 * startsWith(123, 'hello') // false
 * ```
 */
declare const startsWith: (str: unknown, prefix: string) => boolean;
/**
 * Check if a value is ending with the given suffix
 *
 * 检查值是否以给定后缀结尾
 *
 * @param str - The string to check / 要检查的字符串
 * @param suffix - The suffix to match / 要匹配的后缀
 *
 * @returns Whether the string ends with the suffix / 字符串是否以后缀结尾
 *
 * @example
 * ```ts
 * endsWith('hello world', 'world') // true
 * endsWith('hello world', 'hello') // false
 * endsWith(123, 'world') // false
 * ```
 */
declare const endsWith: (str: unknown, suffix: string) => boolean;
declare const entries: {
  <T>(o: {
    [s: string]: T;
  } | ArrayLike<T>): [string, T][];
  (o: {}): [string, any][];
};
declare const fromEntries: {
  <T = any>(entries: Iterable<readonly [PropertyKey, T]>): {
    [k: string]: T;
  };
  (entries: Iterable<readonly any[]>): any;
};
declare const keys: <ObjectType extends object>(obj: ObjectType) => `${keyof ObjectType & (string | number | boolean | null | undefined)}`[];
declare const values: {
  <T>(o: {
    [s: string]: T;
  } | ArrayLike<T>): T[];
  (o: {}): any[];
};
//#endregion
//#region src/shared/link.d.ts
/**
 * Check if a value is a valid absolute url
 *
 * 检查值是否为有效的绝对 URL
 *
 * @param test - The value to test / 要测试的值
 *
 * @returns Whether the value is a valid absolute url / 值是否为有效的绝对 URL
 *
 * @example
 * ```ts
 * isLinkAbsolute('/path/to/page') // true
 * isLinkAbsolute('//example.com') // false
 * isLinkAbsolute('relative/path') // false
 * ```
 */
declare const isLinkAbsolute: (test: unknown) => boolean;
/**
 * Check if a link is relative
 *
 * 检查链接是否为相对链接
 *
 * @param link - The link to check / 要检查的链接
 *
 * @returns Whether the link is relative / 链接是否为相对链接
 *
 * @example
 * ```ts
 * isLinkRelative('path/to/page') // true
 * isLinkRelative('/absolute/path') // true
 * isLinkRelative('https://example.com') // false
 * ```
 */
declare const isLinkRelative: (link: string) => boolean;
//#endregion
//#region src/shared/locales.d.ts
/**
 * Generate locale config with exact locale data
 */
type ExactLocaleConfig<T extends LocaleData = LocaleData> = Record<string, T>;
//#endregion
//#region src/shared/key.d.ts
interface KeyOptions {
  /**
   * Value of `event.key` to trigger the hot key
   *
   * 热键的 `event.key` 值
   */
  key: string;
  /**
   * Whether to press `event.altKey` at the same time
   *
   * 是否同时按下 `event.altKey`
   *
   * @default false
   */
  alt?: boolean;
  /**
   * Whether to press `event.ctrlKey` at the same time
   *
   * 是否同时按下 `event.ctrlKey`
   *
   * @default false
   */
  ctrl?: boolean;
  /**
   * Whether to press `event.shiftKey` at the same time
   *
   * 是否同时按下 `event.shiftKey`
   *
   * @default false
   */
  shift?: boolean;
  /**
   * Whether to press `event.metaKey` at the same time
   *
   * 是否同时按下 `event.metaKey`
   *
   * @default false
   */
  meta?: boolean;
}
//#endregion
//#region src/shared/types.d.ts
type Primitive = Function | boolean | number | string | null | undefined;
type NotNill<T> = T extends null | undefined ? never : T;
/**
 * Recursively make all properties of an object required
 *
 * @template T - The type to make required
 * @returns The type with all properties required
 *
 * @example
 * ```ts
 * type A = {
 *   a?: {
 *     b?: string
 *    c?: number
 *   }
 * }
 *
 * type B = DeepRequired<A>
 * // B is now:
 * // type B = {
 * //   a: {
 * //     b: string
 * //     c: number
 * //   }
 * // }
 * ```
 */
type DeepRequired<T> = T extends Primitive ? NotNill<T> : { [Key in keyof T]-?: T[Key] extends (infer ItemType)[] ? DeepRequired<ItemType>[] : T[Key] extends readonly (infer ReadOnlyItemType)[] ? DeepRequired<ReadOnlyItemType> : DeepRequired<T[Key]> };
/**
 * A literal type that supports custom further strings but preserves autocompletion in IDEs.
 *
 * @see {@link https://github.com/microsoft/TypeScript/issues/29729#issuecomment-471566609 | copied from issue}
 */
type LiteralUnion<Union extends Base, Base = string> = Union | (Base & {
  IGNORE_ME?: never;
});
/**
 * Convert a union type to an intersection type using [distributive conditional types](https://www.typescriptlang.org/docs/handbook/release-notes/typescript-2-8.html#distributive-conditional-types).
 *
 */
type UnionToIntersection<Union> = (Union extends unknown ? (distributedUnion: Union) => void : never) extends ((mergedIntersection: infer Intersection) => void) ? // The `& Union` is to ensure result of `UnionToIntersection<A | B>` is always assignable to `A | B`
Intersection & Union : never;
//#endregion
export { DeepRequired, ExactLocaleConfig, GetHeadersOptions, HeaderLevels, KeyOptions, LiteralUnion, UnionToIntersection, dateSorter, deepAssign, endsWith, ensureEndingSlash, ensureLeadingSlash, entries, fromEntries, getDate, isArray, isBoolean, isDef, isFunction, isLinkAbsolute, isLinkExternal, isLinkHttp, isLinkRelative, isLinkWithProtocol, isNumber, isPlainObject, isRegExp, isString, keys, removeEndingSlash, removeLeadingSlash, startsWith, values };
//# sourceMappingURL=index.d.ts.map