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

void send_request(int fd){
    char send_buf[BUF_SIZE];
	char recv_buf[BUF_SIZE + 1];

	int send_size, recv_size;

    FILE * send_fp = fopen("client-input.dat", "rb");

	printf("[CLIENT] start request\n");

    while(!feof(send_fp)){
        send_size = fread(send_buf, 1, BUF_SIZE, send_fp);

        if(send(fd, send_buf, send_size, 0) < 0){
			perror("[CLIENT] send failed");
			exit(1);
		}
		
		recv_size = recv(fd, recv_buf, sizeof(recv_buf) - 1, 0);
		if(recv_size <= 0){
			perror("[CLIENT] receive response fail");
			exit(1);
		}
    }

	printf("[CLIENT] request complete\n");

    fclose(send_fp);
    
    close(fd);

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

    printf("[CLIENT] connected to server\n");

    send_request(sockfd);

    return NULL;

}
