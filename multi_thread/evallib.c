#include "evallib.h"

void request_start(){
    if(!start_flag){
        gettimeofday(&start, &tz);
        start_flag = 1;
    }
}

void request_end(){
    int sec, usec;

    handle_request_cnt++;

    gettimeofday(&end, &tz);

    if(end.tv_usec < start.tv_usec){
        end.tv_usec += 1000000;
        end.tv_sec -= 1;
    }

    sec = end.tv_sec - start.tv_sec;
    usec = end.tv_usec - start.tv_usec;
    elapsed_time = (float)sec + ((float)usec/(float)1000000.0);

    float tps = (float)handle_request_cnt / elapsed_time;
    float thruput = (float)byte_sent / elapsed_time;

    log(INFO, "elapsed time: %f, handle request: %d, tps: %f, byte sent: %d, throughput: %f", 
            elapsed_time, handle_request_cnt, tps, byte_sent, thruput);
}