# 内部状态 Dashboard 设计（DASH-1）

- 日期：2026-11-03
- 状态：实现中（阶段1-2 已落地，阶段3 页面与文档进行中）
- 口径来源：todo.md「内部状态 Dashboard（用户令 2026-11-02）」
- 关联：Prometheus 指标面（apps/admin `/metrics`，9107）互不重复；本页做状态面

## 1. 目标与边界

提供「人看的」内部状态页：编队成员（启停/readiness/日志尾）、link/TLS 状态、orders 队列、script runtime、tick/负载、进程信息。数据源一律取各模块已有状态接口，缺的先补只读查询面，不新造指标；Prometheus 继续做指标面，本页做状态面，两者不重复。验收：本地起服后浏览器可看全部状态页，状态变化实时刷新，带测试。

## 2. 技术栈适配（用户令「axum」的等价物）

本仓为 C++20 单栈仓（`CMakeLists.txt:22` `project(ApolloMMORPG VERSION 0.1.0 LANGUAGES CXX)`），无 Rust 工具链与依赖面；引入 axum 即引入整套 Rust 构建链，违背仓内单一工具链纪律。「axum」在此按仓内等价物落地：

- **嵌入式 HTTP responder**：沿用 `apps/admin` 的 `MetricsHttp` 泵驱动纪律（`apps/admin/include/admin/metrics_http.hpp:42-49`——accept/read/write 全在 `pump(now_ms)` 内、宿主泵驱动、零内部线程、连接占用超时、超上限拒收），新建 `DashboardResponder` 同型组件。
- **装配位置**：嵌入 `world`/`cellServer` 进程（link/TLS、orders、script runtime、tick、进程信息都在本进程内，直接读既有接口零 IPC）；编队成员块读本机编队 roster 文件（dev_fleet 与 dashboard 同主机，文件面即既有接口）。
- **开关**：`--dashboard-port <port>`（0 = 关，默认 0 = 不装配）；装配时默认绑 `127.0.0.1`。

不引入 Rust/axum、不引入新第三方 HTTP 依赖。

## 3. 安全模型

| 项 | 默认 | 说明 |
|---|---|---|
| 监听地址 | `127.0.0.1` | 状态页比 Prometheus 指标更敏感（含模块版本、链路对端、队列明细），默认只绑环回；对外需显式 `--dashboard-host 0.0.0.0` |
| token | 未设 | 设置 `--dashboard-token <t>` 后，所有路由（HTML 与 JSON API）要求 `Authorization: Bearer <t>`，否则 401。不用查询串传递 token，避免浏览器历史、访问日志与 Referer 泄漏 |
| 方向 | 只听不拨 | dashboard 只 accept，不外发任何连接（「不外发」纪律同 logging §5.10 出站白名单口径） |
| 读写 | 只读 | 无任何状态变更端点；全部为 GET |
| 端口 | `--dashboard-port` 显式给 | 0 = 不装配（缺省关闭，不占端口面） |

## 4. 状态数据源清单（已有接口 → 缺口补齐）

| 区块 | 数据 | 已有接口（file:line） | 缺口与补齐计划 |
|---|---|---|---|
| 编队成员 | 启停/readiness/日志尾 | dev_fleet pid 记账 `.run/<role>.pid` + 日志 `logs/<role>.log`（`scripts/dev_fleet.sh`，模板 start.sh 同款 pid 面）；端口预检 `check_interfaces_port_free()`（`scripts/dev_fleet.sh:91`） | **readiness 无结构化标记**（start.sh 明示 "readiness not asserted"）→ 补：dev_fleet/start 脚本在各成员就绪日志行出现后落 `.run/<role>.ready`（内容=就绪时刻 ms）；dashboard 读 pid+ready+日志尾 |
| link/TLS | 链路状态/对端/TLS | `LinkState{kIdle,kHandshaking,kUp,kDown}`（`modules/net/link/include/apollo/net/link/interserver_link.hpp:55-59`）；LinkManager peers/staging 分域（`modules/net/link/include/apollo/net/link/link_manager.hpp:17-18,129,138`）；TLS verify 开关与凭据（`modules/net/tls/include/apollo/net/tls/tls_client.hpp:72-83`） | **LinkManager 无逐链路快照查询** → 补：`LinkManager::snapshot_links()` 只读枚举（peer 名/state/收发计数/TLS 与否），pump 线程外只读语义 |
| orders 队列 | pending/delivered/明细 | `OrderStore::pending()/pending_count()/delivered_count()`（`modules/game/orders/src/order_store.cpp:82,102,113`）；`OrderRecord` 状态字段 | 齐，直接接 |
| script runtime | 模块/版本/状态 | 版本库 `VersionEntry{name,version}`（`modules/script/include/apollo/script/module_version_store.hpp:26-28`）；rollout 版本面（`modules/script/include/apollo/script/module_rollout.hpp:42`） | **运行时当前装载版本无快照查询** → 补：`ScriptRuntime` 只读 `module_versions()`（装载表逐模块 name→version） |
| tick/负载 | ticks/队列水位 | `MetricRegistry::counter()/gauge()/snapshot()`（`modules/core/metrics/include/apollo/core/metrics/metric_registry.h:143,146,168`）；既有 `world_ticks_total` 等键 | 齐，`snapshot()` 直取 |
| 进程信息 | pid/uptime/rss/线程 | `/proc/self/*`（Linux）；启动时刻进程内已有 | Linux 直取；Windows 本批退化显示 "n/a"（与 metrics_http 的 POSIX-only 口径一致） |

