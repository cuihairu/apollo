---
title: 依赖安全与升级
icon: shield
order: 8
---

# 依赖安全与升级（dependabot 台账）

docs 站点工具链的 dependabot 告警台账：逐条登记处置结果（fixed / blocked），
blocked 项挂**复核触发条件**，触发即重估，未触发不追。

## 工具链现状

| 项 | 值 |
|---|---|
| 包管理器 | pnpm（CI `docs.yml` 用 pnpm 9 + node 24，lockfileVersion 9.0 兼容） |
| 直依赖 | `docs/package.json` devDeps：mermaid `^12.1.0` / vue `^3.5.43` / vitepress `^1.6.4` |
| 唯一 lockfile | `docs/pnpm-lock.yaml`（原 `docs/package-lock.json` 系 npm 残留、CI 从不读取，2026-10-11 删除——双 lock 期同一包告警翻倍） |
| 验证门禁 | `pnpm install && pnpm run docs:build`（与 CI 同口径） |

## 2026-10-11 告警清账（19 条全处置）

| 包 | 告警 | 锁定 → 处置后 | 处置 | 说明 |
|---|---|---|---|---|
| @vue/server-renderer | #95 | 3.5.30 → 3.5.43 | fixed | vue 直依赖升 `^3.5.43`（af30531a） |
| dompurify | #98 #91 | 3.4.13 → 3.4.16 | fixed | mermaid 允许范围（`^3.4.12`）内 `pnpm update` |
| postcss | #85 #67 | 8.5.12 → 8.5.29 | fixed | vite 允许范围（`^8.4.43`）内 `pnpm update` |
| nanoid | #90 | 3.3.11 → 3.3.20 | fixed | postcss 允许范围内 `pnpm update` |
| source-map-js | #96 | 1.2.1 → 1.2.2 | fixed | postcss 允许范围内 `pnpm update` |
| vite | #63 #61 #42 | 5.4.21 → 6.4.4 | fixed | `pnpm.overrides` 钉 6.4.4，build 实测绿 |
| esbuild | #1 | 0.21.5 → 0.25.12 | fixed | 随 vite 6.4.4 上游自动解（其声明 `^0.25.0`） |
| katex | #97 | 0.16.47（修复版 ≥0.18.2） | **blocked** | 见下节 |
| package-lock 重复件 | #92 #94 #93 #40 #64 #62 #41 | — | cleared | 弃用 lockfile 删除，告警随清单消失 |

vite 一行的 override 与 katex 不取 override 的差别：vite 是 vitepress 的构建引擎
（外部包、构建期实际加载），override 后走的就是 6.4.4 的真实代码，`docs:build`
两阶段（bundle + render）全绿即为验证；katex 的漏洞代码被 mermaid 内联出货，
override 独立包动不到它——纸面修复，见下节。

## blocked 登记

### katex（#97）——mermaid 内联钉死

- **状态**：blocked（2026-10-11 登记）
- **告警**：katex `>= 0.11.0, < 0.18.2`，修复版 0.18.2+（现 latest 0.18.11）。
- **卡点**：mermaid 12.1.0（latest）将 katex 0.16.47 **内联进预构建 dist**：
  `dist/chunks/mermaid.esm/katex-F5DCXIFN.mjs` 头注
  `// ../../node_modules/.pnpm/katex@0.16.47/...`，经内部
  `import("./katex-F5DCXIFN.mjs")` 加载；全 dist 无一处 import 外部 katex 包。
  pnpm override 换独立 katex 包只改 lockfile，mermaid 实际执行的仍是内联的
  0.16.47——告警清了、漏洞代码照旧出货，纸面修复不取。
- **缓解**：本站 13 个 ```` ```mermaid ```` 块无一处 `$$` 数学定界符；katex chunk
  仅在文本出现数学定界符时才动态加载（`hasKatex` 探测），实际渲染路径不触达。
- **复核触发条件**（满足其一即重估）：
  1. mermaid 发布版本 changelog 写明内联 katex ≥0.18.2（或 dist 不再内联、改为
     外部依赖）→ `pnpm update mermaid` 后 `pnpm why katex` 复核锁定版本，重跑
     `pnpm run docs:build`；
  2. 抽查法：对候选 mermaid 版本 `grep -r "katex@" <pkg>/dist/chunks`，内联注释
     版本 ≥0.18.2 即可升级重验。

## 复跑口径

1. `gh api "repos/cuihairu/apollo/dependabot/alerts?state=open"` 列在案告警；
2. 逐条 `npm view <pkg>` 查修复版本：落入上游 semver 范围 → `pnpm update <pkg>`；
   未落入 → `pnpm why <pkg>` 看钉链，override 能否**真实生效**（改的是实际执行
   代码，不只是 lockfile 数字）；
3. 改动后 `pnpm install && pnpm run docs:build` 全绿才提交推送；同批跑 C++ 三树
   门禁（本轮随批 43/41/37 全绿）；
4. blocked 项按上节触发条件重估，触发不了不动。
