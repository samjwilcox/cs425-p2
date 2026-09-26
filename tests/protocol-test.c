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
