#include "utils.h"

#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/wait.h>

static void handler(int sig) {
    (void)sig;
}

static int bufsize(int sock) {
    int optval;
    socklen_t optlen = sizeof(optval);
    int result = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
    return result == -1 ? -1 : optval;
}

static int loop(int from, int to, int size) {
    char buffer[size];
    int length = read(from, buffer, size);
    if (length == -1) {
        perror("Failed to read socket");
        return 0;
    }
    for (char* ptr = buffer; length > 0;) {
        int written = write(to, ptr, length);
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

int main(int argc, char** argv) {
    int result = EXIT_FAILURE;
    int fds[] = {-1, -1};
    do {
        int port = argc > 1 ? atoi(argv[1]) : 0;
        if (!port) {
            fprintf(stderr, "Usage: %s <udp_port>\n", argv[0]);
            break;
        }
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = handler;
        if (sigaction(SIGINT, &act, NULL) == -1) {
            perror("Failed to set up signal handler");
            break;
        }
        if ((fds[0] = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
            perror("Failed to create socket");
            break;
        }
        int size = bufsize(fds[0]);
        if (size == -1) {
            perror("Failed to get socket buffer size");
            break;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (bind(fds[0], (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            perror("Failed to bind socket");
            break;
        }
        int pares = pactl(7, "load-module", "module-pipe-source",
                          "source_name=pamnc", "file=/tmp/pamnc.pipe",
                          "format=s16le", "rate=16000", "channels=1");
        if (!pares) {
            break;
        }
        if ((fds[1] = open("/tmp/pamnc.pipe", O_WRONLY)) == -1) {
            perror("Failed to open pipe");
            break;
        }
        if (unlink("/tmp/pamnc.pipe")) {
            perror("Failed to unlink pipe");
            break;
        }
        while (loop(fds[0], fds[1], size))
            ;
    } while (result = EXIT_SUCCESS, 0);
    FOR_EACH (int* i, fds) {
        if (*i != -1 && close(*i) == -1) {
            perror("Failed to close fd");
        }
    }
    pactl(2, "unload-module", "module-pipe-source");
    return result;
}
