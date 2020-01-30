#ifndef __COMMON_H__
#define __COMMON_H__

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

#define SWAP(a,b) do{a^=b;b^=a;a^=b;}while(0)

struct server_node {
    char ** ip_addr;
    int port;
};

int client_thread_num;

pthread_mutex_t fin_client_thread_lock;
int fin_client_thread = 0;

void * client_thread(void * argv);
void * server_thread(void * arg);

int itoa(int n, char s[]);

#endif