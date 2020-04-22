#include "server.h"

#define __NR_gettid 186

int init_ring_buff(struct ring_buf * buffer){
    buffer->buf_len = BUF_SIZE / KV_ITEM_SIZE * KV_ITEM_SIZE;
    buffer->buf_start = (char *)malloc(buffer->buf_len);
    buffer->buf_end = buffer->buf_start;
    buffer->buf_read = buffer->buf_write = 0;
    return 1;
}

int ring_buff_free(struct ring_buf * buffer){
    if(buffer->buf_read == buffer->buf_write){
        return buffer->buf_len - KV_ITEM_SIZE;
    }else{
        return buffer->buf_len - KV_ITEM_SIZE - ring_buff_used(buffer);
    }
}

int ring_buff_to_write(struct ring_buf * buffer){
    if(buffer->buf_write >= buffer->buf_read){
		if(buffer->buf_read == 0){
			return buffer->buf_len - buffer->buf_write - KV_ITEM_SIZE;
		}else{
			return buffer->buf_len - buffer->buf_write;
		}
	}else if(buffer->buf_read > buffer->buf_write){
		return buffer->buf_read - buffer->buf_write - KV_ITEM_SIZE;
	}
}

int ring_buff_used(struct ring_buf * buffer){
    if(buffer->buf_read == buffer->buf_write){
        return 0;
    }else{
        return (buffer->buf_write + buffer->buf_len - buffer->buf_read) % buffer->buf_len;
    }
}

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
    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    struct accept_args * args = (struct accept_args *)arg;
    int thread_id = args->thread_id;
    struct event_base * base = args->base;
    struct hikv * hi = args->hi; 
    struct hikv_arg * hikv_args = args->hikv_args; 

    //init ring buffer for recv
    struct ring_buf * recv_buf = (struct ring_buf *)malloc(RING_BUF_SIZE);
    init_ring_buff(recv_buf);

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
    read_arg->hikv_args = hikv_args;
    read_arg->recv_buf = recv_buf;

    bufferevent_setcb(bev, read_cb , NULL, event_cb, read_arg);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);

    //bufferevent_setwatermark(bev, EV_READ, 0, BUF_SIZE / KV_ITEM_SIZE * KV_ITEM_SIZE - KV_ITEM_SIZE);

}

void event_cb(struct bufferevent * bev, short event, void * arg){
    if(event & BEV_EVENT_EOF){
#ifdef __REAL_TIME_STATS__
        pthread_mutex_lock(&end_lock);
        gettimeofday(&g_end, NULL);
        pthread_mutex_unlock(&end_lock);
#endif

#ifdef __EVAL_KV__
        pthread_mutex_lock(&end_lock);
        gettimeofday(&g_end, NULL);
        pthread_mutex_unlock(&end_lock);
#endif

//        printf("====== close connection ======\n");
        bufferevent_free(bev);

    }
}

