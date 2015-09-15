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

using namespace std;

int gTestIndex;
int LOOP_CNT ;

//SharedMemRingBuffer gSharedMemRingBuffer (YIELDING_WAIT); 
//SharedMemRingBuffer gSharedMemRingBuffer (SLEEPING_WAIT); 
SharedMemRingBuffer gSharedMemRingBuffer (BLOCKING_WAIT); 

///////////////////////////////////////////////////////////////////////////////
void TestFunc()
{
    char    szMsg[1024];
    int64_t nMyIndex = -1;
    char    szRawData[1024];

    for(int i=1; i <= LOOP_CNT  ; i++) 
    {
        PositionInfo my_data;
        int nWritePosition = 0;
        

        if(i%2==0)
        {
            my_data.nLen = snprintf(szRawData, sizeof(szRawData), "raw data  %06d", i);
        }
        else
        {
            my_data.nLen = snprintf(szRawData, sizeof(szRawData), "raw data  %08d", i);
        }

        nMyIndex = gSharedMemRingBuffer.ClaimIndex(my_data.nLen, & nWritePosition );

        my_data.nStartPosition = nWritePosition  ; 
        my_data.nOffsetPosition = nWritePosition + my_data.nLen; //fot next write
        my_data.status = DATA_EXISTS ;
         
#ifdef _DEBUG_WRITE_
        snprintf(szMsg, sizeof(szMsg), 
                "[id:%d]    [%s-%d] Write nMyIndex[%ld] %s: want len[%d] nWritePosition [%d] my_data.nLastPosition [%d]", 
                 0, __func__, __LINE__, nMyIndex, szRawData, my_data.nLen, nWritePosition, my_data.nOffsetPosition );
        {AtomicPrint atomicPrint(szMsg);}
#endif

        gSharedMemRingBuffer.SetData( nMyIndex, &my_data, nWritePosition, szRawData );

        gSharedMemRingBuffer.Commit(0, nMyIndex); 
    }

    snprintf(szMsg, sizeof(szMsg), 
            "*********************[id:%d]    [%s-%d] Write Done", 0, __func__, __LINE__ );
    {AtomicPrint atomicPrint(szMsg);}

}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    int MAX_TEST = 1;
    LOOP_CNT = 10000; //should be same as consumer fot TEST
    int MAX_RBUFFER_CAPACITY = 1024*4; //should be same as consumer for TEST
    int MAX_RAW_MEM_BUFFER_SIZE = 100000; //should be same as consumer for TEST

    if(! gSharedMemRingBuffer.Init(123456,
                                   MAX_RBUFFER_CAPACITY, 
                                   923456,
                                   MAX_RAW_MEM_BUFFER_SIZE ) )
    { 
        //Error!
        return 1; 
    }

    for ( gTestIndex=0; gTestIndex < MAX_TEST; gTestIndex++)
    {
        ElapsedTime elapsed;

        TestFunc();

        long long nElapsedMicro= elapsed.SetEndTime(MICRO_SEC_RESOLUTION);
        std::cout << "**** test " << gTestIndex << " / count:"<< LOOP_CNT << " -> elapsed : "<< nElapsedMicro << "(micro sec) /"
            << (long long) (LOOP_CNT*1000000L)/nElapsedMicro <<" TPS\n";
    }

    return 0;
}

