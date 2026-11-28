"""D31 最终拓扑同卷重接演练（L177；docs/review/2026-10-30-actions/06 §10）。

五要素同卷：单区服 machined 编队（roster 双 database/base + 1 World +
2 cellServer + 真实 gateway + 可恢复目录 epochs + world journal）上把
已交付能力各面重接一遍——契约/真实网络（probe-login 经 9113 准入激活落
base A → 958 结算）、持久化（probe-seed/credit/load 双库终态）、Base
更新演练（fleetctl migrate 真编排 A→B + 拒旧 + 跨迁移去重）、gateway
排空（954/955 ops 控制面真拨）、模拟面（963 真拨缩容 cell-22 + cell-21
崩溃 machined 重启再注册）、管理面（world 崩溃重启 journal 重放 + cells
复活重注册）、资料面（base-b 崩溃重启无锚点拒 = 无第二写入者）、目录
D05 面（fleet down/up 后 epochs 水位保留、归属投影按内存态语义未登记）。

与 docs/06 §10 逐能力独立证据的差异：本演练不自起进程、能力各面在**同
一拓扑**上发生（进程崩溃由 machined 1s 退避真重启）。未拍板/未实现面如
实断言现状（不冒充）：归属投影重启不重建（resolve rc=2 = D05 内存态语
义，重登激活重盖随 D22 单端点拍板）；活动级迁移/TCP 无断开不在本卷。
"""
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time

dev_fleet_sh, probe_binary, ops_probe_binary, fleetctl_binary, build_dir = \
    map(str, sys.argv[1:])

PLAYER = 1001
SETTLE_1 = 0x6D310001
DIRECTORY_PORT = 9105
LOGIN_M1_PORT = 9113
BASE_A_PORT = 9201
BASE_B_PORT = 9203
DB_A_PORT = 9200
DB_B_PORT = 9202
WORLD_M1_PORT = 9101
GW_CONTROL_PORT = 9116


def run(args, timeout=30, env=None):
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout,
                          env=env)


def probe_raw(args):
    result = run([probe_binary, *args])
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    return result.returncode, (lines[-1] if lines else ""), result.stderr


def probe(args):
    rc, line, err = probe_raw(args)
    assert line, f"probe silent: rc={rc} {err}"
    return rc, line


def ops_probe(args):
    result = run([ops_probe_binary, *args], timeout=20)
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    return result.returncode, (lines[-1] if lines else "")


def fleetctl(args):
    result = run([fleetctl_binary, *args])
    return result.returncode, (result.stdout + result.stderr).strip()


def fleet_env(fleet_dir):
    env = dict(os.environ)
    env["APOLLO_BUILD_DIR"] = build_dir
    env["APOLLO_FLEET_DIR"] = fleet_dir
    env["APOLLO_FLEET_INTERFACES"] = "0"
    env["APOLLO_FLEET_CELLS"] = "2"  # 五要素：2 个独立 cellServer
    return env


def fleet(argv, env, timeout=60):
    result = run([dev_fleet_sh, *argv], timeout=timeout, env=env)
    assert result.returncode == 0, \
        f"fleet {argv[0]}: rc={result.returncode} {result.stdout} {result.stderr}"
    return result.stdout


