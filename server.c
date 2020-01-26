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

    struct event_base * base = (struct event_base *)arg;

    struct event * ev = (struct event *)malloc(sizeof(struct event));
    
    if(event_assign(ev, base, s, EV_READ | EV_PERSIST, server_process_cb, (void *)ev) < 0){
        perror("[SERVER] event assign server_process_cb failed");
        exit(1);
    }

    event_add(ev, NULL);
}

void server_process_cb(int fd, short events, void * arg){
    char msg[4096];
    struct event * ev = (struct event *)arg;
    int len = read(fd, msg, sizeof(msg) - 1);

    if(len <= 0){
        event_free(ev);
        close(fd);
        return;
    }

    msg[len] = '\0';
    
    char reply_msg[4096];
    strcpy(reply_msg, msg);

    write(fd, reply_msg, strlen(reply_msg));
}

void server_thread(int argc, char * argv[]){
    evutil_socket_t sock;
    if((sock = server_init(12345, 100)) < 0){
        perror("[SERVER] server init error");
        exit(1);
    }

    struct event_base * base = event_base_new();

    struct event * ev_listen = event_new(base, sock, EV_READ | EV_PERSIST, accept_cb, base);

    event_add(ev_listen, NULL);

    event_base_dispatch(base);

}