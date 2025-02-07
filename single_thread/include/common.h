#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
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
#define PATH_SIZE 1024

#define __REAL_TIME_STATS__

#define SWAP(a,b) do{a^=b;b^=a;a^=b;}while(0)

struct server_node {
    char ** ip_addr;
    int port;
};

void * client_thread(void * argv);
void * server_thread(void * arg);

int itoa(int n, char s[]);
