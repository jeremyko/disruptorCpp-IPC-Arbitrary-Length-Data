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

#include <iostream>
#include <thread>

#include "../../ring_buffer_on_shmem.hpp" 
#include "../../shared_mem_manager.hpp" 
#include "../../atomic_print.hpp"
#include "../../elapsed_time.hpp"

std::mutex AtomicPrint::lock_mutex_ ;

int LOOP_CNT ;

//Wait Strategy 
//SharedMemRingBuffer gSharedMemRingBuffer (YIELDING_WAIT); 
SharedMemRingBuffer gSharedMemRingBuffer (SLEEPING_WAIT); 
//SharedMemRingBuffer gSharedMemRingBuffer (BLOCKING_WAIT); 
        

///////////////////////////////////////////////////////////////////////////////
void TestFunc(int consumer_id)
{
    //1. register
    int64_t index_for_customer_use = -1;
    if(!gSharedMemRingBuffer.RegisterConsumer(consumer_id, & index_for_customer_use)) {
        return; //error
    }

    //2. run
    ElapsedTime elapsed;
    char    msg[2048];
    int64_t total_fetched = 0; 
    int64_t  my_index = index_for_customer_use ; 
    int64_t returned_index =-1;
    bool is_first=true;

    char  tmp_data[1024];
    for(int i=0; i < LOOP_CNT  ; i++) {
        if( total_fetched >= LOOP_CNT ) {
            break;
        }
        returned_index = gSharedMemRingBuffer.WaitFor(consumer_id, my_index);

        if(is_first) {
            is_first=false;
            elapsed.SetStartTime();
        }
#ifdef _DEBUG_READ_
        snprintf(msg, sizeof(msg), 
                "[id:%d]    \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d] WaitFor my_index[%" PRId64 "] returned_index[%" PRId64 "]", 
                consumer_id, __func__, __LINE__, my_index, returned_index );
        {AtomicPrint atomicPrint(msg);}
#endif
        for(int64_t j = my_index; j <= returned_index; j++) {
            //batch job 
            size_t data_len =0;
            const char* data =  gSharedMemRingBuffer.GetData(j, & data_len );
            //const char*  SharedMemRingBuffer::GetData(int64_t nIndex, int* nOutLen)

            memset(tmp_data, 0x00, sizeof(tmp_data));
            strncpy(tmp_data, data, data_len);
#ifdef _DEBUG_READ_
            snprintf(msg, sizeof(msg), 
                    "[id:%d]   \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d]  my_index[%" PRId64 ", translated:%" PRId64 "] data [%s] ^^", 
                    consumer_id, __func__, __LINE__, j, gSharedMemRingBuffer.GetTranslatedIndex(j), tmp_data );
            {AtomicPrint atomicPrint(msg);}
#endif
            gSharedMemRingBuffer.CommitRead(consumer_id, j );
            total_fetched++;

        } //for

        my_index = returned_index + 1; 
    }

    long long elapsed_micro= elapsed.SetEndTime(MICRO_SEC_RESOLUTION);
    snprintf(msg, sizeof(msg), "* consumer -> elapsed :%lld (micro sec) last data [%s] TPS %lld ", 
        elapsed_micro, tmp_data, (long long) (LOOP_CNT*1000000L)/elapsed_micro );
    {AtomicPrint atomicPrint(msg);}
}


///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    if(argc != 2) {
        std::cout << "usage: "<< argv[0]<<" consumer_index"  << '\n';
        std::cout << "ex: "<< argv[0]<<" 0"  << '\n';
        std::cout << "ex: "<< argv[0]<<" 1"  << '\n';
        exit(1);
    }

    int consumer_id = atoi(argv[1]);
    LOOP_CNT = 10000;    //should be same as producer for TEST
    int MAX_RBUFFER_CAPACITY = 1024*8; //should be same as producer for TEST
    int MAX_RAW_MEM_BUFFER_SIZE = 1000000; //should be same as producer for TEST

    if(! gSharedMemRingBuffer.Init(123456,
                                   MAX_RBUFFER_CAPACITY, 
                                   923456,
                                   MAX_RAW_MEM_BUFFER_SIZE ) )
    { 
        std::cerr << "error" << '\n';
        return 1; 
    }

    TestFunc(consumer_id);

    return 0;
}

