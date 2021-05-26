#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "chat.h"

typedef struct {
    int index;
    int socketfd;
    int thread_id;
    char name[20];
    char buffer[BUF_LEN];
    int buffer_len;
} thread_context;

void *listen_func(void *);
void *broadcast();
void *wait_input();
void init_workers();
void init_socketfds();
int search_space();

bool g_stop_server = false;
pthread_t g_workers[MAX_CON];
int g_socketfds[MAX_CON];

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