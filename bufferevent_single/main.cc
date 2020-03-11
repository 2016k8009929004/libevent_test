#include "common.h"

extern int client_thread_num;

//extern pthread_mutex_t fin_client_thread_lock;

int main(int argc, char * argv[]){
    if(argc == 5){
        client_thread_num = atoi(argv[1]);

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

    }else{

        core_limit = atoi(argv[1]);

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
            if(sscanf(argv[i], "--pm_size=%llu%c", &n, &junk) == 1){
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
}