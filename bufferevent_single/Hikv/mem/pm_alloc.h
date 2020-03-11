#ifndef INCLUDE_PM_ALLOC_H_
#define INCLUDE_PM_ALLOC_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>

struct pm_alloctor
{
    public:
        pm_alloctor(const char *pool_name, size_t num_paritition, size_t pm_size);
        ~pm_alloctor();
        void *alloc(size_t id, size_t size);
        void print();

    public:
        bool is_pm;
        size_t pm_size;                       // persistent memory total size
        size_t num_partition;                 // partition count
        uint64_t pm_start_address;            // persistent memory mmap start address
        size_t partition_size[32];            // partition size
        uint64_t partition_start_address[32]; // partition start address
        uint64_t partition_end_address[32];   // paritition end address
        uint64_t partition_cur_offset[32];    // partition current pointer
};

#endif