补齐面共三件：① dev_fleet readiness 标记；② `LinkManager::snapshot_links()`；③ `ScriptRuntime::module_versions()`。全部为只读查询面，不改任何既有行为。

## 5. HTTP 面

| 路由 | 响应 | 用途 |
|---|---|---|
| `GET /` | 200 `text/html`（内嵌单页） | 人看的状态页 |
| `GET /api/state` | 200 `application/json` | 页面数据源，亦是脚本化查询面 |
| 其余 | 404 | — |
| 无/错 token（已设置时） | 401 | 全路由生效 |

JSON 形状（`/api/state`）：

```json
{
  "process": {"pid": 0, "uptime_ms": 0, "rss_kb": 0, "threads": 0, "started_ms": 0},
  "fleet":   [{"role": "gateway", "pid": 0, "ready": true, "log_tail": ["..."]}],
  "links":   [{"peer": "cell-21", "state": "up", "tls": true, "tx": 0, "rx": 0}],
  "orders":  {"pending": 0, "delivered": 0, "pending_detail": [{"order_id": "...", "state": "..."}]},
  "script":  {"modules": [{"name": "...", "version": 0}]},
  "load":    {"ticks_total": 0, "gauges": [{"name": "...", "value": 0}]}
}
```

`orders.pending_detail` 最多返回前 20 条 pending 记录；`orders.pending` 仍返回完整积压计数。

## 6. 页面结构（单页，无框架，内嵌 `<script>` 每 2s `fetch('/api/state')`）

```
┌──────────────────────────────────────────────────┐
│ Apollo 状态  · world  · uptime 1h23m  · [token✓] │  头部：进程/版本/uptime/token 状态
├──────────────────────────────────────────────────┤
│ ① 编队成员   role | pid | state | ready | 日志尾  │  表格，ready 红绿点
├──────────────────────────────────────────────────┤
│ ② link/TLS  peer | state | tls | tx | rx         │  表格，kUp 绿 / kDown 红
├──────────────────────────────────────────────────┤
│ ③ orders    pending N · delivered M + 明细表     │  计数 + 前 20 条 pending_detail（上限 20）
├──────────────────────────────────────────────────┤
│ ④ script    module | version | 状态              │  装载表
├──────────────────────────────────────────────────┤
│ ⑤ tick/负载 ticks_total · 队列水位 · gauge 摘要   │  数字卡
├──────────────────────────────────────────────────┤
│ ⑥ 进程信息  pid · rss · threads · started        │  数字卡
└──────────────────────────────────────────────────┘
```

## 7. 与 Prometheus 的分工边界

| | Prometheus 指标面 | Dashboard 状态面 |
|---|---|---|
| 承载 | apps/admin `/metrics`（9107，`metric_registry.h` snapshot→文本） | 本页 `/api/state` + `/` |
| 内容 | 累计计数器/直方图/gauge 时间序列（`world_ticks_total` 等） | 一次性状态快照：链路表、模块版本、队列明细、进程信息、日志尾 |
| 消费方 | Prometheus scrape（机器） | 浏览器（人） |
| 重复约束 | dashboard 不吐计数器时间序列；metrics 不吐状态表/明细。gauge 摘要（⑤）只引用既有 gauge 键值，不新造指标 | |

## 8. 实施阶段（一阶段一 commit，增量测试全绿）

1. **阶段 1 — responder 与装配**：`DashboardResponder`（路由/404/token/连接纪律，同 metrics_http 泵型）+ 内嵌 HTML/JSON 骨架 + `apps/world` 装配（`--dashboard-host/--dashboard-port/--dashboard-token`）+ 测试（路由/token 拒绝/404/JSON 形状/超时弃连）。 **已落地**（`apps/world/include/world_app/dashboard.hpp`、`apps/world/src/dashboard.cpp`、`tests/test_dashboard.cpp`，`DashboardTests` 绿）。
2. **阶段 2 — 数据源接线**：三件补齐查询面（`snapshot_links`/`module_versions`/dev_fleet ready 标记）+ orders/metrics/fleet 文件读取接入 JSON + 测试（各数据源形状与空态）。 **已落地**（`LinkManager::snapshot_links()`/`ILinkTransport::isTls()`/`active_modules()`/machined `.run/<name>.pid`/`DashboardDataCollector`，`DashboardTests` 绿）。
3. **阶段 3 — 页面与文档**：实时刷新 JS、日志尾渲染、docs/design/dashboard.md 状态翻「已实现」+ Directory_Structure/apps-index 双语登记 + 测试（e2e：真进程起 dashboard 浏览器面 HTTP 断言）。
