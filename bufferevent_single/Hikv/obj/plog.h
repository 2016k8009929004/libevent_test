#ifndef INCLUDE_PLOG_H_
#define INCLUDE_PLOG_H_

#include "pm_alloc.h"
#include "config.h"
#include "ntstore.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>

/* We ensure 16B align to use NT Store */
struct plog
{
    public:
        plog(size_t num_partition, size_t num_kv, struct pm_alloctor *allocator)
        {
            size_t p_size = (num_kv / num_partition) * sizeof(struct kv_item) * 3 / 2; // 1.5x space
            this->num_partition = num_partition;

            if (allocator == NULL)
            {
                for (int i = 0; i < num_partition; i++)
                {
                    partition_start_address[i] = (uint64_t)malloc(p_size);
                    if (partition_start_address[i] % 0x10 != 0) // align(16)
                    {
                        partition_start_address[i] += 0x10;
                        partition_start_address[i] &= (~((uint64_t)16 - 1));
                    }
                    partition_cur_offset[i] = 0;
                    partition_size[i] = p_size;
                }
            }
            else
            {
                for (int i = 0; i < num_partition; i++)
                {
                    partition_start_address[i] = (uint64_t)allocator->alloc(i, p_size);
                    if (partition_start_address[i] % 0x10 != 0) // align(16)
                    {
                        partition_start_address[i] += 0x10;
                        partition_start_address[i] &= (~((uint64_t)16 - 1));
                    }
                    partition_cur_offset[i] = 0;
                    partition_size[i] = p_size;
                }
            }
        }

        uint64_t append(int id, uint8_t *key, uint8_t *value)
        {
            int partition_id = id; // CityHash32((const char *)key, KEY_LENGTH) % num_partition;
            uint64_t addr = (uint64_t)(partition_start_address[partition_id] + partition_cur_offset[partition_id]);
            assert(addr % 16 == 0);
            struct kv_item *p = (struct kv_item *)addr;
            nvmem_memcpy((char *)p->key, (char *)key, KEY_LENGTH);
            nvmem_memcpy((char *)p->value, (char *)value, VALUE_LENGTH);
            // memcpy(p->key, key, KEY_LENGTH);
            // memcpy(p->value, value, VALUE_LENGTH);
            // persist_data(p, sizeof(struct kv_item));
            partition_cur_offset[partition_id] += sizeof(struct kv_item);
            return addr;
        }

        void print()
        {
            size_t total_size = 0;

            for (int i = 0; i < num_partition; i++)
            {
                total_size += partition_size[i];
            }

            printf(">>[Persistent Logging]\n");
            printf("  memory usage:%zuMB\n", total_size / (1024 * 1024));

            for (int i = 0; i < num_partition; i++)
            {
                printf("  [%d][0x%llx][%lluMB/%zuMB(%.2f%%)]\n", i, partition_start_address[i], partition_cur_offset[i] / (1024 * 1024), partition_size[i] / (1024 * 1024), 100.0 * partition_cur_offset[i] / partition_size[i]);
            }
        }

    public:
        size_t num_partition;
        size_t partition_size[32];
        uint64_t partition_start_address[32];
        uint64_t partition_cur_offset[32];
};

#endif
