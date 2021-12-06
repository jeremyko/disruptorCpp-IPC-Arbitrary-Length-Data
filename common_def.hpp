
#ifndef COMMON_DEF_HPP
#define COMMON_DEF_HPP
#include <atomic>
#include <iostream>
#include <condition_variable>

#include <pthread.h> //blocking strategy : mutex, condition_var on shared memory
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 

///////////////////////////////////////////////////////////////////////////////
#define COLOR_RED  "\x1B[31m"
#define COLOR_GREEN "\x1B[32m" 
#define COLOR_BLUE "\x1B[34m"
#define COLOR_RESET "\x1B[0m"
#define  LOG_WHERE "("<<__FILE__<<"-"<<__func__<<"-"<<__LINE__<<") "
#define  WHERE_DEF __FILE__,__func__,__LINE__

#ifdef DEBUG_PRINT
#define  DEBUG_LOG(x)  std::cout<<LOG_WHERE << x << "\n"
#define  DEBUG_RED_LOG(x) std::cout<<LOG_WHERE << COLOR_RED<< x << COLOR_RESET << "\n"
#define  DEBUG_GREEN_LOG(x) std::cout<<LOG_WHERE << COLOR_GREEN<< x << COLOR_RESET << "\n"
#define  DEBUG_ELOG(x) std::cerr<<LOG_WHERE << COLOR_RED<< x << COLOR_RESET << "\n"
#else
#define  DEBUG_LOG(x) 
#define  DEBUG_ELOG(x) 
#define  DEBUG_RED_LOG(x) 
#define  DEBUG_GREEN_LOG(x)
#endif

#define  PRINT_ELOG(x) std::cerr<<LOG_WHERE << COLOR_RED<< x << COLOR_RESET << "\n"
#define MAX_CONSUMER 200

///////////////////////////////////////////////////////////////////////////////
typedef struct _StatusOnSharedMem_
{
    int  nBufferSize   ;
    int  nTotalMemSize ;
    std::atomic<int> registered_producer_count ;
    std::atomic<int> registered_consumer_count;
    std::atomic<int64_t> cursor  __attribute__ ((aligned (64))) ;
    std::atomic<int64_t> next    __attribute__ ((aligned (64))) ;
    std::atomic<int64_t> array_consumer_indexes [MAX_CONSUMER] __attribute__ ((aligned (64)));
    std::atomic<int64_t> prev_reset_pos    __attribute__ ((aligned (64))) ; 

    pthread_cond_t   cond_var ;
    pthread_mutex_t  mutex_lock;

} StatusOnSharedMem ;
#endif
