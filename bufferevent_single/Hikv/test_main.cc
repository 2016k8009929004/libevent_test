#include "city.h"
#include "config.h"
#include "pflush.h"
#include "random.h"
#include "pm_alloc.h"
#include "hikv.h"

#include <map>
#include <pthread.h>
#include <string>
#include <limits>
#include <random>
#include <string>
#include <vector>
#include <cstring>
#include <fstream>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <algorithm>
#include <sys/time.h>

using namespace std;

static int cpu_speed_mhz = 2600;
static int sequence_rw = 0;
static const int core_in_numa0[48] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71};

#define KB ((uint64_t)1024)
#define MB ((uint64_t)1024 * 1024)
#define GB ((uint64_t)1024 * 1024 * 1024)

#define TEST_PUT (0)
#define TEST_GET (1)
#define TEST_UPDATE (2)
#define TEST_DELETE (3)
#define TEST_SCAN (4)
#define TEST_SEQ (5)

struct test_result
{
    public:
        double iops;
        double throughput;
        double latency;

    public:
        test_result()
        {
            iops = 0;
            throughput = 0;
        }
};

struct test_args
{
    bool seq;
    int test_type;
    uint64_t get_num_kv;
    uint64_t get_seed;
    uint64_t get_sequence_id;
    uint64_t put_num_kv;
    uint64_t put_seed;
    uint64_t put_sequence_id;
    uint64_t delete_num_kv;
    uint64_t delete_seed;
    uint64_t delete_sequence_id;
    uint64_t scan_num_kv;
    uint64_t scan_seed;
    uint64_t scan_range;
    uint64_t scan_sequence_id;
    uint64_t scan_all;
    uint64_t seed_range;
};

struct test_thread_args
{
    void *obj;
    int thread_id;
    struct test_args test_args;
    struct test_result test_result;
};

void calc(int num_kv)
{
    uint64_t key_length = KEY_LENGTH;
    uint64_t value_length = VALUE_LENGTH;
    uint64_t kv_count = num_kv;
    uint64_t buffer_size = kv_count * (key_length + value_length);
    printf("write size : %lluMB\n", buffer_size / MB);
}

static void print_line()
{
    for (int i = 0; i < 35; i++)
    {
        printf("-");
    }
    printf("\n");
}

// #define REAL_TIME_STATS
#ifdef REAL_TIME_STATS

#define SAR_PRINT

uint64_t collect_interval_ns = (uint64_t)5 * 1000 * 1000 * 1000; // 5 * 1 seconds (10^9ns)
uint64_t collect_interval_opt = 100000;
vector<uint64_t> realtime_iops[32];        // realtime IOPS for each thread
vector<uint64_t> opt_latency[32];          // PUT/GET/DELETE/SCAN
vector<uint64_t> opt_realtime_latency[32]; // realtime latency for each opt
vector<uint64_t> opt_realtime_iops[32];    // realtime iops for each opt
vector<uint64_t> opt_realtime_p99[32];     // realtime P99 latency for each opt
vector<uint64_t> opt_realtime_p999[32];    // realtime P99.9 latency for each opt

static void file_write(const char *name, vector<uint64_t> &data)
{
    ofstream fout(name);
    if (fout.is_open())
    {
        for (int i = 0; i < data.size(); i++)
        {
            fout << data[i] << endl;
        }
        fout.close();
    }
    else
    {
        printf("can not't open write file(%s)\n", name);
    }
}

