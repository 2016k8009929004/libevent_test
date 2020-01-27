#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <pthread.h>

#include <event.h>
#include <event2/util.h>

#define BUF_SIZE 1024

struct server_node {
    char * ip_addr;
    int port;
};

struct client_arg {
    int client_thread_id;
    struct server_node server;
};

void client_thread(void * arg);
void server_thread(void * arg);
