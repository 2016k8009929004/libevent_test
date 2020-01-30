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
    int fd = *((int *)arg);

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

        sent_byte += send_size;
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

	printf("[CLIENT] request complete, sent byte: %d\n", sent_byte);

    fclose(send_fp);
//    fclose(recv_fp);
    
//    close(fd);
    while(1);

}

void response_process(int sock, short event, void * arg){
    char recv_buf[BUF_SIZE + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

    int recv_size = read(sock, recv_buf, BUF_SIZE);

    if(recv_size == 0){
        printf("[CLIENT] close connection\n");
        close(sock);
    }

    pthread_mutex_lock(&recv_lock);
    recv_byte += recv_size;
    pthread_mutex_unlock(&recv_lock);
    
    printf("receive reply: %s\n", recv_buf);

    if(recv_byte == sent_byte){
        close(sock);
    }
}

void * create_response_process(void * arg){
    int sock = (int)arg;

    struct event_base * base = event_base_new();

    struct event * read_ev = (struct event *)malloc(sizeof(struct event));

    event_set(read_ev, sock, EV_READ|EV_PERSIST, response_process, read_ev);

    event_base_set(base, read_ev);

    event_add(read_ev, NULL);

    event_base_dispatch(base);
}

void receive_response_thread(int fd){
    pthread_t thread;
    pthread_create(&thread, NULL, create_response_process, (void *)&fd);
    pthread_detach(thread);
}

void send_request_thread(int fd){
    pthread_t thread;
    pthread_create(&thread, NULL, send_request, (void *)&fd);
    pthread_detach(thread);
}

void * client_thread(void * argv){
    struct server_node * server = (struct server_node *)argv;
//    char ** arg = (char **)argv;

//    printf("[CLIENT] server ip: %s, server port: %d\n", arg[2], atoi(arg[3]));

    int sockfd = connect_server(*(server->ip_addr), server->port);
//    int sockfd = connect_server(arg[2], atoi(arg[3]));
    if(sockfd == -1){
        perror("[CLIENT] tcp connect error");
        exit(1);
    }

    pthread_mutex_init(&recv_lock, NULL);

//    printf("[CLIENT] connected to server\n");

    receive_response_thread(sockfd);

    send_request_thread(sockfd);

//    send_request(sockfd);

    return NULL;

}
