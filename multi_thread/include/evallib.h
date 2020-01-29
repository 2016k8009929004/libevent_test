#include <sys/time.h>
#include <stdio.h>

#include "log.h"

#define REAL_TIME_STATS

struct timeval start, end, time1, time2;

static int start_flag = 0;

static int handle_request_cnt = 0;

static int byte_sent = 0;

float elapsed_time;

struct timezone tz;

void request_start();
void request_end();