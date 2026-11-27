"""D19 清单第 5 行真机样本：不支持版本拒绝（保持现状）+ 兼容窗口受理。

真实进程真实 M1 wire（database store face，握手统一走 interserver_link）：
  1. 本代握手受理（route_version 取自 route_json schema_hash，与各进程同源）。
  2. 版本不在受理集 → 服务端 finish_down 隔离（kRouteVersionMismatch：无握手
     回执、连接被对端关闭 = 客户端 EOF）——「版本对不在清单 → code 5 隔离」
     的现状样本。
  3. APOLLO_ROUTE_COMPAT 开窗（sdk-contract §6 N-1 发布窗）：窗内旧代握手
     受理；窗外第三版本仍拒；本代恒受理（窗不顶替本代）。
对应 06 §1 清单：不支持版本拒绝；版本维度的未升级客户端（业务往返维度由
e869a102 958 旧形样本覆盖，不在此重复）。
"""
import json
import os
import pathlib
import selectors
import socket
import struct
import subprocess
import sys
import tempfile
import time

database_binary, routes = map(str, sys.argv[1:])
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


class VersionDialer:
    """只拨握手：返回 "ok"（收到握手回执）或 "closed"（被服务端隔离断连）。"""

    def __init__(self, host, port, route_version, name="compat-probe", timeout=5.0):
        with open(routes, encoding="utf-8") as file:
            schema_hash = json.load(file)["schema_hash"]
        self.expected = int(schema_hash[:16], 16)
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.route_version = route_version
        self.name = name
        self.seq = 1
        self.buffer = b""
        self.closed = False

    def _read_more(self):
        chunk = self.sock.recv(65536)
        if not chunk:
            self.closed = True
            raise EOFError("peer closed")
        self.buffer += chunk

    def _frame(self):
        while len(self.buffer) < 16:
            self._read_more()
        header = self.buffer[:16]
        magic, version, flags, seq, length, _, header_crc = struct.unpack(
            "<HBBIIHH", header)
        if magic != 0x5041 or version != 1:
            raise AssertionError("bad M1 frame header")
        if crc32c(header[:14]) & 0xFFFF != header_crc:
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

    def dial(self):
        hello = (struct.pack("<I", 0x4C494E4B)
                 + struct.pack("<QQ", self.route_version, 0)
                 + struct.pack("<I", len(self.name))
                 + self.name.encode("utf-8"))
        self.sock.sendall(append_frame(bytes([0]) + hello, self.seq, 0x01))
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            try:
                flags, _, payload = self._frame()
            except (EOFError, ConnectionResetError):
                return "closed"
            if flags & 0x01 and payload and payload[0] == 0:
                return "ok"
        raise AssertionError("handshake timeout (neither reply nor isolation)")


def start(env_extra=None):
    workdir = pathlib.Path(tempdir) / f"db_{start.counter}"
    start.counter += 1
    workdir.mkdir()
    env = dict(os.environ)
    env.pop("APOLLO_ROUTE_COMPAT", None)
    if env_extra:
        env.update(env_extra)
    process = subprocess.Popen(
        [database_binary, "--port", "0", "--route-json", routes,
         "--data-dir", str(workdir / "players"),
         "--journal", str(workdir / "journal.log")],
        cwd=workdir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        bufsize=0, env=env)
    children.append(process)
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    deadline = time.monotonic() + 15
    output = []
    try:
        while time.monotonic() < deadline:
            if selector.select(0.1):
                chunk = os.read(process.stdout.fileno(), 65536).decode(errors="replace")
                output.append(chunk)
                text = ''.join(output)
                if "database ready port=" in text:
                    port = int(text.split("database ready port=", 1)[1]
                               .strip().split()[0])
                    return process, port
                if not chunk and process.poll() is not None:
                    break
        raise AssertionError(f"database not ready: {''.join(output)[-2000:]}")
    finally:
        selector.close()


start.counter = 0


def stop(process):
    process.terminate()
    try:
        process.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate(timeout=5)
    assert process.returncode == 0, f"database exit status {process.returncode}"


try:
    with tempfile.TemporaryDirectory(prefix="apollo_route_compat_smoke_") as directory:
        tempdir = directory

        with open(routes, encoding="utf-8") as file:
            current = int(json.load(file)["schema_hash"][:16], 16)
        wrong_v1 = current ^ 0x5A5A0000
        wrong_v2 = current ^ 0x3C3C0000
        assert len({current, wrong_v1, wrong_v2}) == 3

        # ---- 1. 无窗：本代受理 ----
        database, port = start()
        verdict = VersionDialer("127.0.0.1", port, current).dial()
        assert verdict == "ok", f"current-gen handshake: {verdict}"

        # ---- 2. 无窗：窗外版本隔离（保持现状 = 无回执直接断连）----
        verdict = VersionDialer("127.0.0.1", port, wrong_v1).dial()
        assert verdict == "closed", f"out-of-set version must isolate: {verdict}"

        stop(database)

        # ---- 3. 开窗 APOLLO_ROUTE_COMPAT=wrong_v1：窗内旧代受理 ----
        # env 取 hash 前缀十六进制 token（route_version_from_hash 同口径：
        # ≤16 hex 左对齐，非十进制），16 位全宽 = 无移位直映。
        database, port = start({"APOLLO_ROUTE_COMPAT": f"{wrong_v1:016x}"})
        verdict = VersionDialer("127.0.0.1", port, wrong_v1).dial()
        assert verdict == "ok", f"in-window old-gen handshake: {verdict}"

        # ---- 4. 开窗不敞门：窗外第三版本仍拒、本代恒受理 ----
        verdict = VersionDialer("127.0.0.1", port, wrong_v2).dial()
        assert verdict == "closed", f"third version must stay isolated: {verdict}"
        verdict = VersionDialer("127.0.0.1", port, current).dial()
        assert verdict == "ok", f"current-gen with window open: {verdict}"

        stop(database)
        children.clear()
        print("Route version compat/isolation smoke passed")
finally:
    for process in children:
        if process.poll() is None:
            process.kill()
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                pass
