/*
   Copyright (c) 2018, UNIST. All rights reserved.  The license is a free
   non-exclusive, non-transferable license to reproduce, use, modify and display
   the source code version of the Software, with or without modifications solely
   for non-commercial research, educational or evaluation purposes. The license
   does not entitle Licensee to technical support, telephone assistance,
   enhancements or updates to the Software. All rights, title to and ownership
   interest in the Software, including all intellectual property rights therein
   shall remain in UNIST. 
   */

#ifndef INCLUDE_BTREE_H_
#define INCLUDE_BTREE_H_

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string.h>
#include <cassert>
#include <climits>
#include <future>
#include <mutex>

#define PAGESIZE 512

#define CPU_FREQ_MHZ (1994)
#define DELAY_IN_NS (1000)
#define CACHE_LINE_SIZE 64
#define QUERY_NUM 25

#define IS_FORWARD(c) (c % 2 == 0)

#include "config.h"
#include "pflush.h"
#include "pm_alloc.h"

struct entry_key_t
{
    char key[KEY_LENGTH];

    bool operator>(entry_key_t &k)
    {
        if (memcmp(this->key, k.key, KEY_LENGTH) > 0)
        {
            return true;
        }
        return false;
    }

    bool operator>=(entry_key_t &k)
    {
        if (memcmp(this->key, k.key, KEY_LENGTH) >= 0)
        {
            return true;
        }
        return false;
    }

    entry_key_t &operator=(entry_key_t &k)
    {
        memcpy(this->key, k.key, KEY_LENGTH);
        return (*this);
    }

    bool operator<(entry_key_t &k)
    {
        if (memcmp(this->key, k.key, KEY_LENGTH) < 0)
        {
            return true;
        }
        return false;
    }

    bool operator<=(entry_key_t &k)
    {
        if (memcmp(this->key, k.key, KEY_LENGTH) <= 0)
        {
            return true;
        }
        return false;
    }

    bool operator==(entry_key_t &k)
    {
        if (memcmp(this->key, k.key, KEY_LENGTH) == 0)
        {
            return true;
        }
        return false;
    }
};

using namespace std;

class page;

class btree
{
    private:
        int height;
        char *root;
        struct pm_alloctor *allocator;

    private:
        size_t page_count[32];
        size_t num_kv[32];
        size_t flush_count[32];

    public:
        btree(struct pm_alloctor *allocator);
        page *getNewPage(int id, page *left, entry_key_t key, page *right, uint32_t level);
        page *getNewPage(int id, uint32_t level);
        void setNewRoot(char *);
        void getNumberOfNodes();
        void flush_count_add(int id);
        void btree_insert(int id, entry_key_t &, char *);
        void btree_insert_internal(int id, char *, entry_key_t &, char *, uint32_t);
        void btree_delete(entry_key_t &);
        void btree_delete_internal(entry_key_t &, char *, uint32_t, entry_key_t *, bool *, page **);
        char *btree_search(entry_key_t &);
        size_t btree_search_range(entry_key_t &, entry_key_t &, unsigned long *);
        void print();
        friend class page;
};

class header
{
    private:
        page *leftmost_ptr;     // 8 bytes
        page *sibling_ptr;      // 8 bytes
        uint32_t level;         // 4 bytes
        uint8_t switch_counter; // 1 bytes
        uint8_t is_deleted;     // 1 bytes
        int16_t last_index;     // 2 bytes
        std::mutex *mtx;        // 8 bytes

        friend class page;
        friend class btree;

    public:
        header()
        {
            mtx = new std::mutex();
            leftmost_ptr = NULL;
            sibling_ptr = NULL;
            switch_counter = 0;
            last_index = -1;
            is_deleted = false;
        }

        ~header()
        {
            delete mtx;
        }
};

class entry
{
    private:
        entry_key_t key; // 8 bytes
        char *ptr;       // 8 bytes

    public:
        entry()
        {
            ptr = NULL;
        }

        friend class page;
        friend class btree;
};

const int cardinality = (PAGESIZE - sizeof(header)) / sizeof(entry);
const int count_in_line = CACHE_LINE_SIZE / sizeof(entry);

class page
{
    private:
        header hdr;                 // header in persistent memory, 16 bytes
        entry records[cardinality]; // slots in persistent memory, 16 bytes * n