// print single opterator realtime latency/iops
static void P99(bool output)
{
    int num_put = opt_latency[TEST_PUT].size();
    int num_get = opt_latency[TEST_GET].size();
    int num_delete = opt_latency[TEST_DELETE].size();
    int index99;
    int index999;

    printf("**[P99] PUT/GET/DELETE:%d/%d/%d\n", num_put, num_get, num_delete);

    // 1.PUT latency
    if (num_put > 0)
    {
        index99 = (int)(0.99 * num_put);
        index999 = (int)(0.999 * num_put);
        sort(opt_latency[TEST_PUT].begin(), opt_latency[TEST_PUT].end());

        double avg_latency = 0;
        for (int i = 0; i < opt_latency[TEST_PUT].size(); i++)
        {
            avg_latency += opt_latency[TEST_PUT][i];
        }
        avg_latency /= opt_latency[TEST_PUT].size();

        printf("  [PUT][Avg:%.2f][99%%/%d:%llu][99.9%%/%d:%llu]\n", avg_latency, index99, opt_latency[TEST_PUT][index99], index999, opt_latency[TEST_PUT][index999]);
    }

    // 2.GET latency
    if (num_get > 0)
    {
        index99 = (int)(0.99 * num_get);
        index999 = (int)(0.999 * num_get);
        sort(opt_latency[TEST_GET].begin(), opt_latency[TEST_GET].end());

        double avg_latency = 0;
        for (int i = 0; i < opt_latency[TEST_GET].size(); i++)
        {
            avg_latency += opt_latency[TEST_GET][i];
        }
        avg_latency /= opt_latency[TEST_GET].size();

        printf("  [GET][Avg:%.2f][99%%/%d:%llu][99.9%%/%d:%llu]\n", avg_latency, index99, opt_latency[TEST_GET][index99], index999, opt_latency[TEST_GET][index999]);
    }

    // 3.DELETE latency
    if (num_delete > 0)
    {
        index99 = (int)(0.99 * num_delete);
        index999 = (int)(0.999 * num_delete);
        sort(opt_latency[TEST_DELETE].begin(), opt_latency[TEST_DELETE].end());

        double avg_latency = 0;
        for (int i = 0; i < opt_latency[TEST_DELETE].size(); i++)
        {
            avg_latency += opt_latency[TEST_DELETE][i];
        }
        avg_latency /= opt_latency[TEST_DELETE].size();

        printf("  [DEL][Avg:%.2f][99%%/%d:%llu][99.9%%/%d:%llu]\n", avg_latency, index99, opt_latency[TEST_DELETE][index99], index999, opt_latency[TEST_DELETE][index999]);
    }
}

static void output_sort_latency(const char *_name)
{
    char name[128];
    for (int i = 0; i < 32; i++)
    {
        if (opt_latency[i].size() != 0)
        {
            sprintf(name, "%s_%d.latency", _name, i);
            file_write(name, opt_latency[i]);
        }
    }
}

// print a thread realtime iops
static void output_realtime_iops(const char *_name)
{
    char name[128];

    for (int i = 0; i < 32; i++)
    {
        if (realtime_iops[i].size() != 0)
        {
            sprintf(name, "%s_%d.iops", _name, i);
            file_write(name, realtime_iops[i]);
        }
    }
}

// print average latency
static void output_realtime_latency(const char *_name)
{
    char name[128];

    for (int i = 0; i < 32; i++)
    {
        if (opt_realtime_latency[i].size() != 0)
        {
            sprintf(name, "%s_%d.real_latency", _name, i);
            file_write(name, opt_realtime_latency[i]);
        }
    }
}

static void output_realtime_opt_iops(const char *_name)
{
    char name[128];

    for (int i = 0; i < 32; i++)
    {
        if (opt_realtime_latency[i].size() != 0)
        {
            sprintf(name, "%s_%llu_%d.iops", _name, i);
            file_write(name, opt_realtime_iops[i]);
        }
    }
}

static void output_realtime_p99_latency(const char *_name)
{
    char name[128];

    for (int i = 0; i < 32; i++)
    {
        if (opt_realtime_p99[i].size() != 0)
        {
            sprintf(name, "%s_%d.p99latency", _name, i);
            file_write(name, opt_realtime_p99[i]);
        }
        if (opt_realtime_p999[i].size() != 0)
        {
            sprintf(name, "%s_%d.p999latency", _name, i);
            file_write(name, opt_realtime_p999[i]);
        }
    }
}

