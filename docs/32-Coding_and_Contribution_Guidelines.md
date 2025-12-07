# Coding & Contribution Guidelines

> 目的：为 Apollo MMORPG 服务器项目制定统一的编码规范、代码审核流程、分支策略与提交要求，确保多人协作时保持高质量与一致性。

## 1. 分支与版本管理

### 1.1 主分支策略
- `main`：稳定分支，始终可发布。所有功能 feature 合入前必须经过 CI/测试。
- `develop`（可选）：需要频繁迭代时，可在其上合并 feature，定期回合 `main`。
- Feature 命名：`feature/<module>-<short-desc>`；Bugfix 命名：`fix/<issue-id>-<desc>`。

### 1.2 提交流程
1. 创建/更新 feature 分支。
2. 编写代码 + 单元测试。
3. 进行自测（UT、必要的集成测试）。
4. 提交 PR -> 触发 CI（lint/UT/构建）。
5. 通过评审后合并回 `main`/`develop`。

## 2. 编码规范

### 2.1 C++ 风格
- 采用 Google C++ Style 的变体：
  - 文件名 `snake_case`，类名 `PascalCase`，函数 `CamelCase`，变量 `snake_case`。
  - 使用 `#pragma once`，优先 `std::unique_ptr`/`std::shared_ptr`。
  - 禁止裸 `new/delete`，使用 RAII 或内存池。
  - 常量使用 `constexpr`。
  - 命名空间 `apollo::<module>` 结构。
- 引入 clang-format（配置文件 `.clang-format`）与 clang-tidy 检查。
- 注重 noexcept/const correctness；接口使用 `std::span/std::string_view` 等现代类型。

### 2.2 C#/Unity
- 采用 Unity 官方 C# Style（PascalCase 类/方法，camelCase 字段）。
- 使用 `async/await` 管理异步；避免 `Thread.Sleep`。
- 所有 SDK API 提供 XML 文档注释。

### 2.3 配置与脚本
- YAML/JSON 使用 2 空格缩进。
- Lua 脚本遵循 “需要注释”“小函数” 原则，禁止全局变量污染。
- Proto 文件按字段编号分段，注释包含中文/英文说明。

## 3. 文档要求
- 每个模块必须有设计文档（Markdown）并保持更新。
- PR 如修改协议/配置，需同步更新文档和生成文件。
- README/CHANGELOG 保持最新，记录重要变更。

## 4. 测试与质量门禁
- 提交必须通过 CI 的 UT/格式检查。
- 新功能需附带测试用例；重大缺陷修复需增加回归测试。
- 性能敏感模块在 PR 中附带基准数据或压测结果（若有影响）。

## 5. 代码评审
- 至少 1 名 reviewer（核心模块 2 名）。
- 重点检查：线程安全、内存管理、异常处理、协议兼容性。
- PR 尺寸建议 < 500 行改动，超出需拆分或提前沟通。
- 评审完成后统一 squash/merge（保留清晰历史）。

## 6. Issue 与任务管理
- 使用 Jira/GitHub Issues；每个 Issue 包含：背景、需求、验收标准。
- PR 需关联 Issue ID。
- 任务标签：`architecture`, `network`, `data`, `sdk`, `ops`, `bug` 等。

## 7. 风险与安全
- 禁止将密钥/密码写入代码库；测试用假数据也需标注。
- 第三方依赖需在清单中登记版本，进行安全扫描（SBOM）。
- 引入新依赖需评估许可证兼容性。

## 8. Release 与标签
- 每次发布在 Git 上打 tag `vX.Y.Z`，附 Release Note（功能、新增文档、兼容性）。
- 对应 SDK/服务器版本需写入 manifest，供部署/客户端参考。

## 9. 故障处理
- 线上问题需记录在 Incident 文档中（时间线、根因、修复、预防）。
- 提交 post-mortem 后再关闭 Issue。

## 10. Roadmap
1. 完成 `.clang-format`、`.clang-tidy`、`.editorconfig` 配置提交。
2. 建立 CI 质量门禁（格式/UT/构建）。
3. 文档模板与 PR 模板入库（`.github/PULL_REQUEST_TEMPLATE.md`）。
4. 定期 Code Review 培训/分享，提升团队一致性。

---

此指南为开发者提供统一协作规范，应随团队实践持续完善。***
