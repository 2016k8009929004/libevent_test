#include "server.h"

#define __NR_gettid 186

evutil_socket_t server_init(int port, int listen_num){
    evutil_socket_t sock;
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("[SERVER] create socket failed");
		return -1;
    }

//    evutil_make_listen_socket_reuseable(sock);
    int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

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

    struct accept_args * args = (struct accept_args *)arg;
    int thread_id = args->thread_id;
    struct event_base * base = args->base;
    struct hikv * hi = args->hi; 

    evutil_socket_t * s = (evutil_socket_t *)malloc(sizeof(evutil_socket_t)); 

    if((*s = accept(fd, (struct sockaddr *)&client, &len)) < 0){
        perror("[SERVER] socket accept failed");
        exit(1);
    }

    evutil_make_socket_nonblocking(*s);

    pthread_mutex_lock(&connect_lock);
    connect_cnt++;
    pthread_mutex_unlock(&connect_lock);

    struct bufferevent * bev = bufferevent_socket_new(base, *s, BEV_OPT_CLOSE_ON_FREE);
    
    struct sock_ev_read * read_arg = (struct sock_ev_read *)malloc(sizeof(struct sock_ev_read));
    
    read_arg->fd = *s;

    read_arg->byte_sent = 0;
    read_arg->request_cnt = 0;

    read_arg->total_time = 0;

    read_arg->start_flag = 0; 

    read_arg->thread_id = thread_id;
    read_arg->hi = hi; 

    bufferevent_setcb(bev, read_cb , NULL, event_cb, read_arg);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);

//    bufferevent_setwatermark(bev, EV_READ, 0, KV_ITEM_SIZE);

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

void event_cb(struct bufferevent * bev, short event, void * arg){
    if(event & BEV_EVENT_EOF){
#ifdef __REAL_TIME_STATS__
        pthread_mutex_lock(&end_lock);
        gettimeofday(&g_end, NULL);
        pthread_mutex_unlock(&end_lock);
#endif
//        printf("====== close connection ======\n");
        bufferevent_free(bev);

    }
}

void read_cb(struct bufferevent * bev, void * arg){
//    printf("[read cb] pid: %d, tid:%ld, self: %ld\n", getpid(), (long int)syscall(__NR_gettid), pthread_self());

    struct sock_ev_read * args = (struct sock_ev_read *)arg;

    int thread_id = args->thread_id;
    struct hikv * hi = args->hi;

#ifdef __EVAL_READ__
    struct timeval start;
    gettimeofday(&start, NULL);
#endif

#ifdef __REAL_TIME_STATS__
    pthread_mutex_lock(&start_lock);
    if(!start_flag){
        gettimeofday(&g_start, NULL);
        start_flag = 1;
    }
    pthread_mutex_unlock(&start_lock);
#endif

    //receive
/*
    char msg[BUF_SIZE + 1];
    int len = bufferevent_read(bev, msg, BUF_SIZE);
    msg[len] = '\0';
*/
    struct kv_trans_item * item = (struct kv_trans_item *)malloc(BUF_SIZE);
    size_t len = bufferevent_read(bev, (char *)item, KV_ITEM_SIZE);
    int recv_num = len/KV_ITEM_SIZE;

    uint8_t key[KEY_LENGTH + 10];
    uint8_t value[VALUE_LENGTH + 10];

    memset(key, 0, sizeof(key));
    memset(value, 0, sizeof(value));
    snprintf((char *)key, sizeof(key), "%016llu", seed);
    snprintf((char *)value, sizeof(value), "%llu", seed);

    int res;
    res = hi->insert(thread_id, key, value);
    if (res == true){
        printf("[SERVER] test put success\n");
    }

//    memset(key, 0, sizeof(key));
//    snprintf((char *)key, sizeof(key), "%016llu", seed);
//    snprintf((char *)value, sizeof(value), "%llu", seed);

    res = hi->search(thread_id, key, value);
    if (res == true){
        printf("[SERVER] test get success\n");
    }

    //process request
/*
    printf("[SERVER] recv_num: %d\n", recv_num);

    int i, res, ret;
    for(i = 0;i < recv_num;i++){
        printf("[SERVER] len: %d\n", item[i].len);
        if(item[i].len > 0){
            printf("[SERVER] put KV item\n");
            res = hi->insert(thread_id, (uint8_t *)item[i].key, (uint8_t *)item[i].value);
            if (res == true){
                printf("[SERVER] insert success\n");
                struct kv_trans_item * ret_item = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);
                memcpy((char *)ret_item->key, (char *)item[i].key, KEY_SIZE);
                ret = hi->search(thread_id, item[i].key, ret_item->value);
                if(ret == true){
                    printf("[SERVER] search success\n");
                    bufferevent_write(bev, (char *)ret_item, KV_ITEM_SIZE);
                    printf("[SERVER] send success\n");
                }
            }
        }else if(item[i].len == 0){
            printf("[SERVER] get KV item\n");
//            res = hi->search(thread_id, item[i].key, item[i].value);
        }
    }
*/
    //reply
//    bufferevent_write(bev, item, len);

#ifdef __REAL_TIME_STATS__
    pthread_mutex_lock(&record_lock);
    request_cnt++;
    byte_sent += len;
    pthread_mutex_unlock(&record_lock);
#endif

#ifdef __EVAL_READ__
    struct timeval end;
    gettimeofday(&end, NULL);
    
    double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
    double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

    pthread_mutex_lock(&read_cb_lock);
    request_cnt++;
    total_time += (int)(end_time - start_time);
    pthread_mutex_unlock(&read_cb_lock);
#endif
}

