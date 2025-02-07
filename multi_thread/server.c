#include "server.h"

#define __NR_gettid 186

int request_cnt = 0;

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

#ifdef __GET_CORE__
    cpu_set_t get_core;

    CPU_ZERO(&get_core);

    if (sched_getaffinity(0, sizeof(get_core), &get_core) == -1){
        printf("warning: cound not get thread affinity, continuing...\n");
    }

    int i, num, run_core;
    num = sysconf(_SC_NPROCESSORS_CONF);

    for(i = 0;i < num;i++){
        if(CPU_ISSET(i, &get_core)){
            run_core = i;
        }
    }

    printf("[accept_cb] core: %d, pid: %d, tid: %ld, self: %ld\n", run_core, getpid(), (long int)syscall(__NR_gettid), pthread_self());
#endif

#ifdef __EVAL_PTHREAD__
    struct timeval start, accept_time, end;

    FILE * fp = fopen("accept_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);

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

#ifdef __EVAL_PTHREAD__
    gettimeofday(&accept_time, NULL);
#endif

    struct server_process_arg * thread_arg = (struct server_process_arg *)malloc(SERVER_PROCESS_ARG_SIZE);
    thread_arg->fd = s;
    thread_arg->byte_sent = (int *)malloc(sizeof(int));
    *(thread_arg->byte_sent) = 0;
    thread_arg->request_cnt = (int *)malloc(sizeof(int));
    *(thread_arg->request_cnt) = 0;
    thread_arg->start = (struct time_record *)malloc(sizeof(struct time_record));
    thread_arg->start->flag = 0;
    
//    pthread_mutex_lock(&connect_cnt_lock);
    thread_arg->sequence = connect_cnt;
    connect_cnt++;
//    pthread_mutex_unlock(&connect_cnt_lock);

//    log(INFO, "connect count: %d, fd: %d", connect_cnt, *s);

    pthread_t thread;
    pthread_create(&thread, NULL, server_process, (void *)thread_arg);
    pthread_detach(thread);
#ifdef __EVAL_PTHREAD__
    gettimeofday(&end, NULL);

    char buff[1024];

    long pthread_elapsed_time;

    if(accept_time.tv_usec > end.tv_usec){
        end.tv_usec += 1000000;
        end.tv_sec -= 1;
    }

    pthread_elapsed_time = end.tv_usec - accept_time.tv_usec;

    sprintf(buff, "pthread_create_time %ld\n", pthread_elapsed_time);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif
}

