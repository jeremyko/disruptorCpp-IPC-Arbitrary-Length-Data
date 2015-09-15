/****************************************************************************
 Copyright (c) 2015, ko jung hyun
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#ifndef __WAIT_STRATEGY_INTERFACE_HPP__
#define __WAIT_STRATEGY_INTERFACE_HPP__

#include <thread>

#include "common_def.hpp"
#include "ring_buffer_on_shmem.hpp"

using namespace std;

typedef enum __ENUM_WAIT_STRATEGY__
{
    BLOCKING_WAIT,
    YIELDING_WAIT,
    SLEEPING_WAIT

} ENUM_WAIT_STRATEGY;

///////////////////////////////////////////////////////////////////////////////
class WaitStrategyInterface
{
    public:
        WaitStrategyInterface( )
        {
        };
        virtual ~WaitStrategyInterface() { };

        virtual int64_t Wait( int64_t nIndex ) =0;
        virtual void SignalAllWhenBlocking() = 0; //blocking strategy only

    protected:
        StatusOnSharedMem* pStatusOnSharedMem_;
};

///////////////////////////////////////////////////////////////////////////////
class YieldingWaitStrategy:public WaitStrategyInterface
{
    public:
        
        YieldingWaitStrategy( StatusOnSharedMem* pStatusOnSharedMem) 
        {
            pStatusOnSharedMem_= pStatusOnSharedMem ;
        };
        ~YieldingWaitStrategy() { };

        int64_t Wait(int64_t nIndex) 
        {
            int nCounter = 100;

            while (true)
            {
                int64_t nCurrentCursor = pStatusOnSharedMem_->cursor.load() ;

                if( nIndex > nCurrentCursor )
                {
                    //spins --> yield
                    if(nCounter ==0)
                    {
                        std::this_thread::yield();
                    }
                    else
                    {
                        nCounter--;
                    }
                    continue;
                }
                else
                {
                    return nCurrentCursor;
                }
            }
        }

        void SignalAllWhenBlocking()  //blocking strategy only
        {
        }
};

///////////////////////////////////////////////////////////////////////////////
class SleepingWaitStrategy:public WaitStrategyInterface
{
    public:
        
        SleepingWaitStrategy( StatusOnSharedMem* pStatusOnSharedMem) 
        {
            pStatusOnSharedMem_= pStatusOnSharedMem ;
        };
        ~SleepingWaitStrategy() { };

        int64_t Wait(int64_t nIndex) 
        {
            int nCounter = 200;

            while (true)
            {
                int64_t nCurrentCursor = pStatusOnSharedMem_->cursor.load() ;

                if( nIndex > nCurrentCursor )
                {
                    //spins --> yield --> sleep
                    if(nCounter > 100)
                    {
                        nCounter--;
                    }
                    else if(nCounter > 0)
                    {
                        std::this_thread::yield();
                        nCounter--;
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::nanoseconds(1)); 
                    }
                    continue;
                }
                else
                {
                    return nCurrentCursor;
                }
            }
        }

        void SignalAllWhenBlocking()  //blocking strategy only
        {
        }
};


///////////////////////////////////////////////////////////////////////////////
#include <sys/time.h>
class BlockingWaitStrategy:public WaitStrategyInterface
{
    public:
        BlockingWaitStrategy( StatusOnSharedMem* pStatusOnSharedMem) 
        {
            pStatusOnSharedMem_= pStatusOnSharedMem ;
        };

        ~BlockingWaitStrategy() { };

        int64_t Wait(int64_t nIndex) 
        {
            while (true)
            {
                int64_t nCurrentCursor = pStatusOnSharedMem_->cursor.load() ;

                if( nIndex > nCurrentCursor )
                {
                    struct timespec timeToWait;
                    struct timeval now;
                    gettimeofday(&now,NULL);
                  
                    timeToWait.tv_sec  = now.tv_sec;
                    timeToWait.tv_nsec = now.tv_usec * 1000;
                    //timeToWait.tv_sec += 1;
                    timeToWait.tv_nsec += 1000;

                    pthread_mutex_lock(&(pStatusOnSharedMem_->mtxLock) );

                    pthread_cond_timedwait(& (pStatusOnSharedMem_->condVar), 
                                           &(pStatusOnSharedMem_->mtxLock),
                                           & timeToWait );

                    pthread_mutex_unlock(&(pStatusOnSharedMem_->mtxLock));
                }
                else
                {
                    return nCurrentCursor;
                }
            }
        }

        void SignalAllWhenBlocking()  //blocking strategy only
        {
            //생산자가 Commit 시 호출됨.
            pthread_mutex_lock(&(pStatusOnSharedMem_->mtxLock));
            pthread_cond_broadcast(&(pStatusOnSharedMem_->condVar));
            pthread_mutex_unlock(&(pStatusOnSharedMem_->mtxLock));
        }

    private:
};


#endif

