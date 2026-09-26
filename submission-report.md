# Submission Report

- Submission generated at 09/26/2026 at 08:26:02

- Machine info: Linux runnervmtr4k5 6.17.0-1022-azure #22-Ubuntu SMP Mon Jul 27 17:24:03 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux

## Note to Students

Please read this report carefully before submission.
Ensure that all sections are complete and accurate.
Look for any errors in the build or test outputs.
If you find any issues, correct them before submitting.
Post any questions on the class discussion board for help.


---

## README

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

---


## Build Output

This section was generated by running `make all` in the project root directory.

```bash
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/protocol.c -o build/debug/protocol.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug/lab.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug/main.c.o
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address build/debug/protocol.c.o build/debug/lab.c.o build/debug/main.c.o -o build/debug/myapp_d -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/protocol.c -o build/release/protocol.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/lab.c -o build/release/lab.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/main.c -o build/release/main.c.o
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion build/release/protocol.c.o build/release/lab.c.o build/release/main.c.o -o build/release/myapp 
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/protocol.c -o build/tests/protocol.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/lab.c -o build/tests/lab.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/main.c -o build/tests/main.c.o
mkdir -p build/tests/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/protocol-test.c -o build/tests/protocol-test.c.o
mkdir -p build/tests/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/lab-test.c -o build/tests/lab-test.c.o
mkdir -p build/tests/harness/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/harness/unity.c -o build/tests/harness/unity.c.o
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage build/tests/protocol.c.o build/tests/lab.c.o build/tests/main.c.o build/tests/protocol-test.c.o build/tests/lab-test.c.o build/tests/harness/unity.c.o -o build/tests/myapp_t -fprofile-arcs -ftest-coverage
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/protocol.c -o build/debug-test/protocol.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug-test/lab.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug-test/main.c.o
mkdir -p build/debug-test/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/protocol-test.c -o build/debug-test/protocol-test.c.o
mkdir -p build/debug-test/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/lab-test.c -o build/debug-test/lab-test.c.o
mkdir -p build/debug-test/harness/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/harness/unity.c -o build/debug-test/harness/unity.c.o
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address build/debug-test/protocol.c.o build/debug-test/lab.c.o build/debug-test/main.c.o build/debug-test/protocol-test.c.o build/debug-test/lab-test.c.o build/debug-test/harness/unity.c.o -o build/debug-test/myapp_td -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
Builds completed. You can run the application with: ./build/release/myapp
You can run the debug build with: ./build/debug/myapp_d
You can run the test build with: ./build/tests/myapp_t
You can run the debug-test build with: ./build/debug-test/myapp_td
```

---

## Coverage Report

This section was generated by running `make report` in the project root directory.

```bash
tests/lab-test.c:216:test_sender_defaults:PASS
tests/lab-test.c:217:test_sender_options_and_boundaries:PASS
tests/lab-test.c:218:test_receiver_options:PASS
tests/lab-test.c:219:test_invalid_invocations:PASS
tests/lab-test.c:220:test_invalid_numeric_values:PASS
tests/lab-test.c:221:test_hello_format:PASS
tests/lab-test.c:222:test_hello_replies:PASS
tests/lab-test.c:223:test_rfc_checksum_and_golden_packet:PASS
tests/lab-test.c:224:test_control_packets_and_large_payload:PASS
tests/lab-test.c:225:test_bad_packet_validation:PASS
tests/lab-test.c:215:test_receiver_events:PASS
tests/lab-test.c:216:test_protocol_boundaries:PASS
tests/lab-test.c:217:test_sender_window_and_fin:PASS
tests/lab-test.c:218:test_sender_gives_up:PASS
tests/lab-test.c:219:test_seeded_lossy_transfer:PASS

-----------------------
15 Tests 0 Failures 0 Ignored 
OK
./build/tests/myapp_t
tests/lab-test.c:216:test_sender_defaults:PASS
tests/lab-test.c:217:test_sender_options_and_boundaries:PASS
tests/lab-test.c:218:test_receiver_options:PASS
tests/lab-test.c:219:test_invalid_invocations:PASS
tests/lab-test.c:220:test_invalid_numeric_values:PASS
tests/lab-test.c:221:test_hello_format:PASS
tests/lab-test.c:222:test_hello_replies:PASS
tests/lab-test.c:223:test_rfc_checksum_and_golden_packet:PASS
tests/lab-test.c:224:test_control_packets_and_large_payload:PASS
tests/lab-test.c:225:test_bad_packet_validation:PASS
tests/lab-test.c:215:test_receiver_events:PASS
tests/lab-test.c:216:test_protocol_boundaries:PASS
tests/lab-test.c:217:test_sender_window_and_fin:PASS
tests/lab-test.c:218:test_sender_gives_up:PASS
tests/lab-test.c:219:test_seeded_lossy_transfer:PASS

-----------------------
15 Tests 0 Failures 0 Ignored 
OK
mkdir -p ./build/report/html
mkdir -p ./build/report/txt
gcovr -r . --html --html-details --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$' -o ./build/report/html/coverage_report.html
(INFO) Reading coverage data...

(INFO) Writing coverage report...

gcovr -r . --txt                 --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$'
(INFO) Reading coverage data...

(INFO) Writing coverage report...

------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                       Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
src/lab.c                                    153      153   100%
src/protocol.c                               103      103   100%
------------------------------------------------------------------------------
TOTAL                                        256      256   100%
------------------------------------------------------------------------------
```

---

## Address Sanitizer Report

This section was generated by running `make leak-test` in the project root directory.

```bash
tests/lab-test.c:216:test_sender_defaults:PASS
tests/lab-test.c:217:test_sender_options_and_boundaries:PASS
tests/lab-test.c:218:test_receiver_options:PASS
tests/lab-test.c:219:test_invalid_invocations:PASS
tests/lab-test.c:220:test_invalid_numeric_values:PASS
tests/lab-test.c:221:test_hello_format:PASS
tests/lab-test.c:222:test_hello_replies:PASS
tests/lab-test.c:223:test_rfc_checksum_and_golden_packet:PASS
tests/lab-test.c:224:test_control_packets_and_large_payload:PASS
tests/lab-test.c:225:test_bad_packet_validation:PASS
tests/lab-test.c:215:test_receiver_events:PASS
tests/lab-test.c:216:test_protocol_boundaries:PASS
tests/lab-test.c:217:test_sender_window_and_fin:PASS
tests/lab-test.c:218:test_sender_gives_up:PASS
tests/lab-test.c:219:test_seeded_lossy_transfer:PASS

-----------------------
15 Tests 0 Failures 0 Ignored 
OK
```

---

## Src Files
### lab.c

