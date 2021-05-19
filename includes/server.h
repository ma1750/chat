#include "chat.h"

typedef struct
{
    int socketfd;
    int thread_id;
    char name[20];
    char buffer[BUF_LEN];
    int buffer_len;
} thread_context;
