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

void *listen_func(void*);
void *wait_input();

bool g_stop_server = false;

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
    pthread_t worker;
    pthread_t input;
    pthread_create(&input, NULL, (void *)wait_input, NULL);

    while (!g_stop_server) {
        if ((client_socketfd = accept(socketfd, (struct sockaddr *)&server, &sin_size)) == -1) {
            perror("accept failed");
            if (close(socketfd) == -1) {
                perror("close failed");
            }
            exit(EXIT_FAILURE);
        }

        pthread_create(&worker, NULL, (void *)listen_func, (void *)client_socketfd);
        if (worker != NULL) {
            if ((pthread_join(worker, NULL) != 0)){
                perror("pthread_join failed");
                if (close(socketfd) == -1) {
                    perror("close failed");
                    exit(EXIT_FAILURE);
                }
            }
            worker = NULL;
        }

        if (close(client_socketfd) == -1) {
            perror("close failed");
            exit(EXIT_FAILURE);
        }
    }

    if (close(socketfd) == -1) {
        perror("close failed");
        exit(EXIT_FAILURE);
    }
    return 0;
}


void *listen_func(void *socketfd)
{
    thread_context ctx;
    ctx.socketfd = (int)socketfd;
    ctx.name[0] = '\0';
    ctx.thread_id = pthread_self();
    int send_len;
    if ((send_len = send(ctx.socketfd, "connect OK", 10, 0)) == -1) {
        fprintf(stderr, "[tid: %d]send failed: connect OK\n", ctx.thread_id);
        perror(NULL);
        return NULL;
    }

    do {
        if ((ctx.buffer_len = recv(ctx.socketfd, ctx.buffer, BUF_LEN, 0)) == -1) {
            fprintf(stderr, "[tid: %d]recv failed", ctx.thread_id);
            perror(NULL);
            break;
        }
        ctx.buffer[ctx.buffer_len] = '\0';
        if (!strlen(ctx.name))
            strncpy(ctx.name, ctx.buffer, 20);
        printf("[tid: %d][%s]%s\n", ctx.thread_id, ctx.name, ctx.buffer);
    } while (strncmp(ctx.buffer, "exit", 4) != 0);

    if ((send_len = send(ctx.socketfd, "exit_ack", 8, 0)) == -1) {
        fprintf(stderr, "[tid: %d]send failed: exit_ack\n", ctx.thread_id);
        perror(NULL);
        return NULL;
    }

    return NULL;
}


void *wait_input()
{
    int _c;
    _c = fgetc(stdin);

    g_stop_server = true;
    return NULL;
}