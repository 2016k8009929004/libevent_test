#include "evallib.h"
/*
void request_start(struct time_record * record){
    if(!record->flag){
        gettimeofday(&record->time, NULL);
        record->flag = 1;
    }
}
*/
void request_end(int fd, struct timeval start, int byte_sent, int request_cnt){
    char file_name[1024];
    sprintf(file_name, "record_core_%d.txt", fd);

//    FILE * fp = fopen("record.txt", "a+");
    FILE * fp = fopen(file_name, "a+");
    fseek(fp, 0, SEEK_END);

    int sec, usec;

    struct timeval end;
    gettimeofday(&end, NULL);

    double start_time = (double)start.tv_sec + ((double)start.tv_usec/(double)1000000);
    double end_time = (double)end.tv_sec + ((double)end.tv_usec/(double)1000000);

    char buff[1024];

    sprintf(buff, "start %lf end %lf tot_request %d tot_byte %d\n", 
            start_time, end_time, request_cnt, byte_sent);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
}