#include "common.h"
#include "evallib.h"

#define __BIND_CORE__
//#define __GET_CORE__

#define __EVAL_CB__
#define __EVAL_PTHREAD__

#define BUF_SIZE 4096

#ifdef __EVAL_CB__
pthread_mutex_t record_lock;
#endif

struct sock_ev_read {
#ifdef __BIND_CORE__
    int core_sequence;
#endif
    int * byte_sent;
    int * request_cnt;
    struct time_record * start;
};

pthread_mutex_t connect_lock;
int connect_cnt = 0;

struct time_record {
    struct timeval time;
    int flag;
};

struct server_process_arg {
    evutil_socket_t * fd;
    struct event_base * base;
    int sequence;
};

#define SERVER_PROCESS_ARG_SIZE sizeof(struct server_process_arg)

evutil_socket_t server_init(int port, int listen_num);

void accept_cb(int fd, short events, void * arg);

void * server_process(void * arg);

void read_cb(struct bufferevent * bev, void * arg);
