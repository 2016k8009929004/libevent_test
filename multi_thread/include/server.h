#include "common.h"
#include "evallib.h"

#ifdef __REAL_TIME_STATS__
pthread_mutex_t record_lock;
#endif

#define __BIND_CORE__
//#define __GET_CORE__

#define __EVAL_CB__
//#define __EVAL_PTHREAD__

#define BUF_SIZE 1024

struct sock_ev_read {
    struct event_base * base;
    struct event * read_ev;
};

struct sock_ev_write {
    struct event * write_ev;
    char * buff;
};

pthread_mutex_t connect_cnt_lock;
int connect_cnt = 0;

struct server_process_arg {
    evutil_socket_t * fd;
    int sequence;
};

#define SERVER_PROCESS_ARG_SIZE sizeof(struct server_process_arg)

pthread_mutex_t send_lock;

static int byte_sent = 0;

pthread_mutex_t request_lock;

evutil_socket_t server_init(int port, int listen_num);

void accept_cb(int fd, short events, void * arg);

void * server_process(void * arg);

void request_process_cb(int fd, short events, void * arg);

void response_process_cb(int fd, short events, void * arg);