void read_cb(struct bufferevent * bev, void * arg){
//    printf("====== read_cb ======\n");

//    printf("[read cb] pid: %d, tid:%ld, self: %ld\n", getpid(), (long int)syscall(__NR_gettid), pthread_self());

#ifdef __REAL_TIME_STATS__
    pthread_mutex_lock(&start_lock);
    if(!start_flag){
        gettimeofday(&g_start, NULL);
        start_flag = 1;
    }
    pthread_mutex_unlock(&start_lock);
#endif

#ifdef __EVAL_KV__
    pthread_mutex_lock(&start_lock);
    if(!start_flag){
        gettimeofday(&g_start, NULL);
        start_flag = 1;
    }
    pthread_mutex_unlock(&start_lock);
#endif

#ifdef __EVAL_CB__
    struct timeval start;
    gettimeofday(&start, NULL);
#endif

    struct sock_ev_read * args = (struct sock_ev_read *)arg;

    int thread_id = args->thread_id;
    struct hikv * hi = args->hi;
    struct hikv_arg * hikv_args = args->hikv_args;
    struct ring_buf * recv_buf = args->recv_buf;

    size_t pm_size = hikv_args->pm_size;
    uint64_t num_server_thread = hikv_args->num_server_thread;
    uint64_t num_backend_thread = hikv_args->num_backend_thread;
    uint64_t num_warm_kv = hikv_args->num_warm_kv;
    uint64_t num_put_kv = hikv_args->num_put_kv;
    uint64_t num_get_kv = hikv_args->num_get_kv;
    uint64_t num_delete_kv = hikv_args->num_delete_kv;
    uint64_t num_scan_kv = hikv_args->num_scan_kv;
    uint64_t scan_range = hikv_args->scan_range;

/*receive
    //struct kv_trans_item * recv_item = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);
    //int len = bufferevent_read(bev, (char *)recv_item, KV_ITEM_SIZE);

    //size_t len = bufferevent_read(bev, (char *)(recv_buf->buf_start + recv_buf->buf_write), ring_buff_free(recv_buf));
    //recv_buf->buf_write = (recv_buf->buf_write + len) % recv_buf->buf_len;

    //printf("[SERVER] read: %d, write: %d, recv len: %d\n", recv_buf->buf_read, recv_buf->buf_write, len);
    //int recv_num = len / KV_ITEM_SIZE;
    
    //printf("[SERVER] recv len: %d\n", len);

    int len, recv_len;
	len = 0;
*/
/*
	FILE * fp = fopen("log.txt", "a+");

	char buff[1024];
	sprintf(buff, "===== HandleReadEvent =====\n");
	fwrite(buff, strlen(buff), 1, fp);
	fflush(fp);
*/
    int len, recv_len;
    len = 0;

    char * recv_item = (char *)malloc(BUF_SIZE);

#ifdef __EVAL_READ__
    struct timeval read_start;
    gettimeofday(&read_start, NULL);
#endif

	len = bufferevent_read(bev, recv_item, BUF_SIZE);
    //printf(" >> recv len: %d\n", len);

#ifdef __EVAL_READ__
    struct timeval read_end;
    gettimeofday(&read_end, NULL);

    int start_time = read_start.tv_sec * 1000000 + read_start.tv_usec;
    int end_time = read_end.tv_sec * 1000000 + read_end.tv_usec;

    pthread_mutex_lock(&read_cb_lock);
    read_cnt++;
    read_time += (end_time - start_time);
    pthread_mutex_unlock(&read_cb_lock);
#endif

    //printf("[SERVER] recv len: %d\n", len);

    int res;
    if(len == KV_ITEM_SIZE){
        //printf("[SERVER] put KV item\n");
        struct kv_trans_item * request = (struct kv_trans_item *)recv_item;
        res = hi->insert(thread_id, (uint8_t *)request->key, (uint8_t *)request->value);
        //printf("[SERVER] put key: %.*s\nput value: %.*s\n", KEY_SIZE, request->key, VALUE_SIZE, request->value);
        char * reply = (char *)malloc(REPLY_SIZE);
        memset(reply, 0, REPLY_SIZE);
        if (res == true){
            //printf("[SERVER] PUT success! key: %.*s\n", KEY_SIZE, request->key);
            //recv_item->len = VALUE_SIZE;
            //bufferevent_write(bev, (char *)recv_item, KV_ITEM_SIZE);
            char message[] = "put success";
            memcpy(reply, message, strlen(message));
            bufferevent_write(bev, reply, REPLY_SIZE);
        /*
            sprintf(buff, "[SERVER] PUT success! key: %.*s\n", KEY_SIZE, recv_item->key);
			fwrite(buff, strlen(buff), 1, fp);
			fflush(fp);
        */
        }else{
            //printf("[SERVER] put KV item failed\n");
            //recv_item->len = -1;
            //bufferevent_write(bev, (char *)recv_item, KV_ITEM_SIZE);
            char message[] = "put failed";
            memcpy(reply, message, strlen(message));
            bufferevent_write(bev, reply, REPLY_SIZE);
        /*
            sprintf(buff, "[SERVER] PUT failed! key: %.*s\n", KEY_SIZE, recv_item->key);
			fwrite(buff, strlen(buff), 1, fp);
			fflush(fp);
        */
        }
        free(reply);
    #ifdef __REAL_TIME_STATS__
        pthread_mutex_lock(&record_lock);
        request_cnt++;
        byte_sent += REPLY_SIZE;
        pthread_mutex_unlock(&record_lock);
    #endif

    #ifdef __EVAL_KV__
        pthread_mutex_lock(&record_lock);
        put_cnt++;
        pthread_mutex_unlock(&record_lock);
    #endif
    
    }else if(len == NUM_BATCH * KEY_SIZE){
    #ifdef __EVAL_KV__
        pthread_mutex_lock(&put_end_lock);
        if(!put_end_flag){
            gettimeofday(&put_end, NULL);
            put_end_flag = 1;
        }
        pthread_mutex_unlock(&put_end_lock);
    #endif
        //printf("[SERVER] get KV item\n");

    #ifndef BATCHED_KEY
        char * value = (char *)malloc(BUF_SIZE);
        res = hi->search(thread_id, (uint8_t *)recv_item, (uint8_t *)value);
        //printf(" >> GET key: %.*s\n value: %.*s\n", KEY_SIZE, recv_item, VALUE_SIZE, value);
        if(res == true){
            bufferevent_write(bev, value, VALUE_SIZE);
        }else{
            char * reply = (char *)malloc(REPLY_SIZE);
            memset(reply, 0, REPLY_SIZE);
            char message[] = "get failed";
            memcpy(reply, message, strlen(message));
            bufferevent_write(bev, reply, REPLY_SIZE);
        }
    #else
        int key_num = len / KEY_SIZE;
		char * value = (char *)malloc(key_num * VALUE_LENGTH);

        int i;
		for(i = 0;i < key_num;i++){
            //printf(" >> GET key: %.*s\n", KEY_SIZE, recv_item + i * KEY_SIZE);
			res = hi->search(thread_id, (uint8_t *)(recv_item + i * KEY_SIZE), (uint8_t *)(value + i * VALUE_LENGTH));
            if(res == true){
                //memcpy(value + i * VALUE_LENGTH, buff, VALUE_LENGTH);
                //printf(" >> GET success! value: %.*s\n", VALUE_LENGTH, value + i * VALUE_LENGTH);
            }else{
                //printf(" >> GET failed\n");
	            memset(value + i * VALUE_LENGTH, 0, VALUE_LENGTH);
    	        char message[] = "get failed";
        	    memcpy(value + i * VALUE_LENGTH, message, strlen(message));
			}
		}

        bufferevent_write(bev, value, key_num * VALUE_LENGTH);
    
        free(value);
    #endif
    
    #ifdef __REAL_TIME_STATS__
        pthread_mutex_lock(&record_lock);
        request_cnt++;
        byte_sent += KV_ITEM_SIZE;
        pthread_mutex_unlock(&record_lock);
    #endif

    #ifdef __EVAL_KV__
        pthread_mutex_lock(&record_lock);
        get_cnt += key_num;
        pthread_mutex_unlock(&record_lock);
    #endif
    }else if(len == 2 * KEY_SIZE){
        //printf(" >> SCAN key: %.*s, %.*s\n", KEY_SIZE, recv_item, KEY_SIZE, recv_item + KEY_SIZE);
        
        //char * scan_buff = (char *)malloc(scan_range * VALUE_LENGTH);
        char * scan_buff = (char *)malloc(sizeof(unsigned long *) * scan_range);

        int total_scan_count = hi->range_scan((uint8_t *)recv_item, (uint8_t *)(recv_item + KEY_SIZE), scan_buff);

        //printf(" >> SCAN total count: %d\n", total_scan_count);

        char * value = (char *)malloc(total_scan_count * VALUE_LENGTH);
        
        int i;
        for(i = 0;i < total_scan_count;i++){
            unsigned long * ptr = (unsigned long *)scan_buff;
            struct kv_item * item = (struct kv_item *)ptr[i];
            memcpy(value + i * VALUE_LENGTH, item->value, VALUE_LENGTH);
            //printf(" >> SCAN value: %.*s\n", VALUE_LENGTH, value + i * VALUE_LENGTH);
        }

        bufferevent_write(bev, value, total_scan_count * VALUE_LENGTH);
    
        free(scan_buff);
    }

    #ifdef __EVAL_CB__
        struct timeval end;
        gettimeofday(&end, NULL);
        double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
        double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

        pthread_mutex_lock(&read_lock);
        get_cnt++;
        get_time += (int)(end_time - start_time);
        pthread_mutex_unlock(&read_lock);
    #endif

    free(recv_item);

	//fclose(fp);
/*
    int res, ret;
    if(recv_item->len > 0){
        //printf("[SERVER] put KV item\n");
        res = hi->insert(thread_id, (uint8_t *)recv_item->key, (uint8_t *)recv_item->value);
        //printf("[SERVER] put key: %.*s\nput value: %.*s\n", KEY_SIZE, recv_item[i].key, VALUE_SIZE, recv_item[i].value);
        if (res == true){
            //printf("[SERVER] insert success\n");
        }
    }else if(recv_item->len == 0){
        res = hi->search(thread_id, (uint8_t *)recv_item->key, (uint8_t *)recv_item->value);
        if(res == true){
            //printf("[SERVER] search success\n");
            recv_item->len = VALUE_SIZE;
            bufferevent_write(bev, (char *)recv_item, KV_ITEM_SIZE);
        }else{
            //printf("[SERVER] search failed\n");
            recv_item->len = -1;
            bufferevent_write(bev, (char *)recv_item, KV_ITEM_SIZE);
        }
    }

    free(recv_item);
*/
/*
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
*/
}

