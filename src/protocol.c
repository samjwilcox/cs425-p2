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
