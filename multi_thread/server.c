#include "server.h"

#define __NR_gettid 186

int connect_cnt = 0;

//extern int byte_sent;

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
//    printf("------enter accept_cb function------\n");
    
//    printf("[accept_cb] pid: %d, tid: %ld, self: %ld\n", getpid(), (long int)syscall(__NR_gettid), pthread_self());

#ifdef __EVAL_PTHREAD__
    struct timeval start, accept_time, end;

    FILE * fp = fopen("accept_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);

    gettimeofday(&start, NULL);
#endif

    struct sockaddr_in client;
    socklen_t len = sizeof(client);

//    evutil_socket_t s;

    evutil_socket_t * s = (evutil_socket_t *)malloc(sizeof(evutil_socket_t)); 

    if((*s = accept(fd, (struct sockaddr *)&client, &len)) < 0){
        perror("[SERVER] socket accept failed");
        exit(1);
    }

    evutil_make_socket_nonblocking(*s);

#ifdef __EVAL_PTHREAD__
    gettimeofday(&accept_time, NULL);
#endif

//    connect_cnt++;
//    log(INFO, "connect count: %d, fd: %d", connect_cnt, *s);

    pthread_t thread;
    pthread_create(&thread, NULL, server_process, (void *)s);
    pthread_detach(thread);
#ifdef __EVAL_PTHREAD__
    gettimeofday(&end, NULL);

    char buff[1024];

//    long accept_elapsed_time, total_elapsed_time;
    long pthread_elapsed_time;
/*    
    if(start.tv_usec > end.tv_usec){
        end.tv_usec += 1000000;
        end.tv_sec -= 1;
    }

    if(start.tv_usec > accept_time.tv_usec){
        accept_time.tv_usec += 1000000;
        accept_time.tv_sec -= 1;
    }
*/

    if(accept_time.tv_usec > end.tv_usec){
        end.tv_usec += 1000000;
        end.tv_sec -= 1;
    }
/*
    accept_elapsed_time = end_time.tv_usec - accept_time.tv_usec;

    total_elapsed_time = end.tv_usec - start.tv_usec;
*/

    pthread_elapsed_time = end.tv_usec - accept_time.tv_usec;

//    sprintf(buff, "accept_time:%ld, total_elapsed_time:%ld\n", accept_elapsed_time, total_elapsed_time);

    sprintf(buff, "pthread_create_time %ld\n", pthread_elapsed_time);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif
//    printf("------leave accept_cb function------\n");
}

void request_process_cb(int fd, short events, void * arg){
//    printf("[request_process_cb] pid: %d, tid: %ld, self: %ld\n", getpid(), (long int)syscall(__NR_gettid), pthread_self());

#ifdef __EVAL_CB__
    struct timeval start, end;

    FILE * fp = fopen("request_process_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);

    gettimeofday(&start, NULL);
#endif

#ifdef REAL_TIME_STATS
    pthread_mutex_lock(&record_lock);
    request_start();
    pthread_mutex_unlock(&record_lock);
#endif
    struct sock_ev_read * read_arg = (struct sock_ev_read *)arg;

    char * msg = (char *)malloc(BUF_SIZE + 1);

	int len = read(fd, msg, sizeof(msg) - 1);

    if(len <= 0){
        pthread_mutex_lock(&request_end_lock);
#ifdef REAL_TIME_STATS
        pthread_mutex_lock(&send_lock);
        pthread_mutex_lock(&record_lock);
        request_end(byte_sent);
        pthread_mutex_unlock(&record_lock);
        pthread_mutex_unlock(&send_lock);
#endif
        pthread_mutex_unlock(&request_end_lock);

//		printf("[SERVER sock: %d] close connection\n", fd);

        event_del(read_arg->read_ev);
        close(fd);

        return;
    }

    msg[len] = '\0';

    struct event * write_ev = (struct event *)malloc(sizeof(struct event));

    struct sock_ev_write * write_arg = (struct sock_ev_write *)malloc(sizeof(struct sock_ev_write));
    write_arg->write_ev = write_ev;
    write_arg->buff = msg;

    event_set(write_ev, fd, EV_WRITE, response_process_cb, write_arg);

    event_base_set(read_arg->base, write_ev);

    event_add(write_ev, NULL);
#ifdef __EVAL_CB__
    gettimeofday(&end, NULL);

    char buff[1024];

    long elapsed_time;
    
    if(start.tv_usec > end.tv_usec){
        end.tv_usec += 1000000;
        end.tv_sec -= 1;
    }

    elapsed_time = end.tv_usec - start.tv_usec;

    sprintf(buff, "elapsed_time:%ld\n", elapsed_time);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif
}

void response_process_cb(int fd, short events, void * arg){
//    printf("[response_process_cb] pid: %d, tid: %ld, self: %ld\n", getpid(), (long int)syscall(__NR_gettid), pthread_self());

#ifdef __EVAL_CB__
    struct timeval start, end;

    FILE * fp = fopen("response_process_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);

    gettimeofday(&start, NULL);
#endif
    struct sock_ev_write * write_arg = (struct sock_ev_write *)arg;

    char * msg = write_arg->buff;

    char * reply_msg = (char *)malloc(BUF_SIZE + 1);
    strcpy(reply_msg, msg);

    int send_byte_cnt = write(fd, reply_msg, strlen(reply_msg));

    pthread_mutex_lock(&send_lock);
    byte_sent += send_byte_cnt;
    pthread_mutex_unlock(&send_lock);
#ifdef __EVAL_CB__
    gettimeofday(&end, NULL);

    char buff[1024];

    long elapsed_time;
    
    if(start.tv_usec > end.tv_usec){
        end.tv_usec += 1000000;
        end.tv_sec -= 1;
    }

    elapsed_time = end.tv_usec - start.tv_usec;

    sprintf(buff, "elapsed_time:%ld\n", elapsed_time);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif
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
//    printf("[server_thread] pid: %d, tid: %ld, self: %ld\n", getpid(), (long int)syscall(__NR_gettid), pthread_self());

    cpu_set_t core_set, get_core;

    CPU_ZERO(&core_set);
    CPU_SET(0, &core_set);

    if (sched_setaffinity(0, sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

    CPU_ZERO(&get_core);

    if (sched_getaffinity(0, sizeof(get_core), &get_core) == -1){
        printf("warning: cound not get thread affinity, continuing...\n");
    }
/*
    int i;
    int num = sysconf(_SC_NPROCESSORS_CONF);
    for(i = 0;i < num;i++){
        if(CPU_ISSET(i, &get_core)){
            printf("this thread %d is running processor : %d\n", i,i);
        }
    }
*/
    evutil_socket_t sock;
    if((sock = server_init(12345, 100)) < 0){
        perror("[SERVER] server init error");
        exit(1);
    }

    pthread_mutex_init(&request_end_lock, NULL);
    pthread_mutex_init(&send_lock, NULL);

#ifdef REAL_TIME_STATS
    pthread_mutex_init(&record_lock, NULL);
#endif

    struct event_base * base = event_base_new();

    struct event * ev_listen = event_new(base, sock, EV_READ | EV_PERSIST, accept_cb, base);

    event_add(ev_listen, NULL);

    event_base_dispatch(base);

    return NULL;
}