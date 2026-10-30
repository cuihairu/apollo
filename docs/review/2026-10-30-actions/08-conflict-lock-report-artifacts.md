# S04 / S06 / S07 / S09 · 冲突解冻命令、卷锁已决、967 上报事实、旧构建产物清单

> 来源：docs/review/2026-10-30-decisions.md S04（todo.md:126/133/134）、S06（todo.md:126/134）、
> S07（todo.md:126/133）、S09（todo.md:150）。本文件为设计稿与事实记录。

## 1. S04 · 冲突 Space 受控人工确认命令设计

### 1.1 设计约束

- 首阶段受控人工确认命令；按单 Space 解冻（不整 cell 解冻）。
- 要求期望管理代次/版本匹配；留下原因、操作 ID 和结果。

### 1.2 命令草案

```
# 查看冲突证据（观测面）
world-admin conflict list
world-admin conflict show --space <S>

# 受控解冻（人工确认）
world-admin conflict resolve --space <S> \
  --expect-mgmt-epoch <E> --expect-version <V> \
  --reason "<核对旧源隔离、信任此记录的原因>" --op-id <OP> --record
```

### 1.3 验收

| # | 用例 | 断言 |
|---|---|---|
| 1 | 重试幂等 | 同 op-id 重试不重复修复 |
| 2 | 版本不符 | 期望代次/版本不匹配 → 拒绝 |
| 3 | 修复后重启 | journal 补 A 行后解冻，重推 962，重启对账一致 |
| 4 | 强制全部 | 不提供绕过隔离检查的「强制全部解冻」 |

> 明确不做：延后自动冲突修复；不提供绕过隔离检查的强制全部解冻；不直接手改正在使用的 journal。

## 2. S06 · 持久卷锁：已决 = 强制拒启（含实现与边界）

### 2.1 已决与实现

- **已代决：强制拒启**（限定单主机同一持久卷）。当前源码 `apps/world/src/world_app.cpp:85-113`
  已实现：活锁 PID 检查抛「locked by live pid」、陈旧锁破锁（PID 死亡可证明锁失主）、
  畸形锁破锁、写锁失败抛错。批C 已代决并落地（todo 批㊸）。
- 维护标记：`docs/zh/design/world-reconciliation.md` §6「待拍板」中的
  「持久卷锁文件策略：advisory（仅告警）或强制拒启」**改为已决 = 强制拒启**。

### 2.2 补强负例设计

| # | 用例 | 断言 |
|---|---|---|
| 1 | 两进程同时启动 | 第二进程拒启（活锁互斥） |
| 2 | 陈旧锁误判 | PID 复用/死亡 → 破锁仅当可证明失主 |
| 3 | 跨容器/跨主机 | 当前**不支持**（明确支持环境 = 单主机同一卷） |
| 4 | 删锁文件 | 不作为常规恢复动作 |

> 明确不做：不降级为仅告警；延后跨主机热备。

## 3. S07 · 963/967 事实记录与上报设计

### 3.1 事实（已核契约）

`sdks/cpp/generated/apollo_contract.h`：
- **963 = `cell_retire_request`**（op：1=begin_retire、2=cancel_retire）；
- 966 = `cell_retire_reply`；**967 = `cell_space_report`**（自报，六字段）。
- 原「963 条目数上限与分段」问题已消解：967 逐条目一帧（一条 Space 一帧）。

### 3.2 上报设计（重注册洪峰控制）

- 估算：单 cell 最大持有 Space 数（按分配池上限）；
- 分批发送、限速、队列满时重试（有界队列原则）；
- 每帧有界不代表总量有界——重注册洪峰按批/限速处理。

## 4. S09 · build-examples 旧目标产物候选清单

### 4.1 候选路径与归属（静态核对，2026-10-30）

`build-examples/apps/` 现有二进制：`admin`、`apolloc`、`base`、`database`、`fleetctl`、
`gateway`、`interfaces`、`logger`、`login`、`machined`、`world`。

对照现行 `apps/`（admin、apolloc、base、common、database、fleetctl、gateway、interfaces、
logger、login、machined、world）：

| 候选 | 归属 | 已删除目标产物？ |
|---|---|---|
| 上述 11 个二进制 | 均对应现行 apps/ 目录的现行目标 | **否**——全部为现行目标输出 |

### 4.2 结论

- 经构建清单核对，`build-examples/apps/` 未发现**已删除目标**生成的残留产物；
  11 个二进制均为现行目标输出（属本机构建缓存）。
- 按决策「仅清理由已删除目标生成、经构建清单确认的残留产物」+「本次不清理任何文件」：
  **本次无删除动作**；候选清单已生成，供后续在干净隔离构建目录验证后按需处理。

### 4.3 构建目录口径（接 S10）

- 脚本运行来源固定到**已验证构建树**（`APOLLO_BUILD_DIR` 唯一显式覆盖，默认 ``<repo>/build``）；
  不自动扫描多目录找「能运行」的旧二进制。
- 当前 `build/` 缺 `interfaces` 二进制（`build-off`/`build-examples` 有）——按 S10 应显式指定
  已验证树，不凭名称认定可用；roster 对未构建二进制跳过（已有行为）。
