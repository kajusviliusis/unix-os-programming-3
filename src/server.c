#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

enum { LISTEN_BACKLOG = 5, MAX_PORT = 65535 };

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
    close(client_socket);
    close(server_socket);
    return EXIT_SUCCESS;
}
