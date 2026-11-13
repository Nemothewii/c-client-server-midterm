#include "common.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 1
#endif

int log_level = 1;

void log_msg(int level, const char *fmt, ...) {
#if DEBUG_ENABLED
    if (level > log_level) return;

    fprintf(stderr, "[%s pid=%ld] ",
            level == 1 ? "INFO" : "DEBUG",
            (long)getpid());

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
    fflush(stderr);
#else
    (void)level;
    (void)fmt;
#endif
}

int send_all(int fd, const char *text) {
    size_t len = strlen(text);

    while (len) {
        ssize_t n = send(fd, text, len, 0);

        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;

        text += n;
        len -= (size_t)n;
    }
    return 0;
}

ssize_t recv_line(int fd, char *buf,
                  size_t cap, int reject_long) {
    size_t n = 0;
    int too_long = 0;

    if (cap < 2) return -1;

    for (;;) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);

        if (r < 0 && errno == EINTR) continue;

        if (r < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
            return -4;

        if (r < 0) return -1;
        if (r == 0) return -3;
        if (c == '\n') break;

        if (n < cap - 1)
            buf[n++] = c;
        else
            too_long = 1;
    }

    if (n && buf[n - 1] == '\r') n--;
    buf[n] = '\0';

    if (too_long && reject_long) return -2;

    return (ssize_t)n;
}
