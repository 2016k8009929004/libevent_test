#include "client.h"

int connect_server(char * server_ip, int port){
    int sockfd;
    
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        perror("[CLIENT] create socket failed");
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("[CLIENT] connect server failed");
        return -1;
    }

//    evutil_make_socket_nonblocking(sockfd);

    return sockfd;

}

void * send_request(void * arg){
    struct send_info * info = (struct send_info *)arg;

    int fd = *(info->sockfd);

    char send_buf[buf_size];
    char recv_buf[buf_size + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

	int send_size, recv_size;
    int total_time, request_cnt;

    total_time = request_cnt = 0;

    FILE * send_fp = fopen("client-input.dat", "rb");
#ifdef RECEIVE_DEBUG
    FILE * recv_fp = fopen("server-ouput.dat", "wb");
#endif

#ifdef __EV_RTT__
    FILE * fp = fopen("rtt.txt", "a+");
    fseek(fp, 0, SEEK_END);
#endif

    struct timeval time1;
    gettimeofday(&time1, NULL);

    while(!feof(send_fp)){

        struct timeval start;
        gettimeofday(&start, NULL);
  
//send request
        send_size = fread(send_buf, 1, buf_size, send_fp);

        if(write(fd, send_buf, send_size) < 0){
			perror("[CLIENT] send failed");
	    	exit(1);
    	}

//receive reply
        int temp = 0;
        while(1){
            recv_size = read(fd, recv_buf, buf_size);

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

#ifdef RECEIVE_DEBUG
            fwrite(recv_buf, recv_size, 1, recv_fp);
            fflush(recv_fp);
#endif
            temp += recv_size;

            if(temp == send_size){
                break;
            }
        }

        struct timeval end;
        gettimeofday(&end, NULL);

#ifdef __EV_RTT__
        request_cnt++;
	    total_time += (int)(end_time - start_time);
#endif

        if(end.tv_sec - time1.tv_sec > 10){
            printf("[CLIENT] request complete\n");
            break;
        }
    }

#ifdef __EV_RTT__
    struct timeval time2;
    gettimeofday(&time2, NULL);

    double start_time = (double)time1.tv_sec * 1000000 + (double)time1.tv_usec;
    double end_time = (double)time2.tv_sec * 1000000 + (double)time2.tv_usec;

    char buff[1024];

    sprintf(buff, "rtt %.4f\n", ((end_time - start_time)/request_cnt));
        
    pthread_mutex_lock(&rtt_lock);

    fwrite(buff, strlen(buff), 1, fp);
    fclose(fp);

    pthread_mutex_unlock(&rtt_lock);
#endif

    fclose(send_fp);
    
    return NULL;
}

void response_process(int sock, short event, void * arg){
#ifdef RECEIVE_DEBUG
    struct debug_response_arg * debug_arg = (struct debug_response_arg *)arg;

    struct event * read_ev = debug_arg->read_ev;
    struct send_info * info = debug_arg->info;
    FILE * fp = debug_arg->fp;
#else
    struct response_arg * response_process_arg = (struct response_arg *)arg;

    struct event * read_ev = response_process_arg->read_ev;
    struct send_info * info = response_process_arg->info;
#endif

//    pthread_mutex_t * recv_lock = info->recv_lock;
    int * recv_byte = info->recv_byte;
    int * send_byte = info->send_byte;

    char recv_buf[buf_size + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

    int recv_size = read(sock, recv_buf, buf_size);

    if(recv_size == 0){
        printf("[CLIENT] close connection\n");
        close(sock);
    }

#ifdef RECEIVE_DEBUG
    fwrite(recv_buf, recv_size, 1, fp);
    fflush(fp);
#endif

//    pthread_mutex_lock(recv_lock);
    (*recv_byte) += recv_size;
//    pthread_mutex_unlock(recv_lock);
    
//    printf("[CLIENT %d] receive reply: %s\n", sock, recv_buf);

    if((*recv_byte) == (*send_byte)){
        printf("[CLIENT %d] receive reply complete, close connection\n", sock);

        work_done_flag = 1;

        event_del(read_ev);
#ifdef RECEIVE_DEBUG
        fclose(fp);
#endif
        close(sock);
    }
}

void * create_response_process(void * arg){
    struct send_info * info = (struct send_info *)arg;

    int fd = *(info->sockfd);

    struct event_base * base = event_base_new();

    struct event * read_ev = (struct event *)malloc(sizeof(struct event));

#ifdef RECEIVE_DEBUG
    FILE * recv_fp = fopen("server-ouput.dat", "wb");

    struct debug_response_arg * debug_arg = (struct debug_response_arg *)malloc(sizeof(struct debug_response_arg));
    debug_arg->read_ev = read_ev;
    debug_arg->info = info;
    debug_arg->fp = recv_fp;

    event_set(read_ev, fd, EV_READ|EV_PERSIST, response_process, debug_arg);
#else
    struct response_arg * response_process_arg = (struct response_arg *)malloc(RESPONSE_ARG_SIZE);
    response_process_arg->read_ev = read_ev;
    response_process_arg->info = info;

    event_set(read_ev, fd, EV_READ|EV_PERSIST, response_process, response_process_arg);
#endif

    event_base_set(base, read_ev);

    event_add(read_ev, NULL);

    event_base_dispatch(base);
}

void receive_response_thread(struct send_info * info){
    pthread_t thread;
    pthread_create(&thread, NULL, create_response_process, (void *)info);
    pthread_detach(thread);
}

void send_request_thread(struct send_info * info){
    pthread_t thread;
    pthread_create(&thread, NULL, send_request, (void *)info);
    pthread_detach(thread);
}

void * client_thread(void * argv){
#if 0
    struct client_arg * server = (struct client_arg *)argv;

    buf_size = server->buf_size;
    
    int send_byte, recv_byte;
    send_byte = recv_byte = 0;
/*
    pthread_mutex_t send_lock, recv_lock;
    pthread_mutex_init(&send_lock, NULL);
    pthread_mutex_init(&recv_lock, NULL);
    
    pthread_mutex_init(&work_done_lock, NULL);
*/
    int sockfd = connect_server(*(server->ip_addr), server->port);
    if(sockfd == -1){
        perror("[CLIENT] tcp connect error");
        exit(1);
    }

    struct send_info * info = (struct send_info *)malloc(SEND_INFO_SIZE);
    info->sockfd = &sockfd;
//    info->send_lock = &send_lock;
    info->send_byte = &send_byte;
//    info->recv_lock = &recv_lock;
    info->recv_byte = &recv_byte;

    receive_response_thread(info);

    send_request_thread(info);

    while(!work_done_flag);

    free(info);

    return NULL;
#endif

    struct client_arg * server = (struct client_arg *)argv;

    buf_size = server->buf_size;
    
    int send_byte, recv_byte;
    send_byte = recv_byte = 0;

#ifdef __EV_RTT__
    pthread_mutex_init(&rtt_lock, NULL);
#endif

    int sockfd = connect_server(*(server->ip_addr), server->port);
    if(sockfd == -1){
        perror("[CLIENT] tcp connect error");
        exit(1);
    }

    struct send_info * info = (struct send_info *)malloc(SEND_INFO_SIZE);
    info->sockfd = &sockfd;
    info->send_byte = &send_byte;
    info->recv_byte = &recv_byte;

    send_request(info);

//    while(!work_done_flag);

    free(info);

    return NULL;
}
