/* Apollo docs locale auto-switch.
 *
 * First visit (no stored choice): follow the browser's preferred language,
 * redirecting between the zh root (/) and the English locale (/en/) while
 * keeping every page's path, so a shared deep link lands on the same page in
 * the target locale. Once the visitor explicitly picks a locale in-app, that
 * choice is remembered by the theme (see theme/index.ts, key apollo-docs-lang)
 * and the auto-switch stays off for this browser.
 *
 * Keeps root=zh: the zh locale lives at /, English at /en/, and /zh/ is only a
 * rewrites-compat prefix (a served /zh/ URL is a 404). Redirect targets are
 * therefore limited to the two locale roots — see docs/审计-文档一致性.md §面二.
 *
 * The deploy base is injected by config.mts as window.__APOLLO_BASE__ right
 * before this script is inlined (dev base '/', prod base '/apollo/').
 */
;(function () {
  'use strict'

  function pick(langs) {
    for (let i = 0; i < langs.length; i++) {
      const l = String(langs[i] || '').toLowerCase()
      if (l.indexOf('en') === 0) return 'en'
      if (l.indexOf('zh') === 0) return 'zh'
    }
    return ''
  }

  function resolve(base, pathname, want) {
    const EN = base + 'en/'
    let p = pathname
    if (p === base.slice(0, -1)) p = base
    const inEn = p === EN || p === base + 'en' || p.indexOf(EN) === 0
    if (want === 'en' && !inEn) {
      const tail = p.indexOf(base) === 0 ? p.slice(base.length) : p
      return EN + tail.replace(/^\/+/, '')
    }
    if (want === 'zh' && inEn) {
      const t = p === base + 'en' || p === EN ? '' : p.slice(EN.length)
      return base + t
    }
    return null
  }

  function run() {
    try {
      const KEY = 'apollo-docs-lang'
      const s = window.localStorage
      if (!s || s.getItem(KEY) !== null) return
      const want = pick(navigator.languages || [navigator.language || ''])
      if (!want) return
      const target = resolve(window.__APOLLO_BASE__ || '', location.pathname, want)
      if (target) location.replace(target + location.search + location.hash)
    } catch (e) {
      /* never break the site */
    }
  }

  if (typeof window !== 'undefined') run()

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { pick, resolve }
  }
})()