    public:
        friend class btree;

        page(uint32_t level = 0)
        {
            hdr.level = level;
            records[0].ptr = NULL;
        }

        // this is called when tree grows
        page(page *left, entry_key_t key, page *right, uint32_t level = 0)
        {
            hdr.leftmost_ptr = left;
            hdr.level = level;
            records[0].key = key;
            records[0].ptr = (char *)right;
            records[1].ptr = NULL;

            hdr.last_index = 0;
#ifdef PERSIST_BTREE
            persist_data((char *)this, sizeof(page));
#endif
        }

        void *operator new(size_t size)
        {
            void *ret;
            posix_memalign(&ret, 64, size);
            return ret;
        }

        inline int count()
        {
            uint8_t previous_switch_counter;
            int count = 0;
            do
            {
                previous_switch_counter = hdr.switch_counter;
                count = hdr.last_index + 1;

                while (count >= 0 && records[count].ptr != NULL)
                {
                    if (IS_FORWARD(previous_switch_counter))
                        ++count;
                    else
                        --count;
                }

                if (count < 0)
                {
                    count = 0;
                    while (records[count].ptr != NULL)
                    {
                        ++count;
                    }
                }

            } while (previous_switch_counter != hdr.switch_counter);

            return count;
        }

        inline bool remove_key(entry_key_t key)
        {
            // Set the switch_counter
            if (IS_FORWARD(hdr.switch_counter))
                ++hdr.switch_counter;

            bool shift = false;
            int i;
            for (i = 0; records[i].ptr != NULL; ++i)
            {
                if (!shift && records[i].key == key)
                {
                    records[i].ptr = (i == 0) ? (char *)hdr.leftmost_ptr : records[i - 1].ptr;
                    shift = true;
                }

                if (shift)
                {
                    records[i].key = records[i + 1].key;
                    records[i].ptr = records[i + 1].ptr;

                    // flush
                    uint64_t records_ptr = (uint64_t)(&records[i]);
                    int remainder = records_ptr % CACHE_LINE_SIZE;
                    bool do_flush = (remainder == 0) ||
                        ((((int)(remainder + sizeof(entry)) / CACHE_LINE_SIZE) == 1) &&
                         ((remainder + sizeof(entry)) % CACHE_LINE_SIZE) != 0);
                    if (do_flush)
                    {
#ifdef PERSIST_BTREE
                        persist_data((char *)records_ptr, CACHE_LINE_SIZE);
#endif
                    }
                }
            }

            if (shift)
            {
                --hdr.last_index;
            }
            return shift;
        }

        bool remove(btree *bt, entry_key_t key, bool only_rebalance = false, bool with_lock = true)
        {
            hdr.mtx->lock();

            bool ret = remove_key(key);

            hdr.mtx->unlock();

            return ret;
        }

