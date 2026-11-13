
#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define BUFFER_SIZE 4096

void log_message(int debug_level, int required_level,
                 const char *message);

int get_system_information(char *buffer, size_t size);

int send_all(int socket_fd, const void *data, size_t length);

int receive_line(int socket_fd, char *buffer, size_t size);

#endif