#endif

void *thread_task(void *test_args)
{
    bool res;
    uint64_t match_search = 0;
    uint64_t match_delete = 0;
    uint64_t match_insert = 0;
    uint8_t key[KEY_LENGTH + 10];
    uint8_t value[VALUE_LENGTH + 10];
    uint8_t buff[KEY_LENGTH + VALUE_LENGTH + 100];
    size_t key_length = KEY_LENGTH;
    size_t value_length = VALUE_LENGTH;

    struct test_thread_args *args = (struct test_thread_args *)test_args;
    struct hikv *hi = (struct hikv *)args->obj;

    int thread_id = args->thread_id;
    uint64_t get_num_kv = args->test_args.get_num_kv;
    uint64_t get_seed = args->test_args.get_seed;
    uint64_t get_sequence_id = args->test_args.get_sequence_id;
    uint64_t put_num_kv = args->test_args.put_num_kv;
    uint64_t put_seed = args->test_args.put_seed;
    uint64_t put_sequence_id = args->test_args.put_sequence_id;
    uint64_t delete_num_kv = args->test_args.delete_num_kv;
    uint64_t delete_seed = args->test_args.delete_seed;
    uint64_t delete_sequence_id = args->test_args.delete_sequence_id;
    uint64_t scan_num_kv = args->test_args.scan_num_kv;
    uint64_t scan_seed = args->test_args.scan_seed;
    uint64_t scan_sequence_id = args->test_args.scan_sequence_id;
    uint64_t scan_range = args->test_args.scan_range;
    uint64_t scan_all = args->test_args.scan_all;
    uint64_t seed_range = args->test_args.seed_range;
    char *scan_buff = (char *)malloc(sizeof(unsigned long *) * scan_range);
    size_t total_scan_count = 0;

    uint64_t num_kv = get_num_kv + put_num_kv + delete_num_kv + scan_num_kv;
    uint64_t get_count = 0;
    uint64_t put_count = 0;
    uint64_t delete_count = 0;
    uint64_t scan_count = 0;
    uint64_t scan_kv_count = 0;
    uint64_t scan_all_count = 0;
    uint64_t seq_count = 0;
    uint64_t real_get_count = 0;

    Random *get_random = new Random(get_seed);
    Random *put_random = new Random(put_seed);
    Random *delete_random = new Random(delete_seed);
    Random *scan_random = new Random(scan_seed);

    struct timeval begin_time, end_time;
    uint64_t start_ns, end_ns, latency_ns;
    uint64_t timestamp_sum, timestamp_begin, timestamp_end, timestamp_iops;
    uint64_t sum_latency[32] = {0};
    uint64_t sum_latency_count[32] = {0};

    vector<uint64_t> p99_latency[32];
    vector<string> scan_vector;

    double exe_time;

    printf(">>[TEST%d] seed(%llu/%llu/%llu), sequence(%llu,%llu,%llu)\n", thread_id, put_seed, get_seed, delete_seed, put_sequence_id, get_sequence_id, delete_sequence_id);
    printf("  [PUT]:%llu(%.2f%%), [GET]:%llu(%.2f%%), [DELETE]:%llu(%.2f%%), [SCAN]:%llu(%.2f%%)\n",
            put_num_kv, 100.0 * put_num_kv / num_kv,
            get_num_kv, 100.0 * get_num_kv / num_kv,
            delete_num_kv, 100.0 * delete_num_kv / num_kv,
            scan_num_kv, 100.0 * scan_num_kv / num_kv);

#ifdef THREAD_BIND_CPU
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_in_numa0[thread_id], &mask);
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
    {
        printf("threadpool, set thread affinity failed.\n");
    }
