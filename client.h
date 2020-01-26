#include "common.h"

int connect_server(const char * server_ip, int port);

void send_request(int fd);

void receive_response_cd(int fd, short events, void * arg);

