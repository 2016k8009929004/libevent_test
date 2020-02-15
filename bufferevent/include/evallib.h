#include <sys/time.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

//#define __REAL_TIME_STATS__

//struct timeval start, end, time1, time2;

//static int start_flag = 0;

//void request_start(struct time_record * record);

void request_end(int fd, struct timeval start, int byte_sent, int request_cnt);