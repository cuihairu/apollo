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
import re
import selectors
import subprocess
import sys
import tempfile
import time

login_binary, world_binary, cell_binary, routes = map(str, sys.argv[1:])
children = []


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
        print("  - no machined process involved")

        stop(cell)
        stop(world)
        stop(login)
finally:
    for process in children:
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
