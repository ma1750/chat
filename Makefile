CC = gcc
CFLAGS = -O2 -Wall -lpthread

all: chat

chat: client.o
		$(CC) $(CFLAGS) -o client.out client.o

client: client.o
		$(CC) $(CFLAGS) -o client.out client.c

clean:; rm -f *.o *.out *~