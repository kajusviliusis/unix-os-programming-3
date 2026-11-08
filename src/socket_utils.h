#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <stddef.h>

int send_all(int socket_fd, const char *data, size_t length);
int is_valid_port(const char *port);

#endif
