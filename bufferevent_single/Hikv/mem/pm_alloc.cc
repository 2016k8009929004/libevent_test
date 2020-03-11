#include "pm_alloc.h"
#include "config.h"
#include "libpmem.h"
#include <assert.h>

pm_alloctor::pm_alloctor(const char *pool_name, size_t num_paritition, size_t pm_size)
{
    this->num_partition = num_paritition;
#ifdef PM_USED
#ifdef PMDK_USED
    size_t mapped_len;
    int is_pmem;
    /* create a new memory pool file */
    // void *dest = pmem_map_file(pool_name, pm_size, 0, 0666, &mapped_len, &is_pmem);
    void *dest = pmem_map_file(pool_name, 0, 0, 0666, &mapped_len, &is_pmem);
    pm_size = mapped_len;
    printf("Use PMDK to mmap file! (%s) (%zuMB/%zuMB) (%d)\n", pool_name, pm_size / (1024 * 1024), mapped_len / (1024 * 1024), is_pmem);
#else
    int fd = open(pool_name, O_RDWR);
    void *dest = mmap(NULL, pm_size, PROT_READ | PROT_WRITE, MAP_SHARED /*| MAP_POPULATE*/, fd, 0);
#endif
    assert(dest != NULL);
    // memset(dest, 0, pm_size);
    this->pm_start_address = (uint64_t)dest;
    this->pm_size = pm_size;
    printf("Persistent Memory Pool Start Address : 0x%llx\n", (uint64_t)dest);
#else
    pm_start_address = (uint64_t)malloc(pm_size);
    assert(pm_start_address != 0);
    // memset((void *)pm_start_address, 0, pm_size);
    this->pm_size = pm_size;
    printf("DRAM Pool Start Address : 0x%llx\n", (uint64_t)pm_start_address);
#endif

    size_t p_size = this->pm_size / num_paritition;
    uint64_t cut = this->pm_start_address;

    for (int i = 0; i < num_paritition; i++)
    {
        partition_start_address[i] = cut;
        partition_cur_offset[i] = 0;
        partition_size[i] = p_size;
        cut += p_size;
        partition_end_address[i] = cut;
    }
}

pm_alloctor::~pm_alloctor()
{
#ifdef PM_USED
#ifdef PMDK_USED
    pmem_unmap((void *)this->pm_start_address, this->pm_size);
#else
    munmap((void *)this->pm_start_address, this->pm_size);
#endif
#else
    free((void *)this->pm_start_address);
#endif
}

void *pm_alloctor::alloc(size_t id, size_t size)
{
    void *ret = (void *)(partition_start_address[id] + partition_cur_offset[id]);
    partition_cur_offset[id] += size;

    if (partition_cur_offset[id] > partition_size[id])
    {
        printf("No enough memory space!\n");
        for (;;)
        {
        };
    }

    return ret;
}

void pm_alloctor::print()
{
    size_t total_size = 0;

    for (int i = 0; i < num_partition; i++)
    {
        total_size += partition_size[i];
    }

    printf(">>[Persistent Memory Pool]\n");
    printf("  memory usage:%zuMB\n", total_size / (1024 * 1024));

    for (int i = 0; i < num_partition; i++)
    {
        printf("  [%d][0x%llx, 0x%llx][%lluMB/%zuMB(%.2f%%)]\n", i, partition_start_address[i], partition_end_address[i], partition_cur_offset[i] / (1024 * 1024), partition_size[i] / (1024 * 1024), 100.0 * partition_cur_offset[i] / partition_size[i]);
    }
}
