#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

    sigset_t sigmask;

    if (sigemptyset(&sigmask) == -1) {
        perror("sigemptyset() failed");
        exit(EXIT_FAILURE);
    }

    if (sigaddset(&sigmask, SIGUSR1) == -1) {
        perror("sigaddset() failed");
        exit(EXIT_FAILURE);
    }

    if (pthread_sigmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        perror("pthread_sigmask() falied");
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

    if (listen(socketfd, SOMAXCONN) == -1) {
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
            if (pthread_join(g_workers[i], NULL) != 0) {
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

void *listen_func(void *index)
{
    thread_context ctx;
    ctx.index = (int)index;
    ctx.socketfd = g_socketfds[ctx.index];
    ctx.name[0] = '\0';
    ctx.thread_id = pthread_self();
    int send_len;

    sigset_t sigmask;
    pid_t pid = getpid();

    if (sigemptyset(&sigmask) == -1) {
        perror("sigemptyset() failed");
        exit(EXIT_FAILURE);
    }

    if (sigaddset(&sigmask, SIGUSR1) == -1) {
        perror("sigaddset() failed");
        exit(EXIT_FAILURE);
    }

    if (pthread_sigmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        perror("pthread_sigmask() falied");
        exit(EXIT_FAILURE);
    }

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

        // broadcast
        if (sigqueue(pid, SIGUSR1, (sigval_t)((void *)&ctx)) == -1) {
            perror("sigqueue() failed");
            // TODO: shutdown or kill only this node
            exit(EXIT_FAILURE);
        }
    } while (strncmp(ctx.buffer, "exit", 4) != 0);

    if ((send_len = send(ctx.socketfd, "exit_ack", 8, 0)) == -1) {
        fprintf(stderr, "[tid: %d]send failed: exit_ack\n", ctx.thread_id);
        perror(NULL);
        return NULL;
    }

    if (close(g_socketfds[ctx.index]) == -1) {
        perror("close failed");
        exit(EXIT_FAILURE);
    }

    g_socketfds[ctx.index] = -1;

    return NULL;
}

void *broadcast()
{
    thread_context *ctx;
    sigset_t sigmask;
    pid_t pid = getpid();

    if (sigemptyset(&sigmask) == -1) {
        perror("sigemptyset() failed");
        exit(EXIT_FAILURE);
    }

    if (sigaddset(&sigmask, SIGUSR1) == -1) {
        perror("sigaddset() failed");
        exit(EXIT_FAILURE);
    }

    siginfo_t info;
    int _socketfds[MAX_CON];
    char _send_buf[BUF_LEN];

    do {
        if (sigwaitinfo(&sigmask, &info) == -1) {
            if (errno != EINTR) {
                perror("sigwaitinfo() failed");
                // TODO: shutdown
                exit(EXIT_FAILURE);
            }
            continue;
        }

        ctx = (thread_context *)info.si_value.sival_ptr;
        printf("[broadcast]: %s> %s\n", ctx->name, ctx->buffer);
        // TODO: buffer over flow
        snprintf(_send_buf, strlen(ctx->name) + ctx->buffer_len + 3, "%s> %s", ctx->name, ctx->buffer);
        _send_buf[strlen(_send_buf)] = '\0';

        for (int i = 0; i < MAX_CON; ++i)
            _socketfds[i] = g_socketfds[i];
        for (int i = 0; i < MAX_CON; ++i) {
            if (_socketfds[i] != -1)
                send(_socketfds[i], _send_buf, strlen(_send_buf) + 1, 0);
        }

    } while (1);

    return NULL;
}

void *wait_input()
{
    int _c;
    _c = fgetc(stdin);

    g_stop_server = true;
    return NULL;
}

void init_workers()
{
    for (int i = 0; i < MAX_CON; ++i)
        g_workers[i] = NULL;
    return NULL;
}

void init_socketfds()
{
    for (int i = 0; i < MAX_CON; ++i)
        g_socketfds[i] = -1;
    return NULL;
}

int search_space()
{
    for (int i = 0; i < MAX_CON; ++i) {
        if (g_socketfds[i] == -1)
            return i;
    }
    return -1;
}