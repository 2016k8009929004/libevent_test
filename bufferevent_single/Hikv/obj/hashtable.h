#ifndef NVM_HASH_TABLE_H_
#define NVM_HASH_TABLE_H_

#include "city.h"
#include "pflush.h"
#include "spinlock.h"
#include "pm_alloc.h"
#include "btree.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>

#define NUM_SLOT (4)
#define SEED1 (4396)
#define SEED2 (1995)

struct hash_bucket // 64B
{
    uint64_t signature[NUM_SLOT]; // 32B
    uint64_t addr[NUM_SLOT];      // 32B
};

struct hash_table
{
    public:
        hash_table(size_t num_partition, size_t num_bucket, struct pm_alloctor *allocator)
        {
            this->num_partition = num_partition;
            this->num_bucket = num_bucket;
            this->allocator = allocator;
            memset(flush_count, 0, sizeof(flush_count));

            if (allocator == NULL)
            {
                for (int i = 0; i < num_partition; i++)
                {
                    partition[i] = (struct hash_partition *)malloc(num_bucket * sizeof(struct hash_bucket));
                    memset(partition[i], 0, num_bucket * sizeof(struct hash_bucket));
                }
            }
            else
            {
                for (int i = 0; i < num_partition; i++)
                {
                    partition[i] = (struct hash_partition *)allocator->alloc(i, num_bucket * sizeof(struct hash_bucket));
                    memset(partition[i], 0, num_bucket * sizeof(struct hash_bucket));
                }
            }
        }

        bool insert(int id, uint8_t *key, uint64_t value)
        {
            bool flag = false;
            bool same = false;
            int partition_id = id; // CityHash32((const char *)key, KEY_LENGTH) % num_partition;
            uint64_t hash64_1 = CityHash64WithSeed((const char *)key, KEY_LENGTH, SEED1);
            uint64_t hash64_2 = CityHash64WithSeed((const char *)key, KEY_LENGTH, SEED2);
            uint64_t bucket_id1 = hash64_1 % num_bucket;
            uint64_t bucket_id2 = hash64_2 % num_bucket;
            struct hash_bucket *bucket1 = (struct hash_bucket *)partition[partition_id] + bucket_id1;
            struct hash_bucket *bucket2 = (struct hash_bucket *)partition[partition_id] + bucket_id2;

            if (bucket_id1 == bucket_id2)
            {
                same = true;
            }

            for (int i = 0; i < NUM_SLOT; i++)
            {
                if (bucket1->signature[i] == 0)
                {
                    bucket1->addr[i] = value;
#ifdef PERSIST_HASHTABLE
                    persist_data((void *)&(bucket1->addr[i]), sizeof(bucket1->addr[i]));
#endif
                    bucket1->signature[i] = hash64_1;
#ifdef PERSIST_HASHTABLE
                    persist_data((void *)&(bucket1->signature[i]), sizeof(bucket1->signature[i]));
#endif
                    flag = true;
                    break;
                }
                if (!same && bucket2->signature[i] == 0)
                {
                    bucket2->addr[i] = value;
#ifdef PERSIST_HASHTABLE
                    persist_data((void *)&(bucket2->addr[i]), sizeof(bucket2->addr[i]));
#endif
                    bucket2->signature[i] = hash64_2;
#ifdef PERSIST_HASHTABLE
                    persist_data((void *)&(bucket2->signature[i]), sizeof(bucket2->signature[i]));
#endif
                    flag = true;
                    break;
                }
            }
            return flag;
        }

        uint64_t search(int id, uint8_t *key)
        {
            uint64_t addr = 0;
            int partition_id = id; // CityHash32((const char *)key, KEY_LENGTH) % num_partition;
            uint64_t hash64_1 = CityHash64WithSeed((const char *)key, KEY_LENGTH, SEED1);
            uint64_t hash64_2 = CityHash64WithSeed((const char *)key, KEY_LENGTH, SEED2);
            uint64_t bucket_id1 = hash64_1 % num_bucket;
            uint64_t bucket_id2 = hash64_2 % num_bucket;
            struct hash_bucket *bucket1 = (struct hash_bucket *)partition[partition_id] + bucket_id1;
            struct hash_bucket *bucket2 = (struct hash_bucket *)partition[partition_id] + bucket_id2;

            for (int i = 0; i < NUM_SLOT; i++)
            {
                if (bucket1->signature[i] == hash64_1)
                {
                    addr = bucket1->addr[i];
                    break;
                }
                if (bucket2->signature[i] == hash64_2)
                {
                    addr = bucket2->addr[i];
                    break;
                }
            }

            return addr;
        }

        void print()
        {
            size_t total_size = num_partition * num_bucket * sizeof(struct hash_bucket);
            size_t sum_flush_count = 0;

            for (int i = 0; i < num_partition; i++)
            {
                sum_flush_count += flush_count[i];
            }

            printf(">>[HashTable]\n");
            printf("  partition/bucket/slot:%d/%d/%d\n", num_partition, num_bucket, NUM_SLOT);
            printf("  memory usage:%zuMB\n", total_size / (1024 * 1024));
            printf("  flush count:%zu\n", sum_flush_count);
        }

    public:
        int num_partition;
        int num_bucket;
        struct pm_alloctor *allocator;
        struct hash_partition *partition[32];

    public:
        size_t flush_count[32];
};

#endif