static void signal_cb(evutil_socket_t sig, short events, void * arg){
    struct sock_ev_read * args = (struct sock_ev_read *)arg;

    int thread_id = args->thread_id;
    struct hikv * hi = args->hi;

//    printf("----- signal callback -----\n");
#ifdef __REAL_TIME_STATS__
    double start_time = (double)g_start.tv_sec + ((double)g_start.tv_usec/(double)1000000);
    double end_time = (double)g_end.tv_sec + ((double)g_end.tv_usec/(double)1000000);

	double elapsed = end_time - start_time;

	FILE * fp = fopen("throughput.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "rps %.4f throughput %.4f\n", 
            ((double)request_cnt)/elapsed, ((double)byte_sent)/elapsed);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

#ifdef __EVAL_READ__
    FILE * fp = fopen("read_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "callback %.4f\n", ((double)total_time)/request_cnt);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

//    event_base_loopexit(base, NULL);

    delete (hi);

	exit(0);
}

void * server_thread(void * arg){
//	int core = *(int *)arg;

    struct server_arg * thread_arg = (struct server_arg *)arg;
    int core = thread_arg->core;
    int thread_id = thread_arg->thread_id;
    struct hikv_arg hikv_thread_arg = thread_arg->hikv_thread_arg;

    size_t pm_size = hikv_thread_arg.pm_size;
    uint64_t num_server_thread = hikv_thread_arg.num_server_thread;
    uint64_t num_backend_thread = hikv_thread_arg.num_backend_thread;
    uint64_t num_warm_kv = hikv_thread_arg.num_warm_kv;
    uint64_t num_put_kv = hikv_thread_arg.num_put_kv;
    uint64_t num_get_kv = hikv_thread_arg.num_get_kv;
    uint64_t num_delete_kv = hikv_thread_arg.num_delete_kv;
    uint64_t num_scan_kv = hikv_thread_arg.num_scan_kv;
    uint64_t scan_range = hikv_thread_arg.scan_range;

    seed = hikv_thread_arg.seed;
    
//    uint64_t scan_all = hikv_thread_arg.scan_all;

    char pmem[128] = "/home/pmem0/pm";
    char pmem_meta[128] = "/home/pmem0/pmMETA";

    cpu_set_t core_set;

    CPU_ZERO(&core_set);
    CPU_SET(core, &core_set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

    //Initialize Key-Value storage
    struct hikv *hi = new hikv(pm_size * 1024 * 1024 * 1024, num_server_thread, num_backend_thread, num_server_thread * (num_put_kv + num_warm_kv), pmem, pmem_meta);
/*
    pthread_t tid[32];

    int i;
    for (i = 0;i < num_server_thread;i++){
        args[i] = (struct test_thread_args *)malloc(sizeof(struct test_thread_args));
        args[i]->thread_id = i;
        args[i]->obj = hi;
        memcpy(&args[i]->test_args, &test_args[i], sizeof(struct test_args));

        pthread_create(tid[i], NULL, thread_task, args[i]);
        pthread_detach(tid[i]);
    }
*/
#ifdef __EVAL_CB__
    pthread_mutex_init(&accept_cb_lock, NULL);
#endif

#ifdef __EVAL_READ__
    pthread_mutex_init(&read_cb_lock, NULL);
#endif

#ifdef __REAL_TIME_STATS__
    pthread_mutex_init(&record_lock, NULL);
    request_cnt = byte_sent = 0;

    pthread_mutex_init(&start_lock, NULL);
    start_flag = 0;

    pthread_mutex_init(&end_lock, NULL);
#endif

    event_init();

    pthread_mutex_init(&connect_lock, NULL);

    evutil_socket_t sock;
    if((sock = server_init(12345, 100)) < 0){
        perror("[SERVER] server init error");
        exit(1);
    }

    struct event_base * base = event_base_new();

    struct accept_args args = {thread_id, base, hi};

    struct event * ev_listen = event_new(base, sock, EV_READ | EV_PERSIST, accept_cb, (void *)&args);
    event_add(ev_listen, NULL);

    struct event * ev_signal = evsignal_new(base, SIGINT, signal_cb, (void *)&args);
    event_add(ev_signal, NULL);

    event_base_dispatch(base);

    return 0;
}

int main(int argc, char * argv[]){

    struct hikv_arg hikv_thread_arg = {
        2,      //pm_size
        1,      //num_server_thread
        1,      //num_backend_thread
        0,      //num_warm_kv
        0,      //num_put_kv
        0,      //num_get_kv
        0,      //num_delete_kv
        0,      //num_scan_kv
        100,    //scan_range
        1234,   //seed
        0       //scan_all
    };

    int i;

    for (i = 0; i < argc; i++){
        double d;
        uint64_t n;
        char junk;
        if(sscanf(argv[i], "--num_thread=%llu%c", &n, &junk) == 1){
            core_limit = n;
        }else if(sscanf(argv[i], "--pm_size=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.pm_size = n;
        }else if(sscanf(argv[i], "--num_server_thread=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_server_thread = n;
        }else if(sscanf(argv[i], "--num_backend_thread=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_backend_thread = n;
        }else if(sscanf(argv[i], "--num_warm=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_warm_kv = n;
        }else if(sscanf(argv[i], "--num_put=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_put_kv = n;
        }else if(sscanf(argv[i], "--num_get=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_get_kv = n;
        }else if(sscanf(argv[i], "--num_delete=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_delete_kv = n;
        }else if(sscanf(argv[i], "--num_scan=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_scan_kv = n;
        }else if(sscanf(argv[i], "--scan_range=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.scan_range = n;
        }else if(sscanf(argv[i], "--num_scan_all=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.scan_all = n;
        }else if(sscanf(argv[i], "--seed=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.seed = n;
        }else if (sscanf(argv[i], "--seq=%llu%c", &n, &junk) == 1){
            if (n == 1){
                sequence_rw = 1;
            }
        }else if(i > 0){
            printf("error (%s)!\n", argv[i]);
        }
    }

    for(i = 0;i < core_limit;i++){
		cores[i] = i;
        done[i] = 0;
        sv_thread_arg[i].core = i;
        sv_thread_arg[i].thread_id = i;
        memcpy(&sv_thread_arg[i].hikv_thread_arg, &hikv_thread_arg, HIKV_ARG_SIZE);
        pthread_create(&sv_thread[i], NULL, server_thread, (void *)&sv_thread_arg[i]);
    }

    for(i = 0;i < core_limit;i++){
        pthread_join(sv_thread[i], NULL);
    }
}