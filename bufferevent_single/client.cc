#include "client.h"

// Generate keys and values for client number cn
void gen_corpus(LL * key_corpus, uint8_t * value_corpus){
	int key_i;
	LL temp;

	for(key_i = 0; key_i < NUM_KEYS; key_i ++) {
		LL rand1 = (LL) lrand48();
		LL rand2 = (LL) lrand48();
		key_corpus[key_i] = (rand1 << 32) ^ rand2;
		if((char) key_corpus[key_i] == 0) {
			key_i --;
		}
	}

    FILE * fp = fopen("client-input.dat", "rb");
    fread(value_corpus, 1, NUM_KEYS * VALUE_SIZE, fp);
    fclose(fp);

    return;
}

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

void * send_request(void * arg){
    struct send_info * info = (struct send_info *)arg;

    int fd = *(info->sockfd);

    //initial Key
    LL * key_corpus = (LL *)malloc(NUM_KEYS * sizeof(LL));
    uint8_t * value_corpus = (uint8_t *)malloc(NUM_KEYS * VALUE_SIZE);
    
    gen_corpus(key_corpus, value_corpus);

#ifdef __TEST_FILE__
    char send_buf[buf_size];
    char recv_buf[buf_size + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

	int send_size, recv_size;

    FILE * send_fp = fopen("client-input.dat", "rb");
#ifdef RECEIVE_DEBUG
    FILE * recv_fp = fopen("server-ouput.dat", "wb");
#endif

#ifdef __EV_RTT__
    struct timeval record_start[250000], record_end[250000];

    int request_cnt;
    request_cnt = 0;
    
    FILE * fp = fopen("rtt.txt", "a+");
    fseek(fp, 0, SEEK_END);
#endif

    struct timeval time1, time2;
    gettimeofday(&time1, NULL);

    while(!feof(send_fp)){

#ifdef __EV_RTT__
        gettimeofday(&record_start[request_cnt], NULL);
#endif

//send request
        send_size = fread(send_buf, 1, buf_size, send_fp);

        if(write(fd, send_buf, send_size) < 0){
			perror("[CLIENT] send failed");
	    	exit(1);
    	}

//receive reply
        int temp = 0;
        while(1){
            recv_size = read(fd, recv_buf, buf_size);

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

#ifdef RECEIVE_DEBUG
            fwrite(recv_buf, recv_size, 1, recv_fp);
            fflush(recv_fp);
#endif
            temp += recv_size;

            if(temp == send_size){
                break;
            }
        }

#ifdef __EV_RTT__
        gettimeofday(&record_end[request_cnt], NULL);

        if(record_end[request_cnt].tv_sec - record_start[0].tv_sec > 10){
            printf("[CLIENT] request complete\n");
            break;
        }

        request_cnt++;
#else
        struct timeval end;
        gettimeofday(&end, NULL);
        
        if(end.tv_sec - time1.tv_sec > 10){
            gettimeofday(&time2, NULL);
            printf("[CLIENT] request complete\n");
            break;
        }
#endif
    }

#ifdef __EV_RTT__
    int j;
    for(j = 0;j <= request_cnt;j++){
        long start_time = (long)record_start[j].tv_sec * 1000000 + (long)record_start[j].tv_usec;
        long end_time = (long)record_end[j].tv_sec * 1000000 + (long)record_end[j].tv_usec;

        char buff[1024];

        sprintf(buff, "%ld\n", end_time - start_time);
        
        pthread_mutex_lock(&rtt_lock);

        fwrite(buff, strlen(buff), 1, fp);
        fflush(fp);

        pthread_mutex_unlock(&rtt_lock);
    }

    fclose(fp);

#endif

    fclose(send_fp);
#elif defined(__TEST_KV__)
//    printf("===== start real work ======\n");
    int i, iter, key_i;
    
    key_i = 0;
    
    struct kv_trans_item * req_kv = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);

    struct timeval time1, time2;
    gettimeofday(&time1, NULL);

//PUT
/*
    for(iter = 0;iter < 3;iter++){
        if(rand() % 100 <= PUT_PERCENT || iter < NUM_KEYS){
            snprintf((char *)req_kv->key, KEY_SIZE + 1, "%0llu", key_corpus[key_i]);     //set Key
			req_kv->len = VALUE_SIZE;
			memcpy((char *)req_kv->value, (char *)&value_corpus[key_i * VALUE_SIZE], VALUE_SIZE);   //set Value
            //printf("[CLIENT] key: %.*s\nvalue: %.*s\n", KEY_SIZE, req_kv->key, VALUE_SIZE, req_kv->value);
            printf("[CLIENT] key: %llu, value: %.*s\n", key_corpus[key_i], VALUE_SIZE, req_kv->value);
			key_i = (key_i + 1) & NUM_KEYS_;
		}else{
			key_i = rand() & NUM_KEYS_;
			req_kv->key[0] = key_corpus[key_i];
			req_kv->len = 0;
			memset((char *)req_kv->value, 0, VALUE_SIZE);
		}

        if(write(fd, req_kv, KV_ITEM_SIZE) < 0){
			perror("[CLIENT] send failed");
	    	exit(1);
    	}

//test PUT

        int temp = 0;
        
        struct kv_trans_item * recv_item = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);

	    int recv_size;
        
        while(1){
            recv_size = read(fd, recv_item, KV_ITEM_SIZE);
            
            printf("[CLIENT] recv len: %d\n", recv_size);

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

            temp += recv_size;

            if(temp == KV_ITEM_SIZE){
                printf("[CLIENT] reply key: %llu, value: %.*s\n", str_to_ll((char *)req_kv->key, KEY_SIZE), KEY_SIZE, VALUE_SIZE, req_kv->value);
                break;
            }
        }

        gettimeofday(&time2, NULL);
        if(time2.tv_sec - time1.tv_sec > 10){
            printf("[CLIENT] request complete\n");
            break;
        }
    }
*/

//PUT
    for(iter = 0;iter < 3;iter++){
        snprintf((char *)req_kv->key, KEY_SIZE + 1, "%0llu", key_corpus[key_i]);     //set Key
		req_kv->len = VALUE_SIZE;
		memcpy((char *)req_kv->value, (char *)&value_corpus[key_i * VALUE_SIZE], VALUE_SIZE);   //set Value
        //printf("[CLIENT] key: %.*s\nvalue: %.*s\n", KEY_SIZE, req_kv->key, VALUE_SIZE, req_kv->value);
        printf("[CLIENT] key: %llu, value: %.*s\n", key_corpus[key_i], VALUE_SIZE, req_kv->value);
		key_i = (key_i + 1) & NUM_KEYS_;

        if(write(fd, req_kv, KV_ITEM_SIZE) < 0){
			perror("[CLIENT] send failed");
	    	exit(1);
    	}

        sleep(1);
    }

//GET
    struct kv_trans_item * res_kv = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);
    
    for(iter = 0, key_i = 0;iter < 3;iter++){
        snprintf((char *)req_kv->key, KEY_SIZE + 1, "%0llu", key_corpus[key_i]);     //set Key
		req_kv->len = 0;
		memset((char *)req_kv->value, 0, VALUE_SIZE);

        if(write(fd, req_kv, KV_ITEM_SIZE) < 0){
			perror("[CLIENT] send failed");
	    	exit(1);
    	}

        int temp = 0;

	    int recv_size;

        while(1){
            recv_size = read(fd, res_kv, KV_ITEM_SIZE);

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

            temp += recv_size;

            if(temp == KV_ITEM_SIZE){
                printf("[CLIENT] key: %llu, value: %.*s\n", key_corpus[key_i], VALUE_SIZE, res_kv->value);
                break;
            }
        }
        key_i = (key_i + 1) & NUM_KEYS_;
    }
