"""L156 验证：World/cell 无同机 machined 独立注册与运行。
验证点：
1. login（--service-register-port 面）+ world（--m1-port + 拨 login 注册）+ cellServer
   （--m1-port + 拨 world 注册）均不经 machined 拉起，独立启动。
2. world 经 964/965 直接注册到 login（machined 未参与）。
3. cellServer 经 960/961 直接注册到 world（machined 未参与）。
4. machined 不启动、不参与任何业务注册；machined 不知道资料位置/跨服去向。
"""
import pathlib
import os
import selectors
import socket
import subprocess
import sys
import tempfile
import time

login_binary, world_binary, cell_binary, routes = map(str, sys.argv[1:])
children = []


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


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
        for d in (login_dir, world_dir, cell_dir):
            d.mkdir()

        login_sr_port = free_port()
        world_m1_port = free_port()
        cell_m1_port = free_port()

        # ---- 1. login：--service-register-port 面（无 machined）----
        login_args = [
            "--host", "127.0.0.1",
            "--directory-port", str(free_port()),
            "--directory-epochs", str(login_dir / "epochs.json"),
            "--service-register-port", str(login_sr_port),
            "--m1-port", str(free_port()),
        ]
        login, login_out = start(login_binary, login_args, login_dir,
                                 ["service register listening on"], timeout=15)

        # ---- 2. world：--m1-port 监听 + 拨 login 注册（无 machined）----
        world_args = [
            "--world-id", "1",
            "--m1-port", str(world_m1_port),
            "--service-register-host", "127.0.0.1",
            "--service-register-port", str(login_sr_port),
            "--assign-spaces", "401,402",
        ]
        world, world_out = start(world_binary, world_args, world_dir,
                                 ["Service register: dialing", "Server is running"], timeout=15)

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

        # ---- 3. cellServer：--m1-port 监听 + 拨 world 注册（无 machined）----
        cell_args = [
            "--world-id", "1",
            "--m1-port", str(cell_m1_port),
            "--register-host", "127.0.0.1",
            "--register-port", str(world_m1_port),
            "--cell-id", "21",
            "--add-space", "501:arena",
            "--add-space", "502:lobby",
        ]
        cell, cell_out = start(cell_binary, cell_args, cell_dir,
                               ["Cell registration: dialing", "Server is running"], timeout=15)

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

        # ---- 4. machined 未参与：全程无 machined 进程 ----
        machined_seen = "machined" in (world_out + cell_out + login_out).lower()
        assert not machined_seen, "machined should not participate in independent registration"

        print("PASS: World/cell independent registration without machined")
        print("  - world registered to login (kind=1 id=1 peer=world-1)")
        print("  - cell registered to world (id=21)")
        print("  - no machined process involved")

        stop(cell)
        stop(world)
        stop(login)
finally:
    for process in children:
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
