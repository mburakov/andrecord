#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/wait.h>

#define PIPE_FILE "/tmp/pamnc.pipe"
#define UNDERFLOW_TIMEOUT 1000

static void handler(int sig) { (void)sig; }

static int loop(int in, int out, int buffer_size, const struct sockaddr* addr) {
  char buffer[buffer_size];
  struct pollfd fds = {.fd = in, .events = POLLIN};
  int result = poll(&fds, 1, UNDERFLOW_TIMEOUT);
  switch (result) {
    case -1:
      perror("Failed to poll socket");
      return 0;
    case 0:
      if (sendto(in, NULL, 0, 0, addr, sizeof(*addr))) {
        perror("Failed to send broadcast");
        return 0;
      }
      return 1;
    default:
      break;
  }
  int length = read(in, buffer, buffer_size);
  if (length == -1) {
    perror("Failed to read socket");
    return 0;
  }
  for (char* ptr = buffer; length > 0;) {
    int written = write(out, ptr, length);
    if (written == -1) {
      perror("Failed to write pipe");
      return 0;
    }
    length -= written;
    ptr += written;
  }
  return 1;
}

static int pactl(int argc, ...) {
  va_list args;
  va_start(args, argc);
  char* argv[argc + 2];
  argv[0] = "pactl";
  for (int i = 1; i < argc + 1; ++i) {
    argv[i] = va_arg(args, char*);
  }
  argv[argc + 1] = NULL;
  va_end(args);
  switch (fork()) {
    case -1:
      perror("Failed to fork");
      return 0;
    case 0:
      execvp(argv[0], argv);
      perror("Failed to exec");
      return 0;
    default:
      break;
  }
  int result;
  if (wait(&result) == -1) {
    perror("Failed to wait");
    return 0;
  }
  return result == EXIT_SUCCESS;
}

static int make_socket(int* bufsize) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1) {
    perror("Failed to create socket");
    return -1;
  }
  do {
    socklen_t len = sizeof(*bufsize);
    if (getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, &len) == -1) {
      perror("Failed to get socket buffer size");
      break;
    }
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast,
                   sizeof(broadcast)) == -1) {
      perror("Failed to enable broadcast");
      break;
    }
    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
      perror("Failed to make socket non-blocking");
      break;
    }
    return sock;
  } while (0);
  if (close(sock) == -1) {
    perror("Failed to close socket");
  }
  return -1;
}

static int make_pipe() {
  int pares =
      pactl(7, "load-module", "module-pipe-source", "source_name=pamnc",
            "file=" PIPE_FILE, "format=s16le", "rate=16000", "channels=1");
  if (!pares) {
    return -1;
  }
  int pipe = open(PIPE_FILE, O_WRONLY);
  if (pipe == -1) {
    perror("Failed to open pipe");
    return -1;
  }
  if (unlink(PIPE_FILE) == -1) {
    perror("Failed to unlink pipe");
  }
  return pipe;
}

int main(int argc, char** argv) {
  int port = argc > 1 ? atoi(argv[1]) : 0;
  if (!port) {
    fprintf(stderr, "Usage: %s <udp_port>\n", argv[0]);
    return EXIT_FAILURE;
  }
  struct sigaction act = {.sa_handler = handler};
  act.sa_handler = handler;
  if (sigaction(SIGINT, &act, NULL) == -1) {
    perror("Failed to set up signal handler");
    return EXIT_FAILURE;
  }
  int buffer_size;
  int in = make_socket(&buffer_size);
  if (in == -1) {
    return EXIT_FAILURE;
  }
  int out = make_pipe();
  if (out != -1) {
    struct sockaddr_in addr = {.sin_family = AF_INET,
                               .sin_port = htons(port),
                               .sin_addr.s_addr = INADDR_BROADCAST};
    while (loop(in, out, buffer_size, (struct sockaddr*)&addr))
      ;
    if (close(out) == -1) {
      perror("Failed to close pipe");
    }
  }
  if (close(in) == -1) {
    perror("Failed to close socket");
  }
  pactl(2, "unload-module", "module-pipe-source");
  return in == -1 || out == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
}
