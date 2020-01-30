#include "common.h"

int sent_byte = 0;

pthread_mutex_t recv_lock;

int recv_byte = 0;

int connect_server(char * server_ip, int port);

void send_request_thread(int fd);

void * send_request(void * arg);

void receive_response_thread(int fd);

void response_process(int sock, short event, void * arg);
