#include "common.h"

struct sock_ev_read {
    struct event_base * base;
    struct event * read_ev;
};

evutil_socket_t server_init(int port, int listen_num);

void accept_cb(int fd, short events, void * arg);

void * server_process(void * arg);

void request_process_cb(int fd, short events, void * arg);

void close_read_event(struct sock_ev_read * arg);
