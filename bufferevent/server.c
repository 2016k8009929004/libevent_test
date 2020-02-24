#include "server.h"

#define __NR_gettid 186

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
#ifdef __EVAL_CB__
    struct timeval start;
    gettimeofday(&start, NULL);
#endif
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
    thread_arg->sequence = connect_cnt;

    pthread_mutex_lock(&connect_lock);
    connect_cnt++;
    pthread_mutex_unlock(&connect_lock);

    pthread_t thread;

#ifdef __EVAL_CB__
    struct timeval pthread_start, pthread_end;
    gettimeofday(&pthread_start, NULL);
#endif

    pthread_create(&thread, NULL, server_process, (void *)thread_arg);

#ifdef __EVAL_CB__
    gettimeofday(&pthread_end, NULL);
#endif

    pthread_detach(thread);

#ifdef __EVAL_CB__
    struct timeval end;
    gettimeofday(&end, NULL);

    double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
    double pthread_start_time = (double)pthread_start.tv_sec * 1000000 + (double)pthread_start.tv_usec;
    double pthread_end_time = (double)pthread_end.tv_sec * 1000000 + (double)pthread_end.tv_usec;
    double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

    char buff[100];
    
    sprintf(buff, "total_time %d pthread_create_time %d\n", 
            (int)(end_time - start_time), (int)(pthread_end_time - pthread_start_time));

    pthread_mutex_lock(&accept_cb_lock);
    FILE * fp = fopen("accept_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);
    
    fwrite(buff, strlen(buff), 1, fp);
    fclose(fp);
    pthread_mutex_unlock(&accept_cb_lock);
    
#endif
}

void * server_process(void * arg){
    struct server_process_arg * thread_arg = (struct server_process_arg *)arg;
    evutil_socket_t fd = *(thread_arg->fd);
    struct event_base * base = thread_arg->base;
    int sequence = thread_arg->sequence;

#ifdef __BIND_CORE__
    int core_sequence = (sequence % 46) + 1;

    cpu_set_t core_set;

    CPU_ZERO(&core_set);
    CPU_SET(core_sequence, &core_set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }
#endif

    struct bufferevent * bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    
    struct sock_ev_read * read_arg = (struct sock_ev_read *)malloc(sizeof(struct sock_ev_read));
    
    read_arg->fd = fd;

    read_arg->byte_sent = (int *)malloc(sizeof(int));
    *(read_arg->byte_sent) = 0;

    read_arg->request_cnt = (int *)malloc(sizeof(int));
    *(read_arg->request_cnt) = 0;

    read_arg->start = (struct time_record *)malloc(sizeof(struct time_record));
    read_arg->start->flag = 0;  

    bufferevent_setcb(bev, read_cb , NULL, NULL, read_arg);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);

}

void read_cb(struct bufferevent * bev, void * arg){
#ifdef __EVAL_CB__
    struct timeval start;
    gettimeofday(&start, NULL);
#endif

#ifdef __REAL_TIME_STATS__
    if(!read_arg->start->flag){
        gettimeofday(&read_arg->start->time, NULL);
        read_arg->start->flag = 1;
    }
#endif

    struct sock_ev_read * read_arg = (struct sock_ev_read *)arg;

    char msg[BUF_SIZE + 1];
    size_t len = bufferevent_read(bev, msg, sizeof(msg));
    msg[len] = '\0';

//    printf("[SERVER] recv len: %d\n", len);

#ifdef __REAL_TIME_STATS__
    (*(read_arg->request_cnt))++;
#endif

//    char reply_msg[BUF_SIZE + 1];
//    memcpy(reply_msg, msg, len);
    bufferevent_write(bev, msg, len);

#ifdef __REAL_TIME_STATS__
    *(read_arg->byte_sent) += len;
    request_end(read_arg->fd, read_arg->start->time, *(read_arg->byte_sent), *(read_arg->request_cnt));
#endif

#ifdef __EVAL_CB__
    struct timeval end;
    gettimeofday(&end, NULL);

    double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
    double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

    char buff[100];
    
    sprintf(buff, "total_time %d\n", (int)(end_time - start_time));

    pthread_mutex_lock(&read_cb_lock);
    FILE * fp = fopen("read_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);
    
    fwrite(buff, strlen(buff), 1, fp);
    fclose(fp);
    pthread_mutex_unlock(&read_cb_lock);
#endif
}

void * server_thread(void * arg){
    cpu_set_t core_set;

    CPU_ZERO(&core_set);
    CPU_SET(0, &core_set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

#ifdef __EVAL_CB__
    pthread_mutex_init(&accept_cb_lock, NULL);
    pthread_mutex_init(&read_cb_lock, NULL);
#endif

    event_init();

    pthread_mutex_init(&connect_lock, NULL);

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