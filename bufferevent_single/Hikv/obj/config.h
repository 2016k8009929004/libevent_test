#ifndef INCLUDE_CONFIG_H_
#define INCLUDE_CONFIG_H_

#include <stdint.h>

#define KEY_LENGTH (64)
#define VALUE_LENGTH (256)

#define HIKV_ASYNC

// #define THREAD_CPU_BIND
// #define PERSIST_HASHTABLE
// #define PERSIST_BTREE
// #define PMDK_USED
// #define PM_USED

//#define TEST
#define HIKV
// #define HIKV_OPT
// #define HIKV_OPT_PLUS

/* just for test in macos */
#ifdef TEST
// #define PERSIST_BTREE
// #define PERSIST_HASHTABLE
#endif

/* 
   [HiKV]
   [1] hashtable index in Persistent Memory
   [2] b+-tree index in DRAM 
   */
#ifdef HIKV
#define THREAD_BIND_CPU
//#define PERSIST_HASHTABLE
//#define PM_USED
//#define PMDK_USED
#endif

/* 
   [HiKV_Opt]
   [1] hashtable index in DRAM
   [2] b+-tree index in Persistent Memory
   */
#ifdef HIKV_OPT
#define THREAD_BIND_CPU
#define PERSIST_BTREE
#define PM_USED
#define PMDK_USED
#endif

/* 
   [HiKV_Opt_Plus]
   [1] hashtable index in DRAM
   [2] b+-tree index in DRAM
   */
#ifdef HIKV_OPT_PLUS
#define THREAD_BIND_CPU
#define PM_USED
#define PMDK_USED
#endif

struct kv_item
{
    uint8_t key[KEY_LENGTH];
    uint8_t value[VALUE_LENGTH];
};

#endif
