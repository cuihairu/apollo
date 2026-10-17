
# Coding & Contribution Guidelines

> Purpose: Establish unified coding standards, code review process, branch strategy,
> and commit requirements for the Apollo instance-based multiplayer game server project, so that high quality
> and consistency are maintained under multi-person collaboration.

## 1. Branches and Version Management

### 1.1 Main Branch Strategy
- `main`: stable branch, always releasable. Every feature must pass CI/tests before merging.
- `develop` (optional): for frequent iteration, features may merge here and be merged back into `main` periodically.
- Feature naming: `feature/<module>-<short-desc>`; bugfix naming: `fix/<issue-id>-<desc>`.

### 1.2 Commit Process
1. Create/update the feature branch.
2. Write code + unit tests.
3. Perform self-testing (UT, necessary integration tests).
4. Submit PR -> triggers CI (lint/UT/build).
5. Merge back into `main`/`develop` after review.

## 2. Coding Standards

### 2.1 C++ Style
- Use a variant of Google C++ Style:
  - File names in `snake_case`, class names in `PascalCase`, functions in `CamelCase`, variables in `snake_case`.
  - Use `#pragma once`; prefer `std::unique_ptr`/`std::shared_ptr`.
  - No raw `new`/`delete`; use RAII or memory pools.
  - Use `constexpr` for constants.
  - Namespace structure `apollo::<module>`.
- Adopt clang-format (config file `.clang-format`) and clang-tidy checks.
- Emphasize noexcept/const correctness; use modern types such as `std::span`/`std::string_view` in interfaces.

### 2.2 C#/Unity
- Use the official Unity C# style (PascalCase classes/methods, camelCase fields).
- Use `async`/`await` for asynchrony; avoid `Thread.Sleep`.
- All SDK APIs must provide XML doc comments.

### 2.3 Configuration and Scripts
- YAML/JSON use 2-space indentation.
- Lua scripts follow the "comments where needed" / "small functions" principles; global variable pollution is forbidden.
- Proto files are split by field number ranges; comments include both Chinese and English descriptions.

## 3. Documentation Requirements

- Every module must have a design document (Markdown) kept up to date.
- If a PR modifies protocol/configuration, the documentation and generated files must be updated in sync.
- Keep README/CHANGELOG current, recording important changes.
- Use VitePress built-in internationalization: Chinese is the default at `docs/<path>.md`; English uses `docs/en/<path>.md`. Keep matching relative page paths.
- Configure language, navigation, and theme labels through `locales.root` / `locales.en` in `docs/.vitepress/config.mts`. Local search translations belong in shared `themeConfig.search.options.locales`.
- Use one prose language per page. Do not concatenate translations or use `.zh.md` / `.en.md` suffixes for site languages. Preserve API identifiers and real external-engine names.
- Keep page links, home actions, and previous/next navigation in the current locale. Explicitly label intentional cross-language references.
- Run `python3 scripts/ci/docs_i18n_check.py` before a production VitePress build. Respect workspace dependency-directory restrictions; use an external isolated directory for local builds.

## 4. Testing and Quality Gates
- Commits must pass CI's UT/format checks.
- New features require test cases; major defect fixes require regression tests.
- Performance-sensitive modules should attach benchmark data or stress-test results in the PR (when there is an impact).

## 5. Code Review
- At least 1 reviewer (2 for core modules).
- Focus checks: thread safety, memory management, exception handling, protocol compatibility.
- Recommended PR size is under 500 lines of changes; larger ones need splitting or prior discussion.
- After review, use squash/merge consistently (preserving a clear history).

## 6. Issue and Task Management
- Use Jira/GitHub Issues; every Issue includes: background, requirements, acceptance criteria.
- PRs must reference an Issue ID.
- Task labels: `architecture`, `network`, `data`, `sdk`, `ops`, `bug`, etc.

## 7. Risk and Security
- Do not write secrets/passwords into the codebase; test data with fake values must be labeled as such.
- Third-party dependencies must register their version in a manifest and undergo security scanning (SBOM).
- Introducing a new dependency requires license-compatibility assessment.

## 8. Release and Tags
- Each release is tagged on Git as `vX.Y.Z`, with a Release Note (features, added documentation, compatibility).
- The corresponding SDK/server version must be written into the manifest for deployment/client reference.

## 9. Incident Handling
- Production issues must be recorded in an Incident document (timeline, root cause, fix, prevention).
- Close the Issue only after submitting the post-mortem.

## 10. Roadmap
1. Submit the `.clang-format`, `.clang-tidy`, `.editorconfig` configuration files.
2. Establish CI quality gates (format/UT/build).
3. Check in the documentation template and PR template (`.github/PULL_REQUEST_TEMPLATE.md`).
4. Hold regular Code Review training/sharing sessions to improve team consistency.

---

This guideline provides developers with a unified collaboration standard and should be continuously refined as the team practices evolve.
