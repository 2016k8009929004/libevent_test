#include "common.h"

extern int client_thread_num;

extern pthread_mutex_t fin_client_thread_lock;

int main(int argc, char * argv[]){
    if(argc == 5){
//        client_thread(argc, argv);

        client_thread_num = atoi(argv[1]);

        pthread_mutex_init(&fin_client_thread_lock, NULL);

//        thread_num = atoi(argv[1]);

        pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * client_thread_num);

        int i;
        for(i = 0;i < client_thread_num;i++){

            struct client_arg arg;
            arg.ip_addr = &argv[2];
            arg.port = atoi(argv[3]);
            arg.buf_size = atoi(argv[4]);
            arg.sequence = i;

            pthread_create(&threads[i], NULL, client_thread, (void *)&arg);
//            pthread_create(&threads[i], NULL, client_thread, (void *)argv);
        }

        for(i = 0;i < client_thread_num;i++){
            pthread_join(threads[i], NULL);
        }
    }else{
//        server_thread(argc, argv);

        int thread_num;
        thread_num = atoi(argv[1]);
        pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_num);

        int i;
        for(i = 0;i < thread_num;i++){
            pthread_create(&threads[i], NULL, server_thread, NULL);
        }

        for(i = 0;i < thread_num;i++){
            pthread_join(threads[i], NULL);
        }

#if 0
        int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
        printf("[SERVER] server has %d processor(s)\n", cpu_num);

        int * thread_id = (int *)malloc(sizeof(int) * cpu_num);
        pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * cpu_num);
        
        int i;
        for(i = 0;i < cpu_num;i++){
            thread_id[i] = i;
            pthread_create(&threads[i], NULL, server_thread, (void *)&thread_id[i]);
        }

        for(i = 0;i < cpu_num;i++){
            pthread_join(threads[i], NULL);
        }
#endif

    }
}