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

    log(DEBUG, "start sec: %ld, usec: %ld  end sec: %ld, usec: %ld, byte sent: %d", 
            start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec, byte_sent);
/*
    if(end.tv_usec < start.tv_usec){
        end.tv_usec += 1000000;
        end.tv_sec -= 1;
    }
*/
    sec = end.tv_sec - start.tv_sec;
    usec = end.tv_usec - start.tv_usec;

    double start_time = (double)start.tv_sec + ((double)start.tv_usec/(double)1000000);
    double end_time = (double)end.tv_sec + ((double)end.tv_usec/(double)1000000);
/*
    elapsed_time = (float)sec + ((float)usec/(float)1000000.0);

    float tps = (float)handle_request_cnt / elapsed_time;
//    float thruput = (float)byte_sent / elapsed_time;
*/

//    unsigned long start = 1000000 * start.tv_sec + 

    handle_request_cnt++;
/*
    log(INFO, "elapsed time: %f, handle request: %d, tps: %f, byte sent: %d, throughput: %f", 
            elapsed_time, handle_request_cnt, tps, byte_sent, thruput);


    log(INFO, "start:%f end:%f tot_request: %d tot_byte:%d\n", 
            start_time, end_time, handle_request_cnt, byte_sent);
*/

    char buff[1024];
/*
    printf("start:%lf end:%lf tot_request: %d tot_byte:%d\n", 
            start_time, end_time, handle_request_cnt, byte_sent);
*/
    sprintf(buff, "start:%lf end:%lf tot_request:%d tot_byte:%d\n", 
            start_time, end_time, handle_request_cnt, byte_sent);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
}