        /*
         * Although we implemented the rebalancing of B+-Tree, it is currently blocked for the performance.
         * Please refer to the follow.
         * Chi, P., Lee, W. C., & Xie, Y. (2014, August). 
         * Making B+-tree efficient in PCM-based main memory. In Proceedings of the 2014
         * international symposium on Low power electronics and design (pp. 69-74). ACM.
         */
        // new()
        bool remove_rebalancing(int id, btree *bt, entry_key_t key, bool only_rebalance = false, bool with_lock = true)
        {
            if (with_lock)
            {
                hdr.mtx->lock();
            }

            if (hdr.is_deleted)
            {
                if (with_lock)
                {
                    hdr.mtx->unlock();
                }
                return false;
            }

            if (!only_rebalance)
            {
                register int num_entries_before = count();

                // This node is root
                if (this == (page *)bt->root)
                {
                    if (hdr.level > 0)
                    {
                        if (num_entries_before == 1 && !hdr.sibling_ptr)
                        {
                            bt->root = (char *)hdr.leftmost_ptr;
#ifdef PERSIST_BTREE
                            persist_data((char *)&(bt->root), sizeof(char *));
#endif
                            hdr.is_deleted = 1;
                        }
                    }

                    // Remove the key from this node
                    bool ret = remove_key(key);

                    if (with_lock)
                    {
                        hdr.mtx->unlock();
                    }
                    return true;
                }

                bool should_rebalance = true;
                // check the node utilization
                if (num_entries_before - 1 >= (int)((cardinality - 1) * 0.5))
                {
                    should_rebalance = false;
                }

                // Remove the key from this node
                bool ret = remove_key(key);

                if (!should_rebalance)
                {
                    if (with_lock)
                    {
                        hdr.mtx->unlock();
                    }
                    return (hdr.leftmost_ptr == NULL) ? ret : true;
                }
            }

            //Remove a key from the parent node
            entry_key_t deleted_key_from_parent;
            bool is_leftmost_node = false;
            page *left_sibling;
            bt->btree_delete_internal(key, (char *)this, hdr.level + 1,
                    &deleted_key_from_parent, &is_leftmost_node, &left_sibling);

            if (is_leftmost_node)
            {
                if (with_lock)
                {
                    hdr.mtx->unlock();
                }

                if (!with_lock)
                {
                    hdr.sibling_ptr->hdr.mtx->lock();
                }
                hdr.sibling_ptr->remove(bt, hdr.sibling_ptr->records[0].key, true, with_lock);
                if (!with_lock)
                {
                    hdr.sibling_ptr->hdr.mtx->unlock();
                }
                return true;
            }

            if (with_lock)
            {
                left_sibling->hdr.mtx->lock();
            }

            while (left_sibling->hdr.sibling_ptr != this)
            {
                if (with_lock)
                {
                    page *t = left_sibling->hdr.sibling_ptr;
                    left_sibling->hdr.mtx->unlock();
                    left_sibling = t;
                    left_sibling->hdr.mtx->lock();
                }
                else
                    left_sibling = left_sibling->hdr.sibling_ptr;
            }

            register int num_entries = count();
            register int left_num_entries = left_sibling->count();

            // Merge or Redistribution
            int total_num_entries = num_entries + left_num_entries;
            if (hdr.leftmost_ptr)
            {
                ++total_num_entries;
            }

            entry_key_t parent_key;

            if (total_num_entries > cardinality - 1)
            {
                // Redistribution
                register int m = (int)ceil(total_num_entries / 2);

                if (num_entries < left_num_entries)
                {
                    // left -> right
                    if (hdr.leftmost_ptr == nullptr)
                    {
                        for (int i = left_num_entries - 1; i >= m; i--)
                        {
                            insert_key(left_sibling->records[i].key, left_sibling->records[i].ptr, &num_entries);
                        }

                        left_sibling->records[m].ptr = nullptr;
#ifdef PERSIST_BTREE
                        persist_data((char *)&(left_sibling->records[m].ptr), sizeof(char *));
#endif
                        left_sibling->hdr.last_index = m - 1;
#ifdef PERSIST_BTREE
                        persist_data((char *)&(left_sibling->hdr.last_index), sizeof(int16_t));
#endif
                        parent_key = records[0].key;
                    }
                    else
                    {
                        insert_key(deleted_key_from_parent, (char *)hdr.leftmost_ptr,
                                &num_entries);

                        for (int i = left_num_entries - 1; i > m; i--)
                        {
                            insert_key(left_sibling->records[i].key, left_sibling->records[i].ptr, &num_entries);
                        }

                        parent_key = left_sibling->records[m].key;

                        hdr.leftmost_ptr = (page *)left_sibling->records[m].ptr;
#ifdef PERSIST_BTREE
                        persist_data((char *)&(hdr.leftmost_ptr), sizeof(page *));
#endif

                        left_sibling->records[m].ptr = nullptr;
#ifdef PERSIST_BTREE
                        persist_data((char *)&(left_sibling->records[m].ptr), sizeof(char *));
#endif
                        left_sibling->hdr.last_index = m - 1;
#ifdef PERSIST_BTREE
                        persist_data((char *)&(left_sibling->hdr.last_index), sizeof(int16_t));
#endif
                    }

                    if (left_sibling == ((page *)bt->root))
                    {
                        // page *new_root = new page(left_sibling, parent_key, this, hdr.level + 1);
                        page *new_root = bt->getNewPage(id, left_sibling, parent_key, this, hdr.level + 1);
                        bt->setNewRoot((char *)new_root);
                    }
                    else
                    {
                        bt->btree_insert_internal(id, (char *)left_sibling, parent_key, (char *)this, hdr.level + 1);
                    }
                }
                else
                { // from leftmost case
                    hdr.is_deleted = 1;
#ifdef PERSIST_BTREE
                    persist_data((char *)&(hdr.is_deleted), sizeof(uint8_t));
#endif

                    // page *new_sibling = new page(hdr.level);
                    page *new_sibling = bt->getNewPage(id, hdr.level);
                    new_sibling->hdr.mtx->lock();
                    new_sibling->hdr.sibling_ptr = hdr.sibling_ptr;

                    int num_dist_entries = num_entries - m;
                    int new_sibling_cnt = 0;

                    if (hdr.leftmost_ptr == nullptr)
                    {
                        for (int i = 0; i < num_dist_entries; i++)
                        {
                            left_sibling->insert_key(records[i].key, records[i].ptr,
                                    &left_num_entries);
                        }

                        for (int i = num_dist_entries; records[i].ptr != NULL; i++)
                        {
                            new_sibling->insert_key(records[i].key, records[i].ptr,
                                    &new_sibling_cnt, false);
                        }
#ifdef PERSIST_BTREE
                        persist_data((char *)(new_sibling), sizeof(page));
#endif
                        left_sibling->hdr.sibling_ptr = new_sibling;
#ifdef PERSIST_BTREE
                        persist_data((char *)&(left_sibling->hdr.sibling_ptr), sizeof(page *));
#endif
                        parent_key = new_sibling->records[0].key;
                    }
                    else
                    {
                        left_sibling->insert_key(deleted_key_from_parent,
                                (char *)hdr.leftmost_ptr, &left_num_entries);

                        for (int i = 0; i < num_dist_entries - 1; i++)
                        {
                            left_sibling->insert_key(records[i].key, records[i].ptr,
                                    &left_num_entries);
                        }

                        parent_key = records[num_dist_entries - 1].key;

                        new_sibling->hdr.leftmost_ptr = (page *)records[num_dist_entries - 1].ptr;
                        for (int i = num_dist_entries; records[i].ptr != NULL; i++)
                        {
                            new_sibling->insert_key(records[i].key, records[i].ptr,
                                    &new_sibling_cnt, false);
                        }
#ifdef PERSIST_BTREE
                        persist_data((char *)(new_sibling), sizeof(page));
#endif

                        left_sibling->hdr.sibling_ptr = new_sibling;
#ifdef PERSIST_BTREE
                        persist_data((char *)&(left_sibling->hdr.sibling_ptr), sizeof(page *));
#endif
                    }

                    if (left_sibling == ((page *)bt->root))
                    {
                        // page *new_root = new page(left_sibling, parent_key, new_sibling, hdr.level + 1);
                        page *new_root = bt->getNewPage(id, left_sibling, parent_key, new_sibling, hdr.level + 1);
                        bt->setNewRoot((char *)new_root);
                    }
                    else
                    {
                        bt->btree_insert_internal(id, (char *)left_sibling, parent_key, (char *)new_sibling, hdr.level + 1);
                    }

                    new_sibling->hdr.mtx->unlock();
                }
            }
            else
            {
                hdr.is_deleted = 1;
#ifdef PERSIST_BTREE
                persist_data((char *)&(hdr.is_deleted), sizeof(uint8_t));
#endif
                if (hdr.leftmost_ptr)
                    left_sibling->insert_key(deleted_key_from_parent,
                            (char *)hdr.leftmost_ptr, &left_num_entries);

                for (int i = 0; records[i].ptr != NULL; ++i)
                {
                    left_sibling->insert_key(records[i].key, records[i].ptr, &left_num_entries);
                }

                left_sibling->hdr.sibling_ptr = hdr.sibling_ptr;
#ifdef PERSIST_BTREE
                persist_data((char *)&(left_sibling->hdr.sibling_ptr), sizeof(page *));
#endif
            }

            if (with_lock)
            {
                left_sibling->hdr.mtx->unlock();
                hdr.mtx->unlock();
            }

            return true;
        }

