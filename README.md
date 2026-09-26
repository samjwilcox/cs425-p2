# P2 — Reliable Data Transfer

- Name: Sam Wilcox
- Class: CS 425-001, Computer Networks

## Build and run

The executable is `./build/release/myapp`. Build and run the tests in Codespaces or Onyx:

```sh
make clean
make all
make check
make report
make leak
make leak-test
python3 tests/integration-relay.py
```

`make report` requires `gcovr`. The integration script uses a deterministic local test relay. For the required course relay, start its Python file in a separate terminal and run the receiver first:

In VS Code, open the project **inside Codespaces or a Remote-SSH session on Onyx** and select the `Codespaces / Onyx (GCC)` C/C++ configuration. Its compiler path and system headers are Linux paths. A VS Code window running locally on Windows will not find Linux socket headers even when `src` is added to `includePath`; reopen the folder remotely and run **C/C++: Reset IntelliSense Database** if old diagnostics remain.

```sh
python3 cs425_relay.py --delay 50
./build/release/myapp recv -s sam-1 127.0.0.1 out.bin
./build/release/myapp send -s sam-1 -w 16 -l 0.1 -c 0.05 127.0.0.1 in.bin
cmp in.bin out.bin
```

On a shared host, give the relay `--port 23450` and add `-p 23450` to both endpoints. The sender supports `-w` (1–64, default 8), `-T` (timeout in ms, default 250), `-l`, `-c`, and `-d` (probabilities 0–0.5, default 0), and `-p` (default 4250). The receiver supports `-s` and `-p`. The session name must contain 1–32 lowercase letters, digits, or hyphens. With no arguments, the program prints usage and exits 0. An invalid command exits 1; relay, file, network, and transfer failures exit 2.

## Design

The implementation separates three layers:

1. `src/lab.c` parses arguments and implements pure RFC 1071 checksum and packet encode/decode functions. It writes each 10-byte header field explicitly in network byte order, rejects malformed datagrams before copying the payload, and supports up to 1024 payload bytes.
2. `src/protocol.c` implements pure sender and receiver state machines. It receives a monotonic time value from its caller, retains up to 64 outstanding packets, slides on cumulative ACKs, and resends the entire outstanding window on timeout. The receiver writes only the next expected packet, re-ACKs duplicates or gaps, and lingers two seconds after FIN. These functions have no socket, clock, or file calls.
3. `src/main.c` handles hostname resolution, one UDP socket per endpoint, registration, polling, clocks, and file I/O. The sender reads at most 16 MiB into an immutable image; the protocol retains a copy of each packet in flight. The receiver closes its output before acknowledging FIN, then keeps its socket open for the linger interval.

This split lets Unity tests simulate packet delivery and time precisely. The seeded test disturbs both directions with drops, bit flips, and duplicates, then verifies byte-identical output. `tests/integration-relay.py` separately drives the release executable through real UDP sockets for empty, exact-multiple, and damaged transfers.

## Results

These measurements used the **course relay** with `--delay 50` and a 1 MiB file in Codespaces. The `scripts/measure.py` runner made three byte-verified transfers for each combination and timed the sender's completion. Throughput is 1024 KiB divided by the mean time.

| Window | Loss | Corrupt | Dup | Mean time (s) | Throughput (KiB/s) |
|---:|---:|---:|---:|---:|---:|
| 1 | 0 | 0 | 0 | 103.112 | 9.93 |
| 16 | 0 | 0 | 0 | 6.573 | 155.79 |
| 1 | 0.05 | 0 | 0 | 128.405 | 7.97 |
| 16 | 0.05 | 0 | 0 | 24.116 | 42.46 |

The no-loss window-1 transfer sends 1024 DATA packets plus FIN, waiting a round trip for each. Its observed RTT estimate is 103.112 / 1025 = **100.597 ms**. The relay adds 100 ms, leaving about **0.597 ms per round trip** for process scheduling, socket and protocol handling, and measurement overhead. This is an estimate from the total time, not a per-packet timestamp trace.

At zero loss, window 16 was **15.69× faster** than window 1 (103.112 / 6.573), close to the ideal 16× because the sender can keep 16 packets in flight during the relay's round trip. The remaining gap reflects packet processing, ACK handling, and finite window refill timing. With 5% loss, window 1 slowed by **24.5%**, while window 16 took **3.67×** its own clean-channel time (a 266.9% increase). A window-16 timeout retransmits the entire outstanding window after a gap, including packets sent after the lost packet that the Go-Back-N receiver discarded. Window 1 resends only its single outstanding packet. In absolute seconds the loss added 25.293 s for window 1 and 17.543 s for window 16; the larger window still finished faster, but lost much more of its clean-channel speedup.

## Known issues and remaining verification

The course relay transfer, the 12 performance runs, `make leak`, and `make leak-test` succeeded in Codespaces. The source ZIP is not attached to the user's Git repository here, so a green GitHub CI run and a fresh submission-report DOCX still need confirmation after pushing the final README. Local sandbox LeakSanitizer could not inspect `/proc`, but the Codespaces run completed successfully.

## Experience

The most important implementation detail was keeping the Go-Back-N rules independent of I/O. A deterministic clock and channel made retransmission, duplicate ACKs, and the final ACK loss straightforward to test before connecting them to UDP and files.