```c

#include "lab.h"

#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Parse an unsigned decimal option without accepting signs or trailing text. */
static bool parse_unsigned(const char *text, unsigned minimum, unsigned maximum,
                           unsigned *result)
{
  char *end = NULL;
  errno = 0;
  unsigned long value = strtoul(text, &end, 10);
  if (text[0] == '\0' || text[0] == '-' || errno != 0 || *end != '\0' ||
      value < minimum || value > maximum)
    return false;
  *result = (unsigned)value;
  return true;
}

/** Parse a finite relay damage probability in the inclusive range [0, 0.5]. */
static bool parse_probability(const char *text, double *result)
{
  char *end = NULL;
  errno = 0;
  double value = strtod(text, &end);
  if (text[0] == '\0' || errno != 0 || *end != '\0' || !isfinite(value) ||
      value < 0.0 || value > 0.5)
    return false;
  *result = value;
  return true;
}

/** Validate the relay's 1–32 character lowercase session grammar. */
static bool valid_session(const char *session)
{
  size_t length = strlen(session);
  if (length < 1 || length > 32)
    return false;
  for (size_t i = 0; i < length; ++i)
  {
    char c = session[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
      return false;
  }
  return true;
}

/** Parse and validate the fixed send/recv command line with getopt, leaving
 * the caller's configuration untouched if any argument is invalid.
 */
int parse_args(int argc, char *const argv[], app_config_t *config)
{
  if (config == NULL || argv == NULL || argc < 5 || argv[1] == NULL)
    return -1;

  app_config_t parsed = {.window = 8, .timeout_ms = 250, .port = 4250};
  if (strcmp(argv[1], "send") == 0)
    parsed.mode = MODE_SEND;
  else if (strcmp(argv[1], "recv") == 0)
    parsed.mode = MODE_RECV;
  else
    return -1;

  /* Shift the argv view so getopt sees the mode as its program name. */
  optind = 1;
  opterr = 0;
  int option;
  while ((option = getopt(argc - 1, argv + 1,
                           parsed.mode == MODE_SEND ? "s:w:T:l:c:d:p:" : "s:p:")) != -1)
  {
    switch (option)
    {
      case 's': parsed.session = optarg; break;
      case 'w':
        if (!parse_unsigned(optarg, 1, 64, &parsed.window)) return -1;
        break;
      case 'T':
        if (!parse_unsigned(optarg, 1, UINT_MAX, &parsed.timeout_ms)) return -1;
        break;
      case 'l':
        if (!parse_probability(optarg, &parsed.loss)) return -1;
        break;
      case 'c':
        if (!parse_probability(optarg, &parsed.corrupt)) return -1;
        break;
      case 'd':
        if (!parse_probability(optarg, &parsed.dup)) return -1;
        break;
      case 'p':
        if (!parse_unsigned(optarg, 1, 65535, &parsed.port)) return -1;
        break;
      default: return -1;
    }
  }
  if (parsed.session == NULL || !valid_session(parsed.session) ||
      argc - 1 - optind != 2)
    return -1;
  parsed.relay = argv[optind + 1];
  parsed.path = argv[optind + 2];
  if (parsed.relay == NULL || parsed.path == NULL ||
      parsed.relay[0] == '\0' || parsed.path[0] == '\0')
    return -1;
  *config = parsed;
  return 0;
}

/** Format one registration datagram without a newline or terminating NUL
 * on the wire; the NUL remains available to local callers.
 */
int format_hello(const app_config_t *config, char *buffer, size_t capacity)
{
  if (config == NULL || buffer == NULL || capacity == 0 ||
      config->session == NULL || !valid_session(config->session))
    return -1;
  int size = config->mode == MODE_RECV
      ? snprintf(buffer, capacity, "HELLO %s recv", config->session)
      : config->mode == MODE_SEND
      ? snprintf(buffer, capacity, "HELLO %s send %.17g %.17g %.17g",
                 config->session, config->loss, config->corrupt, config->dup)
      : -1;
  return size >= 0 && (size_t)size < capacity ? size : -1;
}

/** Classify an exact OK reply or copy the reason from an ERR datagram. */
hello_reply_t parse_hello_reply(const char *reply, size_t length,
                                char *reason, size_t reason_capacity)
{
  if (reason != NULL && reason_capacity != 0)
    reason[0] = '\0';
  if (reply == NULL)
    return HELLO_INVALID;
  if (length == 2 && memcmp(reply, "OK", 2) == 0)
    return HELLO_OK;
  if (length >= 5 && memcmp(reply, "ERR ", 4) == 0 &&
      reason != NULL && reason_capacity != 0)
  {
    size_t count = length - 4;
    if (count >= reason_capacity)
      count = reason_capacity - 1;
    memcpy(reason, reply + 4, count);
    reason[count] = '\0';
    return HELLO_ERROR;
  }
  return HELLO_INVALID;
}

/** Fold 16-bit network-order words using RFC 1071 one's-complement addition;
 * treat an odd final byte as the high byte of a zero-padded word.
 */
uint16_t packet_checksum(const uint8_t *bytes, size_t length)
{
  uint32_t sum = 0;
  if (bytes == NULL)
    return 0;
  for (size_t i = 0; i < length; i += 2)
  {
    uint16_t word = (uint16_t)((uint16_t)bytes[i] << 8);
    if (i + 1 < length)
      word = (uint16_t)(word | bytes[i + 1]);
    sum += word;
    sum = (sum & 0xffffU) + (sum >> 16);
  }
  sum = (sum & 0xffffU) + (sum >> 16);
  return (uint16_t)(~sum & 0xffffU);
}

/** Serialize a validated packet explicitly, with zeroed checksum bytes
 * during calculation and no structure padding on the wire.
 */
int packet_encode(const packet_t *packet, uint8_t *out, size_t capacity)
{
  if (packet == NULL || out == NULL ||
      (packet->type != PACKET_DATA && packet->type != PACKET_ACK &&
       packet->type != PACKET_FIN) ||
      packet->length > PACKET_PAYLOAD_SIZE ||
      (packet->type != PACKET_DATA && packet->length != 0))
    return -1;
  size_t size = PACKET_HEADER_SIZE + packet->length;
  if (capacity < size)
    return -1;
  out[0] = (uint8_t)packet->type;
  out[1] = 0;
  out[2] = 0;
  out[3] = 0;
  out[4] = (uint8_t)(packet->seq >> 24);
  out[5] = (uint8_t)(packet->seq >> 16);
  out[6] = (uint8_t)(packet->seq >> 8);
  out[7] = (uint8_t)packet->seq;
  out[8] = (uint8_t)(packet->length >> 8);
  out[9] = (uint8_t)packet->length;
  if (packet->length != 0)
    memcpy(out + PACKET_HEADER_SIZE, packet->payload, packet->length);
  uint16_t checksum = packet_checksum(out, size);
  out[2] = (uint8_t)(checksum >> 8);
  out[3] = (uint8_t)checksum;
  return (int)size;
}

/** Validate a complete datagram before copying any payload into the result.
 * Reject bad sizes, types, reserved bits, control lengths, and checksums.
 */
int packet_decode(const uint8_t *bytes, size_t size, packet_t *out)
{
  if (bytes == NULL || out == NULL || size < PACKET_HEADER_SIZE ||
      size > PACKET_MAX_SIZE || bytes[0] > PACKET_FIN || bytes[1] != 0)
    return -1;
  uint16_t length = (uint16_t)((uint16_t)bytes[8] << 8 | bytes[9]);
  if (length > PACKET_PAYLOAD_SIZE || PACKET_HEADER_SIZE + length != size ||
      (bytes[0] != PACKET_DATA && length != 0) ||
      packet_checksum(bytes, size) != 0)
    return -1;
  packet_t decoded = {.type = (packet_type_t)bytes[0],
                      .seq = (uint32_t)bytes[4] << 24 |
                             (uint32_t)bytes[5] << 16 |
                             (uint32_t)bytes[6] << 8 | bytes[7],
                      .length = length};
  if (length != 0)
    memcpy(decoded.payload, bytes + PACKET_HEADER_SIZE, length);
  *out = decoded;
  return 0;
}

```

### lab.h