        inline void
            insert_key(entry_key_t key, char *ptr, int *num_entries, bool flush = true,
                    bool update_last_index = true)
            {
                // update switch_counter
                if (!IS_FORWARD(hdr.switch_counter))
                    ++hdr.switch_counter;

                // FAST
                if (*num_entries == 0)
                { // this page is empty
                    entry *new_entry = (entry *)&records[0];
                    entry *array_end = (entry *)&records[1];
                    new_entry->key = key;
                    new_entry->ptr = (char *)ptr;

                    array_end->ptr = (char *)NULL;

                    if (flush)
                    {
#ifdef PERSIST_BTREE
                        persist_data((char *)this, CACHE_LINE_SIZE);
#endif
                    }
                }
                else
                {
                    int i = *num_entries - 1, inserted = 0, to_flush_cnt = 0;
                    records[*num_entries + 1].ptr = records[*num_entries].ptr;
                    if (flush)
                    {
#ifdef PERSIST_BTREE
                        if ((uint64_t) & (records[*num_entries + 1].ptr) % CACHE_LINE_SIZE == 0)
                            persist_data((char *)&(records[*num_entries + 1].ptr), sizeof(char *));
#endif
                    }

                    // FAST
                    for (i = *num_entries - 1; i >= 0; i--)
                    {
                        if (key < records[i].key)
                        {
                            records[i + 1].ptr = records[i].ptr;
                            records[i + 1].key = records[i].key;

                            if (flush)
                            {
                                uint64_t records_ptr = (uint64_t)(&records[i + 1]);

                                int remainder = records_ptr % CACHE_LINE_SIZE;
                                bool do_flush = (remainder == 0) ||
                                    ((((int)(remainder + sizeof(entry)) / CACHE_LINE_SIZE) == 1) && ((remainder + sizeof(entry)) % CACHE_LINE_SIZE) != 0);
                                if (do_flush)
                                {
#ifdef PERSIST_BTREE
                                    persist_data((char *)records_ptr, CACHE_LINE_SIZE);
#endif
                                    to_flush_cnt = 0;
                                }
                                else
                                    ++to_flush_cnt;
                            }
                        }
                        else
                        {
                            records[i + 1].ptr = records[i].ptr;
                            records[i + 1].key = key;
                            records[i + 1].ptr = ptr;
#ifdef PERSIST_BTREE
                            if (flush)
                                persist_data((char *)&records[i + 1], sizeof(entry));
#endif
                            inserted = 1;
                            break;
                        }
                    }
                    if (inserted == 0)
                    {
                        records[0].ptr = (char *)hdr.leftmost_ptr;
                        records[0].key = key;
                        records[0].ptr = ptr;
#ifdef PERSIST_BTREE
                        if (flush)
                            persist_data((char *)&records[0], sizeof(entry));
#endif
                    }
                }

                if (update_last_index)
                {
                    hdr.last_index = *num_entries;
                }
                ++(*num_entries);
            }

