
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <sys/types.h>

#define MAX_REQUEST 256
#define MAX_RESPONSE 1024

extern int log_level;

void log_msg(int level, const char *fmt, ...);
int send_all(int fd, const char *text);
ssize_t recv_line(int fd, char *buf, size_t cap, int reject_long);

#endif

