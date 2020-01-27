#include "common.h"

int main(int argc, char * argv[]){
    if(argc == 4){
//        client_thread(argc, argv);

        int thread_num;
        thread_num = atoi(argv[1]);
        pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_num);

        int i;
        for(i = 0;i < thread_num;i++){
            struct server_node arg;
            arg.ip_addr = &argv[2];
            arg.port = atoi(argv[3]);

            pthread_create(&threads[i], NULL, client_thread, (void *)&arg);
        }

        for(i = 0;i < thread_num;i++){
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
    }
}