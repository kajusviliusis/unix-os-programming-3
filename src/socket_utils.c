#include "socket_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

enum { MAX_PORT = 65535 };

// TCP send gali perduoti tik dali duomenu todel siunciame iki galo.
int send_all(int socket_fd, const char *data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t count = send(socket_fd, data + sent, length - sent, 0);
        if (count == -1) {
            // EINTR reiskia, kad kvietima pertrauke signalas, bandome dar karta.
            if (errno == EINTR) {
                continue;
            }
            perror("send");
            return -1;
        }
        if (count == 0) {
            fputs("Connection closed while sending.\n", stderr);
            return -1;
        }
        sent += (size_t)count;
    }
    return 0;
}

int is_valid_port(const char *port)
{
    char *end;
    long number = strtol(port, &end, 10);
    return port[0] >= '0' && port[0] <= '9' && *end == '\0' &&
           number >= 1 && number <= MAX_PORT;
}
