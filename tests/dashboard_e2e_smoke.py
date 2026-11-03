"""DASH-1 阶段3 e2e：真 world 进程起 dashboard 浏览器面 HTTP 断言。

验证点：
1. world --dashboard-port 0（内核分配）起，stdout 报出实际监听端口。
2. GET / 200 text/html：内嵌单页骨架（标题「Apollo 状态」+ 轮询脚本
   setInterval + 日志尾渲染 pre 面）——浏览器面可加载。
3. GET /api/state 200 application/json：六键（process/links/orders/fleet/
   script/load）真实形状。
4. GET /nope 404（未路由显形）。
5. token 面：--dashboard-token 设置后无 Authorization → 401，带正确
   Bearer → 200。
"""
import json
import os
import pathlib
import re
import selectors
import socket
import subprocess
import sys
import tempfile
import time


def free_port():
    """分配一枚当前空闲 TCP 端口（bind :0 读回，随即关闭——极小竞态可接受）。"""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


world_binary = sys.argv[1]
children = []


def start(binary, args, cwd, markers, timeout=15):
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
                    return process, text
                if not chunk and process.poll() is not None:
                    break
        raise AssertionError(
            f"{binary} not ready: markers={markers} output={''.join(output)[-2000:]}")
    finally:
        selector.close()


def stop(process):
    process.terminate()
    try:
        process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate(timeout=5)


def http_get(host, port, path, token=None):
    """返回 (status, headers, body)。"""
    with socket.create_connection((host, port), timeout=5) as sock:
        sock.settimeout(5)
        req = (f"GET {path} HTTP/1.1\r\nHost: {host}:{port}\r\n"
               f"Connection: close\r\n")
        if token:
            req += f"Authorization: Bearer {token}\r\n"
        req += "\r\n"
        sock.sendall(req.encode())
        data = b""
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            data += chunk
    text = data.decode(errors="replace")
    head, _, body = text.partition("\r\n\r\n")
    status_line = head.splitlines()[0] if head else ""
    m = re.match(r"HTTP/\S+\s+(\d+)", status_line)
    status = int(m.group(1)) if m else 0
    headers = {}
    for line in head.splitlines()[1:]:
        if ":" in line:
            k, _, v = line.partition(":")
            headers[k.strip().lower()] = v.strip()
    return status, headers, body




def main():
    try:
        with tempfile.TemporaryDirectory(prefix="apollo_dashboard_e2e_") as d:
            root = pathlib.Path(d)
            # world 需要 route 表；m1-port 0 也让内核分配，避免端口冲突。
            dash_port = free_port()
            args = ["--m1-port", "0", "--dashboard-port", str(dash_port)]
            # world 的 route 表来自编译期宏（APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON），
            # 无需 --route-json。m1-port 0 = 内核分配；dashboard 用空闲实端口
            #（0 在 world 面 = 关，不能用内核分配）。
            world, _ = start(world_binary, args, root,
                             ["Dashboard listening on"])
            # start() 已等待 "Dashboard listening on" 标记；端口由 free_port 分配。
            # 实际绑定端口以 start 返回的输出为准（跨检查 stdout）。
            # 1. 首页
            status, headers, body = http_get("127.0.0.1", dash_port, "/")
            assert status == 200, f"GET / status {status}"
            assert "text/html" in headers.get("content-type", ""), headers
            assert "Apollo 状态" in body, "builtin html title missing"
            assert "setInterval" in body, "real-time refresh script missing"
            assert "log_tail" in body, "log tail renderer missing"

            # 2. 状态 JSON
            status, headers, body = http_get("127.0.0.1", dash_port, "/api/state")
            assert status == 200, f"GET /api/state status {status}"
            assert "application/json" in headers.get("content-type", ""), headers
            state = json.loads(body)
            for key in ("process", "links", "orders", "fleet", "script", "load"):
                assert key in state, f"state missing {key}: {list(state)}"
            assert state["process"]["role"] == "world", state["process"]

            # 3. 404
            status, _, _ = http_get("127.0.0.1", dash_port, "/nope")
            assert status == 404, f"GET /nope status {status}"

            stop(world)
            world = None

            # 4. token 面（重启带 token）
            dash_port2 = free_port()
            world2, _ = start(world_binary,
                              ["--m1-port", "0", "--dashboard-port", str(dash_port2),
                               "--dashboard-token", "s3cret"],
                              root, ["Dashboard listening on"])
            status, _, _ = http_get("127.0.0.1", dash_port2, "/api/state")
            assert status == 401, f"no-token GET /api/state status {status}"
            status, _, body = http_get("127.0.0.1", dash_port2, "/api/state",
                                       token="s3cret")
            assert status == 200, f"token GET /api/state status {status}"
            state = json.loads(body)
            assert state["process"]["role"] == "world", state["process"]

            stop(world2)
            world2 = None

            print("PASS: dashboard e2e")
            print("  - GET / 200 text/html (builtin page, setInterval, log_tail)")
            print("  - GET /api/state 200 application/json (6 keys, role=world)")
            print("  - GET /nope 404")
            print("  - token: no-header 401, correct Bearer 200")
    finally:
        for process in children:
            if process is not None and process.poll() is None:
                process.kill()
                process.communicate(timeout=5)


if __name__ == "__main__":
    main()
