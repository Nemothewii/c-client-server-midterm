	
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

void stop_sniffer(int sig) {
    (void)sig;
    running = 0;
}

unsigned read16(const unsigned char *p) {
    return ((unsigned)p[0] << 8) | p[1];
}

int main(void) {
    int fd = socket(AF_PACKET, SOCK_RAW,
                    htons(ETH_P_IP));

    if (fd < 0) {
        perror("socket: try sudo");
        return 1;
    }

    unsigned index = if_nametoindex("lo");
    if (!index) {
        perror("loopback interface");
        close(fd);
        return 1;
    }

    struct sockaddr_ll addr = {0};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_IP);
    addr.sll_ifindex = (int)index;

    if (bind(fd, (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return 1;
    }

    struct sigaction sa = {0};
    sa.sa_handler = stop_sniffer;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    unsigned char packet[65536];

    printf("Sniffing TCP port 9090 on lo...\n");
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);

    while (running) {
        ssize_t n = recv(fd, packet,
                         sizeof(packet), 0);

        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            break;
        }

        /* Ethernet header + IPv4 header */
        if (n < 54) continue;
        if (read16(packet + 12) != ETH_P_IP)
            continue;

        unsigned char *ip = packet + 14;

        if ((ip[0] >> 4) != 4 || ip[9] != 6)
            continue;

        unsigned ihl = (ip[0] & 15) * 4;
        if (ihl < 20 ||
            (size_t)n < 14 + ihl + 20)
            continue;

        unsigned total = read16(ip + 2);
        if (total < ihl + 20) continue;

        if ((read16(ip + 6) & 0x1fff) != 0)
            continue;

        unsigned char *tcp = ip + ihl;

        unsigned src_port = read16(tcp);
        unsigned dst_port = read16(tcp + 2);

        if (src_port != 9090 && dst_port != 9090)
            continue;

        unsigned thl = (tcp[12] >> 4) * 4;
        if (thl < 20 ||
            (size_t)n < 14 + ihl + thl ||
            total < ihl + thl)
            continue;

        unsigned payload_len = total - ihl - thl;
        size_t available =
            (size_t)n - (14 + ihl + thl);

        if (payload_len > available)
            payload_len = (unsigned)available;

        char src[INET_ADDRSTRLEN];
        char dst[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, ip + 12,
                  src, sizeof(src));
        inet_ntop(AF_INET, ip + 16,
                  dst, sizeof(dst));

        printf("\nTCP %s:%u -> %s:%u\n",
               src, src_port, dst, dst_port);

        unsigned flags = tcp[13];

        printf("Flags: %s%s%s%s%s\n",
               flags & 2 ? "SYN " : "",
               flags & 16 ? "ACK " : "",
               flags & 8 ? "PSH " : "",
               flags & 1 ? "FIN " : "",
               flags & 4 ? "RST " : "");

        if (payload_len > 0) {
            unsigned char *data = tcp + thl;

            printf("Payload: ");

            unsigned show =
                payload_len < 160 ? payload_len : 160;

            for (unsigned i = 0; i < show; i++) {
                unsigned char c = data[i];

                if (c == '\n') printf("\\n");
                else if (c >= 32 && c <= 126)
                    putchar(c);
                else putchar('.');
            }

            if (show < payload_len) printf("...");
            putchar('\n');
        }

        fflush(stdout);
    }

    close(fd);
    return 0;
}

