#ifndef INCLUDE_HIKV_H_
#define INCLUDE_HIKV_H_

#include "plog.h"
#include "config.h"
#include "hashtable.h"
#include "threadpool.h"
#include "libpmem.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>

struct hikv;

struct arg_t {
    struct hikv *hikv;
    int i;
};

void* bt_by_ht_ptn(void* p_arg);

struct hikv
{
    public:
        hikv(size_t pm_size, size_t num_server_thread, size_t num_backend_thread, size_t num_kv, const char *pmem, const char *pmem_meta)
        {
            this->pmem_meta = pmem_meta;
#ifdef PM_USED
            int res = restore(pmem, pmem_meta);
            if (res != 0) {
#endif
            /* use NVM to establish a memory pool (partition) */
            allocator = new pm_alloctor(pmem, num_server_thread, pm_size);
            size_t num_partition = num_server_thread;
            size_t num_bucket = (num_kv * 4) / (num_partition * NUM_SLOT);
            /* hashing index */
#ifdef PERSIST_HASHTABLE
            ht = new hash_table(num_partition, num_bucket, allocator);
#else
            /* use pm_allocator to establish a hashtable index */
            ht = new hash_table(num_partition, num_bucket, NULL);
#endif
            /* B+-Tree index (FAST-FAIR B+-Tree) */
#ifdef PERSIST_BTREE
            bt = new btree(allocator);
#else
            bt = new btree(NULL);
#endif
            /* Partition Logging */
            log = new plog(num_partition, num_kv, allocator);
#ifdef PM_USED
            }
#endif
#ifdef HIKV_ASYNC
            tq = new task_queue(num_server_thread, num_backend_thread);
            tq->init();
#else
            tq = NULL;
#endif
            print();
        }

        ~hikv()
        {
            save(pmem_meta);
            delete (allocator); // unmmap total persistent memory
        }

        void save(const char *pmem_meta)
        {
#ifdef PM_USED
            ofstream outf;
            outf.open(pmem_meta);
            if (!outf)
            {
                printf("Error: Metadata file can not open !\n");
                outf.close();
                return;
            }
            uint64_t np = (uint64_t)allocator->num_partition;
            uint64_t tmp;
            outf << (uint64_t)allocator->pm_size << endl;
            outf << np << endl;
            for (int i = 0; i < np; ++i)
            {
                outf << allocator->partition_start_address[i] - allocator->pm_start_address << " ";
                outf << allocator->partition_end_address[i] - allocator->pm_start_address << " ";
                outf << allocator->partition_cur_offset[i] << endl;
            }

            outf << ht->num_bucket << endl;
            for (int i = 0; i < np; ++i)
            {
                tmp = (uint64_t)(ht->partition[i]) - allocator->pm_start_address;
                outf << tmp << endl;
            }

            for (int i = 0; i < np; ++i)
            {
                outf << log->partition_start_address[i] - allocator->pm_start_address << " ";
                outf << log->partition_cur_offset[i] << " ";
                tmp = (uint64_t)(log->partition_size[i]);
                outf << tmp << endl;
            }
            outf.close();
#endif
        }

        void bt_restore_by_ht_ptn(int i) {
            struct hash_bucket *bucket;
            struct kv_item *item;
            entry_key_t entry_key;
            uint64_t addr;
            for (int j = 0; j < ht->num_bucket; ++j)
            {
                bucket = (struct hash_bucket *)ht->partition[i] + j;
                for (int k = 0; k < NUM_SLOT; ++k)
                {
                    if (bucket->signature[k] != 0)
                    {
                        addr = bucket->addr[k] + log->partition_start_address[i];
                        if (addr != 0)
                        {
                            item = (struct kv_item *)addr;
                            memcpy((void *)entry_key.key, item->key, KEY_LENGTH);
                            bt->btree_insert(i, entry_key, (char *)addr);
                        }
                    }
                }
            }
        }

        void bt_restore_by_ht()
        {
            int nums = ht->num_partition;
            int i = 0;
            struct arg_t* args[nums];
            pthread_t tid[nums];
            for (i = 0; i < nums; ++i) {
                args[i] = (struct arg_t *)malloc(sizeof(struct arg_t));
                args[i]->hikv = this;
                args[i]->i = i;
            }
            for (i = 0; i < nums; ++i)
            {
                pthread_create(&tid[i], NULL, bt_by_ht_ptn, args[i]);
            }
            for (i = 0; i < nums; ++i) {
                pthread_join(tid[i], NULL);
            }
        }