        // Insert a new key - FAST and FAIR
        // new()
        page *store(int id, btree *bt, char *left, entry_key_t key, char *right,
                bool flush, bool with_lock, page *invalid_sibling = NULL)
        {
            if (with_lock)
            {
                hdr.mtx->lock(); // Lock the write lock
            }

            if (hdr.is_deleted)
            {
                if (with_lock)
                {
                    hdr.mtx->unlock();
                }

                return NULL;
            }

            // If this node has a sibling node,
            if (hdr.sibling_ptr && (hdr.sibling_ptr != invalid_sibling))
            {
                // Compare this key with the first key of the sibling
                if (key > hdr.sibling_ptr->records[0].key)
                {
                    if (with_lock)
                    {
                        hdr.mtx->unlock(); // Unlock the write lock
                    }
                    return hdr.sibling_ptr->store(id, bt, NULL, key, right,
                            true, with_lock, invalid_sibling);
                }
            }

            register int num_entries = count();

            // FAST
            if (num_entries < cardinality - 1)
            {
                insert_key(key, right, &num_entries, flush);

                if (with_lock)
                {
                    hdr.mtx->unlock(); // Unlock the write lock
                }

                return this;
            }
            else
            {
                // FAIR
                // overflow
                // create a new node
                // page *sibling = new page(hdr.level);
                page *sibling = bt->getNewPage(id, hdr.level);
                register int m = (int)ceil(num_entries / 2);
                entry_key_t split_key = records[m].key;

                // migrate half of keys into the sibling
                int sibling_cnt = 0;
                if (hdr.leftmost_ptr == NULL)
                { // leaf node
                    for (int i = m; i < num_entries; ++i)
                    {
                        sibling->insert_key(records[i].key, records[i].ptr, &sibling_cnt, false);
                    }
                }
                else
                { // internal node
                    for (int i = m + 1; i < num_entries; ++i)
                    {
                        sibling->insert_key(records[i].key, records[i].ptr, &sibling_cnt, false);
                    }
                    sibling->hdr.leftmost_ptr = (page *)records[m].ptr;
                }

                sibling->hdr.sibling_ptr = hdr.sibling_ptr;
#ifdef PERSIST_BTREE
                persist_data((char *)sibling, sizeof(page));
#endif

                hdr.sibling_ptr = sibling;
#ifdef PERSIST_BTREE
                persist_data((char *)&hdr, sizeof(hdr));
#endif

                // set to NULL
                if (IS_FORWARD(hdr.switch_counter))
                    hdr.switch_counter += 2;
                else
                    ++hdr.switch_counter;
                records[m].ptr = NULL;
#ifdef PERSIST_BTREE
                persist_data((char *)&records[m], sizeof(entry));
#endif

                hdr.last_index = m - 1;

#ifdef PERSIST_BTREE
                persist_data((char *)&(hdr.last_index), sizeof(int16_t));
#endif

                num_entries = hdr.last_index + 1;

                page *ret;

                // insert the key
                if (key < split_key)
                {
                    insert_key(key, right, &num_entries);
                    ret = this;
                }
                else
                {
                    sibling->insert_key(key, right, &sibling_cnt);
                    ret = sibling;
                }

                // Set a new root or insert the split key to the parent
                if (bt->root == (char *)this)
                {
                    // only one node can update the root ptr
                    // page *new_root = new page((page *)this, split_key, sibling, hdr.level + 1);
                    page *new_root = bt->getNewPage(id, (page *)this, split_key, sibling, hdr.level + 1);
                    bt->setNewRoot((char *)new_root);
                    if (with_lock)
                    {
                        hdr.mtx->unlock(); // Unlock the write lock
                    }
                }
                else
                {
                    if (with_lock)
                    {
                        hdr.mtx->unlock(); // Unlock the write lock
                    }
                    bt->btree_insert_internal(id, NULL, split_key, (char *)sibling,
                            hdr.level + 1);
                }

                return ret;
            }
        }

