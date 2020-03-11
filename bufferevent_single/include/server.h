#include "common.h"
#include "evallib.h"

//#define __BIND_CORE__
//#define __GET_CORE__

//#define __EVAL_CB__
//#define __EVAL_READ__

//#define __REAL_TIME_STATS__

#ifdef __REAL_TIME_STATS__
pthread_mutex_t record_lock;
int request_cnt;
int byte_sent;

pthread_mutex_t start_lock;
struct timeval g_start;
int start_flag;

pthread_mutex_t end_lock;
struct timeval g_end;
#endif

#define BUF_SIZE 4096

#ifdef __EVAL_CB__
pthread_mutex_t accept_cb_lock;
#endif

#ifdef __EVAL_READ__
pthread_mutex_t read_cb_lock;
int request_cnt;
int total_time;
#endif

struct sock_ev_read {
    int fd;
    
    int byte_sent;
    int request_cnt;

    int total_time;

    struct timeval start;
    int start_flag;

    int thread_id;
    struct hikv * hi;
};

struct accept_args {
    int thread_id;
    struct event_base * base;
    struct hikv * hi;
};

pthread_mutex_t connect_lock;
int connect_cnt = 0;

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

struct test_args {
    bool seq;
    int test_type;
    uint64_t get_num_kv;
    uint64_t get_seed;
    uint64_t get_sequence_id;
    uint64_t put_num_kv;
    uint64_t put_seed;
    uint64_t put_sequence_id;
    uint64_t delete_num_kv;
    uint64_t delete_seed;
    uint64_t delete_sequence_id;
    uint64_t scan_num_kv;
    uint64_t scan_seed;
    uint64_t scan_range;
    uint64_t scan_sequence_id;
    uint64_t scan_all;
    uint64_t seed_range;
};

#define TEST_ARGS_SIZE sizeof(struct test_args)

struct test_thread_args {
    void * obj;
    int thread_id;
    struct test_args test_args;
//    struct test_result test_result;
};

#define TEST_THREAD_ARGS_SIZE sizeof(struct test_thread_args)

struct test_thread_args * args[32];

#define SERVER_ARG_SIZE sizeof(struct server_arg)

evutil_socket_t server_init(int port, int listen_num);

void accept_cb(int fd, short events, void * arg);

void read_cb(struct bufferevent * bev, void * arg);

void event_cb(struct bufferevent * bev, short event, void * arg);
