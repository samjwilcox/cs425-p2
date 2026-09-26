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
