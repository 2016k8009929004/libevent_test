#include "common.h"

int connect_server(char * server_ip, int port);

void send_request(int fd);

void receive_response_thread(int sockfd);

void response_process(int sock, short event, void * arg)；
