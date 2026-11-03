#!/usr/bin/env python3
# Apollo gateway 斜坡 bot（tools/bots）：真协议栈客户端——TCP/WS 同线形
# （ws binary 载荷 = link Frame），逐 bot：
#   connect → PING 建会话 → GATEWAY_ASSIGN_REQUEST 线缆准入（票据换绑定）
#   → CELL_ENTITY_MOVE 移动脚本（圆周巡走）+ PING 心跳 RTT 采样
#   → 掉线退避重连（保留窗内携旧会话号首包 = resume 路径，PONG 回头同号即恢复成）。
# 斜坡三件套（KBEngine defaultAddBots 口径）：--count 总量 / --ramp-tick-time
# 每拍秒数 / --ramp-tick-count 每拍增量，直到 count 满员。
# stdlib-only（仓内 client_demo 惯例）；退出码 0 = 全部达成（登录成功 + 有
# 移动/心跳流量），非 0 = 有 bot 未达成（CI 冒烟可直接断言）。
#
# 协议取证面（modules/protocol + net/link）：
#   link Frame: header 16B = magic u16 LE 0x5041 'AP' / ver u8=1 / flags u8 /
#               seq u32 LE / len u32 LE / 2B 保留 / crc32c(header[0:14]) u16 LE；
#               payload + crc32c(payload) u32 LE。
#   MessageHeader: 28B pack(1) LE = magic u32 0x42575452 / ver u16=1 / type u16 /
#               length u32(body) / sequence u64 / sessionId u64。
#   Ping/Pong body: u64 时间戳。CellEntityMove body: u64 entityId + 12B oldPos
#               + 12B newPos。GatewayAssignRequest body: JSON。

import argparse
import json
import math
import socket
import struct
import sys
import threading
import time
import random

# ---------------------------------------------------------------- wire codec

MAGIC_MSG = 0x42575452
MAGIC_FRAME = 0x5041
MSG_HEADER = struct.Struct("<IHHIQQ")  # 28B pack(1)

T_LOGIN_REQUEST = 0x0001
T_GATEWAY_ASSIGN_REQUEST = 0x0010
T_GATEWAY_ASSIGN_RESPONSE = 0x0011
T_CELL_ENTITY_MOVE = 0x0034
T_PING = 0x1000
T_PONG = 0x1001

_CRC_TABLE = []


def _crc32c_init():
    poly = 0x82F63B78
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ (poly if c & 1 else 0)
        _CRC_TABLE.append(c)


_crc32c_init()


def crc32c(data: bytes) -> int:
    c = 0xFFFFFFFF
    for b in data:
        c = _CRC_TABLE[(c ^ b) & 0xFF] ^ (c >> 8)
    return c ^ 0xFFFFFFFF


def encode_frame(seq: int, flags: int, payload: bytes) -> bytes:
    head = bytearray(16)
    head[0] = MAGIC_FRAME & 0xFF
    head[1] = MAGIC_FRAME >> 8
    head[2] = 1
    head[3] = flags
    head[4:8] = struct.pack("<I", seq)
    head[8:12] = struct.pack("<I", len(payload))
    hc = crc32c(bytes(head[:14]))
    head[14] = hc & 0xFF
    head[15] = (hc >> 8) & 0xFF
    out = bytes(head) + payload
    pc = crc32c(payload)
    return out + struct.pack("<I", pc)


class FrameParser:
    """流式拆 link Frame（对齐 net/link FrameParser 语义：坏头重同步、
    尾校验不过 = fatal）。"""

    def __init__(self):
        self.buf = b""

    def feed(self, data: bytes):
        self.buf += data
        frames = []
        while True:
            if len(self.buf) < 20:
                break
            magic = self.buf[0] | (self.buf[1] << 8)
            if magic != MAGIC_FRAME:
                self.buf = self.buf[1:]  # 坏头重同步
                continue
            length = int.from_bytes(self.buf[8:12], "little")
            if length > 1024 * 1024:
                return frames, True
            total = 16 + length + 4
            if len(self.buf) < total:
                break
            body = self.buf[16:16 + length]
            want = int.from_bytes(self.buf[16 + length:total], "little")
            self.buf = self.buf[total:]
            if crc32c(body) != want:
                return frames, True
            frames.append(body)
        return frames, False


def msg_header(msg_type: int, body: bytes, sequence: int, session_id: int) -> bytes:
    return MSG_HEADER.pack(MAGIC_MSG, 1, msg_type, len(body), sequence, session_id)


