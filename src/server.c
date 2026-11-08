#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum { LISTEN_BACKLOG = 5, MAX_PORT = 65535, BUFFER_SIZE = 1024 };

// TCP send gali perduoti tik dali duomenu, todel siunciame iki galo.
static int send_all(int socket_fd, const char *data, size_t length)
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

static ssize_t receive_line(int socket_fd, char *buffer, size_t capacity)
{
    size_t used = 0;
    while (used < capacity) {
        // recv gali grazinti tik dali eilutes, todel laukiama naujos eilutes zenklo.
        ssize_t count = recv(socket_fd, buffer + used, capacity - used, 0);
        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            return -1;
        }
        if (count == 0) {
            fputs("Client disconnected before sending a full line.\n", stderr);
            return -1;
        }

        char *newline = memchr(buffer + used, '\n', (size_t)count);
        used += (size_t)count;
        if (newline != NULL) {
            return newline - buffer + 1;
        }
    }

    fputs("Message is too long.\n", stderr);
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *end;
    long port = strtol(argv[1], &end, 10);
    if (argv[1][0] < '0' || argv[1][0] > '9' || *end != '\0' ||
        port < 1 || port > MAX_PORT) {
        fprintf(stderr, "Invalid port: enter an integer from 1 to 65535.\n");
        return EXIT_FAILURE;
    }

    // sukuriamas TCP socket naudojant IPv4
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    // tinklo baitu tvarka reikalinga prievado numeriui adrese
    server_address.sin_port = htons((unsigned short)port);

    // priskiriamas IP adresas ir portas serverio socket
    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) == -1) {
        perror("bind");
        close(server_socket);
        return EXIT_FAILURE;
    }

    // serveris pradeda laukti klientu prisijungimu
    if (listen(server_socket, LISTEN_BACKLOG) == -1) {
        perror("listen");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Listening on port %ld...\n", port);
    fflush(stdout);

    // laukiama kliento prisijungimo ir gaunamas naujas socket bendravimui su juo
    int client_socket = accept(server_socket, NULL, NULL);
    if (client_socket == -1) {
        perror("accept");
        close(server_socket);
        return EXIT_FAILURE;
    }

    puts("Client connected.");
    char message[BUFFER_SIZE];
    ssize_t message_length = receive_line(client_socket, message, sizeof(message));
    if (message_length == -1) {
        close(client_socket);
        close(server_socket);
        return EXIT_FAILURE;
    }

    fputs("Received: ", stdout);
    // recv neprideda nulinio simbolio todel naudojame gauta baitu skaiciu
    fwrite(message, 1, (size_t)message_length, stdout);

    const char acknowledgment[] = "Message received\n";
    // Patvirtinimas siunciamas tuo paciu kliento socket
    if (send_all(client_socket, acknowledgment, sizeof(acknowledgment) - 1) == -1) {
        close(client_socket);
        close(server_socket);
        return EXIT_FAILURE;
    }

    close(client_socket);
    close(server_socket);
    return EXIT_SUCCESS;
}
