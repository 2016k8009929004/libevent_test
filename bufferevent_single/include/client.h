#include "common.h"

//#define RECEIVE_DEBUG

//#define __EV_RTT__

//#define __TEST_FILE__
#define __TEST_KV__

int buf_size;

//pthread_mutex_t work_done_lock;
int work_done_flag = 0;

int client_thread_num;

//pthread_mutex_t fin_client_thread_lock;
int fin_client_thread = 0;

#ifdef __EV_RTT__
pthread_mutex_t rtt_lock;
#endif

struct client_arg {
    char ** ip_addr;
    int port;
//    int buf_size;
};

struct debug_response_arg {
    struct event * read_ev;
    struct send_info * info;
    FILE * fp;
};

struct response_arg {
    struct event * read_ev;
    struct send_info * info;
};

#define RESPONSE_ARG_SIZE sizeof(struct response_arg)

struct send_info {
    int * sockfd;
//    pthread_mutex_t * send_lock;
    int * send_byte;
//    pthread_mutex_t * recv_lock;
    int * recv_byte;
};

#define SEND_INFO_SIZE sizeof(struct send_info)

int connect_server(char * server_ip, int port);

void send_request_thread(struct send_info * info);

void * send_request(void * arg);

void receive_response_thread(struct send_info * info);

void response_process(int sock, short event, void * arg);

#define LL long long

#define NUM_ITER 1000000000

#define PUT_PERCENT 5			// Percentage of PUT operations

#define M_1 1048576
#define M_1_ 1048575

#define NUM_KEYS M_1			// 51 * M_4 ~ 200 M keys
#define NUM_KEYS_ M_1_

LL * key_corpus;
uint8_t * value_corpus;

void gen_corpus(LL * key_corpus, uint8_t * value_corpus);
