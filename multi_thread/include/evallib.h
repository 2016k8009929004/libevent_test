//------performance evaluation------
struct timeval start, end, time1, time2;

int start_flag = 0;

int handle_request_cnt = 0;

int byte_sent = 0;

float elapsed_time;

struct timezone tz;

void request_start();
void request_end();