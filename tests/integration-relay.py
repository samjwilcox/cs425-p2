#!/usr/bin/env python3
"""Exercise the release binary against a deterministic local UDP relay.

This is a protocol integration check, not the course relay or a substitute for
its required 50 ms performance measurements.
"""
import pathlib
import random
import socket
import subprocess
import tempfile
import threading
import time

APP = pathlib.Path(__file__).resolve().parents[1] / "build/release/myapp"


class Relay:
    """Pair two HELLO clients and forward UDP with optional damage."""

    def __init__(self, loss=0.0, corrupt=0.0, dup=0.0):
        """Bind a loopback UDP socket and start deterministic forwarding."""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(("127.0.0.1", 0))
        self.socket.settimeout(0.1)
        self.port = self.socket.getsockname()[1]
        self.parties = {}
        self.rng = random.Random(7)
        self.loss, self.corrupt, self.dup = loss, corrupt, dup
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        """Answer registrations and forward all subsequent datagrams."""
        while not self.stop.is_set():
            try:
                data, source = self.socket.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                return
            if data.startswith(b"HELLO "):
                parts = data.split()
                if len(parts) not in (3, 6):
                    self.socket.sendto(b"ERR bad hello", source)
                    continue
                role = parts[2].decode("ascii")
                if role == "send" and "recv" not in self.parties:
                    self.socket.sendto(b"ERR no receiver", source)
                    continue
                self.parties[role] = source
                self.socket.sendto(b"OK", source)
                continue
            if source == self.parties.get("send"):
                destination = self.parties.get("recv")
            elif source == self.parties.get("recv"):
                destination = self.parties.get("send")
            else:
                continue
            if destination is None or self.rng.random() < self.loss:
                continue
            if self.rng.random() < self.corrupt:
                damaged = bytearray(data)
                pos = self.rng.randrange(len(damaged))
                damaged[pos] ^= 1 << self.rng.randrange(8)
                data = bytes(damaged)
            time.sleep(0.002)
            self.socket.sendto(data, destination)
            if self.rng.random() < self.dup:
                self.socket.sendto(data, destination)

    def close(self):
        """Stop the forwarding thread and release its UDP port."""
        self.stop.set()
        self.socket.close()
        self.thread.join(timeout=1)


def check_transfer(content, damaged=False):
    """Start a receiver before the sender and compare the completed file."""
    relay = Relay(0.10 if damaged else 0, 0.05 if damaged else 0,
                  0.10 if damaged else 0)
    try:
        with tempfile.TemporaryDirectory() as folder:
            source = pathlib.Path(folder) / "input.bin"
            target = pathlib.Path(folder) / "output.bin"
            source.write_bytes(content)
            common = ["-s", "sam-1", "-p", str(relay.port), "127.0.0.1"]
            receiver = subprocess.Popen([str(APP), "recv", *common, str(target)],
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                time.sleep(0.05)
                sender = subprocess.run([str(APP), "send", "-s", "sam-1", "-w", "16",
                                         "-p", str(relay.port), "-T", "100",
                                         "127.0.0.1", str(source)],
                                        capture_output=True, timeout=30)
                rec_out, rec_err = receiver.communicate(timeout=6)
                assert sender.returncode == 0, sender.stderr.decode()
                assert receiver.returncode == 0, rec_err.decode()
                assert target.read_bytes() == content
            finally:
                if receiver.poll() is None:
                    receiver.kill()
                    receiver.communicate()
    finally:
        relay.close()


if __name__ == "__main__":
    check_transfer(b"")
    check_transfer(bytes(range(256)) * 8)
    check_transfer(bytes(range(251)) * 19, damaged=True)
    check_transfer(bytes(range(256)) * 256, damaged=True)
    print("UDP transfer checks passed: empty, exact 1024 multiple, damaged channels")
