#include "common.h"
#include "evallib.h"

struct sock_ev_read {
    struct event_base * base;
    struct event * read_ev;
};

struct sock_ev_write {
    struct event * write_ev;
    char * buff;
};

pthread_mutex_t send_lock;

static int byte_sent = 0;

evutil_socket_t server_init(int port, int listen_num);

void accept_cb(int fd, short events, void * arg);

void * server_process(void * arg);

void request_process_cb(int fd, short events, void * arg);

void response_process_cb(int fd, short events, void * arg);
