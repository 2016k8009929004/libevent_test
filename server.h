#include "common.h"

evutil_socket_t server_init(int port, int listen_num);

void accept_cb(int fd, short events, void * arg);
void server_process_cb(int fd, short events, void * arg);