def wait_directory(env, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rc, _ = fleetctl(["resolve", "--host", "127.0.0.1", "--port",
                          str(DIRECTORY_PORT), "--player", str(PLAYER)])
        if rc in (0, 2):
            return
        time.sleep(0.5)
    raise AssertionError("login directory 9105 not ready")


def wait_base(port, timeout=30):
    # 同 dev_fleet_two_base_smoke：未知玩家 credit 显式拒 = base 就绪。
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rc, line, _ = probe_raw(["--probe-base", str(port), "credit",
                                 "9999", "0", "0", "0"])
        if "accepted=0" in line:
            return
        time.sleep(0.5)
    raise AssertionError(f"base {port} not ready")


def seed_player(db_port, player, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rc, line, _ = probe_raw(["--probe-seed", str(db_port), str(player), "0"])
        if rc == 0 and f"PLAYER_SEED player={player}" in line:
            return
        time.sleep(0.5)
    raise AssertionError(f"seed player {player} into database {db_port} failed")


def log_path(env):
    return pathlib.Path(env["APOLLO_FLEET_DIR"]) / "machined.log"


def log_size(env):
    path = log_path(env)
    return path.stat().st_size if path.exists() else 0


def wait_log(env, marker, from_pos, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if log_path(env).exists():
            with log_path(env).open("rb") as handle:
                handle.seek(from_pos)
                text = handle.read().decode("utf-8", "replace")
            if marker in text:
                return
        time.sleep(0.5)
    raise AssertionError(f"log marker not found after restart: {marker!r}")


def fleet_child_pid(env, pattern):
    """machined 子进程里按 cmdline 正则找成员 pid（崩溃重启后实时查）。"""
    pidfile = pathlib.Path(env["APOLLO_FLEET_DIR"]) / "machined.pid"
    with pidfile.open() as handle:
        machined_pid = handle.read().strip()
    children = run(["pgrep", "-P", machined_pid]).stdout.split()
    for child in children:
        args = run(["ps", "-o", "args=", "-p", child]).stdout
        if pattern in args:
            return int(child)
    return None


def kill_member(env, pattern, timeout=20):
    """SIGKILL 成员并确认 machined 拉起新进程（D23/D22 崩溃注入面）。"""
    old_pid = fleet_child_pid(env, pattern)
    assert old_pid, f"member not found for kill: {pattern}"
    deadline = time.monotonic() + timeout
    os.kill(old_pid, signal.SIGKILL)
    while time.monotonic() < deadline:
        new_pid = fleet_child_pid(env, pattern)
        if new_pid and new_pid != old_pid:
            return old_pid, new_pid
        time.sleep(0.5)
    raise AssertionError(f"machined did not restart member: {pattern}")


with tempfile.TemporaryDirectory(prefix="apollo_d31_final_topology_") as directory:
    env = fleet_env(str(pathlib.Path(directory) / "fleet"))

    # ---- 0. 五要素 roster：双 database/base + world + 2 cell + gateway ----
    roster = fleet(["roster"], env)
    roster_names = {line.split("|")[0].strip()
                    for line in roster.splitlines() if "|" in line}
    for name in ("database", "database-b", "base", "base-b", "login",
                 "world", "gateway", "cell-21", "cell-22"):
        assert name in roster_names, f"roster missing {name!r}: {sorted(roster_names)}"

    # ---- 1. 同卷起编队（machined 按 roster 拉起全体成员）----
    fleet(["up"], env, timeout=30)
    try:
        wait_directory(env)
        wait_base(BASE_A_PORT)
        wait_base(BASE_B_PORT)
        seed_player(DB_A_PORT, PLAYER)

        # ---- 1b. 分配面活性：journal 空文件预建后 virgin 装载不触发
        # 「无墓碑全池冻结」（boot 文件缺失 = journalMissingAtBoot_ 保守占位，
        # 首个拨入冻结一切认领）；cell-21 认领 501/502、cell-22 认领 503
        # 必须显形（认领标记缺席 = 分配面静默冻死）。
        whole_log = log_path(env).read_text(errors="replace")
        assert "missing journal evidence" not in whole_log, \
            "assignment freeze fired on virgin fleet"
        wait_log(env, "space assigned: space=501", 0)
        wait_log(env, "space assigned: space=503", 0)

        # ---- 2. 契约/真实网络/持久化：准入激活落 A + 958 结算 ----
        rc, line = probe(["--probe-login", str(LOGIN_M1_PORT), "player1", "pass1"])
        assert rc == 0 and f"ok=1 player={PLAYER}" in line, f"login: {line}"
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port",
                            str(DIRECTORY_PORT), "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{BASE_A_PORT}" in out, \
            f"resolve pre: rc={rc} {out}"
        rc, line = probe(["--probe-base", str(BASE_A_PORT), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "11"])
        assert rc == 0 and "accepted=1 duplicate=0" in line, f"credit: {line}"

        # ---- 3. Base 更新演练（同卷）：migrate 真编排改派 B + 拒旧 + 去重 ----
        rc, out = fleetctl(["migrate", "--host", "127.0.0.1", "--port",
                            str(DIRECTORY_PORT), "--player", str(PLAYER),
                            "--source-port", str(BASE_A_PORT),
                            "--target-port", str(BASE_B_PORT),
                            "--target-zone", "5"])
        assert rc == 0 and f"MIGRATE_OK player={PLAYER} zone=5" in out, \
            f"migrate: rc={rc} {out}"
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port",
                            str(DIRECTORY_PORT), "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{BASE_B_PORT}" in out, \
            f"resolve post: rc={rc} {out}"
        rc, line = probe(["--probe-base", str(BASE_A_PORT), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "12"])
        assert rc != 0 and "accepted=0" in line, f"frozen source: {line}"
        rc, line = probe(["--probe-base", str(BASE_B_PORT), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "13"])
        assert rc == 0 and "accepted=1 duplicate=1" in line, f"replay: {line}"
        rc, line = probe(["--probe-load", str(DB_A_PORT), str(PLAYER)])
        assert rc == 0 and "exp=120 settled=1" in line, f"frozen store: {line}"
        rc, line = probe(["--probe-load", str(DB_B_PORT), str(PLAYER)])
        assert rc == 0 and "exp=120 settled=1" in line, f"authoritative store: {line}"

        # ---- 4. gateway 排空（954/955 ops 控制面真拨）：begin + status ----
        rc, line = ops_probe(["--probe-gateway-drain", str(GW_CONTROL_PORT), "1", "0"])
        assert rc == 0 and "GW_DRAIN ok=1" in line \
            and ("state=1" in line or "state=2" in line), f"drain begin: {line}"
        rc, line = ops_probe(["--probe-gateway-drain", str(GW_CONTROL_PORT), "2", "0"])
        assert rc == 0 and "GW_DRAIN ok=1" in line \
            and ("state=1" in line or "state=2" in line), f"drain status: {line}"

        # ---- 5. 模拟面：963 真拨缩容 cell-22 + cell-21 崩溃重启再注册 ----
        rc, line = ops_probe(["--probe-cell-retire", str(WORLD_M1_PORT), "1", "22"])
        assert rc == 0 and "CELL_RETIRE accepted=1" in line, f"retire: {line}"
        pos = log_size(env)
        _, new_pid = kill_member(env, "cell-id 21")
        assert new_pid, "cell-21 not restarted"
        wait_log(env, "Cell registration: dialing 127.0.0.1:9101 as cell-21", pos)

        # ---- 6. 管理面：world 崩溃重启 journal 重放 + cell 复活重注册 ----
        pos = log_size(env)
        kill_member(env, "world --world-id 1")
        wait_log(env, "world journal loaded", pos)
        # cell 侧启动标记只在进程首启打一次；重启后重拨由 world 受理面显形
        # （cell registered / cell re-registered 均含 "id=21 epoch="）。
        wait_log(env, "id=21 epoch=", pos, timeout=30)
        # 管理面回话：对未知 cell 的缩容请求有 966 回执（显式拒非静默）
        rc, line = ops_probe(["--probe-cell-retire", str(WORLD_M1_PORT), "1", "9999"])
        assert line.startswith("CELL_RETIRE accepted=0"), f"world reply: {line!r}"

        # ---- 7. 资料面：base-b 崩溃重启无锚点拒（无第二写入者复活）----
        kill_member(env, "base --port 9203")
        wait_base(BASE_B_PORT, timeout=30)
        rc, line = probe(["--probe-base", str(BASE_B_PORT), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "14"])
        assert rc != 0 and "accepted=0" in line, f"no anchor after restart: {line}"
        rc, line = probe(["--probe-base", str(BASE_A_PORT), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "15"])
        assert rc != 0 and "accepted=0" in line, f"frozen A still rejects: {line}"
        rc, line = probe(["--probe-load", str(DB_B_PORT), str(PLAYER)])
        assert rc == 0 and "exp=120 settled=1" in line, \
            f"archive survives base restart: {line}"
    finally:
        fleet(["down"], env)

    # ---- 8. 目录 D05 面：down/up 后 epochs 水位保留、投影按内存态未登记 ----
    epochs = pathlib.Path(env["APOLLO_FLEET_DIR"]) / "data" / "login" / "epochs.json"
    assert epochs.exists(), f"epochs file missing: {epochs}"
    before = epochs.read_text().strip()

    fleet(["up"], env, timeout=30)
    try:
        wait_directory(env)
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port",
                            str(DIRECTORY_PORT), "--player", str(PLAYER)])
        # 归属投影是内存态（D05 语义：epochs 只恢复代次水位不重建投影）——
        # 重启后未登记 rc=2 即现状；重登激活重盖随 D22 单端点拍板，不在本卷。
        assert rc == 2, f"projection must be in-memory after restart: rc={rc} {out}"
        assert epochs.read_text().strip() == before, "epoch watermark rewound"
        rc, line = probe(["--probe-load", str(DB_B_PORT), str(PLAYER)])
        assert rc == 0 and "exp=120 settled=1" in line, \
            f"authoritative archive survives fleet restart: {line}"
    finally:
        fleet(["down"], env)

    status = run([dev_fleet_sh, "status"], env=env)
    assert "未运行" in status.stdout, f"fleet status: {status.stdout}"

    print("D31_FINAL_TOPOLOGY_DRILL_OK")
