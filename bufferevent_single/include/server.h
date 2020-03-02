#include "common.h"
#include "evallib.h"

//#define __BIND_CORE__
//#define __GET_CORE__

//#define __EVAL_CB__
##define __EVAL_READ__

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
};

pthread_mutex_t connect_lock;
int connect_cnt = 0;

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