        int restore(const char *pmem, const char *pmem_meta)
        {
#ifdef PM_USED
            ifstream inf;
            inf.open(pmem_meta);
            if (!inf)
            {
                printf("#RESTORE# Metadata file can not open, so create a new hikv\n");
                inf.close();
                return -1;
            }
            uint64_t np;
            uint64_t pm_size;
            allocator = (struct pm_alloctor *)malloc(sizeof(struct pm_alloctor));
            inf >> pm_size;
            inf >> np;
            allocator->num_partition = np;

#ifdef PMDK_USED
            size_t mapped_len;
            int is_pmem;
            /* create a new memory pool file */
            // void *dest = pmem_map_file(pool_name, pm_size, 0, 0666, &mapped_len, &is_pmem);
            void *dest = pmem_map_file(pmem, 0, 0, 0666, &mapped_len, &is_pmem);
            assert(mapped_len == pm_size);
            pm_size = (uint64_t)mapped_len;
            printf("#RESTORE# Use PMDK to mmap file! (%s) (%zuMB/%zuMB) (%d)\n", pmem, pm_size / (1024 * 1024), mapped_len / (1024 * 1024), is_pmem);
#else
            int fd = open(pmem, O_RDWR);
            void *dest = mmap(NULL, pm_size, PROT_READ | PROT_WRITE, MAP_SHARED /*| MAP_POPULATE*/, fd, 0);
#endif
            assert(dest != NULL);
            allocator->pm_start_address = (uint64_t)dest;
            allocator->pm_size = pm_size;
            printf("#RESTORE# Persistent Memory Pool Start Address : 0x%llx\n", (uint64_t)dest);

            uint64_t tmp;
            for (int i = 0; i < np; ++i)
            {
                inf >> tmp;
                allocator->partition_start_address[i] = tmp + allocator->pm_start_address;
                inf >> tmp;
                allocator->partition_end_address[i] = tmp + allocator->pm_start_address;
                inf >> tmp;
                allocator->partition_cur_offset[i] = tmp;
                allocator->partition_size[i] = allocator->partition_end_address[i] - allocator->partition_start_address[i];
            }

            ht = (struct hash_table *)malloc(sizeof(struct hash_table));
            ht->num_partition = np;
            ht->allocator = allocator;
            inf >> ht->num_bucket;
            for (int i = 0; i < np; ++i)
            {
                inf >> tmp;
                ht->partition[i] = (struct hash_partition *)(tmp + allocator->pm_start_address);
            }
            memset(ht->flush_count, 0, sizeof(size_t) * 32);

            log = (struct plog *)malloc(sizeof(struct plog));
            log->num_partition = np;
            for (int i = 0; i < np; ++i)
            {
                inf >> tmp;
                log->partition_start_address[i] = tmp + allocator->pm_start_address;
                inf >> log->partition_cur_offset[i];
                inf >> tmp;
                log->partition_size[i] = (size_t)tmp;
            }

            bt = new btree(allocator);
            struct timeval ts;
            struct timeval te;
            gettimeofday(&ts, NULL);
            bt_restore_by_ht();
            gettimeofday(&te, NULL);
            long long utime = (te.tv_sec - ts.tv_sec) * 1000 * 1000 + (te.tv_usec - ts.tv_usec);
            std::cout << "BTree restore time: " << utime << "us" << std::endl;
            inf.close();
#endif
            return 0;
        }

        bool insert(int id, uint8_t *key, uint8_t *value)
        {
            bool res;
            uint64_t addr = log->append(id, key, value);
//            res = ht->insert(id, key, addr);
            res = ht->insert(id, key, addr - log->partition_start_address[id]);
#ifdef HIKV_ASYNC
            void *argv[8];
            argv[0] = (void *)bt;
            argv[1] = (void *)addr;
            tq->add_task(id, TASK_BTREE_INSERT, argv);
#else
            entry_key_t entry_key;
            memcpy((void *)entry_key.key, key, KEY_LENGTH);
            bt->btree_insert(id, entry_key, (char *)addr);
#endif
            return res;
        }

        bool search(int id, uint8_t *key, uint8_t *value)
        {
            uint64_t addr;
            addr = ht->search(id, key);
            if (addr != 0)
            {
//                struct kv_item *item = (struct kv_item *)addr;
                struct kv_item *item = (struct kv_item *)(addr + log->partition_start_address[id]);
                if (memcmp(item->key, key, KEY_LENGTH) == 0)
                {
                    memcpy(value, item->value, VALUE_LENGTH);
                    return true;
                }
                else
                {
                    return false;
                }
            }
            return false;
        }

        bool search_part(int id, uint8_t *key, uint8_t *value, uint64_t offset, uint64_t size)
        {
            uint64_t addr;
            addr = ht->search(id, key);
            if (addr != 0)
            {
//                struct kv_item *item = (struct kv_item *)addr;
                struct kv_item *item = (struct kv_item *)(addr + log->partition_start_address[id]);
                if (memcmp(item->key, key, KEY_LENGTH) == 0)
                {
                    assert(offset + size <= VALUE_LENGTH);
                    memcpy(value, item->value + offset, size);
                    return true;
                }
                else
                {
                    return false;
                }
            }
            return false;
        }

        size_t range_scan(uint8_t *lower_key, uint8_t *upper_key, char *buff)
        {
            size_t cnt = 0;
            unsigned long *ptr = (unsigned long *)buff;
            entry_key_t entry_lower_key;
            entry_key_t entry_upper_key;
            uint8_t value[VALUE_LENGTH];
            memcpy((void *)entry_lower_key.key, (void *)lower_key, KEY_LENGTH);
            memcpy((void *)entry_upper_key.key, (void *)upper_key, KEY_LENGTH);
            cnt = bt->btree_search_range(entry_lower_key, entry_upper_key, (unsigned long *)buff);
            for (int i = 0; i < cnt; i++)
            {
                struct kv_item *item = (struct kv_item *)ptr[i];
                memcpy(value, item->value, VALUE_LENGTH);
            }
            return cnt;
        }

        void print()
        {
            printf(">>[HiKV]\n");
#ifdef HIKV_ASYNC
            printf("  HIKV ASYNC!\n");
#else
            printf("  HIKV SYNC!\n");
#endif
#ifdef PERSIST_BTREE
            printf("  B+-Tree is persistent\n");
#else
            printf("  B+-Tree is non-persistent\n");
#endif
#ifdef PERSIST_HASHTABLE
            printf("  HashTable is persistent\n");
#else
            printf("  HashTable is non-persistent\n");
#endif
            allocator->print();
            log->print();
            ht->print();
            bt->print();
        }

    private:
        struct pm_alloctor *allocator;
        btree *bt;
        struct hash_table *ht;
        struct plog *log;
        const char *pmem_meta;

    public:
        struct task_queue *tq;
};

void* bt_by_ht_ptn(void* p_arg) {
    struct arg_t *arg = (struct arg_t *)p_arg;
    arg->hikv->bt_restore_by_ht_ptn(arg->i);
}

#endif
