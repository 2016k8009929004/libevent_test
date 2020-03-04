#ifndef __COMMON_H__
#define __COMMON_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define __USE_GNU
#include <sched.h>
#include <pthread.h>

#include <event.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

//#define BUF_SIZE 256

#define SWAP(a,b) do{a^=b;b^=a;a^=b;}while(0)

struct client_arg {
    char ** ip_addr;
    int port;
    int buf_size;
};

#ifndef MAX_CPUS
#define MAX_CPUS		16
#endif

int core_limit;
pthread_t sv_thread[MAX_CPUS];
int done[MAX_CPUS];

void * client_thread(void * argv);
void * server_thread(void * arg);

int itoa(int n, char s[]);

#endif