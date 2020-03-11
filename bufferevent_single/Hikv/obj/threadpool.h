#ifndef INCLUDE_THREAD_POOL_H_
#define INCLUDE_THREAD_POOL_H_

#include <stdint.h>
#include <pthread.h>

#include "config.h"
#include "btree.h"
#include "concurrent_queue.h"

#define MAX_BACKEND_THREAD (16)
#define TASK_BTREE_INSERT (1)
#define TASK_BTREE_DELETE (2)

struct queue_task
{
    public:
        int type;
        void *argv[8];

    public:
        queue_task()
        {
            type = 0;
        }

        queue_task(int type, void *argv[8])
        {
            this->type = type;
            for (int i = 0; i < 8; i++)
            {
                this->argv[i] = argv[i];
            }
        }
};

struct task_queue
{
    public:
        task_queue(size_t num_server_thread, size_t num_backend_thread);
        bool init();
        bool all_queue_empty();
        bool add_task(int id, int type, void *argv[8]);
        void print();

    public:
        size_t num_queue;
        size_t num_server_thread;
        size_t num_backend_thread;
        pthread_t pthread_id[MAX_BACKEND_THREAD];

    public:
        tbb::concurrent_queue<struct queue_task> s_tbb_queue[MAX_BACKEND_THREAD];
};

struct queue_args
{
    public:
        int id;
        struct task_queue *tq;
};

#endif