```c

#ifndef LAB_H
#define LAB_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  MODE_SEND,
  MODE_RECV
} app_mode_t;

typedef struct {
  app_mode_t mode;
  const char *session;
  const char *relay;
  const char *path;
  unsigned window;
  unsigned timeout_ms;
  unsigned port;
  double loss;
  double corrupt;
  double dup;
} app_config_t;

/** Parse the assignment's command line. Returns 0 on success, -1 on error.
 * Strings in config point into argv and remain valid as long as argv does.
 */
int parse_args(int argc, char *const argv[], app_config_t *config);

/* Returns the datagram size, or -1 if the output buffer is too small. */
int format_hello(const app_config_t *config, char *buffer, size_t capacity);

typedef enum { HELLO_OK, HELLO_ERROR, HELLO_INVALID } hello_reply_t;
/* A reply is a datagram, so it need not be NUL terminated. */
hello_reply_t parse_hello_reply(const char *reply, size_t length,
                                char *reason, size_t reason_capacity);

#define PACKET_HEADER_SIZE 10U
#define PACKET_PAYLOAD_SIZE 1024U
#define PACKET_MAX_SIZE (PACKET_HEADER_SIZE + PACKET_PAYLOAD_SIZE)

typedef enum { PACKET_DATA = 0, PACKET_ACK = 1, PACKET_FIN = 2 } packet_type_t;
typedef struct {
  packet_type_t type;
  uint32_t seq;
  uint16_t length;
  uint8_t payload[PACKET_PAYLOAD_SIZE];
} packet_t;

/* RFC 1071 one's-complement checksum, with an implicit final zero for odd sizes. */
uint16_t packet_checksum(const uint8_t *bytes, size_t length);
/* Returns wire length, or -1 for an invalid packet or insufficient capacity. */
int packet_encode(const packet_t *packet, uint8_t *out, size_t capacity);
/* Returns 0 on success, -1 for any malformed or corrupted datagram. */
int packet_decode(const uint8_t *bytes, size_t size, packet_t *out);

#define SENDER_MAX_WINDOW 64U
#define MAX_FILE_SIZE (16U * 1024U * 1024U)
typedef struct {
  const uint8_t *data;
  size_t size;
  uint32_t total_data;
  uint32_t base;
  uint32_t next;
  unsigned window;
  unsigned timeout_ms;
  unsigned fruitless_timeouts;
  int64_t deadline_ms;
  int finished;
  int failed;
  packet_t outstanding[SENDER_MAX_WINDOW];
} sender_state_t;

/* Protocol functions accept a synthetic monotonic time and perform no I/O. */
int sender_init(sender_state_t *state, const uint8_t *data, size_t size,
                unsigned window, unsigned timeout_ms);
/* Fills unused window slots; out must hold SENDER_MAX_WINDOW packets. */
size_t sender_fill(sender_state_t *state, int64_t now_ms, packet_t *out);
/* Returns 1 if this cumulative ACK advances the window, 0 otherwise. */
int sender_ack(sender_state_t *state, const packet_t *ack, int64_t now_ms);
/* Resends the entire outstanding window; returns 0 and sets failed at timeout 10. */
size_t sender_timeout(sender_state_t *state, int64_t now_ms, packet_t *out);

typedef struct {
  uint32_t expected;
  int finished;
  int64_t last_valid_ms;
  int64_t linger_until_ms;
} receiver_state_t;
typedef struct {
  int send_ack;
  packet_t ack;
  int deliver;
  int close_file;
} receiver_action_t;

void receiver_init(receiver_state_t *state, int64_t now_ms);
receiver_action_t receiver_step(receiver_state_t *state, const packet_t *packet,
                                int64_t now_ms);
/* 0: keep waiting; 1: successful linger complete; -1: idle timeout. */
int receiver_status(const receiver_state_t *state, int64_t now_ms);
int receiver_wait_ms(const receiver_state_t *state, int64_t now_ms);

#endif

```

### main.c

