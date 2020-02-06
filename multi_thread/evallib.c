#include "evallib.h"

extern pthread_mutex_t server_send_lock;
extern pthread_mutex_t server_request_lock;

void request_start(){
    if(!start_flag){
        gettimeofday(&start, &tz);
        start_flag = 1;
    }
}

void request_end(int core_sequence, int byte_sent, int request_cnt){
/*
    char file_name[1024];
    sprintf(file_name, "record_core_%d.txt", core_sequence);

//    FILE * fp = fopen("record.txt", "a+");
    FILE * fp = fopen(file_name, "a+");
    fseek(fp, 0, SEEK_END);

    int sec, usec;

    gettimeofday(&end, &tz);

    double start_time = (double)start.tv_sec + ((double)start.tv_usec/(double)1000000);
    double end_time = (double)end.tv_sec + ((double)end.tv_usec/(double)1000000);

    char buff[1024];

    pthread_mutex_lock(&server_send_lock);
    pthread_mutex_lock(&server_request_lock);

    sprintf(buff, "start %lf end %lf tot_request %d tot_byte %d\n", 
            start_time, end_time, request_cnt, byte_sent);

    pthread_mutex_unlock(&server_request_lock);
    pthread_mutex_unlock(&server_send_lock);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
*/
    int sec, usec;

    gettimeofday(&end, &tz);

    double start_time = (double)start.tv_sec + ((double)start.tv_usec/(double)1000000);
    double end_time = (double)end.tv_sec + ((double)end.tv_usec/(double)1000000);

    printf("start %lf end %lf\n", start_time, end_time);
}