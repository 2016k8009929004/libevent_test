#include "btree.h"
#include "threadpool.h"

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

static int core_in_numa0[48] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71};

task_queue::task_queue(size_t num_server_thread, size_t num_backend_thread)
{
    this->num_queue = num_backend_thread;
    this->num_server_thread = num_server_thread;
    this->num_backend_thread = num_backend_thread;
}

static void *thread_pool_task_handler(void *thread_args)
{
    struct queue_args *args = (struct queue_args *)thread_args;
    int id = args->id;
    struct queue_task task;
    struct task_queue *task_queue = (struct task_queue *)args->tq;

    /* binding a core thread to a background thread */
#ifdef THREAD_BIND_CPU
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_in_numa0[(int)args->id + task_queue->num_server_thread], &mask);
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
    {
        printf("threadpool, set thread affinity failed.\n");
    }
#endif

    while (true)
    {
        if (args->tq->s_tbb_queue[id].unsafe_size() > 0)
        {
            bool found = args->tq->s_tbb_queue[id].try_pop(task);
            if (found)
            {
                btree *bt = (btree *)task.argv[0];
                entry_key_t key;
                memcpy((void *)key.key, (void *)task.argv[1], KEY_LENGTH);
                switch (task.type)
                {
                    case TASK_BTREE_INSERT:
                        bt->btree_insert(id, key, (char *)task.argv[1]);
                        break;
                    case TASK_BTREE_DELETE:
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

bool task_queue::init()
{
    /* create backend thread */
    for (int i = 0; i < num_queue; i++)
    {
        struct queue_args *args = (struct queue_args *)malloc(sizeof(struct queue_args));
        args->id = i;
        args->tq = this;
        pthread_create(this->pthread_id + i, NULL, thread_pool_task_handler, (void *)args);
    }
    return true;
}

bool task_queue::add_task(int id, int type, void *argv[8])
{
    while (true)
    {
        /* Max Queue Size */
        if (this->s_tbb_queue[id].unsafe_size() < (1 << 16))
        {
            s_tbb_queue[id].push(queue_task(type, argv));
            break;
        }
    }
    return true;
}

bool task_queue::all_queue_empty()
{
    for (int i = 0; i < this->num_queue; i++)
    {
        if (this->s_tbb_queue[i].unsafe_size() > 0)
        {
            return false;
        }
    }
    return true;
}

void task_queue::print()
{
}
