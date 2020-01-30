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

    pthread_mutex_t * send_lock = info->send_lock;

    int * send_byte = info->send_byte;

    char send_buf[BUF_SIZE];
	char recv_buf[BUF_SIZE + 1];

	int send_size, recv_size;

    FILE * send_fp = fopen("client-input.dat", "rb");
//    FILE * recv_fp = fopen("server-ouput.dat", "wb");

//	printf("[CLIENT] start request\n");

    while(!feof(send_fp)){
        send_size = fread(send_buf, 1, BUF_SIZE, send_fp);

        if(send(fd, send_buf, send_size, 0) < 0){
			perror("[CLIENT] send failed");
			exit(1);
		}

        pthread_mutex_lock(send_lock);
        (*send_byte) += send_size;
        pthread_mutex_unlock(send_lock);

/*
		recv_size = recv(fd, recv_buf, sizeof(recv_buf) - 1, 0);
		if(recv_size < 0){
			perror("[CLIENT] receive response fail");
			exit(1);
		}

        fwrite(recv_buf, recv_size, 1, recv_fp);
        fflush(recv_fp);
*/

    }

	printf("[CLIENT %d] request complete, send byte: %d\n", fd, *send_byte);

    fclose(send_fp);
//    fclose(recv_fp);
    
//    close(fd);

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

    pthread_mutex_t * recv_lock = info->recv_lock;
    int * recv_byte = info->recv_byte;
    int * send_byte = info->send_byte;

    char recv_buf[BUF_SIZE + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

    int recv_size = read(sock, recv_buf, BUF_SIZE);

    if(recv_size == 0){
        printf("[CLIENT] close connection\n");
        close(sock);
    }

#ifdef RECEIVE_DEBUG
    fwrite(recv_buf, recv_size, 1, fp);
    fflush(fp);
#endif

    pthread_mutex_lock(recv_lock);
    (*recv_byte) += recv_size;
    pthread_mutex_unlock(recv_lock);
    
    printf("[CLIENT %d] receive reply: %s\n", sock, recv_buf);

    if((*recv_byte) == (*send_byte)){
        printf("[CLIENT %d] receive reply complete, close connection\n", sock);

        pthread_mutex_lock(&fin_client_thread_lock);
        fin_client_thread++;
        pthread_mutex_unlock(&fin_client_thread_lock);

        if(fin_client_thread == client_thread_num){
            work_done_flag = 1;
        }

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
    struct server_node * server = (struct server_node *)argv;
    
    int send_byte, recv_byte;
    send_byte = recv_byte = 0;

    pthread_mutex_t send_lock, recv_lock;
    pthread_mutex_init(&send_lock, NULL);
    pthread_mutex_init(&recv_lock, NULL);

    int sockfd = connect_server(*(server->ip_addr), server->port);
    if(sockfd == -1){
        perror("[CLIENT] tcp connect error");
        exit(1);
    }

    struct send_info * info = (struct send_info *)malloc(SEND_INFO_SIZE);
    info->sockfd = &sockfd;
    info->send_lock = &send_lock;
    info->send_byte = &send_byte;
    info->recv_lock = &recv_lock;
    info->recv_byte = &recv_byte;

    receive_response_thread(info);

    send_request_thread(info);

    while(!work_done_flag);

    return NULL;

}
