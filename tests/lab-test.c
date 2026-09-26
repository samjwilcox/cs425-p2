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
