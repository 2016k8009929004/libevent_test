#include <sys/time.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "log.h"

//#define REAL_TIME_STATS

struct timeval start, end, time1, time2;

static int start_flag = 0;

float elapsed_time;

struct timezone tz;

void request_start();
void request_end(int byte_sent, int request_cnt);