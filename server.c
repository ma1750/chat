#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include "chat.h"
#include "server.h"


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
    server.sin_port = htons(port);
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
    init_workers();
    init_socketfds();
    pthread_t input, broadcast_thread;
    pthread_create(&input, NULL, (void *)wait_input, NULL);
    pthread_create(&broadcast_thread, NULL, (void *)broadcast, NULL);


    while (!g_stop_server) {
        if ((client_socketfd = accept(socketfd, (struct sockaddr *)&server, &sin_size)) == -1) {
            perror("accept failed");
            if (close(socketfd) == -1) {
                perror("close failed");
            }
            exit(EXIT_FAILURE);
        }

        int i;
        if ((i = search_space()) == -1) {
            // Already maximum connection
            send(client_socketfd, "exit_ack", 8, 0);
            close(client_socketfd);
            continue;
        }

        g_socketfds[i] = client_socketfd;

        pthread_create(&g_workers[i], NULL, (void *)listen_func, (void *)i);
    }

    for (int i = 0; i < MAX_CON; ++i) {
        if (g_workers[i] != NULL) {
            if ((pthread_join(g_workers[i], NULL) != 0)){
                perror("pthread_join failed");
                if (close(socketfd) == -1) {
                    perror("close failed");
                    exit(EXIT_FAILURE);
                }
            }
            g_workers[i] = NULL;
        }
    }

    if (close(socketfd) == -1) {
        perror("close failed");
        exit(EXIT_FAILURE);
    }
    return 0;
}