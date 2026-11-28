"""D12 编队批：dev_fleet 两 Base 编队编排真机验证。

L132 仍缺项「dev_fleet 两 Base 编队编排」收口：常驻开发编队（machined 拉
起 database 9200 + database-b 9202、base 9201 + base-b 9203、login 目录
9105 带 --enable-test-users）上走 D22 同型真链——probe-login 经 9113 准入
激活落 base A（login worldStore 单端点盖章）→ fleetctl resolve 回带归属
端点 → fleetctl migrate 真编排改派 base B（933 冻结 → 935/936 快照 →
937 交权 → 目录改指）→ 拒旧（冻结源业务写拒）→ 跨迁移重放去重（同结算
ID duplicate 不双记）→ 双库终态一致（权威库 = database-b）。

与 D22 演练（base_update_migration_drill.py）的差异：本验证不自起进程，
验证的是 **编队编排面** —— machined 监督下按 roster 拉起双 Base（独立
database 独立 data-dir），迁移在编队成员间完成；编队 up/down（含按
basename 孤儿收割）与 down 后无残留同为验收断言。
"""
import os
import pathlib
import subprocess
import sys
import tempfile
import time

dev_fleet_sh, probe_binary, fleetctl_binary, build_dir = map(str, sys.argv[1:])

PLAYER = 1001
SETTLE_1 = 0x6D220010
DIRECTORY_PORT = 9105
LOGIN_M1_PORT = 9113  # 缺省 9103 被本机常驻 docker 发布端口占用（见 dev_fleet.sh 头注）
BASE_A_PORT = 9201
BASE_B_PORT = 9203
DB_A_PORT = 9200
DB_B_PORT = 9202


def run(args, timeout=30, env=None):
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout, env=env)


def probe_raw(args):
    result = run([probe_binary, *args])
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    return result.returncode, (lines[-1] if lines else ""), result.stderr


def probe(args):
    rc, line, err = probe_raw(args)
    assert line, f"probe silent: rc={rc} {err}"
    return rc, line


def fleetctl(args):
    result = run([fleetctl_binary, *args])
    return result.returncode, (result.stdout + result.stderr).strip()


def fleet_env(fleet_dir):
    env = dict(os.environ)
    env["APOLLO_BUILD_DIR"] = build_dir
    env["APOLLO_FLEET_DIR"] = fleet_dir
    env["APOLLO_FLEET_INTERFACES"] = "0"  # 两 Base 编排面不需要 interfaces
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
        if rc in (0, 2):  # 0=已登记 / 2=目录在、玩家未登记（均证目录面就绪）
            return
        time.sleep(0.5)
    raise AssertionError("login directory 9105 not ready")


def wait_base(port, timeout=30):
    # base 就绪判据：对无锚点玩家 credit 被显式拒（accepted=0）——拨号失败
    # 是空回显。未知玩家拒不改任何状态，作就绪探针无副作用。
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rc, line, _ = probe_raw(["--probe-base", str(port), "credit",
                                 "9999", "0", "0", "0"])
        if "accepted=0" in line:
            return
        time.sleep(0.5)
    raise AssertionError(f"base {port} not ready")


def seed_player(db_port, player, timeout=30):
    # 编队 database 起于空库：激活（avatar 落点）要求档案已在权威库——
    # 先经 --probe-seed 落 player 档案（D22 演练同序，drill §133）。
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rc, line, _ = probe_raw(["--probe-seed", str(db_port), str(player), "0"])
        if rc == 0 and f"PLAYER_SEED player={player}" in line:
            return
        time.sleep(0.5)
    raise AssertionError(f"seed player {player} into database {db_port} failed")


with tempfile.TemporaryDirectory(prefix="apollo_dev_fleet_two_base_") as directory:
    env = fleet_env(str(pathlib.Path(directory) / "fleet"))

    # ---- 1. roster 面：双 database + 双 base 条目在编队且可执行 ----
    roster = fleet(["roster"], env)
    roster_names = {line.split("|")[0].strip() for line in roster.splitlines() if "|" in line}
    for name in ("database", "database-b", "base", "base-b", "login", "world", "gateway"):
        assert name in roster_names, f"roster missing {name!r}: {sorted(roster_names)}"

    # ---- 2. 编队起：machined 按 roster 拉起全体成员 ----
    fleet(["up"], env, timeout=30)
    try:
        wait_directory(env)
        wait_base(BASE_A_PORT)
        wait_base(BASE_B_PORT)
        seed_player(DB_A_PORT, PLAYER)

        # ---- 3. player1 经 9113 准入激活 → 落 base A（worldStore 盖章）----
        rc, line = probe(["--probe-login", str(LOGIN_M1_PORT), "player1", "pass1"])
        assert rc == 0 and f"ok=1 player={PLAYER}" in line, f"login: {line}"

        # ---- 4. 目录归属 = base A（登录实拨端点随落点成行）----
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port",
                            str(DIRECTORY_PORT), "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{BASE_A_PORT}" in out, \
            f"resolve pre: rc={rc} {out}"

        # ---- 5. 迁移前结算受理（随冻结 flush 落源库、随快照过河）----
        rc, line = probe(["--probe-base", str(BASE_A_PORT), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "11"])
        assert rc == 0 and "accepted=1 duplicate=0" in line, f"credit: {line}"

        # ---- 6. fleetctl migrate 真编排：编队成员间迁移改派 base B ----
        rc, out = fleetctl(["migrate", "--host", "127.0.0.1", "--port",
                            str(DIRECTORY_PORT), "--player", str(PLAYER),
                            "--source-port", str(BASE_A_PORT),
                            "--target-port", str(BASE_B_PORT),
                            "--target-zone", "5"])
        assert rc == 0 and f"MIGRATE_OK player={PLAYER} zone=5" in out, \
            f"migrate: rc={rc} {out}"

        # ---- 7. 目录改指 base B ----
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port",
                            str(DIRECTORY_PORT), "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{BASE_B_PORT}" in out, \
            f"resolve post: rc={rc} {out}"

        # ---- 8. 拒旧：冻结源业务写拒 ----
        rc, line = probe(["--probe-base", str(BASE_A_PORT), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "12"])
        assert rc != 0 and "accepted=0" in line, f"frozen source: {line}"

        # ---- 9. 跨迁移重放去重：同结算 ID 落 B = duplicate 不双记 ----
        rc, line = probe(["--probe-base", str(BASE_B_PORT), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "13"])
        assert rc == 0 and "accepted=1 duplicate=1" in line, f"replay: {line}"

        # ---- 10. 双库终态：冻结库（A 后端）与权威库（B 后端）一致 ----
        rc, line = probe(["--probe-load", str(DB_A_PORT), str(PLAYER)])
        assert rc == 0 and "exp=120 settled=1" in line, f"frozen store: {line}"
        rc, line = probe(["--probe-load", str(DB_B_PORT), str(PLAYER)])
        assert rc == 0 and "exp=120 settled=1" in line, f"authoritative store: {line}"
    finally:
        fleet(["down"], env)

    # ---- 11. 编队收尾：down 后 machined 未运行、无残留 ----
    status = run([dev_fleet_sh, "status"], env=env)
    assert "未运行" in status.stdout, f"fleet status: {status.stdout}"