#endif

    gettimeofday(&begin_time, NULL);
    timestamp_sum = 0;
    timestamp_iops = 0;
    uint64_t sum_opt_count = 0;

    while (true) // mixed benchmark
    {
        bool flag = false;
        int test_type = -1;
        sum_opt_count++;
#ifdef REAL_TIME_STATS
        timestamp_begin = asm_rdtscp();
#endif
        if (put_count < put_num_kv)
        {
            flag = true;
            test_type = TEST_PUT;
            memset(key, 0, sizeof(key));
            memset(value, 0, sizeof(value));
            uint64_t seed = (sequence_rw == 1) ? put_sequence_id : put_sequence_id + put_random->Next();
            // uint64_t seed = (sequence_rw == 1) ? put_sequence_id : (put_random->Next() % seed_range);
            snprintf((char *)key, sizeof(key), "%016llu", seed);
            snprintf((char *)value, sizeof(value), "%llu", seed);
            put_sequence_id++;
            put_count++;
#ifdef REAL_TIME_STATS
            start_ns = asm_rdtscp();
#endif
            // TODO INSERT
            res = hi->insert(thread_id, key, value);
            if (res == true)
            {
                match_insert++;
            }
#ifdef REAL_TIME_STATS
            end_ns = asm_rdtscp();
            latency_ns = cycles_to_ns(cpu_speed_mhz, end_ns - start_ns);
            opt_latency[TEST_PUT].push_back(latency_ns);
            sum_latency[TEST_PUT] += latency_ns;
            sum_latency_count[TEST_PUT]++;
            p99_latency[TEST_PUT].push_back(latency_ns);
#endif
        }
        if (get_count < get_num_kv)
        {
            flag = true;
            test_type = TEST_GET;
            uint64_t seed = (sequence_rw == 1) ? get_sequence_id : get_sequence_id + get_random->Next();
            if (get_count % 3 == 0 || get_count % 5 == 0 || get_count % 7 == 0)
            {
                memset(key, 0, sizeof(key));
                snprintf((char *)key, sizeof(key), "%016llu", seed);
                snprintf((char *)value, sizeof(value), "%llu", seed);
            }
            get_sequence_id++;
            get_count++;
#ifdef REAL_TIME_STATS
            start_ns = asm_rdtscp();
#endif
            // TODO LOOKUP
            if (get_count % 3 == 0 || get_count % 5 == 0 || get_count % 7 == 0)
            {
                real_get_count++;
                res = hi->search(thread_id, key, value);
                if (res)
                {
                    match_search++;
                }
            }
#ifdef REAL_TIME_STATS
            end_ns = asm_rdtscp();
            latency_ns = cycles_to_ns(cpu_speed_mhz, end_ns - start_ns);
            opt_latency[TEST_GET].push_back(latency_ns);
            sum_latency[TEST_GET] += latency_ns;
            sum_latency_count[TEST_GET]++;
            p99_latency[TEST_GET].push_back(latency_ns);
#endif
        }
        if (delete_count < delete_num_kv)
        {
            flag = true;
            test_type = TEST_DELETE;
            memset(key, 0, sizeof(key));
            uint64_t seed = (sequence_rw == 1) ? delete_sequence_id : delete_sequence_id + delete_random->Next();
            snprintf((char *)key, sizeof(key), "%016llu", seed);
            delete_sequence_id++;
            delete_count++;
#ifdef REAL_TIME_STATS
            start_ns = asm_rdtscp();
#endif
            // TODO DELETE
#ifdef REAL_TIME_STATS
            end_ns = asm_rdtscp();
            latency_ns = cycles_to_ns(cpu_speed_mhz, end_ns - start_ns);
            opt_latency[TEST_DELETE].push_back(latency_ns);
            sum_latency[TEST_DELETE] += latency_ns;
            sum_latency_count[TEST_DELETE]++;
            p99_latency[TEST_DELETE].push_back(latency_ns);
#endif
            if (res)
            {
                match_delete++;
            }
        }

        if (scan_count < scan_num_kv)
        {
            flag = true;
            test_type = TEST_SCAN;
            memset(key, 0, sizeof(key));     // lower key
            memset(value, 0, sizeof(value)); // upper key
            uint64_t seed = (sequence_rw == 1) ? scan_sequence_id : scan_sequence_id + scan_random->Next();
            // uint64_t seed = (scan_random->Next() % seed_range);
            snprintf((char *)key, sizeof(key), "%016llu", seed);
            snprintf((char *)value, sizeof(value), "%016llu", seed + scan_range);
            if (memcmp(key, value, KEY_LENGTH) > 0)
            {
                memset(key, 0, sizeof(key));     // lower key
                memset(value, 0, sizeof(value)); // upper key
                snprintf((char *)key, sizeof(key), "%016llu", seed);
                snprintf((char *)value, sizeof(key), "%016llu", seed + scan_range);
            }
            scan_sequence_id++;
            scan_count++;
#ifdef REAL_TIME_STATS
            start_ns = asm_rdtscp();
#endif
            // TODO SCAN
            total_scan_count = hi->range_scan((uint8_t *)key, (uint8_t *)value, scan_buff);
            scan_kv_count += total_scan_count;
            // printf("%zu\n", total_scan_count);
#ifdef REAL_TIME_STATS
            end_ns = asm_rdtscp();
            latency_ns = cycles_to_ns(cpu_speed_mhz, end_ns - start_ns);
            opt_latency[TEST_SCAN].push_back(latency_ns);
            sum_latency[TEST_SCAN] += latency_ns;
            sum_latency_count[TEST_SCAN]++;
            p99_latency[TEST_SCAN].push_back(latency_ns);
#endif
        }

        if (scan_all_count < scan_all)
        {
            // TODO SCAN ALL
        }
        if (!flag)
        {
            break;
        }
#ifdef REAL_TIME_STATS
        timestamp_iops++;
        timestamp_end = asm_rdtscp();
        timestamp_sum += cycles_to_ns(cpu_speed_mhz, timestamp_end - timestamp_begin);
        if (sum_opt_count % collect_interval_opt == 0)
            // if (timestamp_sum >= collect_interval_ns)
        {
            realtime_iops[thread_id].push_back(timestamp_iops); // sum iops
            timestamp_iops = 0;
            timestamp_sum = 0;
            for (int a = 0; a < 32; a++)
            {
                if (sum_latency_count[a] > 0)
                {
                    uint64_t _ns = sum_latency[a] / sum_latency_count[a];
                    uint64_t _iops = (uint64_t)1000 * 1000 * 1000 / _ns;
                    opt_realtime_iops[a].push_back(_iops);  // opt iops (average)
                    opt_realtime_latency[a].push_back(_ns); // opt latency (average)
                }
                sum_latency[a] = 0;
                sum_latency_count[a] = 0;
            }
            for (int a = 0; a < 32; a++)
            {
                if (p99_latency[a].size() > 0)
                {
                    sort(p99_latency[a].begin(), p99_latency[a].end());
                    int p99_index = (int)(0.99 * p99_latency[a].size()) - 1;
                    int p999_index = (int)(0.999 * p99_latency[a].size()) - 1;
                    opt_realtime_p99[a].push_back(p99_latency[a][p99_index]);
                    opt_realtime_p999[a].push_back(p99_latency[a][p999_index]);
                }
                p99_latency[a].clear();
            }
        }
#endif
    }

    // continue to run read benchmark until queue is empty
    num_kv = put_count + real_get_count + delete_count + scan_count + scan_all_count;
    gettimeofday(&end_time, NULL);
    exe_time = (double)(1000000.0 * (end_time.tv_sec - begin_time.tv_sec) + end_time.tv_usec - begin_time.tv_usec) / 1000000.0;
    double throughput = 1.0 * num_kv * (KEY_LENGTH + VALUE_LENGTH) / (exe_time * 1024 * 1024);
    printf("  [Result][%llu]time:%.2f, IOPS:%.2f, Throughput:%.2fMB/s, latency:%.2fns\n", num_kv, exe_time, 1.0 * num_kv / exe_time, throughput, 1000000.0 * 1000 * exe_time / num_kv);

