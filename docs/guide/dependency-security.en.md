---
title: Dependency Security & Upgrades
icon: shield
order: 8
---

[English](dependency-security.en.md) | [中文](dependency-security.md)

# Dependency security & upgrades (dependabot ledger)

The dependabot alert ledger for the docs site toolchain: each alert's disposition is registered (fixed / blocked),
and blocked entries carry a **re-review trigger condition** — re-evaluate when triggered, do not chase otherwise.

## Toolchain status

| Item | Value |
|---|---|
| Package manager | pnpm (CI `docs.yml` uses pnpm 9 + node 24, lockfileVersion 9.0 compatible) |
| Direct deps | `docs/package.json` devDeps: mermaid `^12.1.0` / vue `^3.5.43` / vitepress `^1.6.4` |
| Sole lockfile | `docs/pnpm-lock.yaml` (the former `docs/package-lock.json` was an npm leftover that CI never read; deleted 2026-10-11 — during the dual-lock period the same package's alerts doubled) |
| Verification gate | `pnpm install && pnpm run docs:build` (same baseline as CI) |

## 2026-10-11 alert settlement (all 19 disposed)

| Package | Alert | Locked → after | Disposition | Notes |
|---|---|---|---|---|
| @vue/server-renderer | #95 | 3.5.30 → 3.5.43 | fixed | vue direct dep bumped to `^3.5.43` (af30531a) |
| dompurify | #98 #91 | 3.4.13 → 3.4.16 | fixed | `pnpm update` within mermaid's allowed range (`^3.4.12`) |
| postcss | #85 #67 | 8.5.12 → 8.5.29 | fixed | `pnpm update` within vite's allowed range (`^8.4.43`) |
| nanoid | #90 | 3.3.11 → 3.3.20 | fixed | `pnpm update` within postcss's allowed range |
| source-map-js | #96 | 1.2.1 → 1.2.2 | fixed | `pnpm update` within postcss's allowed range |
| vite | #63 #61 #42 | 5.4.21 → 6.4.4 | fixed | `pnpm.overrides` pinned to 6.4.4, build verified green |
| esbuild | #1 | 0.21.5 → 0.25.12 | fixed | auto-resolved upstream with vite 6.4.4 (it declares `^0.25.0`) |
| katex | #97 | 0.16.47 (fixed version ≥0.18.2) | **blocked** | see the section below |
| package-lock duplicates | #92 #94 #93 #40 #64 #62 #41 | — | cleared | deprecated lockfile deleted; the alerts vanished with the list |

The difference between the vite line taking an override and katex not: vite is vitepress's build engine
(an external package, actually loaded at build time); after the override what runs is the real 6.4.4 code, and both
`docs:build` phases (bundle + render) coming back green is the verification. katex's vulnerable code is shipped inlined
into mermaid's dist, so overriding the standalone package cannot touch it — a paper fix; see the section below.

## Blocked entries

### katex (#97) — pinned inline inside mermaid

- **Status**: blocked (registered 2026-10-11)
- **Alert**: katex `>= 0.11.0, < 0.18.2`, fixed in 0.18.2+ (current latest 0.18.11).
- **Blocker**: mermaid 12.1.0 (latest) inlines katex 0.16.47 **into its prebuilt dist**:
  the header of `dist/chunks/mermaid.esm/katex-F5DCXIFN.mjs` reads
  `// ../../node_modules/.pnpm/katex@0.16.47/...`, loaded via the internal
  `import("./katex-F5DCXIFN.mjs")`; nowhere in the dist is an external katex package imported.
  A pnpm override swapping the standalone katex package only changes the lockfile — what mermaid actually executes
  remains the inlined 0.16.47. The alert clears, the vulnerable code still ships; a paper fix, not accepted.
- **Mitigation**: none of this site's 23 ```` ```mermaid ```` blocks contains a `$$` math delimiter; the katex chunk
  is only dynamically loaded when math delimiters appear in text (`hasKatex` probing), so the actual render path never touches it.
- **Re-review trigger conditions** (any one met triggers re-evaluation):
  1. a mermaid release changelog states the inlined katex is ≥0.18.2 (or the dist stops inlining and moves to
     an external dependency) → after `pnpm update mermaid`, re-check the locked version with `pnpm why katex` and re-run
     `pnpm run docs:build`;
  2. spot-check method: for a candidate mermaid version run `grep -r "katex@" <pkg>/dist/chunks`; an inline-comment
     version ≥0.18.2 means it can be upgraded and re-verified.

## Re-run procedure

1. `gh api "repos/cuihairu/apollo/dependabot/alerts?state=open"` lists the alerts on record;
2. for each, `npm view <pkg>` to find the fixed version: if it falls inside the upstream semver range → `pnpm update <pkg>`;
   if not → `pnpm why <pkg>` to inspect the pin chain and whether an override can **actually take effect**
   (changing the code that actually executes, not just a lockfile number);
3. after changes, `pnpm install && pnpm run docs:build` must be fully green before committing and pushing; run the C++ three-tree gate
   in the same batch (this round went green with batches 43/41/37);
4. re-evaluate blocked items against the trigger conditions above; no trigger, no action.
