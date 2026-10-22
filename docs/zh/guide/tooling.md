---
title: 工具脚本
icon: terminal
order: 7
---

# 工具脚本

工具和脚本围绕 gateway、loginServer、baseServer、worldServer、cell、database、machined 目标角色接线，不定义第二套生产拓扑。

## 开发编队

`scripts/dev_fleet.sh` 的入口、配置与角色归属需要随 Base/database、World/cell 拆分更新。现有脚本能启动进程不等于目标角色已接通；不能将 World 中的本地资料端点作为目标 Base/database 部署说明。真实入口映射见[服务器应用](../apps/index.md)，变更清单见根目录 todo.md。

machined 只管理本机进程。World 可独立启动并向 loginServer 服务目录注册；跨服准入和 Base 寻址不依赖同机 machined。脚本需要区分进程存活、服务就绪、准入开放和业务可用。

## 进程与更新演练

现有 drill_kill9.sh、drill_migration.sh、drill_update.sh、drill_proto_update.sh、drill_module_update.sh 保留各自已验证的原语范围。迁入目标角色后重新验证 Base/database 持久确认、场景迁移保留原 Base、完整 Instance 替换、Base 独立升级和跨 gateway 恢复。

协议升级演练必须使用真实新旧业务消息与状态转换；握手白名单不能代替解码兼容。进程重启和等待副本结束不能替代活动副本连续迁移。分别记录持续接入、免重登、客户端 TCP 和玩法连续性。

## 崩溃符号工具

```bash
scripts/split_symbols.sh <binary>
scripts/build_crash_tools.sh
```

符号与发行件保持同一 build-id；工具位于 tools/crash-bin。采集、符号化与资料/副本恢复分别验收，见[崩溃采集](../design/crash-capture.md)。

## 性能与遥测状态

已有自研日志、MetricRegistry 和 span/trace 原语，尚未集成专用 Profiler、OpenTelemetry SDK、OTLP exporter 或完整外部采集链路。使用外部性能工具的指南不等于引擎内置能力，接入按需求立项。

## CI 辅助

- scripts/ci/contract_pack.sh：契约产物打包。
- scripts/ci/schema_hash_report.sh：契约哈希报告。
- scripts/ci/docs_i18n_check.py：原生 VitePress locales 页面配对、路由及生成 HTML 语言/描述检查。

文档构建使用仓库外隔离目录，遵守工作区依赖目录限制。中文在 docs 根目录，英文在 docs/en，页面正文使用各自语言。
