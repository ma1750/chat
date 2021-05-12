#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#define BUF_LEN 128

int input(char*);
void *listen_func(int);

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
        if (close(socketfd) == -1) {
            perror("close failed");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }

    memcpy(&server, ai->ai_addr, ai->ai_addrlen);
    server.sin_port = port;

    freeaddrinfo(ai);

    ret = connect(socketfd, (struct sockaddr *)&server, sizeof(server));
    if (ret == -1) {
        perror("connect() failed");
        if (close(socketfd) == -1) {
            perror("close failed");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }

    char recv_buf[BUF_LEN];
    int recv_len;
    if ((recv_len = recv(socketfd, recv_buf, BUF_LEN, 0)) == -1) {
        perror("recv failed");
        if (close(socketfd) == -1) {
            perror("close failed");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }
    if (strncmp(recv_buf, "connect OK", recv_len) != 0) {
        fprintf(stderr, "invalid packet: %s\n", recv_buf);
        if (close(socketfd) == -1) {
            perror("close failed");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }

    printf("Input your name. Up to 20 characters >");
    char input_buf[BUF_LEN];
    int input_len;
    if ((input_len = input(&input_buf)) == -1) {
        // error
        fprintf(stderr, "invalid input");
        if (close(socketfd) == -1) {
            perror("close failed");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }

    if (input_len > 20)
        input_len = 20;

    send(socketfd, input_buf, input_len, NULL);

    pthread_t listen_thread;
    pthread_create(&listen_thread, NULL, (void *)listen_func, NULL);

    do {
        if ((input_len = input(input_buf)) == -1) {
            fprintf(stderr, "invalid input");
            break;
        }
        send(socketfd, input_buf, input_len, 0);
    } while (strncmp(input_buf, "exit", 4) != 0);

    if (listen_thread != NULL) {
        if ((pthread_join(listen_thread, NULL) != 0)){
            perror("pthread_join failed");
            if (close(socketfd) == -1) {
                perror("close failed");
                exit(EXIT_FAILURE);
            }
        }
        listen_thread = NULL;
    }

    if (close(socketfd) == -1) {
        perror("close failed");
        exit(EXIT_FAILURE);
    }
    return 0;
}


int input(char *ret_ptr)
{
    int ret_len;
    while (fgets(ret_ptr, BUF_LEN, stdin) == NULL || ret_ptr[0] == '\n') {
        fprintf(stderr, "fgets error");
    }

    ret_len = strlen(ret_ptr);
    if (ret_ptr[ret_len-1] == '\n')
        ret_ptr[--ret_len] = '\0';

    return ret_len;
}


void *listen_func(int _socketfd)
{
    char recv_buf[BUF_LEN];
    int recv_len;
    do {
        if ((recv_len = recv(_socketfd, recv_buf, BUF_LEN, 0)) == -1) {
            fprintf(stderr, "[tid: %ld]recv failed", pthread_self());
            perror(NULL);
            break;
        }
    } while (strncmp(recv_buf, "exit_ack", 8) == 0);

    return NULL;
}