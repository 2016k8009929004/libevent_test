#ifndef __COMMON_H__
#define __COMMON_H__

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

#include "log.h"

#define BUF_SIZE 1024
#define REAL_TIME_STATS

#define SWAP(a,b) do{a^=b;b^=a;a^=b;}while(0)

struct server_node {
    char ** ip_addr;
    int port;
};

void * client_thread(void * argv);
void * server_thread(void * arg);

int itoa(int n, char s[]);

//------performance evaluation------
struct timeval start, end, time1, time2;

int start_flag = 0;

int handle_request_cnt = 0;

int byte_sent = 0;

float elapsed_time;

struct timezone tz;

void request_start();
void request_end();

#endif