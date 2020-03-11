#include "btree.h"

/* class btree */
pthread_mutex_t print_mtx;

btree::btree(struct pm_alloctor *allocator)
{
    this->allocator = allocator;
    // root = (char *)new page(); // DRAM
    root = (char *)getNewPage(0, 0);
    height = 1;
    memset(page_count, 0, sizeof(page_count));
}

page *btree::getNewPage(int id, page *left, entry_key_t key, page *right, uint32_t level)
{
    page *pg;

    if (allocator != NULL)
    {
        pg = (page *)allocator->alloc(id, sizeof(page));
    }
    else
    {
        pg = (page *)malloc(sizeof(page));
    }

    memset(pg, 0, sizeof(page));
    // records set
    pg->records[0].ptr = NULL;
    // init header
    pg->hdr.mtx = new std::mutex();
    pg->hdr.leftmost_ptr = NULL;
    pg->hdr.sibling_ptr = NULL;
    pg->hdr.switch_counter = 0;
    pg->hdr.last_index = -1;
    pg->hdr.is_deleted = false;
    // set var
    pg->hdr.leftmost_ptr = left;
    pg->hdr.level = level;
    pg->records[0].key = key;
    pg->records[0].ptr = (char *)right;
    pg->records[1].ptr = NULL;
    pg->hdr.last_index = 0;

    page_count[id]++;
    return pg;
}

page *btree::getNewPage(int id, uint32_t level)
{
    page *pg;

    if (allocator != NULL)
    {
        pg = (page *)allocator->alloc(id, sizeof(page));
    }
    else
    {
        pg = (page *)malloc(sizeof(page));
    }

    memset(pg, 0, sizeof(page));
    // records set
    pg->records[0].ptr = NULL;
    // init header
    pg->hdr.level = level;
    pg->hdr.mtx = new std::mutex();
    pg->hdr.leftmost_ptr = NULL;
    pg->hdr.sibling_ptr = NULL;
    pg->hdr.switch_counter = 0;
    pg->hdr.last_index = -1;
    pg->hdr.is_deleted = false;

    page_count[id]++;
    return pg;
}

void btree::setNewRoot(char *new_root)
{
    this->root = (char *)new_root;
#ifdef PERSIST_BTREE
    persist_data((char *)&(this->root), sizeof(char *));
#endif
    ++height;
}

char *btree::btree_search(entry_key_t &key)
{
    page *p = (page *)root;

    while (p->hdr.leftmost_ptr != NULL)
    {
        p = (page *)p->linear_search(key);
    }

    page *t;
    while ((t = (page *)p->linear_search(key)) == p->hdr.sibling_ptr)
    {
        p = t;
        if (!p)
        {
            break;
        }
    }

    if (!t)
    {
        return NULL;
    }

    return (char *)t;
}

// insert the key in the leaf node
void btree::btree_insert(int id, entry_key_t &key, char *right)
{
    //need to be string
    page *p = (page *)root;

    while (p->hdr.leftmost_ptr != NULL)
    {
        p = (page *)p->linear_search(key);
    }

    if (!p->store(id, this, NULL, key, right, true, true))
    {
        btree_insert(id, key, right);
    }
    else
    {
        num_kv[id]++;
    }
}

// store the key into the node at the given level
void btree::btree_insert_internal(int id, char *left, entry_key_t &key, char *right, uint32_t level)
{
    if (level > ((page *)root)->hdr.level)
    {
        return;
    }

    page *p = (page *)this->root;

    while (p->hdr.level > level)
    {
        p = (page *)p->linear_search(key);
    }

    if (!p->store(id, this, NULL, key, right, true, true))
    {
        btree_insert_internal(id, left, key, right, level);
    }
}

void btree::btree_delete(entry_key_t &key)
{
    page *p = (page *)root;

    while (p->hdr.leftmost_ptr != NULL)
    {
        p = (page *)p->linear_search(key);
    }

    page *t;
    while ((t = (page *)p->linear_search(key)) == p->hdr.sibling_ptr)
    {
        p = t;
        if (!p)
            break;
    }

    if (p)
    {
        if (!p->remove(this, key))
        {
            btree_delete(key);
        }
    }
    else
    {
        printf("not found the key to delete %lu\n", key);
    }
}

void btree::btree_delete_internal(entry_key_t &key, char *ptr, uint32_t level, entry_key_t *deleted_key,
        bool *is_leftmost_node, page **left_sibling)
{
    if (level > ((page *)this->root)->hdr.level)
        return;

    page *p = (page *)this->root;

    while (p->hdr.level > level)
    {
        p = (page *)p->linear_search(key);
    }

    p->hdr.mtx->lock();

    if ((char *)p->hdr.leftmost_ptr == ptr)
    {
        *is_leftmost_node = true;
        p->hdr.mtx->unlock();
        return;
    }

    *is_leftmost_node = false;

    for (int i = 0; p->records[i].ptr != NULL; ++i)
    {
        if (p->records[i].ptr == ptr)
        {
            if (i == 0)
            {
                if ((char *)p->hdr.leftmost_ptr != p->records[i].ptr)
                {
                    *deleted_key = p->records[i].key;
                    *left_sibling = p->hdr.leftmost_ptr;
                    p->remove(this, *deleted_key, false, false);
                    break;
                }
            }
            else
            {
                if (p->records[i - 1].ptr != p->records[i].ptr)
                {
                    *deleted_key = p->records[i].key;
                    *left_sibling = (page *)p->records[i - 1].ptr;
                    p->remove(this, *deleted_key, false, false);
                    break;
                }
            }
        }
    }

    p->hdr.mtx->unlock();
}

// Function to search keys from "min" to "max"
size_t btree::btree_search_range(entry_key_t &min, entry_key_t &max, unsigned long *buf)
{
    page *p = (page *)root;
    size_t cnt = 0;

    while (p)
    {
        if (p->hdr.leftmost_ptr != NULL)
        {
            // The current page is internal
            p = (page *)p->linear_search(min);
        }
        else
        {
            // Found a leaf
            cnt = p->linear_search_range(min, max, buf);
            break;
        }
    }

    return cnt;
}

void btree::print()
{
    size_t sum_page_count = 0;
    size_t sum_num_kv = 0;

    for (int i = 0; i < 32; i++)
    {
        sum_page_count += page_count[i];
        sum_num_kv += num_kv[i];
    }

    printf(">>[FAST-FAIR B+-Tree]\n");
    printf("  page count/size:%zu/%zuMB\n", sum_page_count, sum_page_count * sizeof(page) / (1024 * 1024));
    printf("  kv count:%zu\n", sum_num_kv);
}
