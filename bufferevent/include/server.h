#include "common.h"
#include "evallib.h"

//#define __BIND_CORE__
//#define __GET_CORE__

//#define __EVAL_CB__

#define BUF_SIZE 4096

#ifdef __EVAL_CB__
pthread_mutex_t accept_cb_lock;
pthread_mutex_t read_cb_lock;
#endif

struct sock_ev_read {
    int fd;
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

void event_cb(struct bufferevent * bev, short event, void * arg);
