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

The required measurements must use the **course relay** with `--delay 50` and a 1 MiB file. Run `python3 scripts/measure.py /path/to/cs425_relay.py [port]` on Codespaces or Onyx. It performs three verified runs for each case, records raw times, and prints a Markdown table. Insert its measured output here after running it.

| Window | Loss | Corrupt | Dup | Mean time (s) | Throughput (KiB/s) |
|---:|---:|---:|---:|---:|---:|
| 1 | 0 | 0 | 0 | Pending course relay | Pending |
| 16 | 0 | 0 | 0 | Pending course relay | Pending |
| 1 | 0.05 | 0 | 0 | Pending course relay | Pending |
| 16 | 0.05 | 0 | 0 | Pending course relay | Pending |

The no-loss window-1 transfer sends 1024 DATA packets plus FIN, so its observed RTT estimate is the mean transfer time divided by 1025. Subtract the relay's 0.100-second round trip to estimate local scheduling, processing, and socket overhead. Compare the measured window-16 speedup with 16×: pipelining hides most RTTs, but serialization, handling ACKs, and finite buffers limit the gain. Under loss, a window-16 timeout resends every outstanding packet while window 1 resends only one. Fill in the numeric estimate and measured comparisons after running the script.

## Known issues and remaining verification

The supplied ZIP does not contain the course relay or an attached Git repository, so the course-relay measurements, a Codespaces/Onyx run, and a green GitHub CI run remain to be verified in the actual repository. Do not submit the placeholder Results table. This environment runs AddressSanitizer tests, but its LeakSanitizer cannot inspect `/proc`; repeat `make leak` and `make leak-test` in Codespaces or Onyx.

## Experience

The most important implementation detail was keeping the Go-Back-N rules independent of I/O. A deterministic clock and channel made retransmission, duplicate ACKs, and the final ACK loss straightforward to test before connecting them to UDP and files.