void request_process_cb(int fd, short events, void * arg){
#ifdef __GET_CORE__
    cpu_set_t get_core;

    CPU_ZERO(&get_core);

    if (sched_getaffinity(0, sizeof(get_core), &get_core) == -1){
        printf("warning: cound not get thread affinity, continuing...\n");
    }

    int i, num, run_core;
    num = sysconf(_SC_NPROCESSORS_CONF);

    for(i = 0;i < num;i++){
        if(CPU_ISSET(i, &get_core)){
            run_core = i;
        }
    }

    printf("[request_process_cb] core: %d, pid: %d, tid: %ld, self: %ld\n", run_core, getpid(), (long int)syscall(__NR_gettid), pthread_self());
#endif

#ifdef __EVAL_CB__
    struct timeval start, end;

    FILE * fp = fopen("request_process_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);

    gettimeofday(&start, NULL);
#endif

    struct sock_ev_read * read_arg = (struct sock_ev_read *)arg;

#ifdef __REAL_TIME_STATS__
    if(!read_arg->start->flag){
        gettimeofday(&read_arg->start->time, NULL);
        read_arg->start->flag = 1;
    }
#endif
/*
    char * msg = (char *)malloc(BUF_SIZE + 1);

	int len = read(fd, msg, sizeof(msg) - 1);

    if(len <= 0){
#ifdef __REAL_TIME_STATS__
#ifdef __BIND_CORE__
        request_end(read_arg->core_sequence, read_arg->start->time, *(read_arg->byte_sent), *(read_arg->request_cnt));
#else
        request_end(0, read_arg->start->time, *(read_arg->byte_sent), *(read_arg->request_cnt));
#endif
#endif

        event_del(read_arg->read_ev);
        close(fd);

        return;
    }

    msg[len] = '\0';
#if 0
    struct event * write_ev = (struct event *)malloc(sizeof(struct event));

    struct sock_ev_write * write_arg = (struct sock_ev_write *)malloc(sizeof(struct sock_ev_write));
    write_arg->write_ev = write_ev;
    write_arg->buff = msg;
    write_arg->buff_len = len;
    write_arg->byte_sent = read_arg->byte_sent;

    event_set(write_ev, fd, EV_WRITE, response_process_cb, write_arg);

    event_base_set(read_arg->base, write_ev);

    event_add(write_ev, NULL);

    (*(read_arg->request_cnt))++;
#endif

    (*(read_arg->request_cnt))++;
    printf("[SERVER] recv len: %d, request cnt: %d\n", len, *(read_arg->request_cnt));

    char * reply_msg = (char *)malloc(len + 1);

    strcpy(reply_msg, msg);

    int send_byte;

    int byte_left = strlen(reply_msg);

    while(byte_left > 0){
        send_byte = write(fd, reply_msg, byte_left);

        byte_left -= send_byte;
        *(read_arg->byte_sent) += send_byte;
    }

    printf("[SERVER] send byte: %d\n", *(read_arg->byte_sent));

#ifdef __EVAL_CB__
    gettimeofday(&end, NULL);

    char buff[1024];

    long elapsed_time;
    
    if(start.tv_usec > end.tv_usec){
        end.tv_usec += 1000000;
        end.tv_sec -= 1;
    }

    elapsed_time = end.tv_usec - start.tv_usec;

    sprintf(buff, "elapsed_time %ld\n", elapsed_time);
    
    pthread_mutex_lock(&record_lock);
    fwrite(buff, strlen(buff), 1, fp);
    pthread_mutex_unlock(&record_lock);

    fclose(fp);
#endif
*/
}

void response_process_cb(int fd, short events, void * arg){
#ifdef __GET_CORE__
    cpu_set_t get_core;

    CPU_ZERO(&get_core);

    if (sched_getaffinity(0, sizeof(get_core), &get_core) == -1){
        printf("warning: cound not get thread affinity, continuing...\n");
    }

    int i, num, run_core;
    num = sysconf(_SC_NPROCESSORS_CONF);

    for(i = 0;i < num;i++){
        if(CPU_ISSET(i, &get_core)){
            run_core = i;
        }
    }

    printf("[response_process_cb] core: %d, pid: %d, tid: %ld, self: %ld\n", run_core, getpid(), (long int)syscall(__NR_gettid), pthread_self());
#endif

#ifdef __EVAL_CB__
    struct timeval start, end;

    FILE * fp = fopen("response_process_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);

    gettimeofday(&start, NULL);
#endif
    struct sock_ev_write * write_arg = (struct sock_ev_write *)arg;

    char * msg = write_arg->buff;
    int msg_len = write_arg->buff_len;

    char * reply_msg = (char *)malloc(msg_len + 1);
    strcpy(reply_msg, msg);

    int send_byte_cnt = write(fd, reply_msg, strlen(reply_msg));

    *(write_arg->byte_sent) += send_byte_cnt;

#ifdef __EVAL_CB__
    gettimeofday(&end, NULL);

    char buff[1024];

    long elapsed_time;
    
    if(start.tv_usec > end.tv_usec){
        end.tv_usec += 1000000;
        end.tv_sec -= 1;
    }

    elapsed_time = end.tv_usec - start.tv_usec;

    sprintf(buff, "elapsed_time %ld\n", elapsed_time);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif
}

void * server_process(void * arg){

    struct server_process_arg * thread_arg = (struct server_process_arg *)arg;
    evutil_socket_t fd = *(thread_arg->fd);
    int sequence = thread_arg->sequence;
    
//    evutil_socket_t fd = *((evutil_socket_t *)arg);

#ifdef __BIND_CORE__
    int core_sequence = (sequence % 46) + 1;

    cpu_set_t core_set;

    CPU_ZERO(&core_set);
    CPU_SET(core_sequence, &core_set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }
#endif

#ifdef __GET_CORE__
    CPU_ZERO(&get_core);

    if (pthread_getaffinity_np(pthread_self(), sizeof(get_core), &get_core) == -1){
        printf("warning: cound not get thread affinity, continuing...\n");
    }

    int i, num, run_core;
    num = sysconf(_SC_NPROCESSORS_CONF);

    for(i = 0;i < num;i++){
        if(CPU_ISSET(i, &get_core)){
            run_core = i;
        }
    }

    printf("[server_thread] core: %d, pid: %d, tid: %ld, self: %ld\n", run_core, getpid(), (long int)syscall(__NR_gettid), pthread_self());
#endif

    struct event_base * base = event_base_new();
    struct event * read_ev = (struct event *)malloc(sizeof(struct event));

    struct sock_ev_read * read_arg = (struct sock_ev_read *)malloc(sizeof(struct sock_ev_read));
    read_arg->base = base;
    read_arg->read_ev = read_ev;
    read_arg->byte_sent = thread_arg->byte_sent;
    read_arg->request_cnt = thread_arg->request_cnt;
    read_arg->start = thread_arg->start;
    
#ifdef __BIND_CORE__
    read_arg->core_sequence = sequence;
#endif

    event_set(read_ev, fd, EV_READ | EV_PERSIST, request_process_cb, read_arg);

    event_base_set(base, read_ev);

    event_add(read_ev, NULL);

    event_base_dispatch(base);

    return NULL;
}

void * server_thread(void * arg){
    cpu_set_t core_set;

    CPU_ZERO(&core_set);
    CPU_SET(0, &core_set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

#ifdef __GET_CORE__
    cpu_set_t get_core;
    CPU_ZERO(&get_core);

    if (sched_getaffinity(0, sizeof(get_core), &get_core) == -1){
        printf("warning: cound not get thread affinity, continuing...\n");
    }

    int i, num, run_core;
    num = sysconf(_SC_NPROCESSORS_CONF);

    for(i = 0;i < num;i++){
        if(CPU_ISSET(i, &get_core)){
            run_core = i;
        }
    }

    printf("[server_thread] core: %d, pid: %d, tid: %ld, self: %ld\n", run_core, getpid(), (long int)syscall(__NR_gettid), pthread_self());
#endif

#ifdef __EVVAL__CB__
    pthread_mutex_init(&record, NULL);
#endif

    evutil_socket_t sock;
    if((sock = server_init(12345, 100)) < 0){
        perror("[SERVER] server init error");
        exit(1);
    }

//    pthread_mutex_init(&connect_cnt_lock, NULL);

#ifdef __REAL_TIME_STATS__
    pthread_mutex_init(&record_lock, NULL);
#endif

    struct event_base * base = event_base_new();

    struct event * ev_listen = event_new(base, sock, EV_READ | EV_PERSIST, accept_cb, base);

    event_add(ev_listen, NULL);

    event_base_dispatch(base);

    return NULL;
}