```c

#define _POSIX_C_SOURCE 200809L
#include "lab.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

/** Print the two command forms exactly as accepted by the CLI parser. */
static void usage(void)
{
  puts("Usage: myapp send -s <session> [-w window] [-T timeout-ms] [-l loss]"
       " [-c corrupt] [-d dup] [-p port] <relay> <file>\n"
       "       myapp recv -s <session> [-p port] <relay> <file>");
}

/** Read a monotonic clock in milliseconds so wall-clock changes cannot
 * affect retries or the Go-Back-N retransmission timer.
 */
static int64_t monotonic_ms(void)
{
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    return -1;
  return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/* A connected UDP socket retains the same source port for the entire run. */
/** Resolve a hostname or numeric address and connect a single UDP socket
 * to the first usable relay address.
 */
static int open_relay(const app_config_t *config)
{
  char port[6];
  snprintf(port, sizeof(port), "%u", config->port);
  struct addrinfo hints = {0};
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_UNSPEC;
  struct addrinfo *addresses = NULL;
  int result = getaddrinfo(config->relay, port, &hints, &addresses);
  if (result != 0)
  {
    fprintf(stderr, "Relay lookup failed: %s\n", gai_strerror(result));
    return -1;
  }
  int fd = -1;
  for (struct addrinfo *entry = addresses; entry != NULL; entry = entry->ai_next)
  {
    fd = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
    if (fd < 0)
      continue;
    if (connect(fd, entry->ai_addr, entry->ai_addrlen) == 0)
      break;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(addresses);
  if (fd < 0)
    perror("Relay socket");
  return fd;
}

/* 0 on OK, 2 on refusal, timeout, or network failure. */
/** Register with an undamaged text HELLO, retrying at one-second intervals
 * up to five attempts while preserving the same UDP source port.
 */
static int register_relay(int fd, const app_config_t *config)
{
  char hello[160];
  int length = format_hello(config, hello, sizeof(hello));
  if (length < 0)
    return 2;
  for (int attempt = 0; attempt < 5; ++attempt)
  {
    if (send(fd, hello, (size_t)length, 0) != length)
    {
      perror("Send HELLO");
      return 2;
    }
    int64_t deadline = monotonic_ms();
    if (deadline < 0)
    {
      perror("Clock");
      return 2;
    }
    deadline += 1000;
    for (;;)
    {
      int64_t now = monotonic_ms();
      if (now < 0)
      {
        perror("Clock");
        return 2;
      }
      if (now >= deadline)
        break;
      struct pollfd pfd = {.fd = fd, .events = POLLIN};
      int ready = poll(&pfd, 1, (int)(deadline - now));
      if (ready < 0)
      {
        if (errno == EINTR)
          continue;
        perror("Wait for relay");
        return 2;
      }
      if (ready == 0)
        break;
      if (!(pfd.revents & POLLIN))
      {
        fprintf(stderr, "Relay socket failed.\n");
        return 2;
      }
      char reply[512];
      ssize_t received = recv(fd, reply, sizeof(reply), 0);
      if (received < 0)
      {
        if (errno == EINTR)
          continue;
        perror("Receive relay reply");
        return 2;
      }
      char reason[512];
      hello_reply_t status = parse_hello_reply(reply, (size_t)received,
                                               reason, sizeof(reason));
      if (status == HELLO_OK)
        return 0;
      if (status == HELLO_ERROR)
      {
        fprintf(stderr, "Relay refused registration: %s\n", reason);
        return 2;
      }
      /* Ignore an unrelated datagram, but keep the original deadline. */
    }
  }
  fprintf(stderr, "Relay registration timed out after five attempts.\n");
  return 2;
}

/** Encode and transmit exactly one protocol packet through the registered
 * socket. A short UDP send is treated as a network failure.
 */
static int send_packet(int fd, const packet_t *packet)
{
  uint8_t bytes[PACKET_MAX_SIZE];
  int size = packet_encode(packet, bytes, sizeof(bytes));
  if (size < 0 || send(fd, bytes, (size_t)size, 0) != size)
  {
    perror("Send packet");
    return -1;
  }
  return 0;
}

/** Read a bounded input file into an immutable image for the sender state
 * machine. The returned allocation belongs to the caller, even for size zero.
 */
static uint8_t *read_input(const char *path, size_t *size)
{
  FILE *file = fopen(path, "rb");
  if (file == NULL)
  {
    perror(path);
    return NULL;
  }
  if (fseek(file, 0, SEEK_END) != 0)
  {
    perror("Seek input");
    fclose(file);
    return NULL;
  }
  long length = ftell(file);
  if (length < 0 || (unsigned long)length > MAX_FILE_SIZE ||
      fseek(file, 0, SEEK_SET) != 0)
  {
    fprintf(stderr, "Input must be a seekable file of at most 16 MiB.\n");
    fclose(file);
    return NULL;
  }
  *size = (size_t)length;
  uint8_t *data = malloc(*size ? *size : 1);
  if (data == NULL)
  {
    perror("Allocate input");
    fclose(file);
    return NULL;
  }
  size_t bytes_read = fread(data, 1, *size, file);
  int close_status = fclose(file);
  if (bytes_read != *size || close_status != 0)
  {
    fprintf(stderr, "Unable to read input file.\n");
    free(data);
    return NULL;
  }
  return data;
}

/** Send a group of protocol actions produced by a state-machine event. */
static int send_batch(int fd, const packet_t *packets, size_t count)
{
  for (size_t i = 0; i < count; ++i)
    if (send_packet(fd, &packets[i]) != 0)
      return -1;
  return 0;
}

/** Drive the pure Go-Back-N sender with real socket, file and clock events. */
static int run_sender(int fd, const app_config_t *config)
{
  size_t size = 0;
  uint8_t *data = read_input(config->path, &size);
  if (data == NULL)
    return 2;
  sender_state_t *state = malloc(sizeof(*state));
  if (state == NULL)
  {
    perror("Allocate sender");
    free(data);
    return 2;
  }
  int result = 2;
  packet_t packets[SENDER_MAX_WINDOW];
  if (sender_init(state, data, size, config->window, config->timeout_ms) != 0)
    goto done;
  int64_t now = monotonic_ms();
  if (now < 0 || send_batch(fd, packets, sender_fill(state, now, packets)) != 0)
    goto done;
  while (!state->finished && !state->failed)
  {
    now = monotonic_ms();
    if (now < 0)
      goto done;
    int64_t remaining = state->deadline_ms - now;
    if (remaining <= 0)
    {
      size_t count = sender_timeout(state, now, packets);
      if (state->failed)
      {
        fprintf(stderr, "Transfer gave up after 10 timeouts without progress.\n");
        break;
      }
      if (send_batch(fd, packets, count) != 0)
        break;
      continue;
    }
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    int ready = poll(&pfd, 1, remaining > INT_MAX ? INT_MAX : (int)remaining);
    if (ready < 0)
    {
      if (errno == EINTR)
        continue;
      perror("Wait for ACK");
      break;
    }
    if (ready == 0)
      continue;
    if (!(pfd.revents & POLLIN))
    {
      fprintf(stderr, "Sender socket failed.\n");
      break;
    }
    uint8_t bytes[2048];
    ssize_t received = recv(fd, bytes, sizeof(bytes), 0);
    if (received < 0)
    {
      if (errno == EINTR)
        continue;
      perror("Receive ACK");
      break;
    }
    packet_t ack;
    if (packet_decode(bytes, (size_t)received, &ack) != 0)
      continue;
    now = monotonic_ms();
    if (now < 0)
      break;
    if (sender_ack(state, &ack, now) && !state->finished &&
        send_batch(fd, packets, sender_fill(state, now, packets)) != 0)
      break;
  }
  if (state->finished)
    result = 0;
done:
  free(state);
  free(data);
  return result;
}

/** Drive the pure receiver, writing only in-order payloads and retaining
 * the socket for two seconds after FIN to answer repeated final packets.
 */
static int run_receiver(int fd, const app_config_t *config)
{
  FILE *file = fopen(config->path, "wb");
  if (file == NULL)
  {
    perror(config->path);
    return 2;
  }
  int result = 2;
  receiver_state_t state;
  int64_t now = monotonic_ms();
  if (now < 0)
    goto done;
  receiver_init(&state, now);
  for (;;)
  {
    now = monotonic_ms();
    if (now < 0)
      break;
    int status = receiver_status(&state, now);
    if (status != 0)
    {
      if (status < 0)
        fprintf(stderr, "Receiver timed out after 30 seconds idle.\n");
      else
        result = 0;
      break;
    }
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    int ready = poll(&pfd, 1, receiver_wait_ms(&state, now));
    if (ready < 0)
    {
      if (errno == EINTR)
        continue;
      perror("Wait for DATA");
      break;
    }
    if (ready == 0)
      continue;
    if (!(pfd.revents & POLLIN))
    {
      fprintf(stderr, "Receiver socket failed.\n");
      break;
    }
    uint8_t bytes[2048];
    ssize_t received = recv(fd, bytes, sizeof(bytes), 0);
    if (received < 0)
    {
      if (errno == EINTR)
        continue;
      perror("Receive DATA");
      break;
    }
    packet_t packet;
    if (packet_decode(bytes, (size_t)received, &packet) != 0)
      continue;
    now = monotonic_ms();
    if (now < 0)
      break;
    receiver_action_t action = receiver_step(&state, &packet, now);
    if (action.deliver && fwrite(packet.payload, 1, packet.length, file) != packet.length)
    {
      perror("Write output");
      break;
    }
    if (action.close_file)
    {
      if (fclose(file) != 0)
      {
        file = NULL;
        perror("Close output");
        break;
      }
      file = NULL;
    }
    if (action.send_ack && send_packet(fd, &action.ack) != 0)
      break;
  }
done:
  if (file != NULL && fclose(file) != 0)
    result = 2;
  return result;
}

/** Parse the CLI, register with the relay, and run the requested endpoint. */
int main(int argc, char **argv)
{
  if (argc == 1)
  {
    usage();
    return 0;
  }
  app_config_t config;
  if (parse_args(argc, argv, &config) != 0)
  {
    usage();
    return 1;
  }
  int fd = open_relay(&config);
  if (fd < 0)
    return 2;
  int result = register_relay(fd, &config);
  if (result == 0)
    result = config.mode == MODE_SEND ? run_sender(fd, &config)
                                      : run_receiver(fd, &config);
  close(fd);
  return result;
}

```

### protocol.c

