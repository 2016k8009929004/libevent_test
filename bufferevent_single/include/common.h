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

//libevent library
#include <event.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "sizes.h"

//#define BUF_SIZE 256

#define LL long long

#define REPLY_SIZE 50

#define NUM_ITER 1000000000

#define PUT_PERCENT 50			// Percentage of PUT operations

#define NUM_KEYS M_1
#define NUM_KEYS_ M_1_

#define SWAP(a,b) do{a^=b;b^=a;a^=b;}while(0)

#define VALUE_SIZE 256
#define KEY_SIZE 64

//#define INDEPENDENT_KEY
//#define BATCHED_KEY_256B
#define BATCHED_KEY_1KB

// The key-value struct in network connection
struct __attribute__((__packed__)) kv_trans_item {
//	uint16_t len;
    uint8_t key[KEY_SIZE];
	uint8_t value[VALUE_SIZE];
};

#define KV_ITEM_SIZE sizeof(struct kv_trans_item)

struct hikv_arg {
    size_t pm_size;
    uint64_t num_server_thread;
    uint64_t num_backend_thread;
    uint64_t num_warm_kv;
    uint64_t num_put_kv;
    uint64_t num_get_kv;
    uint64_t num_delete_kv;
    uint64_t num_scan_kv;
    uint64_t scan_range;
    uint64_t seed;
    uint64_t scan_all;
};

#define HIKV_ARG_SIZE sizeof(struct hikv_arg)

struct server_arg {
    int core;
    int thread_id;
    struct hikv * hi;
    struct hikv_arg hikv_thread_arg;
};

struct client_arg {
    int thread_id;
    char * ip_addr;
    int port;
//    int buf_size;
    struct hikv_arg hikv_thread_arg;
};

#ifndef MAX_CPUS
#define MAX_CPUS		16
#endif

static int sequence_rw = 0;

static int core_limit;

static pthread_t sv_thread[MAX_CPUS];
static struct server_arg sv_thread_arg[MAX_CPUS];

static pthread_t cl_thread[200];
static struct client_arg cl_thread_arg[200];

static int done[MAX_CPUS];
static int cores[MAX_CPUS];

static LL str_to_ll(char * buff, int size){
    int i;
    LL temp;
    for(i = 0, temp = 0;(i < size) && (buff[i] >= '0') && (buff[i] <= '9');i++){
        temp = (temp * 10) + (buff[i] - '0');
    }

    return temp;
}

void * client_thread(void * argv);
void * server_thread(void * arg);

#endif