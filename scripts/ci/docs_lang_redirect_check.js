#!/usr/bin/env node
/**
 * Regression check for the Apollo docs locale auto-switch.
 *
 * Covers the pre-hydration redirect logic (docs/.vitepress/lang-redirect.js):
 * browser-language detection, zh root <-> /en/ mapping with path preservation,
 * explicit-choice memory, and the dev/prod base injection. Run with --dist to
 * additionally assert every built page embeds the script with the prod base.
 *
 * The logic must stay first-visit-only: once the visitor picks a locale in-app
 * (theme/index.ts writes apollo-docs-lang), the auto-switch stays off.
 */
'use strict'

const assert = require('node:assert/strict')
const fs = require('node:fs')
const path = require('node:path')
const vm = require('node:vm')

const ROOT = path.resolve(__dirname, '..', '..')
const SRC = path.join(ROOT, 'docs', '.vitepress', 'lang-redirect.js')
const source = fs.readFileSync(SRC, 'utf-8')

let failures = 0
function test(name, fn) {
  try {
    fn()
    console.log(`  ok - ${name}`)
  } catch (err) {
    failures++
    console.error(`  FAIL - ${name}\n    ${err.message}`)
  }
}

// ---------------------------------------------------------------------------
// Unit: pure pick / resolve functions (exported via the module guard).
// ---------------------------------------------------------------------------
const { pick, resolve } = require(SRC)

function unit(name, fn) {
  test(name, fn)
}

unit('pick: first matching browser language wins', () => {
  assert.equal(pick(['en-US']), 'en')
  assert.equal(pick(['zh-CN']), 'zh')
  assert.equal(pick(['fr-FR', 'en-US']), 'en')
  assert.equal(pick(['ja-JP', 'zh-CN']), 'zh')
  assert.equal(pick(['EN-gb']), 'en')
  assert.equal(pick(['ZH']), 'zh')
})

unit('pick: unknown languages fall through to no redirect', () => {
  assert.equal(pick(['fr-FR']), '')
  assert.equal(pick([]), '')
  assert.equal(pick(['ko-KR', 'pt-BR']), '')
})

unit('resolve: zh -> en keeps path, prod base', () => {
  assert.equal(resolve('/apollo/', '/apollo/', 'en'), '/apollo/en/')
  assert.equal(resolve('/apollo/', '/apollo/guide/installation', 'en'), '/apollo/en/guide/installation')
  assert.equal(resolve('/apollo/', '/apollo/architecture/overview', 'en'), '/apollo/en/architecture/overview')
})

unit('resolve: en -> zh strips locale prefix, prod base', () => {
  assert.equal(resolve('/apollo/', '/apollo/en/', 'zh'), '/apollo/')
  assert.equal(resolve('/apollo/', '/apollo/en/guide/installation', 'zh'), '/apollo/guide/installation')
})

unit('resolve: dev base "/"', () => {
  assert.equal(resolve('/', '/', 'en'), '/en/')
  assert.equal(resolve('/', '/guide/installation', 'en'), '/en/guide/installation')
  assert.equal(resolve('/', '/en/', 'zh'), '/')
  assert.equal(resolve('/', '/en/guide/installation', 'zh'), '/guide/installation')
})

unit('resolve: no redirect when already in the wanted locale', () => {
  assert.equal(resolve('/apollo/', '/apollo/en/', 'en'), null)
  assert.equal(resolve('/apollo/', '/apollo/', 'zh'), null)
  assert.equal(resolve('/apollo/', '/apollo/en/guide', 'en'), null)
})

unit('resolve: missing trailing slash on locale roots', () => {
  assert.equal(resolve('/apollo/', '/apollo', 'en'), '/apollo/en/')
  assert.equal(resolve('/apollo/', '/apollo/en', 'zh'), '/apollo/')
  assert.equal(resolve('/', '/en', 'zh'), '/')
})

// ---------------------------------------------------------------------------
// Integration: the inlined run() path with browser-ish globals.
// ---------------------------------------------------------------------------
function runInline({ base, pathname, languages, language, stored, search, hash }) {
  const store = stored === undefined ? {} : { 'apollo-docs-lang': stored }
  let replaced = null
  const sandbox = {
    window: {
      __APOLLO_BASE__: base,
      localStorage: {
        getItem: (k) => Object.prototype.hasOwnProperty.call(store, k) ? store[k] : null,
        setItem: (k, v) => { store[k] = v },
      },
    },
    location: {
      pathname,
      search: search || '',
      hash: hash || '',
      replace: (target) => { replaced = target },
    },
    navigator: languages ? { languages } : { language },
  }
  vm.createContext(sandbox)
  vm.runInContext(source, sandbox)
  return { replaced, store }
}