```c

#include "lab.h"

#include <limits.h>
#include <string.h>

/** Initialize a Go-Back-N sender for an immutable in-memory file image.
 * The file image must remain alive until the transfer completes.
 */
int sender_init(sender_state_t *state, const uint8_t *data, size_t size,
                unsigned window, unsigned timeout_ms)
{
  if (state == NULL || (size != 0 && data == NULL) || size > MAX_FILE_SIZE ||
      window == 0 || window > SENDER_MAX_WINDOW || timeout_ms == 0)
    return -1;
  memset(state, 0, sizeof(*state));
  state->data = data;
  state->size = size;
  state->total_data = (uint32_t)((size + PACKET_PAYLOAD_SIZE - 1) /
                                 PACKET_PAYLOAD_SIZE);
  state->window = window;
  state->timeout_ms = timeout_ms;
  return 0;
}

/** Fill all available window positions with fresh DATA packets, or emit FIN
 * once all DATA is acknowledged. Each emitted packet is retained for timeout.
 */
size_t sender_fill(sender_state_t *state, int64_t now_ms, packet_t *out)
{
  if (state == NULL || out == NULL || state->failed || state->finished)
    return 0;
  size_t count = 0;
  while (state->next < state->total_data &&
         state->next - state->base < state->window)
  {
    uint32_t seq = state->next;
    packet_t *packet = &state->outstanding[seq % SENDER_MAX_WINDOW];
    packet->type = PACKET_DATA;
    packet->seq = seq;
    size_t offset = (size_t)seq * PACKET_PAYLOAD_SIZE;
    size_t remaining = state->size - offset;
    packet->length = (uint16_t)(remaining < PACKET_PAYLOAD_SIZE
                                    ? remaining : PACKET_PAYLOAD_SIZE);
    memcpy(packet->payload, state->data + offset, packet->length);
    out[count++] = *packet;
    if (state->base == state->next)
      state->deadline_ms = now_ms + state->timeout_ms;
    ++state->next;
  }
  if (state->base == state->total_data && state->next == state->total_data)
  {
    packet_t *packet = &state->outstanding[state->next % SENDER_MAX_WINDOW];
    packet->type = PACKET_FIN;
    packet->seq = state->next;
    packet->length = 0;
    out[count++] = *packet;
    state->deadline_ms = now_ms + state->timeout_ms;
    ++state->next;
  }
  return count;
}

/** Apply a valid cumulative ACK, resetting the single timer only on progress.
 * ACKs beyond the highest sent packet and duplicate ACKs have no effect.
 */
int sender_ack(sender_state_t *state, const packet_t *ack, int64_t now_ms)
{
  if (state == NULL || ack == NULL || state->failed || state->finished ||
      ack->type != PACKET_ACK || ack->length != 0 ||
      ack->seq <= state->base || ack->seq > state->next)
    return 0;
  state->base = ack->seq;
  state->fruitless_timeouts = 0;
  if (state->base == state->total_data + 1)
  {
    state->finished = 1;
    state->deadline_ms = 0;
  }
  else if (state->base == state->next)
    state->deadline_ms = 0;
  else
    state->deadline_ms = now_ms + state->timeout_ms;
  return 1;
}

/** Retransmit every unacknowledged packet when the one outstanding timer
 * expires. Ten timeouts with no advancing ACK permanently fail the sender.
 */
size_t sender_timeout(sender_state_t *state, int64_t now_ms, packet_t *out)
{
  if (state == NULL || out == NULL || state->failed || state->finished ||
      state->base == state->next || now_ms < state->deadline_ms)
    return 0;
  if (++state->fruitless_timeouts >= 10)
  {
    state->failed = 1;
    return 0;
  }
  size_t count = 0;
  for (uint32_t seq = state->base; seq < state->next; ++seq)
    out[count++] = state->outstanding[seq % SENDER_MAX_WINDOW];
  state->deadline_ms = now_ms + state->timeout_ms;
  return count;
}

/** Start a receiver expecting DATA 0, with a thirty-second idle deadline. */
void receiver_init(receiver_state_t *state, int64_t now_ms)
{
  if (state == NULL)
    return;
  *state = (receiver_state_t){.last_valid_ms = now_ms};
}

/** Consume a validated protocol packet and describe the resulting ACK,
 * ordered payload delivery, and FIN close action without performing I/O.
 */
receiver_action_t receiver_step(receiver_state_t *state, const packet_t *packet,
                                int64_t now_ms)
{
  receiver_action_t action = {0};
  if (state == NULL || packet == NULL || packet->type == PACKET_ACK)
    return action;
  state->last_valid_ms = now_ms;
  if (!state->finished && packet->seq == state->expected)
  {
    if (packet->type == PACKET_DATA)
    {
      action.deliver = 1;
      ++state->expected;
    }
    else if (packet->type == PACKET_FIN)
    {
      action.close_file = 1;
      state->finished = 1;
      ++state->expected;
      state->linger_until_ms = now_ms + 2000;
    }
  }
  if (!state->finished || packet->type == PACKET_FIN ||
      packet->type == PACKET_DATA)
  {
    action.send_ack = 1;
    action.ack.type = PACKET_ACK;
    action.ack.seq = state->expected;
  }
  return action;
}

/** Return the receiver's completion or idle-timeout status at a fake or real
 * monotonic timestamp. A closed receiver succeeds after two seconds of linger.
 */
int receiver_status(const receiver_state_t *state, int64_t now_ms)
{
  if (state == NULL)
    return -1;
  if (state->finished)
    return now_ms >= state->linger_until_ms ? 1 : 0;
  return now_ms - state->last_valid_ms >= 30000 ? -1 : 0;
}

/** Compute the remaining poll interval until the receiver's next deadline. */
int receiver_wait_ms(const receiver_state_t *state, int64_t now_ms)
{
  if (state == NULL)
    return 0;
  int64_t deadline = state->finished ? state->linger_until_ms
                                     : state->last_valid_ms + 30000;
  int64_t remaining = deadline - now_ms;
  if (remaining <= 0)
    return 0;
  return remaining > INT_MAX ? INT_MAX : (int)remaining;
}

```

## Tests Files
### lab-test.c

