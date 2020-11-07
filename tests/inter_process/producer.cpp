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

//SharedMemRingBuffer gSharedMemRingBuffer (YIELDING_WAIT); 
SharedMemRingBuffer gSharedMemRingBuffer (SLEEPING_WAIT); 
//SharedMemRingBuffer gSharedMemRingBuffer (BLOCKING_WAIT); 

///////////////////////////////////////////////////////////////////////////////
void TestFunc()
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
                "Write my_index[%ld] %s: want len[%ld] write_position [%ld] my_data.nLastPosition [%ld]", 
                 my_index, raw_data, my_data.len, write_position, my_data.offset_position );
        {AtomicPrint atomicPrint(msg);}
#endif
        gSharedMemRingBuffer.SetData( my_index, &my_data, write_position, raw_data );

        gSharedMemRingBuffer.Commit( my_index); 
    }

    snprintf(msg, sizeof(msg), "*  write done, last data [%s]",  
            raw_data );
    {AtomicPrint atomicPrint(msg);}

}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    LOOP_CNT = 10000; //should be same as consumer fot TEST
    int MAX_RBUFFER_CAPACITY = 1024*8; //should be same as consumer for TEST
    int MAX_RAW_MEM_BUFFER_SIZE = 1000000; //should be same as consumer for TEST

    if(! gSharedMemRingBuffer.Init(123456,
                                   MAX_RBUFFER_CAPACITY, 
                                   923456,
                                   MAX_RAW_MEM_BUFFER_SIZE ) ) { 
        //Error!
        return 1; 
    }

    ElapsedTime elapsed;
    TestFunc();
    long long elapsed_micro = elapsed.SetEndTime(MICRO_SEC_RESOLUTION);
    std::cout << "**** count:"<< LOOP_CNT << " -> elapsed : "<< elapsed_micro << "(micro sec) /"
        << (long long) (LOOP_CNT*1000000L)/elapsed_micro <<" TPS\n";

    return 0;
}

