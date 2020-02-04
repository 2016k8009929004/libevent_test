#include "evallib.h"

void request_start(){
    if(!start_flag){
        gettimeofday(&start, &tz);
        start_flag = 1;
    }
}

void request_end(int byte_sent){
    FILE * fp = fopen("record.txt", "a+");
    fseek(fp, 0, SEEK_END);

    int sec, usec;

    gettimeofday(&end, &tz);

    double start_time = (double)start.tv_sec + ((double)start.tv_usec/(double)1000000);
    double end_time = (double)end.tv_sec + ((double)end.tv_usec/(double)1000000);

    char buff[1024];

    sprintf(buff, "start:%lf end:%lf tot_request:%d tot_byte:%d\n", 
            start_time, end_time, handle_request_cnt, byte_sent);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
}