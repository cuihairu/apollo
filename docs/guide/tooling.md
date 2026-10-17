---
title: 工具脚本
icon: terminal
order: 7
---

# 工具脚本

脚本服务于当前应用与本地验收，不定义额外的生产进程角色。

## 开发编队

`scripts/dev_fleet.sh` 使用 machined 管理 Login、Gateway、World，并按需启动 logger、admin 和模块装配示例。

```bash
scripts/dev_fleet.sh up
scripts/dev_fleet.sh status
scripts/dev_fleet.sh roster
scripts/dev_fleet.sh down
```

`APOLLO_BUILD_DIR` 指定构建目录，`APOLLO_FLEET_DIR` 指定运行目录。需要已构建相关应用；未构建的入口在 roster 中标记跳过。

| 开发端口 | 用途 |
|---|---|
| 9002 | Gateway 客户端 TCP |
| 9101 | World 玩法链路 |
| 9104 | World 玩家档案链路 |
| 9103 | Login 认证/准入链路 |
| 9105 | Login 在线目录 |
| 9102 | 日志收集 |
| 9106 / 9107 | 指标链路 / HTTP 指标 |

这些端口是脚本约定，不是统一配置来源。编队存活不能证明用户已经登录或副本已经可玩；测试账号仍需对应的玩家档案。

## 进程与更新演练

`scripts/drill_kill9.sh`、`drill_migration.sh` 和 `drill_update.sh` 的当前入口接线及验证维护在根目录 todo.md。演练必须迁入现役 Login / Gateway / World 后再作为验收证据，不能以原语测试或已有日志代替当前完整链路。

需要分别验证：进程异常后的资料恢复、单权威迁移、路由更新、结算去重、Gateway 排空、World 副本结束与发布回滚。TCP 连续、免重新登录和活动副本连续分别记录结果。

## 崩溃符号工具

```bash
scripts/split_symbols.sh <binary>
scripts/build_crash_tools.sh
```

符号与发行件保持同一 build-id。工具构建结果位于 `tools/crash-bin/`，缺少工具或符号时明确记录，不能将原始地址回溯算作符号化验收。使用方法见[崩溃采集](../design/crash-capture.md)。

## CI 辅助

- `scripts/ci/contract_pack.sh`：契约产物打包。
- `scripts/ci/schema_hash_report.sh`：契约哈希报告。
- `scripts/ci/docs_i18n_check.py`：VitePress 中英文页面配对和语言路由检查；`--dist <path>` 检查构建页面语言与描述。

文档构建遵守工作区依赖目录操作限制，本地使用仓库外隔离目录。文档页面使用标准 locales，不在一页拼接中英文正文。