def ping_packet(seq, session_id, timestamp):
    return msg_header(T_PING, struct.pack("<Q", timestamp), seq, session_id) + struct.pack("<Q", timestamp)


def move_packet(seq, session_id, entity_id, old, new):
    body = struct.pack("<Q", entity_id) + struct.pack("<3f", *old) + struct.pack("<3f", *new)
    return msg_header(T_CELL_ENTITY_MOVE, body, seq, session_id) + body


def create_entity_packet(seq, session_id, entity_id, space_id, x, y, z):
    # CellCreateEntity body：u64 entityId + u8 entityType(1=PLAYER) + u32
    # spaceId + 3×f32 position（codec.cpp 顺序 memcpy，无对齐填充）。
    body = struct.pack("<QBI", entity_id, 1, space_id) + struct.pack("<3f", x, y, z)
    return msg_header(0x0030, body, seq, session_id) + body


def assign_packet(seq, session_id, player_id, ticket):
    body = json.dumps({"playerId": player_id, "sessionId": session_id,
                       "loginTicket": ticket}).encode()
    return msg_header(T_GATEWAY_ASSIGN_REQUEST, body, seq, session_id) + body


# ---------------------------------------------------------------- ws client

def ws_send_masked(sock, payload: bytes):
    key = bytes([random.randint(0, 255) for _ in range(4)])
    maskbit = 0x80
    n = len(payload)
    if n < 126:
        head = struct.pack("!BB", maskbit | 0x2, maskbit | n)
    elif n <= 0xFFFF:
        head = struct.pack("!BBH", maskbit | 0x2, maskbit | 126, n)
    else:
        head = struct.pack("!BBQ", maskbit | 0x2, maskbit | 127, n)
    sock.sendall(head + key + bytes(b ^ key[i % 4] for i, b in enumerate(payload)))


# ---------------------------------------------------------------- stats

class Stats:
    def __init__(self):
        self.lock = threading.Lock()
        self.logins_ok = 0
        self.logins_fail = 0
        self.moves = 0
        self.pings = 0
        self.pongs = 0
        self.reconnects = 0
        self.resumes_ok = 0
        self.frames_rx = 0
        self.rtts = []
        self.errors = []

    def snapshot(self):
        with self.lock:
            rtts = sorted(self.rtts)
            n = len(rtts)
            pct = lambda p: rtts[min(n - 1, int(n * p))] if n else 0
            return {
                "logins_ok": self.logins_ok, "logins_fail": self.logins_fail,
                "moves": self.moves, "pings": self.pings, "pongs": self.pongs,
                "reconnects": self.reconnects, "resumes_ok": self.resumes_ok,
                "frames_rx": self.frames_rx,
                "rtt_ms": {"p50": pct(0.50), "p95": pct(0.95),
                           "min": rtts[0] if n else 0, "max": rtts[-1] if n else 0,
                           "n": n},
                "errors": list(self.errors)[:8],
            }


STATS = Stats()


# ---------------------------------------------------------------- bot

