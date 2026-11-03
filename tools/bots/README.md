# Apollo Gateway Bots

批量机器人客户端（真协议栈，非旁路 mock）：走 gateway 客户端入口（TCP 或
WS `/game`，同一 link Frame 线形），逐 bot 执行

```
connect → PING 建会话 → GATEWAY_ASSIGN_REQUEST 线缆准入（票据换会话绑定）
        → CELL_ENTITY_MOVE 移动脚本（圆周巡走）+ PING 心跳（RTT 采样）
        → 掉线退避重连（重连首包携旧会话号 = 保留窗 resume 路径）
```

stdlib-only Python 3（协议编解码对齐 `modules/protocol` + `modules/net/link`：
MessageHeader 28B pack(1) LE + link Frame 16B 头 + CRC32C 尾）。

## 用法

```sh
# ≥8 bot 斜坡（每 1s 加 4 个，跑 20s）
python3 tools/bots/apollo_bots.py --port 9002 --count 8 \
    --ramp-tick-time 1 --ramp-tick-count 4 --duration 20

# WS 入口
python3 tools/bots/apollo_bots.py --transport ws --port 9008 --ws-path /game

# 退出码：0 = 全部登录成功且有移动/心跳流量（CI 冒烟可断言）；非 0 = FAILED
```

## 斜坡三件套（KBEngine defaultAddBots 口径）

| flag | 语义 |
|---|---|
| `--count` | totalCount（总 bot 数） |
| `--ramp-tick-time` | tickTime（每拍秒数） |
| `--ramp-tick-count` | tickCount（每拍增量） |

统计输出（`[stats]`/`[final]` JSON）：登录成功/失败、移动帧、PING/PONG、
RTT p50/p95/min/max、重连与 resume 成功数、收帧吞吐、错误样本。

配套观测：gateway `--dashboard-port`（`/` 面板 + `/api/state`）的 sessions/
auth/rejects/tick 面与 bot 统计对账（在线会话数 = 存活 bot 数）。
