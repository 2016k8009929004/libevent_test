#include "client.h"

int connect_server(const char * server_ip, int port){
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

    evutil_make_socket_nonblocking(sockfd);

    return sockfd;

}

void send_request(int fd){
    char buf[BUF_SIZE];

    FILE * fp = fopen("client-input.dat", "rb");

    while(!feof(fp)){
        int size = fread(buf, 1, BUF_SIZE, fp);
        write(fd, buf, size);
    }

    fclose(fp);
    
    close(fd);

}

void receive_response_cd(int fd, short events, void * arg){
    char buf[BUF_SIZE + 1];

    int len = read(fd, buf, sizeof(buf) - 1);

    if(len <= 0){
        perror("[CLIENT] receive response fail");
        exit(1);
    }

    FILE * fp = *((FILE **)arg);

    fwrite(buf, 1, len, fp);
    fflush(fp);
    
}

void client_thread(int argc, char * argv[]){
    int sockfd = connect_server(argv[1], atoi(argv[2]));

    if(sockfd == -1){
        perror("[CLIENT]tcp connect error");
        return -1;
    }

    printf("[CLIENT] connected to server\n");

    FILE * fp = fopen("server-output.dat", "wb");
    
    struct event_base * base = event_base_new();

    struct event * ev_sockfd = event_new(base, sockfd, EV_READ | EV_PERSIST, receive_response_cd, (void *)&fp);

    event_add(ev_sockfd, NULL);

    event_base_dispatch(base);

    send_request(sockfd);

}