class Bot:
    def __init__(self, index, args):
        self.index = index
        self.args = args
        self.player_id = args.player_base + index
        self.ticket = f"bot-ticket-{self.index}-{random.randint(0, 1 << 30)}"
        self.session_id = 0
        self.entity_id = self.player_id
        self.msg_seq = 0
        self.frame_seq = 1
        self.online = False
        self.stop_flag = False

    def _next_msg(self):
        self.msg_seq += 1
        return self.msg_seq

    def _next_frame(self):
        self.frame_seq += 1
        return self.frame_seq

    def run(self):
        backoff = self.args.backoff_ms / 1000.0
        deadline = time.monotonic() + self.args.duration
        while not self.stop_flag and time.monotonic() < deadline:
            try:
                self._session(deadline)
                backoff = self.args.backoff_ms / 1000.0  # 成功在册即复位
            except Exception as exc:  # noqa: BLE001
                with STATS.lock:
                    STATS.errors.append(f"bot{self.index}: {type(exc).__name__}: {exc}")
            if self.stop_flag or time.monotonic() >= deadline:
                break
            # 退避重连（有界指数 + 抖动）
            with STATS.lock:
                STATS.reconnects += 1
            time.sleep(backoff * (1 + random.random() * 0.3))
            backoff = min(backoff * 2, self.args.backoff_max_ms / 1000.0)

    def _connect(self):
        sock = socket.create_connection((self.args.host, self.args.port), timeout=5)
        sock.settimeout(2.0)
        if self.args.transport == "ws":
            path = self.args.ws_path
            key = "dGhlIHNhbXBsZSBub25jZQ=="
            req = (f"GET {path} HTTP/1.1\r\nHost: {self.args.host}\r\n"
                   f"Upgrade: websocket\r\nConnection: Upgrade\r\n"
                   f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n")
            sock.sendall(req.encode())
            resp = b""
            while b"\r\n\r\n" not in resp:
                chunk = sock.recv(4096)
                if not chunk:
                    raise ConnectionError("ws handshake aborted")
                resp += chunk
            if b" 101 " not in resp.split(b"\r\n", 1)[0]:
                raise ConnectionError("ws upgrade refused: " + resp[:60].decode(errors="replace"))
        return sock

    def _send(self, sock, payload):
        if self.args.transport == "ws":
            ws_send_masked(sock, encode_frame(self._next_frame(), 0, payload))
        else:
            sock.sendall(encode_frame(self._next_frame(), 0, payload))

    def _recv_messages(self, sock, parser):
        data = sock.recv(65536)
        if not data:
            raise ConnectionError("eof")
        frames, fatal = parser.feed(data)
        if fatal:
            raise ConnectionError("frame protocol error")
        return frames

    def _session(self, deadline):
        sock = self._connect()
        parser = FrameParser()
        try:
            # ① PING 建会话；重连首包携旧会话号 = 恢复尝试（gateway 保留窗）。
            resume = self.session_id != 0
            self._send(sock, ping_packet(0, self.session_id, int(time.time() * 1000)))
            pong = self._wait_type(sock, parser, T_PONG, 5.0)
            if pong is None:
                raise TimeoutError("no pong to session ping")
            sid = pong[1]
            if resume:
                if sid == self.session_id:
                    with STATS.lock:
                        STATS.resumes_ok += 1
                    print(f"[bot{self.index}] resumed session {sid}")
                else:
                    # 窗过/号死 → 全新会话 = 完整重登路径
                    print(f"[bot{self.index}] resume fell back to fresh session {sid}")
            self.session_id = sid
            self.msg_seq = 1

            # ② 线缆准入：票据换会话绑定（未认证会话发移动会被 gateway 拒）。
            self._send(sock, assign_packet(0, sid, self.player_id, self.ticket))
            reply = self._wait_type(sock, parser, T_GATEWAY_ASSIGN_RESPONSE, 5.0)
            if reply is None:
                raise TimeoutError("no assign response")
            payload = reply[0]
            blen = MSG_HEADER.unpack_from(payload, 0)[3]
            body = payload[MSG_HEADER.size:MSG_HEADER.size + blen]
            resp = json.loads(body.decode())
            if not resp.get("success"):
                raise RuntimeError("admission rejected: " + resp.get("errorMessage", ""))
            with STATS.lock:
                STATS.logins_ok += 1
                STATS.frames_rx += 1
            self.online = True
            print(f"[bot{self.index}] logged in session {sid} player {self.player_id}")

            # ②′ 入场：建玩家实体（e2e 同序——cell 收到 create 才有 AOI/移动
            # 权威态；落点 = 各 bot 圆周起点，space 取 --space）。
            x0 = self.args.radius * math.cos(self.index * 0.7)
            z0 = self.args.radius * math.sin(self.index * 0.7)
            self._send(sock, create_entity_packet(0, sid, self.entity_id,
                                                  self.args.space, x0, 0.0, z0))
            self._send(sock, move_packet(0, sid, self.entity_id, (x0, 0.0, z0),
                                         (x0, 0.0, z0)))
            with STATS.lock:
                STATS.moves += 1

            # ③ 移动 + 心跳循环（圆周巡走；PING 计 RTT）。
            radius = self.args.radius
            angle0 = self.index * 0.7
            last_move = 0.0
            last_ping = 0.0
            while time.monotonic() < deadline and not self.stop_flag:
                now = time.monotonic()
                if now - last_move >= self.args.move_interval_ms / 1000.0:
                    last_move = now
                    t = now + angle0
                    x = radius * math.cos(t)
                    y = radius * math.sin(t)
                    z = 0.0
                    self._send(sock, move_packet(0, sid, self.entity_id, (x, y, z), (x, y, z)))
                    with STATS.lock:
                        STATS.moves += 1
                if now - last_ping >= self.args.heartbeat_ms / 1000.0:
                    last_ping = now
                    ts = int(time.time() * 1000)
                    self._send(sock, ping_packet(0, sid, ts))
                    with STATS.lock:
                        STATS.pings += 1
                # 收面：PONG 计 RTT；其余帧计数（下行含 world 回带/广播）。
                try:
                    for payload in self._recv_messages(sock, parser):
                        if len(payload) < MSG_HEADER.size:
                            continue
                        magic, ver, mtype, blen, seq, rsid = MSG_HEADER.unpack_from(payload, 0)
                        if magic != MAGIC_MSG:
                            continue
                        with STATS.lock:
                            STATS.frames_rx += 1
                        if mtype == T_PONG:
                            sent = struct.unpack_from("<Q", payload, MSG_HEADER.size)[0]
                            with STATS.lock:
                                STATS.pongs += 1
                                STATS.rtts.append(max(0.0, time.time() * 1000 - sent))
                except socket.timeout:
                    continue  # 收面空闲（无下行拍）不是错误——继续发面
                except ConnectionError:
                    raise
        finally:
            self.online = False
            sock.close()

    def _wait_type(self, sock, parser, msg_type, timeout_s):
        end = time.monotonic() + timeout_s
        while time.monotonic() < end:
            sock.settimeout(max(0.05, end - time.monotonic()))
            for payload in self._recv_messages(sock, parser):
                if len(payload) >= MSG_HEADER.size:
                    magic, ver, mtype, blen, seq, rsid = MSG_HEADER.unpack_from(payload, 0)
                    if magic == MAGIC_MSG and mtype == msg_type:
                        return payload, rsid
        return None


def main():
    ap = argparse.ArgumentParser(description="Apollo gateway ramp bots")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=9002)
    ap.add_argument("--transport", choices=["tcp", "ws"], default="tcp")
    ap.add_argument("--ws-path", default="/game")
    # 斜坡三件套（defaultAddBots 口径）：count 总量 / 每 tick-time 秒加 tick-count 个
    ap.add_argument("--count", type=int, default=8, help="total bots (totalCount)")
    ap.add_argument("--ramp-tick-time", type=float, default=1.0,
                    help="seconds per ramp tick (tickTime)")
    ap.add_argument("--ramp-tick-count", type=int, default=4,
                    help="bots added per tick (tickCount)")
    ap.add_argument("--player-base", type=int, default=900000)
    ap.add_argument("--duration", type=float, default=20.0, help="run seconds")
    ap.add_argument("--move-interval-ms", type=int, default=200)
    ap.add_argument("--heartbeat-ms", type=int, default=1000)
    ap.add_argument("--radius", type=float, default=5.0)
    ap.add_argument("--space", type=int, default=501)
    ap.add_argument("--backoff-ms", type=int, default=300, help="reconnect backoff base")
    ap.add_argument("--backoff-max-ms", type=int, default=5000)
    ap.add_argument("--summary-interval-s", type=float, default=5.0)
    args = ap.parse_args()

    bots = []
    spawned = 0
    next_tick = time.monotonic()

    def spawn(n):
        nonlocal spawned
        for i in range(n):
            b = Bot(spawned, args)
            spawned += 1
            t = threading.Thread(target=b.run, daemon=True)
            b.thread = t
            bots.append((b, t))
            t.start()

    print(f"bots target={args.host}:{args.port} transport={args.transport} "
          f"count={args.count} ramp={args.ramp_tick_count}/{args.ramp_tick_time}s "
          f"duration={args.duration}s")

    stop = time.monotonic() + args.duration
    last_summary = time.monotonic()
    while time.monotonic() < stop:
        now = time.monotonic()
        if spawned < args.count and now >= next_tick:
            add = min(args.ramp_tick_count, args.count - spawned)
            spawn(add)
            print(f"[ramp] +{add} bots ({spawned}/{args.count})")
            next_tick = now + args.ramp_tick_time
        if now - last_summary >= args.summary_interval_s:
            last_summary = now
            print("[stats] " + json.dumps(STATS.snapshot()))
        time.sleep(0.05)

    for b, _ in bots:
        b.stop_flag = True
    for _, t in bots:
        t.join(timeout=5)

    final = STATS.snapshot()
    print("[final] " + json.dumps(final))
    ok = (final["logins_ok"] >= args.count
          and final["moves"] > 0
          and final["pongs"] > 0)
    print("RESULT:", "OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
