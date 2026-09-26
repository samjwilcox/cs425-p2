#!/usr/bin/env python3
"""Measure the four required 1 MiB transfer cases with the course relay.

Usage: python3 scripts/measure.py /path/to/cs425_relay.py [port]
The relay must support --delay 50 and --port as stated in P2.
"""
import pathlib
import secrets
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
APP = ROOT / "build/release/myapp"
CASES = ((1, 0), (16, 0), (1, 0.05), (16, 0.05))


def run_case(relay_port, source, target, window, loss, repetition):
    """Run one sender/receiver pair and return verified wall time in seconds."""
    session = f"sam-p2-{window}-{int(loss * 100)}-{repetition}"
    receiver = subprocess.Popen(
        [str(APP), "recv", "-s", session, "-p", str(relay_port),
         "127.0.0.1", str(target)], stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE)
    try:
        time.sleep(0.2)
        start = time.monotonic()
        sender = subprocess.run(
            [str(APP), "send", "-s", session, "-p", str(relay_port),
             "-w", str(window), "-l", str(loss), "127.0.0.1", str(source)],
            capture_output=True, timeout=300)
        elapsed = time.monotonic() - start
        _, receiver_error = receiver.communicate(timeout=8)
        if sender.returncode or receiver.returncode:
            raise RuntimeError(f"sender: {sender.stderr.decode()} receiver: {receiver_error.decode()}")
        if target.read_bytes() != source.read_bytes():
            raise RuntimeError("transferred file differs from source")
        return elapsed
    finally:
        if receiver.poll() is None:
            receiver.kill()
            receiver.communicate()


def main():
    """Run three verified trials per case and print the requested table."""
    if len(sys.argv) not in (2, 3):
        raise SystemExit(__doc__)
    relay_script = pathlib.Path(sys.argv[1]).resolve()
    port = int(sys.argv[2]) if len(sys.argv) == 3 else 23450
    if not relay_script.is_file() or not APP.is_file():
        raise SystemExit("Build the release program and provide the course relay file first.")
    with tempfile.TemporaryDirectory() as folder:
        directory = pathlib.Path(folder)
        source = directory / "1mib.bin"
        target = directory / "received.bin"
        source.write_bytes(secrets.token_bytes(1048576))
        log = (directory / "relay.log").open("wb")
        relay = subprocess.Popen([sys.executable, str(relay_script),
                                  "--delay", "50", "--port", str(port)],
                                 stdout=log, stderr=subprocess.STDOUT)
        try:
            time.sleep(0.4)
            if relay.poll() is not None:
                raise RuntimeError("course relay exited; check its arguments and port")
            print("| Window | Loss | Corrupt | Dup | Mean time (s) | Throughput (KiB/s) |")
            print("|---:|---:|---:|---:|---:|---:|")
            for window, loss in CASES:
                times = []
                for repetition in range(3):
                    elapsed = run_case(port, source, target, window, loss, repetition)
                    times.append(elapsed)
                    print(f"trial w={window} loss={loss} #{repetition + 1}: {elapsed:.3f}s",
                          file=sys.stderr)
                mean = sum(times) / 3
                print(f"| {window} | {loss:g} | 0 | 0 | {mean:.3f} | {1024 / mean:.2f} |",
                      flush=True)
            print("Window-1 no-loss RTT estimate: first row mean / 1025.", file=sys.stderr)
        finally:
            relay.terminate()
            try:
                relay.wait(timeout=2)
            except subprocess.TimeoutExpired:
                relay.kill()
                relay.wait()
            log.close()


if __name__ == "__main__":
    main()
