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

//HiKV library
#include "city.h"
#include "config.h"
#include "pflush.h"
#include "random.h"
#include "pm_alloc.h"
#include "hikv.h"

//#define BUF_SIZE 256

#define SWAP(a,b) do{a^=b;b^=a;a^=b;}while(0)

struct client_arg {
    char ** ip_addr;
    int port;
    int buf_size;
};

#define VALUE_SIZE 256
#define KEY_SIZE 64

// The key-value struct in network connection
struct __attribute__((__packed__)) kv_trans_item {
	uint16_t len;
	uint8_t value[VALUE_SIZE];
	uint8_t key[KEY_SIZE];
};

#define KV_ITEM_SIZE sizeof(struct kv_trans_item)

#ifndef MAX_CPUS
#define MAX_CPUS		16
#endif

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
    struct hikv_arg hikv_thread_arg;
};


static int sequence_rw = 0;

static int core_limit;
static pthread_t sv_thread[MAX_CPUS];
static struct server_arg sv_thread_arg[MAX_CPUS];
static int done[MAX_CPUS];
static int cores[MAX_CPUS];

void * client_thread(void * argv);
void * server_thread(void * arg);

int itoa(int n, char s[]);

#endif