test('inline: en browser on zh homepage redirects to /en/ preserving nothing', () => {
  const { replaced } = runInline({ base: '/apollo/', pathname: '/apollo/', languages: ['en-US'] })
  assert.equal(replaced, '/apollo/en/')
})

test('inline: en browser on zh deep link preserves path, search and hash', () => {
  const { replaced } = runInline({
    base: '/apollo/', pathname: '/apollo/guide/installation', languages: ['en-GB'],
    search: '?theme=dark', hash: '#install',
  })
  assert.equal(replaced, '/apollo/en/guide/installation?theme=dark#install')
})

test('inline: zh browser on en page redirects back to zh', () => {
  const { replaced } = runInline({
    base: '/apollo/', pathname: '/apollo/en/guide/installation', languages: ['zh-CN'],
  })
  assert.equal(replaced, '/apollo/guide/installation')
})

test('inline: stored explicit choice disables the auto-switch', () => {
  const { replaced } = runInline({
    base: '/apollo/', pathname: '/apollo/', languages: ['en-US'], stored: 'en',
  })
  assert.equal(replaced, null)
})

test('inline: unknown browser language never redirects', () => {
  const zh = runInline({ base: '/apollo/', pathname: '/apollo/', languages: ['fr-FR'] })
  assert.equal(zh.replaced, null)
  const en = runInline({ base: '/apollo/', pathname: '/apollo/en/', languages: ['fr-FR'] })
  assert.equal(en.replaced, null)
})

test('inline: navigator.language fallback when languages is absent', () => {
  const { replaced } = runInline({ base: '/', pathname: '/guide', language: 'en-US' })
  assert.equal(replaced, '/en/guide')
})

test('inline: never breaks on missing storage or thrown access', () => {
  const sandbox = {
    window: { __APOLLO_BASE__: '/apollo/', localStorage: { getItem: () => { throw new Error('denied') } } },
    location: { pathname: '/apollo/', search: '', hash: '', replace: () => {} },
    navigator: { languages: ['en-US'] },
  }
  vm.createContext(sandbox)
  assert.doesNotThrow(() => vm.runInContext(source, sandbox))
})

// ---------------------------------------------------------------------------
// Config / artifact wiring.
// ---------------------------------------------------------------------------
test('config.mts injects the script into head with the deploy base', () => {
  const cfg = fs.readFileSync(path.join(ROOT, 'docs', '.vitepress', 'config.mts'), 'utf-8')
  assert.match(cfg, /lang-redirect\.js/)
  assert.match(cfg, /window\.__APOLLO_BASE__='\$\{langRedirectBase\}';/)
  assert.match(cfg, /\['script', \{\}, langRedirectSrc\]/)
})

const distArg = process.argv.indexOf('--dist')
if (distArg !== -1) {
  const dist = path.resolve(ROOT, process.argv[distArg + 1] || 'docs/.vitepress/dist')
  const html = []
  ;(function walk(dir) {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name)
      if (entry.isDirectory()) walk(full)
      else if (entry.name.endsWith('.html')) html.push(full)
    }
  })(dist)
  assert.ok(html.length > 0, `no built html under ${dist}`)
  // The head script is esbuild-minified before inlining: quotes may be rewritten
  // and identifiers kept, so match the semantics rather than the raw source.
  const baseLiteral = /__APOLLO_BASE__\s*=\s*['"]\/apollo\/['"]/
  let missing = 0
  for (const file of html) {
    const content = fs.readFileSync(file, 'utf-8')
    if (!content.includes('apollo-docs-lang') || !baseLiteral.test(content)) missing++
  }
  console.log(`  dist: checked ${html.length} html files, ${missing} missing the script`)
  if (missing > 0) {
    failures += missing
  }
  const enIndex = path.join(dist, 'en', 'index.html')
  if (fs.existsSync(enIndex)) {
    const content = fs.readFileSync(enIndex, 'utf-8')
    assert.match(content, /apollo-docs-lang/, 'en/index.html embeds the auto-switch')
  }
}

if (failures > 0) {
  console.error(`\n${failures} failure(s)`)
  process.exit(1)
}
console.log('\nall docs locale auto-switch checks passed')