static void signal_cb(evutil_socket_t sig, short events, void * arg){
    struct sock_ev_read * args = (struct sock_ev_read *)arg;

    int thread_id = args->thread_id;
    struct hikv * hi = args->hi;

#ifdef __EVAL_CB__
    FILE * fp = fopen("callback.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "%.4f\n", ((double)get_time)/get_cnt);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

//    printf("----- signal callback -----\n");
#ifdef __REAL_TIME_STATS__
    double start_time = (double)g_start.tv_sec + ((double)g_start.tv_usec/(double)1000000);
    double end_time = (double)g_end.tv_sec + ((double)g_end.tv_usec/(double)1000000);

	double elapsed = end_time - start_time;

	FILE * fp = fopen("throughput.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "rps %.4f bthroughput %.4f\n", 
            ((double)request_cnt)/elapsed, ((double)byte_sent)/elapsed);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

#ifdef __EVAL_KV__
    double start_time = (double)g_start.tv_sec + ((double)g_start.tv_usec/(double)1000000);
    double put_end_time = (double)put_end.tv_sec + ((double)put_end.tv_usec/(double)1000000);
    double end_time = (double)g_end.tv_sec + ((double)g_end.tv_usec/(double)1000000);

    double put_exe_time = put_end_time - start_time;
	double get_exe_time = end_time - put_end_time;

	FILE * fp = fopen("kv_throughput.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "put_iops %.4f get_iops %.4f\n", 
                    ((double)put_cnt)/put_exe_time, ((double)get_cnt)/get_exe_time);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

#ifdef __EVAL_READ__
    FILE * fp = fopen("read_cb.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "read %.4f write %.4f\n", 
                ((double)read_time)/read_cnt, ((double)write_time)/write_cnt);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

//    event_base_loopexit(base, NULL);

	exit(0);
}

void * server_thread(void * arg){
//	int core = *(int *)arg;

    struct server_arg * thread_arg = (struct server_arg *)arg;
    int core = thread_arg->core;
    int thread_id = thread_arg->thread_id;
    struct hikv * hi = thread_arg->hi;
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

    uint64_t seed = hikv_thread_arg.seed;
    
//  uint64_t scan_all = hikv_thread_arg.scan_all;

    cpu_set_t core_set;

    CPU_ZERO(&core_set);
    CPU_SET(core, &core_set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

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
    pthread_mutex_init(&read_lock, NULL);
    get_cnt = get_time = 0;
#endif

#ifdef __EVAL_READ__
    pthread_mutex_init(&read_cb_lock, NULL);
    read_cnt = read_time = 0;
    write_cnt = write_time = 0;
#endif

#ifdef __REAL_TIME_STATS__
    pthread_mutex_init(&record_lock, NULL);
    request_cnt = byte_sent = 0;

    pthread_mutex_init(&start_lock, NULL);
    start_flag = 0;

    pthread_mutex_init(&end_lock, NULL);
#endif

#ifdef __EVAL_KV__
    pthread_mutex_init(&record_lock, NULL);
    put_cnt = get_cnt = 0;

    pthread_mutex_init(&start_lock, NULL);
    start_flag = 0;

    pthread_mutex_init(&put_end_lock, NULL);
    put_end_flag = 0;

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

    struct accept_args args = {thread_id, base, hi, &hikv_thread_arg};

    struct event * ev_listen = event_new(base, sock, EV_READ | EV_PERSIST, accept_cb, (void *)&args);
    event_add(ev_listen, NULL);

    struct event * ev_signal = evsignal_new(base, SIGINT, signal_cb, (void *)&args);
    event_add(ev_signal, NULL);

    event_base_dispatch(base);

    return 0;
}

int main(int argc, char * argv[]){
    int tot_test = NUM_KEYS;
    int put_percent = PUT_PERCENT;

    struct hikv_arg hikv_thread_arg = {
        20,                                      //pm_size
        1,                                      //num_server_thread
        1,                                      //num_backend_thread
        0,                                      //num_warm_kv
        tot_test * put_percent / 100,           //num_put_kv
        tot_test * (100 - put_percent) / 100,   //num_get_kv
        0,                                      //num_delete_kv
        0,                                      //num_scan_kv
        100,                                    //scan_range
        1234,                                   //seed
        0                                       //scan_all
    };

    int i;
    int client_num = 1;

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
        }else if(sscanf(argv[i], "--num_test=%llu%c", &n, &junk) == 1){
            tot_test = n;
        }else if(sscanf(argv[i], "--num_put=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_put_kv = n;
        }else if(sscanf(argv[i], "--put_percent=%d%c", &n, &junk) == 1){
//            hikv_thread_arg.num_get_kv = hikv_thread_arg.num_put_kv * (100 - n) / n;
//            printf("[CLIENT] [PUT]: %llu [GET]: %llu\n", hikv_thread_arg.num_put_kv, hikv_thread_arg.num_get_kv);
            hikv_thread_arg.num_put_kv = tot_test * put_percent / 100;
            hikv_thread_arg.num_get_kv = tot_test * (100 - put_percent) / 100;            
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
        }else if(sscanf(argv[i], "--num_client=%llu%c", &n, &junk) == 1){
            client_num = n;
            hikv_thread_arg.num_put_kv *= n;
            hikv_thread_arg.num_get_kv *= n;            
        }else if(i > 0){
            printf("error (%s)!\n", argv[i]);
        }
    }

    size_t pm_size = hikv_thread_arg.pm_size;
    uint64_t num_server_thread = hikv_thread_arg.num_server_thread;
    uint64_t num_backend_thread = hikv_thread_arg.num_backend_thread;
    uint64_t num_warm_kv = hikv_thread_arg.num_warm_kv;
    uint64_t num_put_kv = hikv_thread_arg.num_put_kv;
    uint64_t num_get_kv = hikv_thread_arg.num_get_kv;
    uint64_t num_delete_kv = hikv_thread_arg.num_delete_kv;
    uint64_t num_scan_kv = hikv_thread_arg.num_scan_kv;
    uint64_t scan_range = hikv_thread_arg.scan_range;

    //Initialize Key-Value storage
    char pmem[128] = "/home/pmem0/pm";
    char pmem_meta[128] = "/home/pmem0/pmMETA";
    struct hikv * hi = new hikv(pm_size * 1024 * 1024 * 1024, num_server_thread, num_backend_thread, num_server_thread * (num_put_kv + num_warm_kv), pmem, pmem_meta);

    for(i = 0;i < core_limit;i++){
		cores[i] = i;
        done[i] = 0;
        sv_thread_arg[i].core = i;
        sv_thread_arg[i].thread_id = i;
        sv_thread_arg[i].hi = hi;
        memcpy(&sv_thread_arg[i].hikv_thread_arg, &hikv_thread_arg, HIKV_ARG_SIZE);
        pthread_create(&sv_thread[i], NULL, server_thread, (void *)&sv_thread_arg[i]);
    }

    for(i = 0;i < core_limit;i++){
        pthread_join(sv_thread[i], NULL);
    }
}