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

#include "ring_buffer_on_shmem.hpp" 
#include "atomic_print.hpp"

int LEN_ONE_BUFFER_DATA = sizeof(PositionInfo);

#define _DEBUG_        0
#define _DEBUG_COMMIT  0

std::mutex AtomicPrint::lock_mutex_ ;


///////////////////////////////////////////////////////////////////////////////
SharedMemRingBuffer::SharedMemRingBuffer()
{
    pStatusOnSharedMem_ = NULL;
    waitStrategyType_ = SLEEPING_WAIT ;
    pWaitStrategy_ = NULL;
}
///////////////////////////////////////////////////////////////////////////////
SharedMemRingBuffer::SharedMemRingBuffer(ENUM_WAIT_STRATEGY waitStrategyType)
{
    pStatusOnSharedMem_ = NULL;
    waitStrategyType_ = waitStrategyType ;
    pWaitStrategy_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
SharedMemRingBuffer::~SharedMemRingBuffer()
{
    if(pWaitStrategy_)
    {
        delete pWaitStrategy_;
        pWaitStrategy_ = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::Terminate()
{
    if(! sharedIndexBuffer_.DetachShMem())
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "Error " << '\n'; 
        return false;
    }

    if(! sharedIndexBuffer_.RemoveShMem())
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << 
            "Ln[" << __LINE__ << "] " << "Error " << '\n'; 
        return false;
    }

    if(! sharedRawMemoryBuffer_.DetachShMem())
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "Error " << '\n'; 
        return false;
    }

    if(! sharedRawMemoryBuffer_.RemoveShMem())
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << 
            "Ln[" << __LINE__ << "] " << "Error " << '\n'; 
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::RegisterConsumer (int nId, int64_t* pIndexforCustomerUse)
{
    if(pStatusOnSharedMem_->arrayOfConsumerIndexes[nId] == -1 )
    {
        //처음 등록 
        pStatusOnSharedMem_->registered_consumer_count++;
        if( pStatusOnSharedMem_->registered_consumer_count >= MAX_CONSUMER)
        {
            std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "Error: Exceeds MAX_CONSUMER : " << MAX_CONSUMER << '\n'; 
            return false;
        }

        if(pStatusOnSharedMem_->cursor >= 0 )
        {
            std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "cursor >= 0 " << '\n'; 
            //데이터 전달 중이데 새로운 소비자가 추가
            pStatusOnSharedMem_->arrayOfConsumerIndexes[nId] = pStatusOnSharedMem_->cursor.load() ; 
        }
        else
        {
            std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "set  0 " << '\n'; 
            pStatusOnSharedMem_->arrayOfConsumerIndexes[nId] = 0; 
        }

        *pIndexforCustomerUse = pStatusOnSharedMem_->arrayOfConsumerIndexes[nId];
    }
    else
    {
        //last read message index 
        //기존 최종 업데이트 했던 인덱스 + 1 돌려준다. consumer가 호출할 인덱스 이므로 ..
        *pIndexforCustomerUse = pStatusOnSharedMem_->arrayOfConsumerIndexes[nId] + 1;
    }

    std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "USAGE_CONSUMER ID : " << nId<< " / index :"<< *pIndexforCustomerUse << '\n'; 

    return true;
}

///////////////////////////////////////////////////////////////////////////////
void SharedMemRingBuffer::ResetRingBufferState() 
{
    if(pStatusOnSharedMem_ == NULL )
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "call InitIndexBuffer first !" << '\n'; 
        return;
    }

    std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " <<  '\n'; 

    pStatusOnSharedMem_->cursor.store(-1);
    pStatusOnSharedMem_->next.store(-1);
    pStatusOnSharedMem_->registered_producer_count.store(0);
    pStatusOnSharedMem_->registered_consumer_count.store(0);

    nTotalMemSize_ = 0;

    for(int i = 0; i < MAX_CONSUMER; i++)
    {
        pStatusOnSharedMem_->arrayOfConsumerIndexes[i] = -1;
    }

    //for blocking wait strategy : shared mutex, shared cond var
    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init( & pStatusOnSharedMem_->mtxLock, &mutexAttr);

    pthread_condattr_t condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init( & pStatusOnSharedMem_->condVar, &condAttr);
}


