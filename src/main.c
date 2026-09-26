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
