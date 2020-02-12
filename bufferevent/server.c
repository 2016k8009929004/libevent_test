#include "server.h"

#define __NR_gettid 186

int request_cnt = 0;

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

    evutil_socket_t * s = (evutil_socket_t *)malloc(sizeof(evutil_socket_t)); 

    if((*s = accept(fd, (struct sockaddr *)&client, &len)) < 0){
        perror("[SERVER] socket accept failed");
        exit(1);
    }

    evutil_make_socket_nonblocking(*s);

    struct server_process_arg * thread_arg = (struct server_process_arg *)malloc(SERVER_PROCESS_ARG_SIZE);
    thread_arg->fd = s;
    thread_arg->base = (struct event_base *)arg;
    thread_arg->byte_sent = (int *)malloc(sizeof(int));
    *(thread_arg->byte_sent) = 0;
    thread_arg->request_cnt = (int *)malloc(sizeof(int));
    *(thread_arg->request_cnt) = 0;
    thread_arg->start = (struct time_record *)malloc(sizeof(struct time_record));
    thread_arg->start->flag = 0;
    
    thread_arg->sequence = connect_cnt;
    connect_cnt++;

    pthread_t thread;
    pthread_create(&thread, NULL, server_process, (void *)thread_arg);
    pthread_detach(thread);
}

void * server_process(void * arg){
    struct server_process_arg * thread_arg = (struct server_process_arg *)arg;
    evutil_socket_t fd = *(thread_arg->fd);
    struct event_base * base = thread_arg->base;
    int sequence = thread_arg->sequence;

    struct bufferevent * bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    
    struct sock_ev_read * read_arg = (struct sock_ev_read *)malloc(sizeof(struct sock_ev_read));
    read_arg->byte_sent = thread_arg->byte_sent;
    read_arg->request_cnt = thread_arg->request_cnt;
    read_arg->start = thread_arg->start;
    
    bufferevent_setcb(bev, read_cb , NULL, NULL, thread_arg);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);
}

void read_cb(struct bufferevent * bev, void * arg){
    struct timeval start;
    gettimeofday(&start,NULL);

    struct sock_ev_read * read_arg = (struct sock_ev_read *)arg;

#ifdef __REAL_TIME_STATS__
    if(!read_arg->start->flag){
        gettimeofday(&read_arg->start->time, NULL);
        read_arg->start->flag = 1;
    }
#endif

    char msg[BUF_SIZE + 1];
    size_t len = bufferevent_read(bev, msg, sizeof(msg));
    msg[len] = '\0';

    (*(read_arg->request_cnt))++;

#ifdef __REAL_TIME_STATS__
#ifdef __BIND_CORE__
        request_end(read_arg->core_sequence, read_arg->start->time, *(read_arg->byte_sent), *(read_arg->request_cnt));
#else
        request_end(0, read_arg->start->time, *(read_arg->byte_sent), *(read_arg->request_cnt));
#endif
#endif

    char reply_msg[BUF_SIZE + 1];
    memcpy(reply_msg, msg, len);
    bufferevent_write(bev, reply_msg, len);
    *(read_arg->byte_sent) += len;
}

void * server_thread(void * arg){
    cpu_set_t core_set;

    CPU_ZERO(&core_set);
    CPU_SET(0, &core_set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

#ifdef __EVAL_CB__
    pthread_mutex_init(&record_lock, NULL);
#endif

    event_init();

    evutil_socket_t sock;
    if((sock = server_init(12345, 100)) < 0){
        perror("[SERVER] server init error");
        exit(1);
    }

    struct event_base * base = event_base_new();

    struct event * ev_listen = event_new(base, sock, EV_READ | EV_PERSIST, accept_cb, base);

    event_add(ev_listen, NULL);

    event_base_dispatch(base);

    return 0;
}