#ifdef COLLECT_STATUS
    printf("  [max PM used]: %.2fMB\n", 1.0 * db->rh_tree->max_nvm_used / (1024 * 1024));
#endif

    if (put_count > 0)
    {
        printf("  [Result]insert match:%llu/%llu(%.2f%%)\n", match_insert, put_count, 100.0 * match_insert / put_count);
    }
    if (get_count > 0)
    {
        printf("  [Result]search match:%llu/%llu(%.2f%%)\n", match_search, real_get_count, 100.0 * match_search / real_get_count);
    }
    if (delete_count > 0)
    {
        printf("  [Result]delete match:%llu/%llu(%.2f%%)\n", match_delete, delete_count, 100.0 * match_delete / delete_count);
    }
    if (scan_count > 0)
    {
        printf("  [Result]scan match:%llu/%llu/%llu\n", scan_kv_count, scan_count, scan_kv_count / scan_count);
    }
    if (scan_all_count > 0)
    {
        double seq_throughput = 1.0 * seq_count / exe_time;
        printf("  [Result]scan count/iops:%llu/%.2f\n", seq_count, seq_throughput);
    }

    args->test_result.iops = 1.0 * num_kv / exe_time;
    args->test_result.throughput = throughput;
    args->test_result.latency = 1000000.0 * 1000 * exe_time / num_kv;
    return NULL;
}

