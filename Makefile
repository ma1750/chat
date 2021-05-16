CC = gcc
CFLAGS = -O2 -Wall -lpthread -I ./includes

all: chat

chat: client.o server.o
		$(CC) $(CFLAGS) -o client.out client.o
		$(CC) $(CFLAGS) -o server.out server.o

client: client.o
		$(CC) $(CFLAGS) -o client.out client.c

server: server.o
		$(CC) $(CFLAGS) -o server.out server.o

clean:; rm -f *.o *.out *~