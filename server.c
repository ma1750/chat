#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char const *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "[Usage] %s server_IP server_Port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_addr = argv[1];
    int port = atoi(argv[2]);

    if (!port) {
        fprintf(stderr, "server port is invalid\n");
        exit(EXIT_FAILURE);
    }

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int val = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1) {
        perror("setsockopt failed");
        if (close(socketfd) == -1) {
            perror("close failed");
        }
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server;
    socklen_t sin_size;

    server.sin_family = AF_INET;
    inet_aton(server_addr, &server.sin_addr);
    sin_size = sizeof(struct sockaddr_in);

    if (bind(socketfd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("bind failed");
        if (close(socketfd) == -1) {
            perror("close failed");
        }
        exit(EXIT_FAILURE);
    }

    if (listen(socketfd, SOMAXCONN)  == -1) {
        perror("listen failed");
        if (close(socketfd) == -1) {
            perror("close failed");
        }
        exit(EXIT_FAILURE);
    }

    int client_socketfd;
    if ((client_socketfd = accept(socketfd, (struct sockaddr *)&server, sizeof(server))) == -1) {
        perror("accept failed");
        if (close(socketfd) == -1) {
            perror("close failed");
        }
        exit(EXIT_FAILURE);
    }

    send(client_socketfd, "connect OK", 10, 0);
    char recv_buf[128];
    do {
        recv(client_socketfd, recv_buf, 128, 0);
        printf("Client: %s\n", recv_buf);
    } while (strncmp("exit", recv_buf, 4));
    send(client_socketfd, "exit_ack", 8, 0);

    if (close(client_socketfd) == -1) {
        perror("close failed");
        exit(EXIT_FAILURE);
    }

    if (close(socketfd) == -1) {
        perror("close failed");
        exit(EXIT_FAILURE);
    }
    return 0;
}