void micro_benchmark(const char *test_name, void *obj, int num_thread, struct test_args test_args[32])
{
    pthread_t tid[32];
    struct test_thread_args *args[32];
    printf(">>[Benchmark] %s ready to run!\n", test_name);

#ifdef REAL_TIME_STATS
    for (int i = 0; i < 32; i++)
    {
        realtime_iops[i].clear();
        opt_latency[i].clear();
        opt_realtime_latency[i].clear();
        opt_realtime_iops[i].clear();
        opt_realtime_p99[i].clear();
        opt_realtime_p999[i].clear();
    }
#endif

    for (int i = 0; i < num_thread; i++)
    {
        args[i] = (struct test_thread_args *)malloc(sizeof(struct test_thread_args));
        args[i]->thread_id = i;
        args[i]->obj = obj;
        memcpy(&args[i]->test_args, &test_args[i], sizeof(struct test_args));
        pthread_create(tid + i, NULL, thread_task, args[i]);
    }

    for (int i = 0; i < num_thread; i++)
    {
        pthread_join(tid[i], NULL);
    }

    double sum_iops = 0;
    double sum_throughput = 0;
    double avg_latency = 0;

    for (int i = 0; i < num_thread; i++)
    {
        sum_iops += args[i]->test_result.iops;
        sum_throughput += args[i]->test_result.throughput;
        avg_latency += args[i]->test_result.latency;
        free(args[i]);
    }

    printf("**[iops:%.2f][throughput:%.2fMB/s][latency:%.2fns]**\n", sum_iops, sum_throughput, avg_latency / num_thread);
}

