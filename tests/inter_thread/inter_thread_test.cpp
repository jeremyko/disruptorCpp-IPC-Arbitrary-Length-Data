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
#include <atomic>
#include <thread>
#include <mutex>
#include <fstream>

#include "../../ring_buffer_on_shmem.hpp" 
#include "../../shared_mem_manager.hpp" 
#include "../../atomic_print.hpp"
#include "../../elapsed_time.hpp"

std::mutex AtomicPrint::lock_mutex_ ;

int MAX_PRODUCER_CNT ; 
int MAX_CONSUMER_CNT;
int LOOP_CNT ;

//SharedMemRingBuffer gSharedMemRingBuffer (YIELDING_WAIT); 
SharedMemRingBuffer gSharedMemRingBuffer (SLEEPING_WAIT); 
//SharedMemRingBuffer gSharedMemRingBuffer (BLOCKING_WAIT); 

///////////////////////////////////////////////////////////////////////////////
void ThreadWorkWrite(std::string tid, int my_id) 
{
    char    msg[2048];
    int64_t my_index = -1;
    char    raw_data[1024];

    for(int i=1; i <= LOOP_CNT  ; i++) {
        PositionInfo my_data;
        size_t write_position = 0;

        if(i%2==0) {
            my_data.len = snprintf(raw_data, sizeof(raw_data), "raw data  %06d", i);
        } else {
            my_data.len = snprintf(raw_data, sizeof(raw_data), "raw data  %08d", i);
        }
        my_index = gSharedMemRingBuffer.ClaimIndex(my_data.len, & write_position );

        my_data.start_position = write_position  ; 
        my_data.offset_position = write_position + my_data.len; //fot next write
        my_data.status = DATA_EXISTS ;
#ifdef _DEBUG_WRITE_
        snprintf(msg, sizeof(msg), 
                "[id:%d]    [%s-%d] Write my_index[%ld] %s: want len[%d] write_position [%d] my_data.nLastPosition [%d]", 
                 my_id, __func__, __LINE__, my_index, raw_data, my_data.nLen, write_position, my_data.offset_position );
        {AtomicPrint atomicPrint(msg);}
#endif
        gSharedMemRingBuffer.SetData( my_index, &my_data, write_position, raw_data );

        gSharedMemRingBuffer.Commit( my_index); 
    }

    snprintf(msg, sizeof(msg), 
            "*********************[id:%d] write done, last data [%s]", 
            my_id, raw_data );
    {AtomicPrint atomicPrint(msg);}

}

///////////////////////////////////////////////////////////////////////////////
void ThreadWorkRead(std::string tid, int my_id, int64_t nIndexforCustomerUse) 
{
    char msg[2048];
    int64_t total_fetched = 0; 
    int64_t my_index = nIndexforCustomerUse ; 
    int64_t returned_index =-1;
    char tmp_data[1024];
    for(int i=0; i < MAX_PRODUCER_CNT * LOOP_CNT  ; i++) {
        if( total_fetched >= (MAX_PRODUCER_CNT*  LOOP_CNT) ) {
            break;
        }

        returned_index = gSharedMemRingBuffer.WaitFor(my_id, my_index);

#ifdef _DEBUG_READ_
        snprintf(msg, sizeof(msg), 
                "[id:%d]    \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d] WaitFor my_index[%" PRId64 "] returned_index[%" PRId64 "]", 
                my_id, __func__, __LINE__, my_index, returned_index );
        {AtomicPrint atomicPrint(msg);}
#endif

        for(int64_t j = my_index; j <= returned_index; j++) {
            //batch job 
            size_t data_len =0;
            const char* pData =  gSharedMemRingBuffer.GetData(j, & data_len );

            memset(tmp_data, 0x00, sizeof(tmp_data));
            strncpy(tmp_data, pData, data_len);
#ifdef _DEBUG_READ_
            snprintf(msg, sizeof(msg), 
                    "[id:%d]   \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d]  my_index[%" PRId64 ", translated:%" PRId64 "] data [%s] ^^", 
                    my_id, __func__, __LINE__, j, gSharedMemRingBuffer.GetTranslatedIndex(j), tmp_data );
            {AtomicPrint atomicPrint(msg);}
#endif
            gSharedMemRingBuffer.CommitRead(my_id, j );
            total_fetched++;

        } //for

        my_index = returned_index + 1; 
    }

    snprintf(msg, sizeof(msg), 
            "\t\t\t\t\t\t\t\t\t\t\t\t********* DONE : ThreadWorkRead [id:%06d] last data [%s] ", my_id, tmp_data );
    {AtomicPrint atomicPrint(msg);}
}

///////////////////////////////////////////////////////////////////////////////
void TestFunc()
{
    std::vector<std::thread> vec_consumers ;
    std::vector<std::thread> vec_producers ;

    //Consumer
    //1. register
    std::vector<int64_t> vecConsumerIndexes ;
    for(int i = 0; i < MAX_CONSUMER_CNT; i++) {
        int64_t nIndexforCustomerUse = -1;
        if(!gSharedMemRingBuffer.RegisterConsumer(i, &nIndexforCustomerUse)){
            return; //error
        }
        vecConsumerIndexes.push_back(nIndexforCustomerUse);
    }

    //2. run
    for(int i = 0; i < MAX_CONSUMER_CNT; i++) {
        vec_consumers.push_back (std::thread (ThreadWorkRead, "consumer", i, vecConsumerIndexes[i] ) );
    }

    //producer run
    for(int i = 0; i < MAX_PRODUCER_CNT; i++) {
        vec_producers.push_back (std::thread (ThreadWorkWrite, "procucer", i ) );
    }
    //wait..
    for(int i = 0; i < MAX_PRODUCER_CNT; i++) {
        vec_producers[i].join();
    }
    for(int i = 0; i < MAX_CONSUMER_CNT; i++) {
        vec_consumers[i].join();
    }
}


///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    int MAX_TEST = 10;
    LOOP_CNT = 1000;
    int MAX_RBUFFER_CAPACITY = 1024*8; 
    int MAX_RAW_MEM_BUFFER_SIZE = 1000000; 
    MAX_PRODUCER_CNT = 1; 
    MAX_CONSUMER_CNT = 100; 

    if(! gSharedMemRingBuffer.Init(123456,
                                   MAX_RBUFFER_CAPACITY, 
                                   923456,
                                   MAX_RAW_MEM_BUFFER_SIZE ) )
    { 
        return 1; //Error!
    }
    for ( int i=0; i < MAX_TEST; i++) {

        ElapsedTime elapsed;
        TestFunc();
        long long nElapsedMicro= elapsed.SetEndTime(MICRO_SEC_RESOLUTION);
        std::cout << "**** test " << i << " / count:"<< LOOP_CNT << " -> elapsed : "<< nElapsedMicro << "(micro sec) /"
            << (long long) (LOOP_CNT*1000000L)/nElapsedMicro <<" TPS\n";
    }
    return 0;
}

