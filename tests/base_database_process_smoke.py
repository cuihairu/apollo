"""Independent processes, independent work directories, actual M1 TCP and file storage."""
import pathlib
import os
import selectors
import subprocess
import sys
import tempfile
import time

base_binary, database_binary, probe_binary, routes = map(str, sys.argv[1:])
children = []

def start(binary, args, cwd, marker):
    process = subprocess.Popen([binary, *args], cwd=cwd, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, bufsize=0)
    children.append(process)
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    deadline = time.monotonic() + 10
    output = []
    try:
        while time.monotonic() < deadline:
            if selector.select(0.1):
                chunk = os.read(process.stdout.fileno(), 65536).decode()
                output.append(chunk)
                for line in ''.join(output).splitlines():
                    if marker in line:
                        return process, int(line.split(marker, 1)[1].strip())
                if not chunk and process.poll() is not None:
                    break
        raise AssertionError(f"{binary} not ready: {''.join(output)}")
    finally:
        selector.close()

def stop(process):
    process.terminate()
    process.communicate(timeout=5)
    assert process.returncode == 0, f"exit status {process.returncode}"

def probe(port, peer, action):
    subprocess.run([probe_binary, "--probe", str(port), peer, action], check=True, timeout=10)

try:
    with tempfile.TemporaryDirectory(prefix="apollo_base_database_process_") as directory:
        root = pathlib.Path(directory)
        base_dir, database_dir = root / "base", root / "database"
        base_dir.mkdir()
        database_dir.mkdir()
        database_args = ["--port", "0", "--route-json", routes,
                         "--data-dir", str(database_dir / "players"),
                         "--journal", str(database_dir / "journal.log")]
        storage, storage_port = start(database_binary, database_args, database_dir,
                                      "database ready port=")
        probe(storage_port, "database", "seed")
        base_args = ["--port", "0", "--database-port", str(storage_port), "--route-json", routes]
        base, base_port = start(base_binary, base_args, base_dir, "base ready port=")
        probe(base_port, "base", "load")
        stop(base)
        base, base_port = start(base_binary, base_args, base_dir, "base ready port=")
        probe(base_port, "base", "load")
        stop(base)
        stop(storage)
        storage, storage_port = start(database_binary, database_args, database_dir,
                                      "database ready port=")
        probe(storage_port, "database", "load")
        stop(storage)
        # Base must fail when database is unavailable, never bootstrap local files.
        failed = subprocess.run([base_binary, *base_args], cwd=base_dir,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=8)
        assert failed.returncode != 0 and "base ready" not in failed.stdout, failed.stdout
        assert not list(base_dir.rglob("player_*.json")), "Base wrote profile files"
        assert not list(base_dir.rglob("*.log")), "Base opened a storage journal"
        print("Independent Base/database process restart and storage isolation passed")
finally:
    for process in children:
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
