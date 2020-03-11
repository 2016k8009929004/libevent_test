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

#define TEST_PUT (0)
#define TEST_GET (1)
#define TEST_UPDATE (2)
#define TEST_DELETE (3)
#define TEST_SCAN (4)
#define TEST_SEQ (5)

int sequence_rw = 0;
static const int core_in_numa0[48] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                      13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                      48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
                                      60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71};

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

#define REAL_TIME_STATS
#ifdef REAL_TIME_STATS

uint64_t collect_interval_ns = (uint64_t)5 * 1000 * 1000 * 1000; // 5 * 1 seconds (10^9ns)
uint64_t collect_interval_opt = 400000;
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

static void output_sort_latency(const char *_name, uint64_t latency)
{
    char name[128];
    for (int i = 0; i < 32; i++)
    {
        if (opt_latency[i].size() != 0)
        {
            sprintf(name, "%s_%llu_%d.latency", _name, latency, i);
            file_write(name, opt_latency[i]);
        }
    }
}

// print a thread realtime iops
static void output_realtime_iops(const char *_name, uint64_t latency)
{
    char name[128];

    for (int i = 0; i < 32; i++)
    {
        if (realtime_iops[i].size() != 0)
        {
            sprintf(name, "%s_%llu_%d.iops", _name, latency, i);
            file_write(name, realtime_iops[i]);
        }
    }
}

// print average latency
static void output_realtime_latency(const char *_name, uint64_t latency)
{
    char name[128];

    for (int i = 0; i < 32; i++)
    {
        if (opt_realtime_latency[i].size() != 0)
        {
            sprintf(name, "%s_%llu_%d.real_latency", _name, latency, i);
            file_write(name, opt_realtime_latency[i]);
        }
    }
}

static void output_realtime_opt_iops(const char *_name, uint64_t latency)
{
    char name[128];

    for (int i = 0; i < 32; i++)
    {
        if (opt_realtime_latency[i].size() != 0)
        {
            sprintf(name, "%s_%llu_%d.iops", _name, latency, i);
            file_write(name, opt_realtime_iops[i]);
        }
    }
}

static void output_realtime_p99_latency(const char *_name, uint64_t latency)
{
    char name[128];

    for (int i = 0; i < 32; i++)
    {
        if (opt_realtime_p99[i].size() != 0)
        {
            sprintf(name, "%s_%llu_%d.p99latency", _name, latency, i);
            file_write(name, opt_realtime_p99[i]);
        }
        if (opt_realtime_p999[i].size() != 0)
        {
            sprintf(name, "%s_%llu_%d.p999latency", _name, latency, i);
            file_write(name, opt_realtime_p999[i]);
        }
    }
}

#endif

