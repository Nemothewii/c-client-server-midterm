#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

static void reap_children(int signum) {
    (void)signum;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

static void handle_client(int fd, int safe) {
    char request[MAX_REQUEST];
    char response[MAX_RESPONSE];

    if (safe) {
        struct timeval tv = {
            .tv_sec = 2,
            .tv_usec = 0
        };

        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                   &tv, sizeof(tv));
    }

    ssize_t n = recv_line(
        fd, request, sizeof(request), safe
    );

    if (n == -4) {
        send_all(fd,
            "ERR timeout waiting for request\n");
        return;
    }

    if (n == -2) {
        send_all(fd, "ERR request too long\n");
        return;
    }

    if (n == -3) {
        log_msg(2, "client disconnected early");
        return;
    }

    if (n < 0) {
        log_msg(1, "socket receive error");
        return;
    }

    log_msg(2, "request: %s", request);

    if (strcmp(request, "SYSINFO") == 0) {
        struct utsname u;
        struct sysinfo s;

        if (uname(&u) != 0 || sysinfo(&s) != 0) {
            snprintf(response, sizeof(response),
                "ERR cannot obtain system information\n");
        } else {
            unsigned long long ram_mb =
                (unsigned long long)s.totalram
                * s.mem_unit / (1024 * 1024);

            snprintf(response, sizeof(response),
                "OK SYSINFO pid=%ld host=%s "
                "os=%s kernel=%s "
                "uptime_sec=%ld ram_mb=%llu\n",
                (long)getpid(),
                u.nodename,
                u.sysname,
                u.release,
                s.uptime,
                ram_mb);
        }

    } else if (strcmp(request, "UPTIME") == 0) {
        struct sysinfo s;

        if (sysinfo(&s) == 0)
            snprintf(response, sizeof(response),
                "OK UPTIME %ld seconds\n", s.uptime);
        else
            snprintf(response, sizeof(response),
                "ERR uptime failed\n");

    } else if (strncmp(request, "ECHO ", 5) == 0) {
        snprintf(response, sizeof(response),
            "OK ECHO %s\n", request + 5);

    } else if (strncmp(request, "SLEEP ", 6) == 0) {
        char *end;
        errno = 0;

        long seconds = strtol(
            request + 6, &end, 10
        );

        if (safe &&
            (errno ||
             end == request + 6 ||
             *end != '\0' ||
             seconds < 0 ||
             seconds > 10)) {

            snprintf(response, sizeof(response),
                "ERR invalid sleep duration (0-10)\n");

        } else {
            if (seconds < 0) seconds = 0;
            if (seconds > 10) seconds = 10;

            sleep((unsigned int)seconds);

            snprintf(response, sizeof(response),
                "OK SLEEP pid=%ld seconds=%ld\n",
                (long)getpid(), seconds);
        }

    } else {
        snprintf(response, sizeof(response),
            "ERR unknown command\n");
    }

    if (send_all(fd, response) != 0)
        log_msg(1, "response send failed");
    else
        log_msg(2, "response sent");
}

int main(int argc, char **argv) {
    int port = 9090;
    int safe = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0
            && i + 1 < argc)
            port = atoi(argv[++i]);

        else if (strcmp(argv[i], "--log-level") == 0
            && i + 1 < argc)
            log_level = atoi(argv[++i]);

        else if (strcmp(argv[i], "--unsafe") == 0)
            safe = 0;

        else {
            fprintf(stderr,
                "Usage: %s [--port 9090] "
                "[--log-level 0|1|2] [--unsafe]\n",
                argv[0]);
            return 1;
        }
    }

    if (port < 1 || port > 65535) {
        fprintf(stderr, "invalid port\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    struct sigaction sa = {0};
    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    int server_fd = socket(
        AF_INET, SOCK_STREAM, 0
    );

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int one = 1;
    setsockopt(server_fd, SOL_SOCKET,
               SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server_fd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        perror("bind (port busy?)");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 64) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening on 127.0.0.1:%d "
           "(mode=%s)\n",
           port, safe ? "SAFE" : "UNSAFE");

    fflush(stdout);

    log_msg(1, "ready, runtime log level=%d",
            log_level);

    for (;;) {
        int client_fd = accept(
            server_fd, NULL, NULL
        );

        if (client_fd < 0 && errno == EINTR)
            continue;

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            send_all(client_fd,
                "ERR server busy\n");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            close(server_fd);
            handle_client(client_fd, safe);
            close(client_fd);
            _exit(0);
        }

        log_msg(1, "forked child pid=%ld",
                (long)pid);

        close(client_fd);
    }
}
