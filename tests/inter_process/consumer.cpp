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

//Wait Strategy 
//SharedMemRingBuffer gSharedMemRingBuffer (YIELDING_WAIT); 
//SharedMemRingBuffer gSharedMemRingBuffer (SLEEPING_WAIT); 
SharedMemRingBuffer gSharedMemRingBuffer (BLOCKING_WAIT); 
        

///////////////////////////////////////////////////////////////////////////////
void TestFunc(int nCustomerId)
{
    //1. register
    int64_t nIndexforCustomerUse = -1;
    if(!gSharedMemRingBuffer.RegisterConsumer(nCustomerId, &nIndexforCustomerUse ) )
    {
        return; //error
    }

    //2. run
    ElapsedTime elapsed;
    char    szMsg[1024];
    char    szData[1024];
    int64_t nTotalFetched = 0; 
    int64_t nMyIndex = nIndexforCustomerUse ; 
    int64_t nReturnedIndex =-1;

    bool bFirst=true;

    for(int i=0; i < LOOP_CNT  ; i++)
    {
        if( nTotalFetched >= LOOP_CNT ) 
        {
            break;
        }

        nReturnedIndex = gSharedMemRingBuffer.WaitFor(nCustomerId, nMyIndex);

        if(bFirst)
        {
            bFirst=false;
            elapsed.SetStartTime();
        }

#ifdef _DEBUG_READ_
        snprintf(szMsg, sizeof(szMsg), 
                "[id:%d]    \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d] WaitFor nMyIndex[%" PRId64 "] nReturnedIndex[%" PRId64 "]", 
                nCustomerId, __func__, __LINE__, nMyIndex, nReturnedIndex );
        {AtomicPrint atomicPrint(szMsg);}
#endif

        for(int64_t j = nMyIndex; j <= nReturnedIndex; j++)
        {
            //batch job 
            int nDataLen =0;
            const char* pData =  gSharedMemRingBuffer.GetData(j, & nDataLen );
            //const char*  SharedMemRingBuffer::GetData(int64_t nIndex, int* nOutLen)

            memset(szData, 0x00, sizeof(szData));
            strncpy(szData, pData, nDataLen);

#ifdef _DEBUG_READ_
            snprintf(szMsg, sizeof(szMsg), 
                    "[id:%d]   \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d]  nMyIndex[%" PRId64 ", translated:%" PRId64 "] data [%s] ^^", 
                    nCustomerId, __func__, __LINE__, j, gSharedMemRingBuffer.GetTranslatedIndex(j), szData );
            {AtomicPrint atomicPrint(szMsg);}
#endif

            gSharedMemRingBuffer.CommitRead(nCustomerId, j );
            nTotalFetched++;

        } //for

        nMyIndex = nReturnedIndex + 1; 
    }

    long long nElapsedMicro= elapsed.SetEndTime(MICRO_SEC_RESOLUTION);
    snprintf(szMsg, sizeof(szMsg), "**** consumer test %d -> elapsed :%lld (micro sec) TPS %lld / data[%s]", 
        gTestIndex , nElapsedMicro, (long long) (10000L*1000000L)/nElapsedMicro, szData );
    {AtomicPrint atomicPrint(szMsg);}
}


///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        std::cout << "usage: "<< argv[0]<<" customer_index"  << '\n';
        std::cout << "ex: "<< argv[0]<<" 0"  << '\n';
        std::cout << "ex: "<< argv[0]<<" 1"  << '\n';
        return 1;
    }

    int nCustomerId = atoi(argv[1]);
    int MAX_TEST = 1;
    LOOP_CNT = 10000;    //should be same as producer for TEST
    int MAX_RBUFFER_CAPACITY = 1024*4; //should be same as producer for TEST
    int MAX_RAW_MEM_BUFFER_SIZE = 100000; //should be same as producer for TEST

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
        TestFunc(nCustomerId);
    }

    return 0;
}

