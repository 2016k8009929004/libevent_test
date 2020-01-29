#include "server.h"

evutil_socket_t server_init(int port, int listen_num){
    evutil_socket_t sock;
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("[SERVER] create socket failed");
		return -1;
    }

    evutil_make_listen_socket_reuseable(sock);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);

    if(bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0){
        perror("[SERVER] bind socket to port failed");
        return -1;
    }

    if(listen(sock, listen_num) < 0){
        perror("[SERVER] socket listen failed");
        return -1;
    }

    evutil_make_socket_nonblocking(sock);

    return sock;
}

void accept_cb(int fd, short events, void * arg){
    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    evutil_socket_t s;

    if((s = accept(fd, (struct sockaddr *)&client, &len)) < 0){
        perror("[SERVER] socket accept failed");
        exit(1);
    }

    evutil_make_socket_nonblocking(s);

    pthread_t thread;
    pthread_create(&thread, NULL, server_process, (void *)&s);
    pthread_detach(thread);
}

void close_read_event(struct sock_ev_read * arg){
    event_del(arg->read_ev);
    event_base_loopexit(arg->base, NULL);
    
    if(arg->read_ev != NULL){
        free(arg->read_ev);
    }

    event_base_free(arg->base);
    free(arg);
}

void request_process_cb(int fd, short events, void * arg){
#ifdef REAL_TIME_STATS
    request_start();
#endif
    struct sock_ev_read * read_arg = (struct sock_ev_read *)arg;

    char msg[BUF_SIZE + 1];

	int len = recv(fd, msg, sizeof(msg) - 1, 0);

    if(len <= 0){
		printf("[SERVER] close connection\n");
//        close_read_event(read_arg);
        event_del(read_arg->read_ev);
#ifdef REAL_TIME_STATS
        request_end(&elapsed_time);
#endif
        close(fd);
        return;
    }

    msg[len] = '\0';
    
    char reply_msg[BUF_SIZE + 1];
    strcpy(reply_msg, msg);

//	printf("[SERVER] send response\n");

    int send_byte_cnt = send(fd, reply_msg, strlen(reply_msg), 0);

    byte_sent += send_byte_cnt;
}

void * server_process(void * arg){
    evutil_socket_t fd = *((evutil_socket_t *)arg);

    struct event_base * base = event_base_new();
    struct event * read_ev = (struct event *)malloc(sizeof(struct event));

    struct sock_ev_read * read_arg = (struct sock_ev_read *)malloc(sizeof(struct sock_ev_read));
    read_arg->base = base;
    read_arg->read_ev = read_ev;

    event_set(read_ev, fd, EV_READ | EV_PERSIST, request_process_cb, read_arg);

    event_base_set(base, read_ev);

    event_add(read_ev, NULL);

    event_base_dispatch(base);

    return NULL;
}

void * server_thread(void * arg){
    evutil_socket_t sock;
    if((sock = server_init(12345, 100)) < 0){
        perror("[SERVER] server init error");
        exit(1);
    }

    struct event_base * base = event_base_new();

    struct event * ev_listen = event_new(base, sock, EV_READ | EV_PERSIST, accept_cb, base);

    event_add(ev_listen, NULL);

    event_base_dispatch(base);

    return NULL;
}