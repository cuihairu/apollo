"""L132 多 Base 真机编队演练：真实进程 + 真实 M1 线 + 真实落盘。

全链（无一桩）：
  1. 双 database + 双 base 独立进程独立工作目录；login 宿主目录（915/918
     承载面）+ M1 准入面（LOGIN_REQUEST 全链：认证 → base 激活绑定 →
     目录绑定 + 落点盖章归属 Base 端点）。
  2. fleetctl assign 重指（915 --base-host/--base-port）→ fleetctl resolve
     （918 回带 base=<host>:<port>）逐玩家断言。
  3. 958 逐 Base 送分（稳定结算 ID）：各自 Base 受理；重放同 ID =
     accepted + duplicate（去重环不双记）；无锚点玩家/错 Base = 拒。
  4. 优雅停机 flush → 档案落盘断言：逐 Base exp 与 settledIds、零串扰
     （player1 不落 db_b、player2 在 db_a 无分值无结算环）。
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

SETTLE_A = 0x5A5A0001  # player1 结算 ID（非 0）
SETTLE_B = 0x5A5A0002  # player2 结算 ID（非 0）
SEED_A, SEED_B = 1000, 2000


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
                        # 两态标记：host:port 形（M1 face listening on）与
                        # ready port=N 形（database/base stdout 标记）
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


def run(args, timeout=20):
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
    with tempfile.TemporaryDirectory(prefix="apollo_multi_base_drill_") as directory:
        root = pathlib.Path(directory)
        dirs = {name: root / name for name in
                ("db_a", "db_b", "base_a", "base_b", "login")}
        for path in dirs.values():
            path.mkdir()

        # ---- 1. 双 database 独立进程 + 登录用户档案播种 ----
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
        probe(["--probe-seed", str(db_a_port), "1001", str(SEED_A)])
        probe(["--probe-seed", str(db_a_port), "1002", str(SEED_B)])
        probe(["--probe-seed", str(db_b_port), "1002", str(SEED_B)])

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

        # ---- 4. LOGIN_REQUEST 全链：player1 → base A 激活绑定 + 目录落点 ----
        rc, line = probe(["--probe-login", str(admission_port), "player1", "pass1"])
        assert rc == 0 and "ok=1 player=1001" in line, f"player1 login: {line}"
        rc, line = probe(["--probe-login", str(admission_port), "player2", "pass2"])
        assert rc == 0 and "ok=1 player=1002" in line, f"player2 login: {line}"

        # ---- 5. fleetctl assign 重指（915 盖章归属 Base 端点）----
        rc, out = fleetctl(["assign", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", "1001", "--base-host", "127.0.0.1",
                            "--base-port", str(base_a_port)])
        assert rc == 0 and "ASSIGN_OK player=1001" in out, f"assign 1001: rc={rc} {out}"
        rc, out = fleetctl(["assign", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", "1002", "--base-host", "127.0.0.1",
                            "--base-port", str(base_b_port)])
        assert rc == 0 and "ASSIGN_OK player=1002" in out, f"assign 1002: rc={rc} {out}"

        # ---- 6. fleetctl resolve（918 回带 base=<host>:<port>）----
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", "1001"])
        assert rc == 0 and f"base=127.0.0.1:{base_a_port}" in out, f"resolve 1001: rc={rc} {out}"
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", "1002"])
        assert rc == 0 and f"base=127.0.0.1:{base_b_port}" in out, f"resolve 1002: rc={rc} {out}"

        # ---- 7. 目录回带与真实监听一致（演练断言：端点不是幻值）----
        assert base_a_port > 0 and base_b_port > 0 and base_a_port != base_b_port

        # ---- 8. player2 锚点落 base B（activate 载入 db_b 档案）----
        rc, line = probe(["--probe-base", str(base_b_port), "activate", "1002",
                          "0", "0", "1"])
        assert rc == 0 and "ok=1" in line, f"activate 1002 on B: {line}"

        # ---- 9. 958 逐 Base 送分 + 稳定结算 ID 重放去重 ----
        rc, line = probe(["--probe-base", str(base_a_port), "credit", "1001",
                          str(SETTLE_A), "120", "11"])
        assert rc == 0 and "accepted=1 duplicate=0" in line, f"credit 1001: {line}"
        rc, line = probe(["--probe-base", str(base_a_port), "credit", "1001",
                          str(SETTLE_A), "120", "12"])
        assert rc == 0 and "accepted=1 duplicate=1" in line, f"replay 1001: {line}"

        rc, line = probe(["--probe-base", str(base_b_port), "credit", "1002",
                          str(SETTLE_B), "90", "21"])
        assert rc == 0 and "accepted=1 duplicate=0" in line, f"credit 1002: {line}"
        rc, line = probe(["--probe-base", str(base_b_port), "credit", "1002",
                          str(SETTLE_B), "90", "22"])
        assert rc == 0 and "accepted=1 duplicate=1" in line, f"replay 1002: {line}"

        # ---- 10. 拒面：错 Base（player1 在 B 无锚点）+ 无锚点玩家 ----
        rc, line = probe(["--probe-base", str(base_b_port), "credit", "1001",
                          str(SETTLE_A), "120", "13"])
        assert rc != 0 and "accepted=0" in line, f"wrong-base credit: {line}"
        rc, line = probe(["--probe-base", str(base_a_port), "credit", "9999",
                          "0", "10", "14"])
        assert rc != 0 and "accepted=0" in line, f"unknown player credit: {line}"

        # ---- 11. resolve 无绑定玩家 → RESOLVE_FAILED ----
        rc, out = fleetctl(["resolve", "--host", "127.0.0.1", "--port", str(directory_port),
                            "--player", "9999"])
        assert rc == 2 and "RESOLVE_FAILED player=9999" in out, f"resolve 9999: rc={rc} {out}"

        # ---- 12. 优雅停机：base 先收线（flush 档案，database 在位）----
        stop(base_a, "base a")
        stop(base_b, "base b")

        # ---- 13. 读档面复核（探针经真 database 链回读，停机 flush 后）----
        rc, line = probe(["--probe-load", str(db_a_port), "1001"])
        assert f"exp={SEED_A + 120} settled=1" in line, f"load 1001: {line}"
        rc, line = probe(["--probe-load", str(db_b_port), "1002"])
        assert f"exp={SEED_B + 90} settled=1" in line, f"load 1002: {line}"

        stop(login, "login")
        stop(db_a, "database a")
        stop(db_b, "database b")
        children.clear()

        # ---- 14. 档案落盘断言：逐 Base exp + settledIds、零串扰 ----
        profile_a1 = load_json(dirs["db_a"] / "players" / "player_1001.json")
        assert profile_a1["exp"] == SEED_A + 120, profile_a1
        assert SETTLE_A in profile_a1.get("settledIds", []), profile_a1

        profile_a2 = load_json(dirs["db_a"] / "players" / "player_1002.json")
        assert profile_a2["exp"] == SEED_B, profile_a2  # 无分值落 A
        assert SETTLE_B not in profile_a2.get("settledIds", []), profile_a2

        profile_b2 = load_json(dirs["db_b"] / "players" / "player_1002.json")
        assert profile_b2["exp"] == SEED_B + 90, profile_b2
        assert SETTLE_B in profile_b2.get("settledIds", []), profile_b2

        assert not (dirs["db_b"] / "players" / "player_1001.json").exists(), \
            "player1 profile leaked onto database B"

        print("Multi-Base real-process settlement drill passed")
finally:
    for process in children:
        if process.poll() is None:
            process.kill()
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                pass
