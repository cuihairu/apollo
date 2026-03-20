//#region src/client/composables/useActiveHeaderLinks.d.ts
interface UseActiveHeaderLinksOptions {
  headerLinkSelector: string;
  headerAnchorSelector: string;
  delay: number;
  offset?: number;
}
declare const useActiveHeaderLinks: ({
  headerLinkSelector,
  headerAnchorSelector,
  delay,
  offset
}: UseActiveHeaderLinksOptions) => void;
//#endregion
export { UseActiveHeaderLinksOptions, useActiveHeaderLinks };
//# sourceMappingURL=index.d.ts.map