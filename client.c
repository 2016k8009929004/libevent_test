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

void send_request(int thread_id, int fd){
    char send_buf[BUF_SIZE];
	char recv_buf[BUF_SIZE + 1];

	int send_size, recv_size;

    char send_path[PATH_SIZE], recv_path[PATH_SIZE];

    memset(send_path, 0, sizeof(send_path));
    itoa(thread_id, send_path);

    strcat(send_path, "/");

    memcpy(recv_path, send_path, sizeof(send_path));

    char * send_file = "client-input.dat";
    char * recv_file = "server-output.dat";

    strcat(send_path, send_file);
    strcat(recv_path, recv_file);

    FILE * send_fp = fopen(send_path, "rb");
	FILE * recv_fp = fopen(recv_path, "wb");

    printf("[CLIENT] thread id: %d, send file path: %s, recv file path: %s\n", thread_id, send_path, recv_path);

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

		fwrite(recv_buf, 1, recv_size, recv_fp);
		fflush(recv_fp);
    }

	printf("[CLIENT] request complete\n");

    fclose(send_fp);
	fclose(recv_fp);
    
    close(fd);

}

void * client_thread(void * arg){
    int thread_id = ((struct client_arg *)arg)->client_thread_id;
    struct server_node * server = &(((struct client_arg *)arg)->server);

    int sockfd = connect_server(*(server->ip_addr), server->port);
    if(sockfd == -1){
        perror("[CLIENT] tcp connect error");
        exit(1);
    }

    printf("[CLIENT] connected to server\n");

    send_request(thread_id, sockfd);

    return NULL;

}
