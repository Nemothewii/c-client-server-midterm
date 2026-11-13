#define _POSIX_C_SOURCE 200809L
#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *command = "SYSINFO";
    int port = 9090;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--command") == 0
            && i + 1 < argc)
            command = argv[++i];

        else if (strcmp(argv[i], "--port") == 0
            && i + 1 < argc)
            port = atoi(argv[++i]);

        else if (strcmp(argv[i], "--log-level") == 0
            && i + 1 < argc)
            log_level = atoi(argv[++i]);

        else {
            fprintf(stderr,
                "Usage: %s [--command SYSINFO] "
                "[--port 9090] [--log-level 0|1|2]\n",
                argv[0]);
            return 1;
        }
    }

    if (port < 1 || port > 65535) {
        fprintf(stderr, "invalid port\n");
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct timeval tv = {
        .tv_sec = 15,
        .tv_usec = 0
    };

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
               &tv, sizeof(tv));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr *)&addr,
                sizeof(addr)) < 0) {
        perror("connect (is server running?)");
        close(fd);
        return 1;
    }

    log_msg(1, "connected to server port %d",
            port);

    if (send_all(fd, command) < 0 ||
        send_all(fd, "\n") < 0) {
        perror("send");
        close(fd);
        return 1;
    }

    char response[MAX_RESPONSE];

    ssize_t n = recv_line(
        fd, response, sizeof(response), 1
    );

    if (n >= 0)
        printf("Server response: %s\n", response);

    else if (n == -4)
        fprintf(stderr,
            "ERROR: client timed out\n");

    else
        fprintf(stderr,
            "ERROR: server disconnected "
            "or invalid response (%ld)\n",
            (long)n);

    close(fd);

    return n < 0 ? 1 : 0;
}
