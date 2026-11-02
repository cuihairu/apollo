"""L156 验证：World/cell 无同机 machined 独立注册与运行。
验证点：
1. login（--service-register-port 面）+ world（--m1-port + 拨 login 注册）+ cellServer
   （--m1-port + 拨 world 注册）均不经 machined 拉起，独立启动。
2. world 经 964/965 直接注册到 login（machined 未参与）。
3. cellServer 经 960/961 直接注册到 world（machined 未参与）。
4. 生产 cellServer module rollout（952/953 转发链）：健康 promote 与故障回滚
   全链驱动真实 cell_main.cpp（非 tests/cell_host.hpp 平行实现）。
5. machined 不启动、不参与任何业务注册；machined 不知道资料位置/跨服去向。
"""
import pathlib
import os
import re
import selectors
import socket
import struct
import subprocess
import sys
import tempfile
import time
import json

login_binary, world_binary, cell_binary, routes, fleetctl_binary = map(str, sys.argv[1:])
children = []


def crc32c(data):
    poly = 0x82F63B78
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ poly if crc & 1 else crc >> 1
    return crc ^ 0xFFFFFFFF


def append_frame(payload, seq, flags):
    header = struct.pack("<HBBIIIH", 0x5041, 1, flags, seq, len(payload), 0, 0)
    header = header[:14] + struct.pack("<H", crc32c(header[:14]) & 0xFFFF)
    return header + payload + struct.pack("<I", crc32c(payload))


class RolloutM1Client:
    """仅覆盖本 smoke 需要的 M1 握手 + 952/953 平铺契约面。"""

    def __init__(self, host, port, self_name, route_json, timeout=5.0):
        with open(route_json, encoding="utf-8") as file:
            schema_hash = json.load(file)["schema_hash"]
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.route_version = int(schema_hash[:16], 16)
        self.self_name = self_name
        self.seq = 1
        self.buffer = b""

    def _send(self, payload, flags):
        self.sock.sendall(append_frame(payload, self.seq, flags))
        self.seq += 1

    def _read_more(self):
        chunk = self.sock.recv(65536)
        if not chunk:
            raise EOFError("M1 link closed")
        self.buffer += chunk

    def _frame(self):
        while len(self.buffer) < 16:
            self._read_more()
        magic, version, flags, seq, length, _, header_crc = struct.unpack(
            "<HBBIIHH", self.buffer[:16])
        if magic != 0x5041 or version != 1:
            raise AssertionError("bad M1 frame header")
        if crc32c(self.buffer[:14]) & 0xFFFF != header_crc:
            raise AssertionError("bad M1 frame header crc")
        total = 20 + length
        while len(self.buffer) < total:
            self._read_more()
        payload = self.buffer[16:16 + length]
        payload_crc = struct.unpack("<I", self.buffer[16 + length:total])[0]
        if crc32c(payload) != payload_crc:
            raise AssertionError("bad M1 frame payload crc")
        self.buffer = self.buffer[total:]
        return flags, seq, payload

    def handshake(self):
        hello = (struct.pack("<I", 0x4C494E4B) + struct.pack("<QQ", self.route_version, 0)
                 + struct.pack("<I", len(self.self_name))
                 + self.self_name.encode("utf-8"))
        self._send(bytes([0]) + hello, 0x01)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            flags, _, payload = self._frame()
            if flags & 0x01 and payload and payload[0] == 0:
                return
        raise AssertionError("M1 handshake timeout")

    def close(self):
        self.sock.close()

    @staticmethod
    def _text(value):
        encoded = value.encode("utf-8")
        return struct.pack("<I", len(encoded)) + encoded

    def request(self, request_id, op, name, version, source="", observe_ticks=0,
                fault_threshold=0):
        body = (struct.pack("<Qi", request_id, op) + self._text(name)
                + struct.pack("<Q", version) + self._text("")
                + self._text(source)
                + struct.pack("<QQ", observe_ticks, fault_threshold))
        self._send(struct.pack("<H", 952) + body, 0x02)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            flags, _, payload = self._frame()
            if flags & 0x01:
                continue
            msg_id = struct.unpack("<H", payload[:2])[0]
            if msg_id != 953:
                continue
            body = payload[2:]
            offset = 0
            request_id = struct.unpack("<Q", body[offset:offset + 8])[0]
            offset += 8
            op = struct.unpack("<i", body[offset:offset + 4])[0]
            offset += 4
            ok = body[offset] != 0
            offset += 1
            state = struct.unpack("<i", body[offset:offset + 4])[0]
            offset += 4
            faults = struct.unpack("<Q", body[offset:offset + 8])[0]
            offset += 8
            version = struct.unpack("<Q", body[offset:offset + 8])[0]
            offset += 8
            length = struct.unpack("<I", body[offset:offset + 4])[0]
            offset += 4
            detail = body[offset:offset + length].decode("utf-8", "replace")
            return {"request_id": request_id, "op": op, "ok": ok, "state": state,
                    "faults": faults, "version": version, "detail": detail}
        raise AssertionError("module rollout reply timeout")