```c

#include "harness/unity.h"
#include "../src/lab.h"

/** Prepare each isolated Unity case; no shared fixture is needed. */
void setUp(void) {}
/** Release the empty test fixture after each Unity case. */
void tearDown(void) {}

/** Verify sender mode and all default command-line settings. */
void test_sender_defaults(void)
{
  char *argv[] = {"myapp", "send", "-s", "sam-1", "localhost", "in.bin"};
  app_config_t cfg;
  TEST_ASSERT_EQUAL_INT(0, parse_args(6, argv, &cfg));
  TEST_ASSERT_EQUAL_INT(MODE_SEND, cfg.mode);
  TEST_ASSERT_EQUAL_STRING("sam-1", cfg.session);
  TEST_ASSERT_EQUAL_STRING("localhost", cfg.relay);
  TEST_ASSERT_EQUAL_STRING("in.bin", cfg.path);
  TEST_ASSERT_EQUAL_UINT(8, cfg.window);
  TEST_ASSERT_EQUAL_UINT(250, cfg.timeout_ms);
  TEST_ASSERT_EQUAL_UINT(4250, cfg.port);
  TEST_ASSERT_TRUE(cfg.loss == 0.0);
}

/** Verify sender overrides and inclusive maximum/minimum values. */
void test_sender_options_and_boundaries(void)
{
  char *argv[] = {"myapp", "send", "-s", "a", "-w", "64", "-T", "1",
                  "-l", "0.5", "-c", "0.1", "-d", "0", "-p", "60000",
                  "127.0.0.1", "file"};
  app_config_t cfg;
  TEST_ASSERT_EQUAL_INT(0, parse_args(18, argv, &cfg));
  TEST_ASSERT_EQUAL_UINT(64, cfg.window);
  TEST_ASSERT_EQUAL_UINT(1, cfg.timeout_ms);
  TEST_ASSERT_EQUAL_UINT(60000, cfg.port);
  TEST_ASSERT_TRUE(cfg.loss == 0.5);
  TEST_ASSERT_TRUE(cfg.corrupt > 0.099 && cfg.corrupt < 0.101);
  TEST_ASSERT_TRUE(cfg.dup == 0.0);
}

/** Verify receiver mode and its permitted port override. */
void test_receiver_options(void)
{
  char *argv[] = {"myapp", "recv", "-s", "sam-1", "-p", "20000", "relay", "out"};
  app_config_t cfg;
  TEST_ASSERT_EQUAL_INT(0, parse_args(8, argv, &cfg));
  TEST_ASSERT_EQUAL_INT(MODE_RECV, cfg.mode);
  TEST_ASSERT_EQUAL_UINT(20000, cfg.port);
}

/** Reject malformed modes, sessions, positional arguments, and options. */
void test_invalid_invocations(void)
{
  app_config_t cfg;
  char *bad_mode[] = {"myapp", "receive", "-s", "a", "host", "file"};
  char *missing[] = {"myapp", "recv", "-s", "a", "host"};
  char *recv_sender_option[] = {"myapp", "recv", "-s", "a", "-w", "2", "host", "file"};
  char *bad_session[] = {"myapp", "send", "-s", "Upper", "host", "file"};
  char *empty_session[] = {"myapp", "send", "-s", "", "host", "file"};
  char *long_session[] = {"myapp", "send", "-s", "abcdefghijklmnopqrstuvwxyz1234567", "host", "file"};
  char *extra[] = {"myapp", "recv", "-s", "a", "host", "file", "extra"};
  TEST_ASSERT_EQUAL_INT(-1, parse_args(6, bad_mode, &cfg));
  TEST_ASSERT_EQUAL_INT(-1, parse_args(5, missing, &cfg));
  TEST_ASSERT_EQUAL_INT(-1, parse_args(8, recv_sender_option, &cfg));
  TEST_ASSERT_EQUAL_INT(-1, parse_args(6, bad_session, &cfg));
  TEST_ASSERT_EQUAL_INT(-1, parse_args(6, empty_session, &cfg));
  TEST_ASSERT_EQUAL_INT(-1, parse_args(6, long_session, &cfg));
  TEST_ASSERT_EQUAL_INT(-1, parse_args(7, extra, &cfg));
  char *empty_host[] = {"myapp", "recv", "-s", "a", "", "file"};
  TEST_ASSERT_EQUAL_INT(-1, parse_args(6, empty_host, &cfg));
  TEST_ASSERT_EQUAL_INT(-1, parse_args(0, NULL, &cfg));
  TEST_ASSERT_EQUAL_INT(-1, parse_args(6, bad_mode, NULL));
}

/** Reject out-of-range, nonfinite, and malformed numeric options. */
void test_invalid_numeric_values(void)
{
  const char *options[] = {"-w", "-w", "-w", "-T", "-T", "-p", "-p",
                           "-l", "-l", "-l", "-c", "-d", "-l"};
  const char *values[] = {"0", "65", "3x", "0", "-1", "0", "65536",
                          "-0.1", "0.51", "nan", "inf", "x", ""};
  app_config_t cfg;
  for (unsigned i = 0; i < sizeof(options) / sizeof(options[0]); ++i)
  {
    char *argv[] = {"myapp", "send", "-s", "a", (char *)options[i],
                    (char *)values[i], "host", "file"};
    TEST_ASSERT_EQUAL_INT(-1, parse_args(8, argv, &cfg));
  }
}

/** Compare sender and receiver registrations with their exact wire text. */
void test_hello_format(void)
{
  app_config_t cfg = {.mode = MODE_RECV, .session = "sam-1"};
  char buffer[160];
  TEST_ASSERT_EQUAL_INT(16, format_hello(&cfg, buffer, sizeof(buffer)));
  TEST_ASSERT_EQUAL_STRING("HELLO sam-1 recv", buffer);
  cfg.mode = MODE_SEND;
  cfg.loss = 0.25;
  cfg.corrupt = 0.5;
  cfg.dup = 0;
  TEST_ASSERT_TRUE(format_hello(&cfg, buffer, sizeof(buffer)) > 0);
  TEST_ASSERT_EQUAL_STRING("HELLO sam-1 send 0.25 0.5 0", buffer);
  TEST_ASSERT_EQUAL_INT(-1, format_hello(&cfg, buffer, 5));
  TEST_ASSERT_EQUAL_INT(-1, format_hello(NULL, buffer, sizeof(buffer)));
}

/** Parse exact OK and bounded ERR replies without trusting termination. */
void test_hello_replies(void)
{
  char reason[8];
  TEST_ASSERT_EQUAL_INT(HELLO_OK, parse_hello_reply("OK", 2, reason, sizeof(reason)));
  TEST_ASSERT_EQUAL_STRING("", reason);
  TEST_ASSERT_EQUAL_INT(HELLO_ERROR,
                        parse_hello_reply("ERR no receiver", 15, reason, sizeof(reason)));
  TEST_ASSERT_EQUAL_STRING("no rece", reason);
  TEST_ASSERT_EQUAL_INT(HELLO_INVALID,
                        parse_hello_reply("OK extra", 8, reason, sizeof(reason)));
  TEST_ASSERT_EQUAL_INT(HELLO_INVALID, parse_hello_reply(NULL, 0, reason, sizeof(reason)));
}

/** Check RFC 1071 and the assignment’s odd-length golden DATA packet. */
void test_rfc_checksum_and_golden_packet(void)
{
  uint8_t example[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
  TEST_ASSERT_EQUAL_HEX16(0x220d, packet_checksum(example, sizeof(example)));
  TEST_ASSERT_EQUAL_HEX16(0, packet_checksum(NULL, 0));
  packet_t data = {.type = PACKET_DATA, .seq = 2, .length = 3,
                   .payload = {'H', 'i', '!'}};
  uint8_t bytes[PACKET_MAX_SIZE];
  uint8_t expected[] = {0x00, 0x00, 0x96, 0x91, 0x00, 0x00, 0x00,
                        0x02, 0x00, 0x03, 'H', 'i', '!'};
  TEST_ASSERT_EQUAL_INT(13, packet_encode(&data, bytes, sizeof(bytes)));
  TEST_ASSERT_EQUAL_MEMORY(expected, bytes, sizeof(expected));
  TEST_ASSERT_EQUAL_HEX16(0, packet_checksum(bytes, sizeof(expected)));
  packet_t decoded;
  TEST_ASSERT_EQUAL_INT(0, packet_decode(bytes, sizeof(expected), &decoded));
  TEST_ASSERT_EQUAL_INT(PACKET_DATA, decoded.type);
  TEST_ASSERT_EQUAL_UINT32(2, decoded.seq);
  TEST_ASSERT_EQUAL_UINT16(3, decoded.length);
  TEST_ASSERT_EQUAL_MEMORY("Hi!", decoded.payload, 3);
}

/** Round-trip ACK, FIN, and maximum-size DATA packets. */
void test_control_packets_and_large_payload(void)
{
  uint8_t bytes[PACKET_MAX_SIZE];
  packet_t packet = {.type = PACKET_ACK, .seq = 3};
  uint8_t ack[] = {0x01, 0x00, 0xfe, 0xfc, 0x00, 0x00, 0x00,
                   0x03, 0x00, 0x00};
  TEST_ASSERT_EQUAL_INT(10, packet_encode(&packet, bytes, sizeof(bytes)));
  TEST_ASSERT_EQUAL_MEMORY(ack, bytes, sizeof(ack));
  packet_t out;
  TEST_ASSERT_EQUAL_INT(0, packet_decode(bytes, 10, &out));
  TEST_ASSERT_EQUAL_INT(PACKET_ACK, out.type);
  packet.type = PACKET_FIN;
  packet.seq = 0x01020304;
  TEST_ASSERT_EQUAL_INT(10, packet_encode(&packet, bytes, sizeof(bytes)));
  TEST_ASSERT_EQUAL_INT(0, packet_decode(bytes, 10, &out));
  TEST_ASSERT_EQUAL_UINT32(0x01020304, out.seq);
  packet.type = PACKET_DATA;
  packet.length = PACKET_PAYLOAD_SIZE;
  for (unsigned i = 0; i < PACKET_PAYLOAD_SIZE; ++i)
    packet.payload[i] = (uint8_t)i;
  TEST_ASSERT_EQUAL_INT(PACKET_MAX_SIZE, packet_encode(&packet, bytes, sizeof(bytes)));
  TEST_ASSERT_EQUAL_INT(0, packet_decode(bytes, PACKET_MAX_SIZE, &out));
  TEST_ASSERT_EQUAL_MEMORY(packet.payload, out.payload, PACKET_PAYLOAD_SIZE);
}

/** Reject truncated, corrupted, malformed, and invalid control packets. */
void test_bad_packet_validation(void)
{
  packet_t packet = {.type = PACKET_DATA, .seq = 7, .length = 1,
                     .payload = {0x55}};
  packet_t out;
  uint8_t bytes[PACKET_MAX_SIZE];
  TEST_ASSERT_EQUAL_INT(11, packet_encode(&packet, bytes, sizeof(bytes)));
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(bytes, 9, &out));
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(bytes, 10, &out));
  bytes[10] ^= 1;
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(bytes, 11, &out));
  bytes[10] ^= 1;
  bytes[0] = 3;
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(bytes, 11, &out));
  bytes[0] = 0;
  bytes[1] = 1;
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(bytes, 11, &out));
  bytes[1] = 0;
  bytes[8] = 4;
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(bytes, 11, &out));
  bytes[8] = 0;
  bytes[0] = 1;
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(bytes, 11, &out));
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(NULL, 11, &out));
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(bytes, 11, NULL));
  TEST_ASSERT_EQUAL_INT(-1, packet_decode(bytes, PACKET_MAX_SIZE + 1, &out));
  packet.type = PACKET_FIN;
  TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, bytes, sizeof(bytes)));
  packet.type = PACKET_DATA;
  packet.length = PACKET_PAYLOAD_SIZE + 1;
  TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, bytes, sizeof(bytes)));
  packet.length = 1;
  TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, bytes, 10));
  packet.type = (packet_type_t)-1;
  TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, bytes, sizeof(bytes)));
  TEST_ASSERT_EQUAL_INT(-1, packet_encode(NULL, bytes, sizeof(bytes)));
}

/** Register pure packet, CLI, and state-machine tests with Unity. */
void run_protocol_tests(void);

/** Run every packet, argument, registration, and protocol Unity case. */
int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_sender_defaults);
  RUN_TEST(test_sender_options_and_boundaries);
  RUN_TEST(test_receiver_options);
  RUN_TEST(test_invalid_invocations);
  RUN_TEST(test_invalid_numeric_values);
  RUN_TEST(test_hello_format);
  RUN_TEST(test_hello_replies);
  RUN_TEST(test_rfc_checksum_and_golden_packet);
  RUN_TEST(test_control_packets_and_large_payload);
  RUN_TEST(test_bad_packet_validation);
  run_protocol_tests();
  return UNITY_END();
}

```