int main(int argc, char *argv[])
{
    size_t pm_size = 2; // GB
    uint64_t num_server_thread = 1;
    uint64_t num_backend_thread = 1;
    uint64_t num_warm_kv = 0;
    uint64_t num_put_kv = 0;
    uint64_t num_get_kv = 0;
    uint64_t num_delete_kv = 0;
    uint64_t num_scan_kv = 0;
    uint64_t scan_range = 100;
    uint64_t seed = 1234;
    uint64_t scan_all = 0;
    char pmem[128] = "/home/pmem0/pm";
    char pmem_meta[128] = "/home/pmem0/pmMETA";

    for (int i = 0; i < argc; i++)
    {
        double d;
        uint64_t n;
        char junk;
        if (sscanf(argv[i], "--pm_size=%llu%c", &n, &junk) == 1)
        {
            pm_size = n;
        }
        else if (sscanf(argv[i], "--num_server_thread=%llu%c", &n, &junk) == 1)
        {
            num_server_thread = n;
        }
        else if (sscanf(argv[i], "--num_backend_thread=%llu%c", &n, &junk) == 1)
        {
            num_backend_thread = n;
        }
        else if (sscanf(argv[i], "--num_warm=%llu%c", &n, &junk) == 1)
        {
            num_warm_kv = n;
        }
        else if (sscanf(argv[i], "--num_put=%llu%c", &n, &junk) == 1)
        {
            num_put_kv = n;
        }
        else if (sscanf(argv[i], "--num_get=%llu%c", &n, &junk) == 1)
        {
            num_get_kv = n;
        }
        else if (sscanf(argv[i], "--num_delete=%llu%c", &n, &junk) == 1)
        {
            num_delete_kv = n;
        }
        else if (sscanf(argv[i], "--num_scan=%llu%c", &n, &junk) == 1)
        {
            num_scan_kv = n;
        }
        else if (sscanf(argv[i], "--scan_range=%llu%c", &n, &junk) == 1)
        {
            scan_range = n;
        }
        else if (sscanf(argv[i], "--num_scan_all=%llu%c", &n, &junk) == 1)
        {
            scan_all = n;
        }
        else if (sscanf(argv[i], "--seed=%llu%c", &n, &junk) == 1)
        {
            seed = n;
        }
        else if (sscanf(argv[i], "--seq=%llu%c", &n, &junk) == 1)
        {
            if (n == 1)
            {
                sequence_rw = 1;
            }
        }
        else if (i > 0)
        {
            printf("error (%s)!\n", argv[i]);
            return 0;
        }
    }

    struct test_args test_args[32];
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> range(0, 1000);
    struct hikv *hi = new hikv(pm_size * 1024 * 1024 * 1024, num_server_thread, num_backend_thread, num_server_thread * (num_put_kv + num_warm_kv), pmem, pmem_meta);

    for (int i = 0; i < num_server_thread; i++)
    {
        test_args[i].put_num_kv = num_warm_kv, test_args[i].put_sequence_id = 1, test_args[i].put_seed = 1000 + i;
        test_args[i].get_num_kv = 0;
        test_args[i].scan_num_kv = 0;
        test_args[i].delete_num_kv = 0;
        test_args[i].scan_all = 0;
    }
    micro_benchmark("warmup test", (void *)hi, num_server_thread, test_args);
    printf("----------------------\n");

    if (hi->tq != NULL)
    {
        while (!hi->tq->all_queue_empty())
            ;
    }

    for (int i = 0; i < num_server_thread; i++)
    {
        test_args[i].put_num_kv = num_put_kv, test_args[i].put_sequence_id = 1, test_args[i].put_seed = 5000 + i;
        test_args[i].get_num_kv = num_get_kv, test_args[i].get_sequence_id = 1, test_args[i].get_seed = 1000 + i;
        test_args[i].scan_num_kv = num_scan_kv, test_args[i].scan_sequence_id = 1, test_args[i].scan_seed = 1000 + i;
        test_args[i].scan_range = scan_range;
        test_args[i].delete_num_kv = 0;
        test_args[i].scan_all = 0;
    }
    micro_benchmark("run test", (void *)hi, num_server_thread, test_args);

#ifdef REAL_TIME_STATS
    // output_sort_latency("rhdb_real_latency"); // print real latency
    P99(false); // sort read latency
    // output_realtime_iops("thread_iops");
    // output_realtime_opt_iops("rhdb_opt_iops");
    // output_sort_latency("rhdb_sort_latency");
    output_realtime_latency("rhdb_avg_latency");
    output_realtime_p99_latency("rhdb_tail_latency");
#endif
    printf("----------------------\n");
    delete hi;
    return 0;
}