#endif

    printf("====== end request ======\n");

    return NULL;
}
#if 0
void response_process(int sock, short event, void * arg){
#ifdef RECEIVE_DEBUG
    struct debug_response_arg * debug_arg = (struct debug_response_arg *)arg;

    struct event * read_ev = debug_arg->read_ev;
    struct send_info * info = debug_arg->info;
    FILE * fp = debug_arg->fp;
#else
    struct response_arg * response_process_arg = (struct response_arg *)arg;

    struct event * read_ev = response_process_arg->read_ev;
    struct send_info * info = response_process_arg->info;
#endif

    int * recv_byte = info->recv_byte;
    int * send_byte = info->send_byte;

    char recv_buf[buf_size + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

    int recv_size = read(sock, recv_buf, buf_size);

    if(recv_size == 0){
        printf("[CLIENT] close connection\n");
        close(sock);
    }

#ifdef RECEIVE_DEBUG
    fwrite(recv_buf, recv_size, 1, fp);
    fflush(fp);
#endif

    (*recv_byte) += recv_size;
    
//    printf("[CLIENT %d] receive reply: %s\n", sock, recv_buf);

    if((*recv_byte) == (*send_byte)){
        printf("[CLIENT %d] receive reply complete, close connection\n", sock);

        work_done_flag = 1;

        event_del(read_ev);

#ifdef RECEIVE_DEBUG
        fclose(fp);
#endif
        close(sock);
    }
}

void * create_response_process(void * arg){
    struct send_info * info = (struct send_info *)arg;

    int fd = *(info->sockfd);

    struct event_base * base = event_base_new();

    struct event * read_ev = (struct event *)malloc(sizeof(struct event));

#ifdef RECEIVE_DEBUG
    FILE * recv_fp = fopen("server-ouput.dat", "wb");

    struct debug_response_arg * debug_arg = (struct debug_response_arg *)malloc(sizeof(struct debug_response_arg));
    debug_arg->read_ev = read_ev;
    debug_arg->info = info;
    debug_arg->fp = recv_fp;

    event_set(read_ev, fd, EV_READ|EV_PERSIST, response_process, debug_arg);
#else
    struct response_arg * response_process_arg = (struct response_arg *)malloc(RESPONSE_ARG_SIZE);
    response_process_arg->read_ev = read_ev;
    response_process_arg->info = info;

    event_set(read_ev, fd, EV_READ|EV_PERSIST, response_process, response_process_arg);
#endif

    event_base_set(base, read_ev);

    event_add(read_ev, NULL);

    event_base_dispatch(base);
}

void receive_response_thread(struct send_info * info){
    pthread_t thread;
    pthread_create(&thread, NULL, create_response_process, (void *)info);
    pthread_detach(thread);
}

void send_request_thread(struct send_info * info){
    pthread_t thread;
    pthread_create(&thread, NULL, send_request, (void *)info);
    pthread_detach(thread);
}
#endif
void * client_thread(void * argv){
    cpu_set_t core_set;

    CPU_ZERO(&core_set);
    CPU_SET(0, &core_set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

    struct client_arg * server = (struct client_arg *)argv;

//    buf_size = server->buf_size;
    
    int send_byte, recv_byte;
    send_byte = recv_byte = 0;

#ifdef __EV_RTT__
    pthread_mutex_init(&rtt_lock, NULL);
#endif

    int sockfd = connect_server(*(server->ip_addr), server->port);
    if(sockfd == -1){
        perror("[CLIENT] tcp connect error");
        exit(1);
    }

    struct send_info * info = (struct send_info *)malloc(SEND_INFO_SIZE);
    info->sockfd = &sockfd;
    info->send_byte = &send_byte;
    info->recv_byte = &recv_byte;

    send_request(info);

//    while(!work_done_flag);

    free(info);

    return NULL;
}

int main(int argc, char * argv[]){
    client_thread_num = atoi(argv[1]);

    pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * client_thread_num);

    int i;
    for(i = 0;i < client_thread_num;i++){
        struct client_arg arg;
        arg.ip_addr = &argv[2];
        arg.port = atoi(argv[3]);
//        arg.buf_size = atoi(argv[4]);
#ifdef __BIND_CORE__
        arg.sequence = i;
#endif
        
        pthread_create(&threads[i], NULL, client_thread, (void *)&arg);
    }

    for(i = 0;i < client_thread_num;i++){
        pthread_join(threads[i], NULL);
    }
}