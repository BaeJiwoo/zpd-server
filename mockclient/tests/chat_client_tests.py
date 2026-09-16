import pathlib
import queue
import re
import socket
import struct
import subprocess
import sys
import threading
import time

BIN = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "out/build/windows-x64/Debug").resolve()


CLIENT_EXE = sys.argv[2] if len(sys.argv) > 2 else "zpd-client.exe"


class Process:
    def __init__(self, name, port):
        self.process = subprocess.Popen(
            [str(BIN / name), str(port)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            encoding="utf-8", errors="replace", bufsize=1,
        )
        self.lines = queue.Queue()
        self.history = []
        self.reader = threading.Thread(target=self.read, daemon=True)
        self.reader.start()

    def read(self):
        for line in self.process.stdout:
            self.history.append(line.rstrip())
            self.lines.put(line.rstrip())

    def command(self, text):
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()

    def expect(self, text, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                line = self.lines.get(timeout=max(0.01, deadline - time.monotonic()))
            except queue.Empty:
                break
            if text in line:
                return line
        raise AssertionError(f"Missing {text!r}: {self.history}")

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait(timeout=5)
        self.reader.join(timeout=1)


processes = []
try:
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    server = Process("zpd-server.exe", port)
    processes.append(server)
    server.expect("server ready")
    clients = [Process(CLIENT_EXE, port) for _ in range(2)]
    processes.extend(clients)
    first, second = clients
    ids = []
    for client in clients:
        client.expect("Notifications arrive")
        client.command("/enter")
        ids.append(int(client.expect("Temporary player ID:").split(":")[-1]))
    first.command("/create nope")
    first.expect("Invalid argument")
    first.command("/create 2")
    room_line = first.expect("Room ")
    room_id = int(re.search(r"Room (\d+)", room_line)[1])
    second.command(f"/join {room_id}")
    second.expect("(2/2)")
    first.expect(f"PlayerJoined: {ids[1]}")
    first.expect("(2/2)")
    first.command("안녕하세요 👋")
    first.expect(f"[Player {ids[0]}] 안녕하세요 👋")
    second.expect(f"[Player {ids[0]}] 안녕하세요 👋")
    second.command("/say hello back")
    second.expect(f"[Player {ids[1]}] hello back")
    first.expect(f"[Player {ids[1]}] hello back")
    first.command("/members")
    first.expect("(2/2)")
    second.command("/leave")
    second.expect("Left room")
    first.expect(f"PlayerLeft: {ids[1]}")
    first.expect("(1/2)")
    second.command(f"/join {room_id}")
    second.expect("(2/2)")
    first.expect("PlayerJoined:")
    first.expect("(2/2)")
    second.command("/quit")
    assert second.process.wait(timeout=3) == 0
    first.expect("PlayerLeft:")
    first.expect("(1/2)")
    first.command("/ping")
    first.command("/echo hello")
    first.expect("Pong")
    first.expect("Echo: hello")
    server.command("")
    assert server.process.wait(timeout=5) == 0
    first.expect("Server disconnected")
    assert first.process.wait(timeout=3) == 1

    for malformed in (False, True):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            bad = Process(CLIENT_EXE, listener.getsockname()[1])
            processes.append(bad)
            peer, _ = listener.accept()
            with peer:
                bad.expect("Notifications arrive")
                bad.command("/ping")
                data = b""
                while len(data) < 8:
                    data += peer.recv(8 - len(data))
                if malformed:
                    peer.sendall(struct.pack("!HBBI", 9, 130, 0, struct.unpack("!I", data[4:])[0]) + b"x")
                    bad.expect("Unexpected Ping body")
            bad.expect("failed: connection closed")
            assert bad.process.wait(timeout=3) == 1
    print("Interactive client checks passed: room snapshots, bidirectional UTF-8 chat, idle notifications, leave, quit, remote close, malformed response, pending cleanup.")
finally:
    for process in reversed(processes):
        process.close()