### protocol-test.c

```c

#include "harness/unity.h"
#include "../src/lab.h"
#include <string.h>

/** Verify receiver delivery, duplicate and gap ACKs, FIN, and linger. */
static void test_receiver_events(void)
{
  receiver_state_t state;
  receiver_init(&state, 100);
  packet_t packet = {.type = PACKET_DATA, .seq = 1, .length = 1,
                     .payload = {'B'}};
  receiver_action_t action = receiver_step(&state, &packet, 101);
  TEST_ASSERT_TRUE(action.send_ack);
  TEST_ASSERT_FALSE(action.deliver);
  TEST_ASSERT_EQUAL_UINT32(0, action.ack.seq);
  packet.seq = 0;
  action = receiver_step(&state, &packet, 102);
  TEST_ASSERT_TRUE(action.deliver);
  TEST_ASSERT_EQUAL_UINT32(1, action.ack.seq);
  action = receiver_step(&state, &packet, 103);
  TEST_ASSERT_FALSE(action.deliver);
  TEST_ASSERT_EQUAL_UINT32(1, action.ack.seq);
  packet.type = PACKET_FIN;
  packet.seq = 2;
  packet.length = 0;
  action = receiver_step(&state, &packet, 104);
  TEST_ASSERT_FALSE(action.close_file);
  TEST_ASSERT_EQUAL_UINT32(1, action.ack.seq);
  packet.seq = 1;
  action = receiver_step(&state, &packet, 105);
  TEST_ASSERT_TRUE(action.close_file);
  TEST_ASSERT_EQUAL_UINT32(2, action.ack.seq);
  action = receiver_step(&state, &packet, 106);
  TEST_ASSERT_FALSE(action.close_file);
  TEST_ASSERT_EQUAL_UINT32(2, action.ack.seq);
  packet.type = PACKET_DATA;
  action = receiver_step(&state, &packet, 107);
  TEST_ASSERT_FALSE(action.deliver);
  TEST_ASSERT_EQUAL_UINT32(2, action.ack.seq);
  TEST_ASSERT_EQUAL_INT(0, receiver_status(&state, 2104));
  TEST_ASSERT_EQUAL_INT(1, receiver_wait_ms(&state, 2104));
  TEST_ASSERT_EQUAL_INT(1, receiver_status(&state, 2105));
  packet.type = PACKET_ACK;
  action = receiver_step(&state, &packet, 2106);
  TEST_ASSERT_FALSE(action.send_ack);
}

/** Check idle timeout and all state-machine argument validation. */
static void test_protocol_boundaries(void)
{
  receiver_state_t receiver;
  receiver_init(&receiver, 0);
  TEST_ASSERT_EQUAL_INT(0, receiver_status(&receiver, 29999));
  TEST_ASSERT_EQUAL_INT(1, receiver_wait_ms(&receiver, 29999));
  TEST_ASSERT_EQUAL_INT(-1, receiver_status(&receiver, 30000));
  TEST_ASSERT_EQUAL_INT(0, receiver_wait_ms(&receiver, 30000));
  TEST_ASSERT_EQUAL_INT(-1, receiver_status(NULL, 0));
  TEST_ASSERT_EQUAL_INT(0, receiver_wait_ms(NULL, 0));
  TEST_ASSERT_FALSE(receiver_step(NULL, NULL, 0).send_ack);
  TEST_ASSERT_FALSE(receiver_step(&receiver, NULL, 0).send_ack);
  receiver_init(NULL, 0);
  sender_state_t sender;
  TEST_ASSERT_EQUAL_INT(-1, sender_init(NULL, NULL, 0, 1, 1));
  TEST_ASSERT_EQUAL_INT(-1, sender_init(&sender, NULL, 1, 1, 1));
  TEST_ASSERT_EQUAL_INT(-1, sender_init(&sender, NULL, MAX_FILE_SIZE + 1, 1, 1));
  TEST_ASSERT_EQUAL_INT(-1, sender_init(&sender, NULL, 0, 0, 1));
  TEST_ASSERT_EQUAL_INT(-1, sender_init(&sender, NULL, 0, 65, 1));
  TEST_ASSERT_EQUAL_INT(-1, sender_init(&sender, NULL, 0, 1, 0));
}

/** Confirm a full window, cumulative progress, duplicate ACK, and timeout. */
static void test_sender_window_and_fin(void)
{
  uint8_t data[2500] = {0};
  sender_state_t state;
  packet_t out[SENDER_MAX_WINDOW];
  TEST_ASSERT_EQUAL_INT(0, sender_init(&state, data, sizeof(data), 2, 250));
  TEST_ASSERT_EQUAL_UINT(2, sender_fill(&state, 1000, out));
  TEST_ASSERT_EQUAL_UINT32(0, out[0].seq);
  TEST_ASSERT_EQUAL_UINT32(1, out[1].seq);
  TEST_ASSERT_EQUAL_UINT(1024, out[0].length);
  TEST_ASSERT_EQUAL_UINT(0, sender_fill(&state, 1000, out));
  TEST_ASSERT_EQUAL_UINT(0, sender_timeout(&state, 1249, out));
  packet_t ack = {.type = PACKET_ACK, .seq = 0};
  TEST_ASSERT_EQUAL_INT(0, sender_ack(&state, &ack, 1100));
  ack.seq = 3;
  TEST_ASSERT_EQUAL_INT(0, sender_ack(&state, &ack, 1100));
  ack.seq = 2;
  TEST_ASSERT_EQUAL_INT(1, sender_ack(&state, &ack, 1100));
  TEST_ASSERT_EQUAL_UINT32(2, state.base);
  TEST_ASSERT_EQUAL_UINT(1, sender_fill(&state, 1100, out));
  TEST_ASSERT_EQUAL_UINT32(2, out[0].seq);
  TEST_ASSERT_EQUAL_UINT(452, out[0].length);
  TEST_ASSERT_EQUAL_UINT(1, sender_timeout(&state, 1350, out));
  TEST_ASSERT_EQUAL_UINT32(2, out[0].seq);
  ack.seq = 3;
  TEST_ASSERT_EQUAL_INT(1, sender_ack(&state, &ack, 1400));
  TEST_ASSERT_EQUAL_UINT(1, sender_fill(&state, 1400, out));
  TEST_ASSERT_EQUAL_INT(PACKET_FIN, out[0].type);
  TEST_ASSERT_EQUAL_UINT32(3, out[0].seq);
  TEST_ASSERT_EQUAL_UINT(1, sender_timeout(&state, 1650, out));
  TEST_ASSERT_EQUAL_INT(PACKET_FIN, out[0].type);
  ack.seq = 4;
  TEST_ASSERT_EQUAL_INT(1, sender_ack(&state, &ack, 1700));
  TEST_ASSERT_TRUE(state.finished);
  TEST_ASSERT_EQUAL_UINT(0, sender_fill(&state, 1700, out));
  TEST_ASSERT_EQUAL_UINT(0, sender_timeout(&state, 1950, out));
}

/** Ensure the sender gives up after exactly ten fruitless expirations. */
static void test_sender_gives_up(void)
{
  sender_state_t state;
  packet_t out[SENDER_MAX_WINDOW];
  TEST_ASSERT_EQUAL_INT(0, sender_init(&state, NULL, 0, 1, 10));
  TEST_ASSERT_EQUAL_UINT(1, sender_fill(&state, 0, out));
  TEST_ASSERT_EQUAL_INT(PACKET_FIN, out[0].type);
  for (int i = 1; i <= 9; ++i)
    TEST_ASSERT_EQUAL_UINT(1, sender_timeout(&state, i * 10, out));
  TEST_ASSERT_EQUAL_UINT(0, sender_timeout(&state, 100, out));
  TEST_ASSERT_TRUE(state.failed);
  TEST_ASSERT_EQUAL_UINT(0, sender_fill(&state, 101, out));
}

/** Generate repeatable independent disturbance choices for both directions. */
static uint32_t next_random(uint32_t *seed)
{
  *seed = *seed * 1664525U + 1013904223U;
  return *seed;
}

/** Simulate a datagram crossing a channel that drops, corrupts, duplicates,
 * or delivers it normally, and decode only valid arrivals.
 */
static unsigned noisy_delivery(const packet_t *source, packet_t *delivered,
                               uint32_t *seed)
{
  uint8_t bytes[PACKET_MAX_SIZE];
  int length = packet_encode(source, bytes, sizeof(bytes));
  TEST_ASSERT_GREATER_THAN(0, length);
  unsigned choice = next_random(seed) % 5U;
  if (choice == 0)
    return 0;
  if (choice == 1)
    bytes[(size_t)length - 1] ^= 1;
  if (packet_decode(bytes, (size_t)length, delivered) != 0)
    return 0;
  return choice == 2 ? 2U : 1U;
}

/** Simulate one complete damaged-channel transfer with a fixed random seed. */
static void simulate_seeded_transfer(uint32_t seed)
{
  uint8_t input[8192];
  uint8_t output[8192];
  for (unsigned i = 0; i < sizeof(input); ++i)
    input[i] = (uint8_t)(i * 17U);
  sender_state_t sender;
  receiver_state_t receiver;
  packet_t outgoing[SENDER_MAX_WINDOW];
  size_t written = 0;
  int64_t now = 0;
  TEST_ASSERT_EQUAL_INT(0, sender_init(&sender, input, sizeof(input), 4, 250));
  receiver_init(&receiver, now);
  for (unsigned step = 0; step < 10000 && !sender.finished && !sender.failed; ++step)
  {
    size_t count = sender_fill(&sender, now, outgoing);
    if (count == 0)
    {
      now = sender.deadline_ms;
      count = sender_timeout(&sender, now, outgoing);
    }
    for (size_t i = 0; i < count; ++i)
    {
      packet_t delivered;
      unsigned copies = noisy_delivery(&outgoing[i], &delivered, &seed);
      for (unsigned copy = 0; copy < copies; ++copy)
      {
        receiver_action_t action = receiver_step(&receiver, &delivered, now);
        if (action.deliver)
        {
          TEST_ASSERT_TRUE(written + delivered.length <= sizeof(output));
          memcpy(output + written, delivered.payload, delivered.length);
          written += delivered.length;
        }
        if (action.send_ack)
        {
          packet_t received_ack;
          unsigned ack_copies = noisy_delivery(&action.ack, &received_ack, &seed);
          for (unsigned j = 0; j < ack_copies; ++j)
            sender_ack(&sender, &received_ack, now);
        }
      }
    }
    now += 1;
  }
  TEST_ASSERT_TRUE(sender.finished);
  TEST_ASSERT_FALSE(sender.failed);
  TEST_ASSERT_EQUAL_UINT(sizeof(input), written);
  TEST_ASSERT_EQUAL_MEMORY(input, output, sizeof(input));
  TEST_ASSERT_TRUE(receiver.finished);
}

/** Verify byte-identical completion across several reproducible channel seeds. */
static void test_seeded_lossy_transfer(void)
{
  simulate_seeded_transfer(7);
  simulate_seeded_transfer(9);
  simulate_seeded_transfer(42);
}

/** Register each protocol test with the project's single Unity runner. */
void run_protocol_tests(void)
{
  RUN_TEST(test_receiver_events);
  RUN_TEST(test_protocol_boundaries);
  RUN_TEST(test_sender_window_and_fin);
  RUN_TEST(test_sender_gives_up);
  RUN_TEST(test_seeded_lossy_transfer);
}

```

## Scripts Files
Report generated on 09/26/2026 at 08:26:04


---

## End of Report

SHA-256 Hash of the report: bb5a7ea2886e95fd64b38792d14a71473ee30d500b37820d97e2810440dccc97

Do not edit the generated report. Any changes will be reported as academic dishonesty

---
## GitHub Info
- GitHub repo name: samjwilcox/cs425-p2
- The repository visibility is public.
- The workflow was triggered by samjwilcox
