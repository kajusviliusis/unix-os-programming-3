CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: server client

server: src/server.c
	$(CC) $(CFLAGS) -o $@ $<

client: src/client.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f server client
