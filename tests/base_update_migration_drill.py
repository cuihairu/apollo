"""D22 Base 更新演练（单区服，一玩家两 Base）：真实进程 + 真实迁移编排。

决策口径（docs/review/2026-10-30-decisions.md D22 立即执行动作）：做一玩家、
两 Base 的更新演练，注入旧 Base 迟到保存和重启；检查拒旧及奖励待办不丢。

全链（无一桩）：
  1. 双 database + 双 base 独立进程；login 宿主目录 + M1 准入（player1
     落 base A，915 盖章归属端点）。
  2. fleetctl migrate（942 → 933 冻结 → 935/936 快照拉取 → 937 交权 →
     目录改指 + 归属端点刷新）真编排真拨。
  3. 断言面：
     - 刷新路由：resolve（918）回带新归属 base=<B>；
     - 拒旧生效：冻结源 958 业务写拒、迟到 DB 存档 wire 拒（旧状态不得
       回写）、重启后旧 Base 无锚点写全拒（无复活）；
     - 奖励待办不丢：冻结期挂账的结算重驱落新 Base（exp 落档）；迁移前
       已受理结算跨迁移重放 = duplicate（去重环随快照携带，不双记）；
     - 无第二个资料写入者：源库档案冻结在冻结时刻态（迟到保存/重启都
       没再动它），权威态在新 Base 库；
     - 更新阶段边界（D25）：源不可达（调用方超时形态 + 目录不动）、交权
       中断（提交不明：冻结已生效、目录不改指、源停冻结）、目标复活后
       重试收口（冻结幂等重受 → 交权直达 → 改指）；
     - 替换端回归服务（D26 C++ 角色进程替换复用面）：反向迁移把玩家交
       回重启后的旧 Base（快照自权威库拉取、目录回指、重放去重）。
"""
import json
import pathlib
import re
import selectors
import socket
import subprocess
import sys
import tempfile
import time
import os

base_binary, database_binary, login_binary, fleetctl_binary, probe_binary, routes = map(str, sys.argv[1:])
children = []

PLAYER = 1001
SEED = 1000
SETTLE_1 = 0x6D220001  # 迁移前受理（随快照过河）
SETTLE_2 = 0x6D220002  # 冻结期挂账（待办，重驱落新 Base）


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def start(binary, args, cwd, markers, port_marker=None, timeout=15):
    process = subprocess.Popen([binary, *args], cwd=cwd, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, bufsize=0)
    children.append(process)
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    deadline = time.monotonic() + timeout
    output = []
    try:
        while time.monotonic() < deadline:
            if selector.select(0.1):
                chunk = os.read(process.stdout.fileno(), 65536).decode(errors="replace")
                output.append(chunk)
                text = ''.join(output)
                if all(marker in text for marker in markers):
                    port = 0
                    if port_marker is not None:
                        rest = text.split(port_marker, 1)[1]
                        match = re.search(r"\s+\S+:(\d+)", rest)
                        if match:
                            port = int(match.group(1))
                        else:
                            port = int(rest.strip().split("=", 1)[-1].split()[0])
                    return process, text, port
                if not chunk and process.poll() is not None:
                    break
        raise AssertionError(f"{binary} not ready: {''.join(output)[-2000:]}")
    finally:
        selector.close()


def stop(process, name="process"):
    process.terminate()
    try:
        process.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate(timeout=5)
    assert process.returncode == 0, f"{name} exit status {process.returncode}"


def run(args, timeout=30):
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout)


def probe(args):
    result = run([probe_binary, *args])
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    assert lines, f"probe silent: rc={result.returncode} {result.stderr}"
    return result.returncode, lines[-1]


def fleetctl(args):
    result = run([fleetctl_binary, *args])
    return result.returncode, result.stdout.strip()


def load_json(path):
    with open(path, encoding="utf-8") as file:
        return json.load(file)


