#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUF_LEN 128

int main(int argc, char const *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "[Usage] %s server_IP server_Port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_addr = argv[1];
    int port = atoi(argv[2]);
    if (!port) {
        fprintf(stderr, "server port is invalid\n");
        exit(EXIT_FAILURE);
    }

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server;
    struct addrinfo hints, *ai;
    int ret = 0;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(server_addr, NULL, &hints, ai);
    if (ret == -1) {
        perror("getaddrinfo() failed");
        freeaddrinfo(ai);
        close(socketfd);
        exit(EXIT_FAILURE);
    }

    memcpy(&server, ai->ai_addr, ai->ai_addrlen);
    server.sin_port = port;

    freeaddrinfo(ai);

    ret = connect(socketfd, (struct sockaddr *)&server, sizeof(server));
    if (ret == -1) {
        perror("connect() failed");
        close(socketfd);
        exit(EXIT_FAILURE);
    }


    close(socketfd);
    return 0;
}
