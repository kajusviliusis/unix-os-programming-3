#define _POSIX_C_SOURCE 200112L

#include "socket_utils.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum { BUFFER_SIZE = 1024 };

static ssize_t receive_line(int socket_fd, char *buffer, size_t capacity)
{
    size_t used = 0;
    while (used < capacity) {
        // recv gali grazinti tik dali atsakymo, todel laukiama naujos eilutes zenklo.
        ssize_t count = recv(socket_fd, buffer + used, capacity - used, 0);
        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            return -1;
        }
        if (count == 0) {
            fputs("Server disconnected before sending a full response.\n", stderr);
            return -1;
        }

        char *newline = memchr(buffer + used, '\n', (size_t)count);
        used += (size_t)count;
        if (newline != NULL) {
            return newline - buffer + 1;
        }
    }

    fputs("Server response is too long.\n", stderr);
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!is_valid_port(argv[2])) {
        fputs("Invalid port: enter an integer from 1 to 65535.\n", stderr);
        return EXIT_FAILURE;
    }

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *addresses;
    // getaddrinfo pagal serverio varda ir porta suranda galimus tinklo adresus
    int result = getaddrinfo(argv[1], argv[2], &hints, &addresses);
    if (result != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        return EXIT_FAILURE;
    }

    int server_socket = -1;
    int last_error = 0;
    const char *failed_call = "connect";
    // Vienas vardas gali tureti kelis adresus, todel bandome juos is eiles.
    for (struct addrinfo *address = addresses; address != NULL;
         address = address->ai_next) {
        int candidate = socket(address->ai_family, address->ai_socktype,
                               address->ai_protocol);
        if (candidate == -1) {
            last_error = errno;
            failed_call = "socket";
            continue;
        }

        // Prisijungus sis socket naudojamas zinutei ir atsakymui.
        if (connect(candidate, address->ai_addr, address->ai_addrlen) == 0) {
            server_socket = candidate;
            break;
        }
        last_error = errno;
        failed_call = "connect";
        close(candidate);
    }
    // atlaisvinama getaddrinfo paskirta atmintis
    freeaddrinfo(addresses);

    if (server_socket == -1) {
        if (last_error != 0) {
            errno = last_error;
            perror(failed_call);
        } else {
            fputs("No server addresses found.\n", stderr);
        }
        return EXIT_FAILURE;
    }

    char message[BUFFER_SIZE];
    if (fgets(message, sizeof(message), stdin) == NULL) {
        if (ferror(stdin)) {
            perror("fgets");
        } else {
            fputs("No message provided.\n", stderr);
        }
        close(server_socket);
        return EXIT_FAILURE;
    }

    size_t message_length = strlen(message);
    if (message_length == 0 ||
        (message[message_length - 1] != '\n' && message_length == sizeof(message) - 1)) {
        fputs("Message is too long.\n", stderr);
        close(server_socket);
        return EXIT_FAILURE;
    }
    if (message[message_length - 1] != '\n') {
        // Jei ivestis baigesi be naujos eilutes, pridedame zinutes skirtuka.
        message[message_length++] = '\n';
    }

    if (send_all(server_socket, message, message_length) == -1) {
        close(server_socket);
        return EXIT_FAILURE;
    }

    // shutdown baigia siuntima, bet leidzia dar gauti serverio atsakyma.
    if (shutdown(server_socket, SHUT_WR) == -1) {
        perror("shutdown");
        close(server_socket);
        return EXIT_FAILURE;
    }

    char response[BUFFER_SIZE];
    ssize_t response_length = receive_line(server_socket, response, sizeof(response));
    if (response_length == -1) {
        close(server_socket);
        return EXIT_FAILURE;
    }

    // recv neprideda nulinio simbolio todel naudojame gauta baitu skaiciu.
    fwrite(response, 1, (size_t)response_length, stdout);
    close(server_socket);
    return EXIT_SUCCESS;
}
