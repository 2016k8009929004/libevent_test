#include "common.h"

#define PTHREAD_TEST
//#define FORK_TEST

extern int core_limit;
extern pthread_t sv_thread[MAX_CPUS];
extern int done[MAX_CPUS];
extern int cores[MAX_CPUS];

//extern pthread_mutex_t fin_client_thread_lock;

int main(int argc, char * argv[]){
    if(argc == 5){
#ifdef PTHREAD_TEST
//        client_thread(argc, argv);

        client_thread_num = atoi(argv[1]);

//        pthread_mutex_init(&fin_client_thread_lock, NULL);

//        thread_num = atoi(argv[1]);

        pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * client_thread_num);

        int i;
        for(i = 0;i < client_thread_num;i++){

            struct client_arg arg;
            arg.ip_addr = &argv[2];
            arg.port = atoi(argv[3]);
            arg.buf_size = atoi(argv[4]);
#ifdef __BIND_CORE__
            arg.sequence = i;
#endif
            pthread_create(&threads[i], NULL, client_thread, (void *)&arg);
//            pthread_create(&threads[i], NULL, client_thread, (void *)argv);
        }

        for(i = 0;i < client_thread_num;i++){
            pthread_join(threads[i], NULL);
        }

#endif

#ifdef FORK_TEST
        pid_t pid;
        int i;
        for(i = 0;i < client_thread_num;i++){
            pid = fork();
            if(pid == -1){
                perror("[CLIENT] fork failed");
                exit(1);
            }else if(pid == 0){
                break;
            }
        }

        if(pid == 0){
            cpu_set_t mask;
            CPU_ZERO(&mask);
            CPU_SET(i % 46, &mask);

            if (sched_setaffinity(0, sizeof(mask), &mask) == -1){
                printf("warning: could not set CPU affinity, continuing...\n");
            }

            struct client_arg arg;
            arg.ip_addr = &argv[2];
            arg.port = atoi(argv[3]);
            arg.buf_size = atoi(argv[4]);

            client_thread(&arg);
        }
#endif
    }else{
//        server_thread(argc, argv);

        core_limit = atoi(argv[1]);

        int i;
        for(i = 0;i < core_limit;i++){
    		cores[i] = i;
            done[i] = 0;
            pthread_create(&sv_thread[i], NULL, server_thread, (void *)&cores[i]);
        }

        for(i = 0;i < core_limit;i++){
            pthread_join(sv_thread[i], NULL);
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