void *thread_task(void *test_args)
{
    bool res;
    uint64_t match_insert = 0;
    uint64_t match_search = 0;
    uint64_t match_delete = 0;
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

    uint64_t num_kv = get_num_kv + put_num_kv + delete_num_kv + scan_num_kv;
    uint64_t get_count = 0;
    uint64_t put_count = 0;
    uint64_t delete_count = 0;
    uint64_t scan_count = 0;
    uint64_t scan_kv_count = 0;
    uint64_t scan_all_count = 0;
    uint64_t seq_count = 0;

    Random *get_random = new Random(get_seed);
    Random *put_random = new Random(put_seed);
    Random *delete_random = new Random(delete_seed);
    Random *scan_random = new Random(scan_seed);

    struct timeval begin_time, end_time;
    uint64_t start_ns, end_ns, latency_ns;
    uint64_t timestamp_sum, timestamp_begin, timestamp_end, timestamp_iops;
    uint64_t sum_latency[32] = {0};
    uint64_t sum_latency_count[32] = {0};

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

    while (true)
    {
        bool flag = false;
        int test_type = -1;
        sum_opt_count++;
        if (put_count < put_num_kv)
        {
            flag = true;
            test_type = TEST_PUT;
            memset(key, 0, sizeof(key));
            memset(value, 0, sizeof(value));
            uint64_t seed = (sequence_rw == 1) ? put_sequence_id : put_sequence_id + put_random->Next();
            // uint64_t seed = (sequence_rw == 1) ? put_sequence_id : (put_random->Next()) % put_num_kv;
            snprintf((char *)key, sizeof(key), "%016llu", seed);
            snprintf((char *)value, sizeof(value), "%llu", seed);
            put_sequence_id++;
            put_count++;
#ifdef REAL_TIME_STATS
            start_ns = asm_rdtscp();
#endif
            res = hi->insert(thread_id, key, value);
            if (res == true)
            {
                match_insert++;
            }
#ifdef REAL_TIME_STATS
            end_ns = asm_rdtscp();
#endif
        }
        if (get_count < get_num_kv)
        {
            flag = true;
            test_type = TEST_GET;
            memset(key, 0, sizeof(key));
            uint64_t seed = (sequence_rw == 1) ? get_sequence_id : get_sequence_id + get_random->Next();
            // uint64_t seed = (sequence_rw == 1) ? get_sequence_id : (get_random->Next()) % get_num_kv;
            snprintf((char *)key, sizeof(key), "%016llu", seed);
            snprintf((char *)value, sizeof(value), "%llu", seed);
            get_sequence_id++;
            get_count++;
#ifdef REAL_TIME_STATS
            start_ns = asm_rdtscp();
#endif
            res = hi->search(thread_id, key, value);
            if (res == true)
            {
                match_search++;
            }
#ifdef REAL_TIME_STATS
            end_ns = asm_rdtscp();
#endif
        }
        if (!flag)
        {
            break;
        }
    }
    gettimeofday(&end_time, NULL);
    exe_time = (double)(1000000.0 * (end_time.tv_sec - begin_time.tv_sec) + end_time.tv_usec - begin_time.tv_usec) / 1000000.0;
    double throughput = 1.0 * num_kv * (KEY_LENGTH + VALUE_LENGTH) / (exe_time * 1024 * 1024);
    printf("  [Result]time:%.2f, IOPS:%.2f, Throughput:%.2fMB/s, latency:%.2fns\n", exe_time, 1.0 * num_kv / exe_time, throughput, 1000000.0 * 1000 * exe_time / num_kv);
    args->test_result.iops = 1.0 * num_kv / exe_time;
    args->test_result.throughput = throughput;
    args->test_result.latency = 1000000.0 * 1000 * exe_time / num_kv;
    printf("  [Match Search] : %llu/%llu\n", match_search, get_count);
    printf("  [Match Insert] : %llu/%llu\n", match_insert, put_count);
    return NULL;
}

void micro_benchmark(const char *test_name, void *obj, int num_thread, struct test_args test_args[32])
{
    pthread_t tid[32];
    struct test_thread_args *args[32];
    printf(">>[Benchmark] %s ready to run!\n", test_name);

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

    random_device rd;
    mt19937 gen(rd());
    struct test_args test_args[32];
    uniform_int_distribution<> range(0, 1000);
    struct hikv *hi = new hikv(pm_size * 1024 * 1024 * 1024, num_server_thread, num_backend_thread, num_server_thread * num_put_kv, pmem);

    for (int i = 0; i < num_server_thread; i++)
    {
        test_args[i].put_num_kv = num_put_kv, test_args[i].put_sequence_id = 1, test_args[i].put_seed = 1000 + i; // range(gen); // seed;
        test_args[i].get_num_kv = 0;
        test_args[i].scan_num_kv = 0;
        test_args[i].delete_num_kv = 0;
        test_args[i].scan_all = 0;
    }
    micro_benchmark("random write test", (void *)hi, num_server_thread, test_args);
    printf("----------------------\n");

    if (hi->tq != NULL)
    {
        while (!hi->tq->all_queue_empty())
            ;
    }

    for (int i = 0; i < num_server_thread; i++)
    {
        test_args[i].put_num_kv = 0;
        test_args[i].get_num_kv = num_get_kv, test_args[i].get_sequence_id = 1, test_args[i].get_seed = test_args[i].put_seed;
        test_args[i].scan_num_kv = 0;
        test_args[i].delete_num_kv = 0;
        test_args[i].scan_all = 0;
    }
    micro_benchmark("random read test", (void *)hi, num_server_thread, test_args);
    printf("----------------------\n");

    if (hi->tq != NULL)
    {
        while (!hi->tq->all_queue_empty())
            ;
    }
    hi->print();
    printf("----------------------\n");
    return 0;
}