        // Search keys with linear search
        size_t linear_search_range(entry_key_t min, entry_key_t max, unsigned long *buf)
        {
            int i, off = 0;
            uint8_t previous_switch_counter;
            page *current = this;

            while (current)
            {
                int old_off = off;
                do
                {
                    previous_switch_counter = current->hdr.switch_counter;
                    off = old_off;

                    entry_key_t tmp_key;
                    char *tmp_ptr;

                    if (IS_FORWARD(previous_switch_counter))
                    {
                        if ((tmp_key = current->records[0].key) > min)
                        {
                            if (tmp_key < max)
                            {
                                if ((tmp_ptr = current->records[0].ptr) != NULL)
                                {
                                    if (tmp_key == current->records[0].key)
                                    {
                                        if (tmp_ptr)
                                        {
                                            buf[off++] = (unsigned long)tmp_ptr;
                                        }
                                    }
                                }
                            }
                            else
                                return off;
                        }

                        for (i = 1; current->records[i].ptr != NULL; ++i)
                        {
                            if ((tmp_key = current->records[i].key) > min)
                            {
                                if (tmp_key < max)
                                {
                                    if ((tmp_ptr = current->records[i].ptr) != current->records[i - 1].ptr)
                                    {
                                        if (tmp_key == current->records[i].key)
                                        {
                                            if (tmp_ptr)
                                                buf[off++] = (unsigned long)tmp_ptr;
                                        }
                                    }
                                }
                                else
                                    return off;
                            }
                        }
                    }
                    else
                    {
                        for (i = count() - 1; i > 0; --i)
                        {
                            if ((tmp_key = current->records[i].key) > min)
                            {
                                if (tmp_key < max)
                                {
                                    if ((tmp_ptr = current->records[i].ptr) != current->records[i - 1].ptr)
                                    {
                                        if (tmp_key == current->records[i].key)
                                        {
                                            if (tmp_ptr)
                                                buf[off++] = (unsigned long)tmp_ptr;
                                        }
                                    }
                                }
                                else
                                    return off;
                            }
                        }

                        if ((tmp_key = current->records[0].key) > min)
                        {
                            if (tmp_key < max)
                            {
                                if ((tmp_ptr = current->records[0].ptr) != NULL)
                                {
                                    if (tmp_key == current->records[0].key)
                                    {
                                        if (tmp_ptr)
                                        {
                                            buf[off++] = (unsigned long)tmp_ptr;
                                        }
                                    }
                                }
                            }
                            else
                                return off;
                        }
                    }
                } while (previous_switch_counter != current->hdr.switch_counter);

                current = current->hdr.sibling_ptr;
            }

            return off;
        }

