#include <pthread.h>
#include <stdbool.h>

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