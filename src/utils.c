
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>

/* Shared logging function */
void log_message(int debug_level, int required_level,
                 const char *message)
{
    if (debug_level >= required_level) {
        printf("[DEBUG] %s\n", message);
        fflush(stdout);
    }
}

/* Get Linux system information */
int get_system_information(char *buffer, size_t size)
{
    struct utsname system_info;
    struct sysinfo memory_info;
    time_t current_time;
    struct tm local_time;
    char time_string[64];

    if (uname(&system_info) == -1) {
        return -1;
    }

    if (sysinfo(&memory_info) == -1) {
        return -1;
    }

    current_time = time(NULL);

    if (localtime_r(&current_time, &local_time) == NULL) {
        return -1;
    }

    if (strftime(time_string, sizeof(time_string),
                 "%Y-%m-%d %H:%M:%S",
                 &local_time) == 0) {
        return -1;
    }

    int result = snprintf(
        buffer, size,
        "\n===== SYSTEM INFORMATION =====\n"
        "Operating System: %s\n"
        "Hostname: %s\n"
        "Kernel Version: %s\n"
        "Architecture: %s\n"
        "Uptime: %ld seconds\n"
        "Total RAM: %lu MB\n"
        "Free RAM: %lu MB\n"
        "Current Time: %s\n"
        "==============================\n",
        system_info.sysname,
        system_info.nodename,
        system_info.release,
        system_info.machine,
        memory_info.uptime,
        (unsigned long)(memory_info.totalram *
                        memory_info.mem_unit / (1024 * 1024)),
        (unsigned long)(memory_info.freeram *
                        memory_info.mem_unit / (1024 * 1024)),
        time_string
    );

    if (result < 0 || (size_t)result >= size) {
        return -1;
    }

    return result;
}

/* Send all bytes, even if send() sends only part */
int send_all(int socket_fd, const void *data, size_t length)
{
    const char *ptr = data;
    size_t total = 0;

    while (total < length) {
        ssize_t sent = send(socket_fd, ptr + total,
                            length - total, MSG_NOSIGNAL);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (sent == 0)
            return -1;

        total += (size_t)sent;
    }

    return 0;
}

/* Receive one line safely */
int receive_line(int socket_fd, char *buffer, size_t size)
{
    size_t total = 0;

    if (size == 0)
        return -1;

    while (total < size - 1) {
        char ch;
        ssize_t received = recv(socket_fd, &ch, 1, 0);

        if (received == 0) {
            if (total == 0)
                return 0;

            break;
        }

        if (received < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (ch == '\n')
            break;

        if (ch == '\r')
            continue;

        buffer[total++] = ch;
    }

    buffer[total] = '\0';

    return (int)total;
}