try:
    with tempfile.TemporaryDirectory(prefix="apollo_base_update_drill_") as directory:
        root = pathlib.Path(directory)
        dirs = {name: root / name for name in
                ("db_a", "db_b", "base_a", "base_b", "login")}
        for path in dirs.values():
            path.mkdir()

        # ---- 1. 双 database 独立进程 + 档案播种（源库）----
        def database_args(db_dir):
            return ["--port", "0", "--route-json", routes,
                    "--data-dir", str(db_dir / "players"),
                    "--journal", str(db_dir / "journal.log")]

        db_a, _, db_a_port = start(database_binary, database_args(dirs["db_a"]),
                                   dirs["db_a"], ["database ready port="],
                                   port_marker="database ready port=")
        db_b, _, db_b_port = start(database_binary, database_args(dirs["db_b"]),
                                   dirs["db_b"], ["database ready port="],
                                   port_marker="database ready port=")
        probe(["--probe-seed", str(db_a_port), str(PLAYER), str(SEED)])

        # ---- 2. 双 base 独立进程（各自挂各自 database）----
        def base_args(db_port):
            return ["--port", "0", "--database-port", str(db_port),
                    "--route-json", routes]

        base_a, _, base_a_port = start(base_binary, base_args(db_a_port),
                                       dirs["base_a"], ["base ready port="],
                                       port_marker="base ready port=")
        base_b, _, base_b_port = start(base_binary, base_args(db_b_port),
                                       dirs["base_b"], ["base ready port="],
                                       port_marker="base ready port=")

        # ---- 3. login 宿主目录 + M1 准入（worldStore 拨 base A）----
        admission_port = free_port()
        login, _, directory_port = start(
            login_binary,
            ["--host", "127.0.0.1", "--port", "0",
             "--directory-port", "0",
             "--directory-epochs", str(dirs["login"] / "epochs.json"),
             "--m1-port", str(admission_port),
             "--world-store-m1-port", str(base_a_port),
             "--gateway", "tcp://127.0.0.1:9999",
             "--enable-test-users"],
            dirs["login"],
            ["M1 admission listening on", "M1 control face listening on"],
            port_marker="M1 control face listening on")

        # ---- 4. player1 登录 → base A 激活 + 目录落点；915 盖章归属 A ----
        rc, line = probe(["--probe-login", str(admission_port),
                          "player1", "pass1"])
        assert rc == 0 and f"ok=1 player={PLAYER}" in line, f"login: {line}"
        rc, out = fleetctl(["assign", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER), "--base-host", "127.0.0.1",
                            "--base-port", str(base_a_port)])
        assert rc == 0 and f"ASSIGN_OK player={PLAYER}" in out, f"assign: rc={rc} {out}"
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{base_a_port}" in out, f"resolve pre: rc={rc} {out}"

        # ---- 5. 迁移前受理结算 1（冻结时随停写 flush 落源库、随快照过河）----
        rc, line = probe(["--probe-base", str(base_a_port), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "11"])
        assert rc == 0 and "accepted=1 duplicate=0" in line, f"credit 1: {line}"

        # ---- 6a. 源不可达（超时形态）：回环拒连也走满拨号窗（StoreDial
        #         waitUp 3s ≥ fleetctl RPC 窗 3s）——调用方见 MIGRATE_
        #         UNREACHABLE（调用方先失 patience 的真实边界形态）；事后
        #         对账 = 目录不动、源锚点照常受理（冻结未生效）。----
        dead_port = free_port()
        rc, out = fleetctl(["migrate", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER),
                            "--source-port", str(dead_port),
                            "--target-port", str(base_b_port),
                            "--target-zone", "5"])
        assert rc != 0 and "MIGRATE" in out, f"unreachable source: rc={rc} {out}"
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{base_a_port}" in out, \
            f"failed resolve disturbed projection: rc={rc} {out}"
        rc, line = probe(["--probe-base", str(base_a_port), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "18"])
        assert rc == 0 and "accepted=1 duplicate=1" in line, \
            f"failed migrate disturbed source anchor: {line}"

        # ---- 6b. 交权中断（提交不明/不可逆转换）：冻结已生效、目标不可达
        #         = 冻结先行落地（源停写），交权拨号失败——回执面同 6a 是
        #         调用方超时形态（编排在后台如实失败），事后对账 = 目录不
        #         改指 + 源锚点已冻结（业务写拒）——③ 后不可撤销面。----
        stop(base_b, "base b before handoff")
        rc, out = fleetctl(["migrate", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER),
                            "--source-port", str(base_a_port),
                            "--target-port", str(base_b_port),
                            "--target-zone", "5"])
        assert rc != 0 and "MIGRATE" in out, f"unreachable target: rc={rc} {out}"
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{base_a_port}" in out, \
            f"partial migrate re-pointed projection: rc={rc} {out}"
        rc, line = probe(["--probe-base", str(base_a_port), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "19"])
        assert rc != 0 and "accepted=0" in line, \
            f"source not frozen after partial migrate: {line}"

        # ---- 6c. 回滚边界（重试收口）：目标复活后同玩家重跑编排 = 补完
        #         交接（冻结幂等重受、交权直达、改指落新端点）----
        base_b, _, base_b_port = start(base_binary, base_args(db_b_port),
                                       dirs["base_b"], ["base ready port="],
                                       port_marker="base ready port=")
        rc, out = fleetctl(["migrate", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER),
                            "--source-port", str(base_a_port),
                            "--target-port", str(base_b_port),
                            "--target-zone", "5"])
        assert rc == 0 and f"MIGRATE_OK player={PLAYER} zone=5" in out, \
            f"retry migrate: rc={rc} {out}"
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{base_b_port}" in out, \
            f"resolve not re-pointed after retry: rc={rc} {out}"

        # ---- 7. 刷新路由：918 回带新归属端点（D22 线 ③ 刷新 Base 路由）----
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{base_b_port}" in out, \
            f"resolve post-migrate not re-pointed (A={base_a_port} " \
            f"B={base_b_port}): rc={rc} {out}"

        # ---- 8. 拒旧（冻结源）：958 业务写拒 + 迁移前结算重放也拒 ----
        rc, line = probe(["--probe-base", str(base_a_port), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "12"])
        assert rc != 0 and "accepted=0" in line, f"frozen source credit: {line}"

        # ---- 9. 跨迁移重放去重：同结算 ID 落新 Base = duplicate（不双记）----
        rc, line = probe(["--probe-base", str(base_b_port), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "13"])
        assert rc == 0 and "accepted=1 duplicate=1" in line, \
            f"cross-migration replay: {line}"

        # ---- 10. 奖励待办不丢：冻结期挂账（旧源拒）→ 重驱落新 Base ----
        rc, line = probe(["--probe-base", str(base_a_port), "credit", str(PLAYER),
                          str(SETTLE_2), "50", "14"])
        assert rc != 0 and "accepted=0" in line, f"held pending on frozen: {line}"
        rc, line = probe(["--probe-base", str(base_b_port), "credit", str(PLAYER),
                          str(SETTLE_2), "50", "15"])
        assert rc == 0 and "accepted=1 duplicate=0" in line, f"redrive: {line}"

        # ---- 11. 迟到保存注入：旧 Base 持旧状态直拨 DB 存档 wire = 拒 ----
        rc, line = probe(["--probe-base", str(base_a_port), "save", str(PLAYER),
                          str(SEED), "16"])
        assert rc != 0 and "saved=0" in line, f"late save accepted: {line}"

        # ---- 12. 旧 Base 重启注入：锚点内存态清零，写全拒无复活 ----
        stop(base_a, "base a")
        base_a2, _, base_a2_port = start(base_binary, base_args(db_a_port),
                                         dirs["base_a"], ["base ready port="],
                                         port_marker="base ready port=")
        rc, line = probe(["--probe-base", str(base_a2_port), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "17"])
        assert rc != 0 and "accepted=0" in line, f"revived base credit: {line}"
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{base_b_port}" in out, \
            f"resolve after restart regressed: rc={rc} {out}"

        # ---- 13. 替换端首停 → 源库冻结态复核（重启零写入）----
        stop(base_a2, "base a restarted")
        rc, line = probe(["--probe-load", str(db_a_port), str(PLAYER)])
        assert f"exp={SEED + 120} settled=1" in line, f"frozen profile: {line}"

        # ---- 14. 替换端回归服务（反向迁移）：重启后的旧 Base 重新承接
        #         玩家——锚点在现任源（B 在位）；快照自权威库拉取、交权、
        #         目录回指替换端 ----
        base_a3, _, base_a3_port = start(base_binary, base_args(db_a_port),
                                         dirs["base_a"], ["base ready port="],
                                         port_marker="base ready port=")
        rc, out = fleetctl(["migrate", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER),
                            "--source-port", str(base_b_port),
                            "--target-port", str(base_a3_port),
                            "--target-zone", "2"])
        assert rc == 0 and f"MIGRATE_OK player={PLAYER} zone=2" in out, \
            f"reverse migrate: rc={rc} {out}"
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", str(PLAYER)])
        assert rc == 0 and f"base=127.0.0.1:{base_a3_port}" in out, \
            f"resolve not re-pointed to replacement: rc={rc} {out}"
        rc, line = probe(["--probe-base", str(base_a3_port), "credit", str(PLAYER),
                          str(SETTLE_1), "120", "20"])
        assert rc == 0 and "accepted=1 duplicate=1" in line, \
            f"replay on replacement: {line}"

        # ---- 15. 双库终态（替换端权威、原端冻结在反向冻结时刻态）----
        stop(base_a3, "base a replacement")
        stop(base_b, "base b reverse source")
        rc, line = probe(["--probe-load", str(db_a_port), str(PLAYER)])
        assert f"exp={SEED + 120 + 50} settled=2" in line, \
            f"replacement authoritative profile: {line}"
        rc, line = probe(["--probe-load", str(db_b_port), str(PLAYER)])
        assert f"exp={SEED + 120 + 50} settled=2" in line, \
            f"reverse frozen profile: {line}"

        stop(login, "login")
        stop(db_a, "database a")
        stop(db_b, "database b")
        children.clear()

        # ---- 16. 档案落盘断言：终态随反向迁移回替换端库、双库一致 ----
        for db_dir in (dirs["db_a"], dirs["db_b"]):
            profile = load_json(db_dir / "players" / f"player_{PLAYER}.json")
            assert profile["exp"] == SEED + 120 + 50, (db_dir, profile)
            assert SETTLE_1 in profile.get("settledIds", []), (db_dir, profile)
            assert SETTLE_2 in profile.get("settledIds", []), (db_dir, profile)
            assert profile["username"] == f"drill-{PLAYER}", (db_dir, profile)

        print("Base update migration drill passed")
finally:
    for process in children:
        if process.poll() is None:
            process.kill()
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                pass
