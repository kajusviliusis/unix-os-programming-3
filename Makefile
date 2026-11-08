CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: server client

server: src/server.c src/socket_utils.c src/socket_utils.h
	$(CC) $(CFLAGS) -o $@ src/server.c src/socket_utils.c

client: src/client.c src/socket_utils.c src/socket_utils.h
	$(CC) $(CFLAGS) -o $@ src/client.c src/socket_utils.c

clean:
	rm -f server client