///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::Init(key_t keyIndexBuffer, int nIndexBufferSize , 
                               key_t keyDataBuffer,  int nDataBufferSize   )
{
    if(!InitIndexBuffer(keyIndexBuffer, nIndexBufferSize))
    {
        return false;
    }

    if(!InitRawMemBuffer(keyDataBuffer, nDataBufferSize))
    {
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::InitRawMemBuffer(key_t keyDataBuffer, int nSize )
{
    if(nSize<= 0)
    {
        std::cout << "Ln[" << __LINE__ << "] " << "Error: Invalid size " << '\n'; 
        return false;
    }

    nMaxDataSize_ = nSize ;

    nRawMemBufferCapacity_ = nSize;

    bool bSharedMemFirstCreated = false;
    if(! sharedRawMemoryBuffer_.CreateShMem(keyDataBuffer, nSize, &bSharedMemFirstCreated )) 
    {
        std::cout << "Ln[" << __LINE__ << "] " << "Error: shared memory failed " << '\n'; 
        return false;
    }

    if(! sharedRawMemoryBuffer_.AttachShMem())
    {
        std::cout << "Ln[" << __LINE__ << "] " << "Error: shared memory failed " << '\n'; 
        return false;
    }

    pRawMemBuffer_ = (char*) sharedRawMemoryBuffer_. GetShMemStartAddr(); 

    return true;
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::InitIndexBuffer(key_t keyIndexBuffer, int nSize )
{
    if(nSize<= 0)
    {
        std::cout << "Ln[" << __LINE__ << "] " << "Error: Invalid size " << '\n'; 
        return false;
    }

    nBufferSize_ = nSize;

    if(!ringBuffer_.SetCapacity(nSize) )
    {
        std::cout << "Ln[" << __LINE__ << "] " << "Error: Invalid size " << '\n'; 
        return false;
    }

    //shared memory consists of : StatusOnSharedMem + actual data
    nTotalMemSize_ = sizeof(_StatusOnSharedMem_)  + (sizeof(PositionInfo) * nSize) ;

    bool bSharedMemFirstCreated = false;
    if(! sharedIndexBuffer_.CreateShMem(keyIndexBuffer, nTotalMemSize_, &bSharedMemFirstCreated )) 
    {
        std::cout << "Ln[" << __LINE__ << "] " << "Error: shared memory failed " << '\n'; 
        return false;
    }

    if(! sharedIndexBuffer_.AttachShMem())
    {
        std::cout << "Ln[" << __LINE__ << "] " << "Error: shared memory failed " << '\n'; 
        return false;
    }

    pStatusOnSharedMem_ = (StatusOnSharedMem*) sharedIndexBuffer_. GetShMemStartAddr(); 
    if(bSharedMemFirstCreated)
    {
        ResetRingBufferState();
    }

    char* pBufferStart = (char*)sharedIndexBuffer_.GetShMemStartAddr() + sizeof(_StatusOnSharedMem_) ; 

    for(int i = 0; i < nSize; i++)
    {
        //PositionInfo* pData = new( (char*)pBufferStart + (sizeof(PositionInfo)*i)) PositionInfo; 
        //ringBuffer_[i] = pData ;
        ringBuffer_[i] = (PositionInfo*) ( (char*)pBufferStart + (sizeof(PositionInfo)*i) ) ;
    }

    pStatusOnSharedMem_->nBufferSize = nSize;
    pStatusOnSharedMem_->nTotalMemSize = nTotalMemSize_;

    //---------------------------------------------
    //wait strategy
    if(waitStrategyType_ == BLOCKING_WAIT )
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "Wait Strategy :BLOCKING_WAIT" << '\n'; 
        pWaitStrategy_ = new BlockingWaitStrategy(pStatusOnSharedMem_);
    }
    else if(waitStrategyType_ == YIELDING_WAIT )
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "Wait Strategy :YIELDING_WAIT" << '\n'; 
        pWaitStrategy_ = new YieldingWaitStrategy(pStatusOnSharedMem_);
    }
    else if(waitStrategyType_ == SLEEPING_WAIT )
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "Wait Strategy :SLEEPING_WAIT" << '\n'; 
        pWaitStrategy_ = new SleepingWaitStrategy(pStatusOnSharedMem_);
    }
    else
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "Invalid Wait Strategy :" << waitStrategyType_<< '\n'; 
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
const char*  SharedMemRingBuffer::GetData(int64_t nIndex, int* nOutLen)
{
    *nOutLen = ringBuffer_[nIndex]->nLen;
    //std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " << "get data / pos:"<< ringBuffer_[nIndex]->nStartPosition << '\n'; 
    return pRawMemBuffer_ + ringBuffer_[nIndex]->nStartPosition; 
}

///////////////////////////////////////////////////////////////////////////////
int64_t SharedMemRingBuffer::GetTranslatedIndex( int64_t sequence)
{
    return ringBuffer_.GetTranslatedIndex(sequence);
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::SetData( int64_t nIndex, PositionInfo* pData, int nWritePosition,  char* pRawData)
{
    //claim 으로 얻은 position을 가지고 계산해서 저장  
    //update usage info. 
    memcpy( ringBuffer_[nIndex], pData, LEN_ONE_BUFFER_DATA ); 

    //actual data of arbitrary length
    memcpy( pRawMemBuffer_ + nWritePosition, pRawData, pData->nLen);

    return true;
}

///////////////////////////////////////////////////////////////////////////////
int64_t SharedMemRingBuffer::GetMinIndexOfConsumers()
{
    int64_t nMinIndex = INT64_MAX ;
    bool bFound = false;

    for(int i = 0; i < pStatusOnSharedMem_->registered_consumer_count; i++)
    {
        int64_t nIndex = pStatusOnSharedMem_->arrayOfConsumerIndexes[i];
        if( nIndex < nMinIndex )
        {
            nMinIndex = nIndex;
            bFound = true;
        }
    }

    if(!bFound)
    {
        return 0;
    }

    return nMinIndex ;
}

///////////////////////////////////////////////////////////////////////////////
int64_t SharedMemRingBuffer::GetNextSequenceForClaim()
{
    return pStatusOnSharedMem_->next.fetch_add(1) + 1;
}

///////////////////////////////////////////////////////////////////////////////
//[입력] nWantLen : 쓰기를 원하는 데이터 크기
//[출력] nOutPositionToWrite : 버퍼에 쓸 위치
//       사용할 index         
int64_t SharedMemRingBuffer::ClaimIndex(int nWantLen, int* nOutPositionToWrite )
{
    if( nWantLen > nMaxDataSize_ )
    {
        std::cout << "["<< __func__ <<"-"<<  __LINE__ << "] " 
                  << "Invalid data length : exceeds MAX  " << nMaxDataSize_<< '\n'; 
        return -1;
    }

    ENUM_DATA_STATUS data_status = DATA_EMPTY;
    int64_t nNextSeqForClaim = GetNextSequenceForClaim() ;
    ringBuffer_[nNextSeqForClaim]->status =DATA_EMPTY; //init

    //다음 쓰기 위치 - 링버퍼 크기 = 링버퍼 한번 순회 이전위치, 
    //데이터 쓰기 작업이 링버퍼를 완전히 한바퀴 순회한 경우
    //여기서 더 진행하면 데이터를 이전 덮어쓰게 된다.
    //만약 아직 데이터를 read하지 못한 상태(min customer index가 wrap position보다 작거나 같은 경우)
    //라면 쓰기 작업은 대기해야 한다
    int64_t wrapPoint = nNextSeqForClaim - nBufferSize_;

    do
    {
        if(nNextSeqForClaim>0)
        {
            data_status = ringBuffer_[nNextSeqForClaim-1]->status ;
        }

        int64_t gatingSequence = GetMinIndexOfConsumers();

        if  ( wrapPoint >=  gatingSequence ) 
        {
            std::this_thread::yield();
            //std::this_thread::sleep_for(std::chrono::nanoseconds(1)); 
            continue;
        }
        else if  ( nNextSeqForClaim>0 && data_status == DATA_EMPTY ) 
        {
            //because of arbitrary data length, we need to wait until previous data is set 

            std::this_thread::yield();
            //std::this_thread::sleep_for(std::chrono::nanoseconds(1)); 
#if _DEBUG_
            //std::this_thread::sleep_for(std::chrono::milliseconds(1)); //FOR DEBUG ONLY!
            std::cout << "Ln[" << __LINE__ << "] " << "continue , DATA_EMPTY / nNextSeqForClaim:" << nNextSeqForClaim << '\n'; 
#endif
            continue;
        }
        else
        {
            //calculate nOutPositionToWrite
            if(nNextSeqForClaim==0)
            {
                *nOutPositionToWrite = 0;
            }
            else
            {
                *nOutPositionToWrite = ringBuffer_[nNextSeqForClaim-1]->nOffsetPosition ;
#if _DEBUG_
                std::cout << "Ln[" << __LINE__ << "] " << "write pos: "<< *nOutPositionToWrite << '\n'; 
#endif
            }


            if( nNextSeqForClaim>0 && 
                ringBuffer_[nNextSeqForClaim-1]->nOffsetPosition + nWantLen > nRawMemBufferCapacity_ )
            {
                //last write position + wanted data length > total data buffer size 
                //--> start from 0 position  
#if _DEBUG_
                std::cout << "Ln[" << __LINE__ << "] " << "prevResetPos : "<< pStatusOnSharedMem_->prevResetPos<<  '\n'; 
#endif
                if( gatingSequence!=0 && gatingSequence < pStatusOnSharedMem_->prevResetPos )
                {
                    //but don't over-write customer data 
                    //'gatingSequence == 0' means a consumer read index 0, so it is OK to write at index 0
                    
                    std::this_thread::yield();
                    //std::this_thread::sleep_for(std::chrono::nanoseconds(1)); 

#if _DEBUG_
                    //std::this_thread::sleep_for(std::chrono::milliseconds(10)); //FOR DEBUG ONLY!
                    std::cout << "Ln[" << __LINE__ << "] " << "spin... / gatingSequence=" << gatingSequence
                              << " / nNextSeqForClaim= "<< nNextSeqForClaim 
                              << " / pStatusOnSharedMem_->prevResetPos=" << pStatusOnSharedMem_->prevResetPos <<  '\n'; 
#endif
                    continue;
                }
                
                pStatusOnSharedMem_->prevResetPos = nNextSeqForClaim ;

#if _DEBUG_
                std::cout << "Ln[" << __LINE__ << "] " <<"nRawMemBufferCapacity_ :"<< nRawMemBufferCapacity_ 
                          << " / gatingSequence: "<< gatingSequence << " / nWantLen:"
                          << nWantLen <<" / ringBuffer_[gatingSequence]->nStartPosition: "
                          << ringBuffer_[gatingSequence]->nStartPosition <<  '\n'; 

                std::cout << "Ln[" << __LINE__ << "] " << "reset pos 0 "<<  '\n'; 
#endif

                *nOutPositionToWrite = 0;
            }

            break;
        }
    }
    while (true);

#if _DEBUG_
    std::cout << "Ln[" << __LINE__ << "] " << "RETURN nNextSeqForClaim:" << nNextSeqForClaim << '\n'; 
#endif
    return nNextSeqForClaim;

}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::Commit(int nUserId, int64_t index)
{
    //cursor 가 index 바로 앞인 경우만 성공한다.
    int64_t expected = index -1 ;

    while (true)
    {
        //if ( cursor_.compare_exchange_strong(expected , index ))
        if ( pStatusOnSharedMem_->cursor == expected )
        {
            pStatusOnSharedMem_->cursor = index;

            break;
        }
    
        std::this_thread::yield();
        //std::this_thread::sleep_for(std::chrono::nanoseconds(1)); 
    }
    pWaitStrategy_->SignalAllWhenBlocking(); //blocking wait strategy only.

    return true;
}

///////////////////////////////////////////////////////////////////////////////
void SharedMemRingBuffer::SignalAll()
{
    pWaitStrategy_->SignalAllWhenBlocking(); //blocking wait strategy only.
}

///////////////////////////////////////////////////////////////////////////////
int64_t SharedMemRingBuffer::WaitFor(int nUserId, int64_t nIndex)
{
    int64_t nCurrentCursor = pStatusOnSharedMem_->cursor.load() ;

    if( nIndex > nCurrentCursor )
    {
#if _DEBUG_
        char szMsg[100];
        snprintf(szMsg, sizeof(szMsg), 
                "[id:%d]    \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d] index [%" PRId64 " - trans : %" PRId64 "] no data, wait for :nCurrentCursor[%" PRId64 "]", 
                nUserId, __func__, __LINE__, nIndex, GetTranslatedIndex(nIndex),nCurrentCursor  );
        {AtomicPrint atomicPrint(szMsg);}
#endif
        //wait strategy
        return pWaitStrategy_->Wait(nIndex);
    }
    else
    {
#if _DEBUG_
        char szMsg[100];
        snprintf(szMsg, sizeof(szMsg), "[id:%d]    \t\t\t\t\t\t\t\t\t\t\t\t [%s-%d] index[%" PRId64 "] returns [%" PRId64 "] ",
                nUserId, __func__, __LINE__, nIndex, pStatusOnSharedMem_->cursor.load() );
        {AtomicPrint atomicPrint(szMsg);}
#endif
        return nCurrentCursor ;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
bool  SharedMemRingBuffer::CommitRead(int nUserId, int64_t index)
{
    pStatusOnSharedMem_->arrayOfConsumerIndexes[nUserId] = index ; //update

#if _DEBUG_
    char szMsg[100];
    snprintf(szMsg, sizeof(szMsg), 
            "[id:%d]     \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d] index[%" PRId64 "] ", nUserId, __func__, __LINE__, index );
    {AtomicPrint atomicPrint(szMsg);}
#endif

    return true;
}


