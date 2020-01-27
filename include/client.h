#include "common.h"

int connect_server(char * server_ip, int port);

void send_request(int thread_id, int fd);

void receive_response_cd(int fd, short events, void * arg);

