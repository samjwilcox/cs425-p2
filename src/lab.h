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
