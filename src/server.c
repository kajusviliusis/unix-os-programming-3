#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

enum { LISTEN_BACKLOG = 5, MAX_PORT = 65535, BUFFER_SIZE = 1024, MAX_CLIENTS = 10 };

// saugomas kiekvieno kliento socket ir jo gautos zinutes duomenys.
struct client {
    int socket_fd;
    char buffer[BUFFER_SIZE];
    size_t length;
};

// TCP send gali perduoti tik dali duomenu todel siunciame iki galo.
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

static int handle_client(struct client *client)
{
    // nuskaitome siuo metu gautus duomenis ir papildome kliento buferi.
    ssize_t count = recv(client->socket_fd, client->buffer + client->length,
                         BUFFER_SIZE - client->length, 0);
    if (count == -1) {
        if (errno == EINTR) {
            return 0;
        }
        perror("recv");
        return 1;
    }
    if (count == 0) {
        puts("Client disconnected.");
        return 1;
    }

    char *newline = memchr(client->buffer + client->length, '\n', (size_t)count);
    client->length += (size_t)count;
    if (newline != NULL) {
        size_t line_length = (size_t)(newline - client->buffer) + 1;
        fputs("Received: ", stdout);
        // recv neprideda nulinio simbolio todel naudojame gauta baitu skaiciu.
        fwrite(client->buffer, 1, line_length, stdout);
        fflush(stdout);

        const char acknowledgment[] = "Message received\n";
        // Patvirtinimas siunciamas tuo paciu kliento socket.
        send_all(client->socket_fd, acknowledgment, sizeof(acknowledgment) - 1);
        return 1;
    }

    if (client->length == BUFFER_SIZE) {
        fputs("Message is too long.\n", stderr);
        return 1;
    }
    return 0;
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
    if (server_socket >= FD_SETSIZE) {
        fputs("Listening socket exceeds FD_SETSIZE.\n", stderr);
        close(server_socket);
        return EXIT_FAILURE;
    }

    // SO_REUSEADDR leidzia greitai vel paleisti serveri tuo paciu prievadu.
    int reuse_address = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                   sizeof(reuse_address)) == -1) {
        perror("setsockopt");
        close(server_socket);
        return EXIT_FAILURE;
    }

    // uzdarius klientui rysi, send klaida neturi sustabdyti serverio.
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        close(server_socket);
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

    struct client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket_fd = -1;
        clients[i].length = 0;
    }

    for (;;) {
        fd_set read_fds;
        // select pakeicia FD rinkini, todel pries kiekviena kvietima ji atnaujiname.
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        int max_fd = server_socket;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket_fd != -1) {
                FD_SET(clients[i].socket_fd, &read_fds);
                if (clients[i].socket_fd > max_fd) {
                    max_fd = clients[i].socket_fd;
                }
            }
        }

        // select tikrina deskriptorius nuo 0 iki max_fd, todel perduodame max_fd + 1.
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket_fd != -1) {
                    close(clients[i].socket_fd);
                }
            }
            close(server_socket);
            return EXIT_FAILURE;
        }

        // tikriname ar yra nauju klientu, kuriuos galima priimti.
        if (FD_ISSET(server_socket, &read_fds)) {
            int client_socket = accept(server_socket, NULL, NULL);
            if (client_socket == -1) {
                if (errno != EINTR) {
                    perror("accept");
                }
            } else if (client_socket >= FD_SETSIZE) {
                fputs("Client socket exceeds FD_SETSIZE.\n", stderr);
                close(client_socket);
            } else {
                int free_slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].socket_fd == -1) {
                        free_slot = i;
                        break;
                    }
                }
                if (free_slot == -1) {
                    fputs("Server full; client connection closed.\n", stderr);
                    close(client_socket);
                } else {
                    clients[free_slot].socket_fd = client_socket;
                    clients[free_slot].length = 0;
                    puts("Client connected.");
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket_fd != -1 &&
                FD_ISSET(clients[i].socket_fd, &read_fds) &&
                handle_client(&clients[i])) {
                close(clients[i].socket_fd);
                clients[i].socket_fd = -1;
                clients[i].length = 0;
            }
        }
    }
}