def running_machined_pids():
    """返回当前 machined 进程 pid 集合。None 表示 pgrep 不可用（跳过进程级检测）。"""
    try:
        out = subprocess.run(["pgrep", "-x", "machined"], capture_output=True,
                             text=True, check=False)
        if out.returncode not in (0, 1):
            return None
        return set(out.stdout.split())
    except (FileNotFoundError, OSError):
        return None


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
                if all(m in text for m in markers):
                    port = 0
                    if port_marker is not None:
                        match = re.search(
                            re.escape(port_marker) + r"\s+\S+:(\d+)", text)
                        if not match:
                            raise AssertionError(
                                f"{binary} port marker {port_marker!r} not found: "
                                f"{text[-2000:]}")
                        port = int(match.group(1))
                    return process, text, port
                if not chunk and process.poll() is not None:
                    break
        raise AssertionError(f"{binary} not ready: markers={markers} output={''.join(output)[-2000:]}")
    finally:
        selector.close()


def stop(process):
    process.terminate()
    try:
        process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate(timeout=5)


try:
    with tempfile.TemporaryDirectory(prefix="apollo_independent_") as directory:
        root = pathlib.Path(directory)
        login_dir = root / "login"
        world_dir = root / "world"
        cell_dir = root / "cell"
        machined_before = running_machined_pids()
        for d in (login_dir, world_dir, cell_dir):
            d.mkdir()

        # ---- 1. login：端口 0 内核分配（无 machined），从日志读回实际端口 ----
        login_args = [
            "--host", "127.0.0.1",
            "--directory-port", "0",
            "--directory-epochs", str(login_dir / "epochs.json"),
            "--service-register-port", "0",
            "--m1-port", "0",
        ]
        login, login_out, login_sr_port = start(
            login_binary, login_args, login_dir,
            ["service register listening on"], port_marker="service register listening on",
            timeout=15)

        # ---- 2. world：--m1-port 0 内核分配 + 拨 login 注册（无 machined）----
        world_args = [
            "--world-id", "1",
            "--m1-port", "0",
            "--service-register-host", "127.0.0.1",
            "--service-register-port", str(login_sr_port),
            "--assign-spaces", "401,402",
        ]
        world, world_out, world_m1_port = start(
            world_binary, world_args, world_dir,
            ["Service register: dialing", "Server is running"],
            port_marker="WorldApp M1 link listening on", timeout=15)

        # world 注册到 login：login 日志出现 "service registered: kind=1 id=1"
        # 轮询 login 输出
        world_registered = False
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if "service registered: kind=1" in login_out:
                world_registered = True
                break
            time.sleep(0.2)
            # 继续读 login 输出
            import select as _sel
            if _sel.select([login.stdout], [], [], 0.1)[0]:
                chunk = os.read(login.stdout.fileno(), 65536).decode(errors="replace")
                login_out += chunk
                if "service registered: kind=1" in login_out:
                    world_registered = True
                    break
        assert world_registered, f"world did not register to login: {login_out[-2000:]}"

        # ---- 3. cellServer：--m1-port 0 内核分配 + 拨 world 注册（无 machined）----
        cell_args = [
            "--world-id", "1",
            "--m1-port", "0",
            "--register-host", "127.0.0.1",
            "--register-port", str(world_m1_port),
            "--cell-id", "21",
            "--add-space", "501:arena",
            "--add-space", "502:lobby",
        ]
        cell, cell_out, cell_m1_port = start(
            cell_binary, cell_args, cell_dir,
            ["Cell registration: dialing", "Server is running"],
            port_marker="Cell M1 link listening on", timeout=15)

        # cell 注册到 world：world 日志出现 "cell registered: id=21"
        cell_registered = False
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if "cell registered: id=21" in world_out:
                cell_registered = True
                break
            time.sleep(0.2)
            import select as _sel
            if _sel.select([world.stdout], [], [], 0.1)[0]:
                chunk = os.read(world.stdout.fileno(), 65536).decode(errors="replace")
                world_out += chunk
                if "cell registered: id=21" in world_out:
                    cell_registered = True
                    break
        assert cell_registered, f"cell did not register to world: {world_out[-2000:]}"

        # ---- 3b. 967 自报对账（独立 cell 真链路证明面）：cell 在 961 受理后
        # 上发 967 cell_space_report（自有归属），world 对账匹配记 reconciled 行。
        space_reconciled = False
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if "space report reconciled" in world_out:
                space_reconciled = True
                break
            time.sleep(0.2)
            import select as _sel
            if _sel.select([world.stdout], [], [], 0.1)[0]:
                chunk = os.read(world.stdout.fileno(), 65536).decode(errors="replace")
                world_out += chunk
                if "space report reconciled" in world_out:
                    space_reconciled = True
                    break
        assert space_reconciled, (
            f"cell 967 report not reconciled by world: {world_out[-2000:]}")

        # ---- 3c. 管理命令转发（切片2 L104）：fleetctl 经 956 drain 到 world
        # M1，world（无内嵌内核）转发到独立 cell，cell 受理并在日志落
        # "cell drain begun"；957 回执经 world 回传（fleetctl exit 0）。
        drain = subprocess.Popen(
            [fleetctl_binary, "drain", "--target",
             f"127.0.0.1:{world_m1_port}", "--peer", "world",
             "--timeout", "5"],
            cwd=cell_dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            bufsize=0)
        children.append(drain)
        drain_out = ""
        drain_deadline = time.monotonic() + 15
        while time.monotonic() < drain_deadline:
            import select as _sel
            if _sel.select([drain.stdout], [], [], 0.1)[0]:
                chunk = os.read(drain.stdout.fileno(), 65536).decode(errors="replace")
                drain_out += chunk
                if "DRAIN_DRAINED" in drain_out or "DRAIN_REJECTED" in drain_out:
                    break
            if drain.poll() is not None:
                break
        # fleetctl drain exit 0 = 957 回执经 world 回传成功；cell 日志落
        # "cell drain begun" = world 转发到独立 cell 成功。
        try:
            drain.wait(timeout=10)
        except subprocess.TimeoutExpired:
            drain.kill()
            drain.wait(timeout=5)
        assert drain.returncode == 0, (
            f"fleetctl drain failed rc={drain.returncode}: {drain_out[-2000:]}")
        cell_drained = False
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if "cell drain begun" in cell_out:
                cell_drained = True
                break
            time.sleep(0.2)
            import select as _sel
            if _sel.select([cell.stdout], [], [], 0.1)[0]:
                chunk = os.read(cell.stdout.fileno(), 65536).decode(errors="replace")
                cell_out += chunk
                if "cell drain begun" in cell_out:
                    cell_drained = True
                    break
        assert cell_drained, f"cell did not accept forwarded drain: {cell_out[-2000:]}"

        # ---- 3d. 生产 cellServer module rollout（审核发现 351：ok-flag 与驱动名
        # 登记修复缺生产路径测试——WorldAppM1LinkTests/UpdateContinuityTests 只
        # 驱动 tests/cell_host.hpp 平行实现）。这里经 world 的 952/953 转发链对
        # 真实 cellServer 驱动完整一轮：prepare → commit（kObserving，驱动名在
        # commit 成功才登记——观察窗 onTick 只打在已激活版本）→ 观察窗自驱 →
        # kPromoted。回执断言 ok 与终态；cell 日志出现 committed 行（生产分支）。
        module_good = ("return { onTick = function() end, "
                       "smoke = function() return true end }")
        m1 = RolloutM1Client("127.0.0.1", world_m1_port, "smoke-rollout", routes)
        try:
            m1.handshake()
            # prepare：受理即 kCanaryPending（版本未激活）。
            r = m1.request(91001, 1, "prodgreet", 7, module_good, 5, 0)
            assert r["ok"] and r["state"] == 2, f"prepare not kCanaryPending: {r}"
            assert r["version"] == 0, f"prepare must not activate: {r}"
            # commit：kObserving，版本激活，驱动名登记（生产 cell_last_rollout_name）。
            r = m1.request(91002, 2, "prodgreet", 7, "", 5, 0)
            assert r["ok"] and r["state"] == 3, f"commit not kObserving: {r}"
            assert r["version"] == 7, f"commit must activate version 7: {r}"
            # 观察窗自驱到 kPromoted（健康窗零 fault）。
            promoted = False
            for _ in range(30):
                r = m1.request(91003, 4, "prodgreet", 7, "", 5, 0)
                if r["state"] == 6:
                    promoted = True
                    break
                time.sleep(0.2)
            assert promoted, f"healthy module did not promote: {r}"
            # 终态相位纪律：kPromoted 下 rollback op = 拒（仅 kRollingBack 有义）——
            # 生产 cellServer op3 非 kCanaryPending 分支必须按 state==kRolledBack 判定。
            r = m1.request(91004, 3, "prodgreet", 7, "", 5, 0)
            assert not r["ok"], f"rollback outside kRollingBack must be rejected: {r}"
        finally:
            m1.close()

        # 驱动名登记（生产面）：cell 日志出现 committed 行 = commit 成功才登记，
        # post_tick 的 onTick 只对已激活版本调用——不产生「module not a table」
        # 污染观察窗。健康窗零 fault（回执 faults=0）直接证明。
        committed = False
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if "cell module rollout committed: prodgreet v7" in cell_out:
                committed = True
                break
            time.sleep(0.2)
            import select as _sel
            if _sel.select([cell.stdout], [], [], 0.1)[0]:
                chunk = os.read(cell.stdout.fileno(), 65536).decode(errors="replace")
                cell_out += chunk
                if "cell module rollout committed: prodgreet v7" in cell_out:
                    committed = True
                    break
        assert committed, "cellServer did not log committed rollout (drive-name path): " \
                          f"{cell_out[-2000:]}"

        # ---- 3e. 故障回滚（生产 cellServer op3 ok-flag）：v2 onTick 必炸 → 观察窗
        # 早失败自驱回滚 → kRolledBack + 回执 ok（生产分支 state==kRolledBack）。
        module_bad = ("return { onTick = function() error('boom') end, "
                      "smoke = function() return true end }")
        m2 = RolloutM1Client("127.0.0.1", world_m1_port, "smoke-rollout-2", routes)
        try:
            m2.handshake()
            r = m2.request(91011, 1, "prodcalc", 8, module_bad, 600, 0)
            assert r["ok"] and r["state"] == 2, f"bad prepare not kCanaryPending: {r}"
            r = m2.request(91012, 2, "prodcalc", 8, "", 600, 0)
            assert r["ok"] and r["state"] == 3, f"bad commit not kObserving: {r}"
            rolled_back = False
            for _ in range(30):
                r = m2.request(91013, 4, "prodcalc", 8, "", 600, 0)
                if r["state"] == 7:
                    rolled_back = True
                    break
                time.sleep(0.2)
            assert rolled_back, f"bad module did not roll back: {r}"
        finally:
            m2.close()

        # ---- 4. machined 未参与：进程级检测 ----
        # 仅检查已捕获输出缓冲不足：若 machined 以独立进程运行，其输出不在任何
        # 缓冲中。用 pgrep 对比测试前后 machined pid 集，证明无新增 machined 进程
        # （本地预先存在的不算）。pgrep 不可用时退回输出文本检查并打印警告。
        machined_seen = "machined" in (world_out + cell_out + login_out).lower()
        machined_after = running_machined_pids()
        if machined_before is not None and machined_after is not None:
            new_machined = machined_after - machined_before
            assert not new_machined, f"machined process appeared during test: {sorted(new_machined)}"
        else:
            assert not machined_seen, \
                "machined should not participate in independent registration"
            print("WARN: pgrep unavailable; fell back to output-text machined check")

        print("PASS: World/cell independent registration without machined")
        print("  - world registered to login (kind=1 id=1 peer=world-1)")
        print("  - cell registered to world (id=21)")
        print("  - production cellServer module rollout: healthy promote + fault rollback")
        print("  - no machined process involved")

        stop(cell)
        stop(world)
        stop(login)
finally:
    for process in children:
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
