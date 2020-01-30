#include "common.h"

#define RECEIVE_DEBUG

int sent_byte = 0;

pthread_mutex_t recv_lock;

int recv_byte = 0;

int work_done_flag = 0;

struct response_process_arg {
    struct event * read_ev;
    FILE * fp;
};

pthread_mutex_t tot_work_lock;
int tot_work = 0;

pthread_mutex_t fin_work_lock;
int fin_work = 0;

int connect_server(char * server_ip, int port);

void send_request_thread(int fd);

void * send_request(void * arg);

void receive_response_thread(int fd);

void response_process(int sock, short event, void * arg);