        char *linear_search(entry_key_t key)
        {
            int i = 1;
            uint8_t previous_switch_counter;
            char *ret = NULL;
            char *t;
            entry_key_t k;

            if (hdr.leftmost_ptr == NULL)
            { // Search a leaf node
                do
                {
                    previous_switch_counter = hdr.switch_counter;
                    ret = NULL;

                    // search from left ro right
                    if (IS_FORWARD(previous_switch_counter))
                    {
                        if ((k = records[0].key) == key)
                        {
                            if ((t = records[0].ptr) != NULL)
                            {
                                if (k == records[0].key)
                                {
                                    ret = t;
                                    continue;
                                }
                            }
                        }

                        for (i = 1; records[i].ptr != NULL; ++i)
                        {
                            if ((k = records[i].key) == key)
                            {
                                if (records[i - 1].ptr != (t = records[i].ptr))
                                {
                                    if (k == records[i].key)
                                    {
                                        ret = t;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else
                    { // search from right to left
                        for (i = count() - 1; i > 0; --i)
                        {
                            if ((k = records[i].key) == key)
                            {
                                if (records[i - 1].ptr != (t = records[i].ptr) && t)
                                {
                                    if (k == records[i].key)
                                    {
                                        ret = t;
                                        break;
                                    }
                                }
                            }
                        }

                        if (!ret)
                        {
                            if ((k = records[0].key) == key)
                            {
                                if (NULL != (t = records[0].ptr) && t)
                                {
                                    if (k == records[0].key)
                                    {
                                        ret = t;
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                } while (hdr.switch_counter != previous_switch_counter);

                if (ret)
                {
                    return ret;
                }

                if ((t = (char *)hdr.sibling_ptr) && key >= ((page *)t)->records[0].key)
                    return t;

                return NULL;
            }
            else
            { // internal node
                do
                {
                    previous_switch_counter = hdr.switch_counter;
                    ret = NULL;

                    if (IS_FORWARD(previous_switch_counter))
                    {
                        if (key < (k = records[0].key))
                        {
                            if ((t = (char *)hdr.leftmost_ptr) != records[0].ptr)
                            {
                                ret = t;
                                continue;
                            }
                        }

                        for (i = 1; records[i].ptr != NULL; ++i)
                        {
                            if (key < (k = records[i].key))
                            {
                                if ((t = records[i - 1].ptr) != records[i].ptr)
                                {
                                    ret = t;
                                    break;
                                }
                            }
                        }

                        if (!ret)
                        {
                            ret = records[i - 1].ptr;
                            continue;
                        }
                    }
                    else
                    { // search from right to left
                        for (i = count() - 1; i >= 0; --i)
                        {
                            if (key >= (k = records[i].key))
                            {
                                if (i == 0)
                                {
                                    if ((char *)hdr.leftmost_ptr != (t = records[i].ptr))
                                    {
                                        ret = t;
                                        break;
                                    }
                                }
                                else
                                {
                                    if (records[i - 1].ptr != (t = records[i].ptr))
                                    {
                                        ret = t;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                } while (hdr.switch_counter != previous_switch_counter);

                if ((t = (char *)hdr.sibling_ptr) != NULL)
                {
                    if (key >= ((page *)t)->records[0].key)
                        return t;
                }

                if (ret)
                {
                    return ret;
                }
                else
                    return (char *)hdr.leftmost_ptr;
            }

            return NULL;
        }
};

#endif
