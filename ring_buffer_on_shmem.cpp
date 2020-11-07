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

size_t LEN_ONE_BUFFER_DATA = sizeof(PositionInfo);
///////////////////////////////////////////////////////////////////////////////
SharedMemRingBuffer::SharedMemRingBuffer()
{
    max_data_size_ = 0;
    buffer_size_ = 0;
    total_mem_size_ = 0;
    ring_buffer_status_on_shared_mem_ = NULL;
    wait_strategy_type_ = SLEEPING_WAIT ;
    wait_strategy_ = NULL;
}
///////////////////////////////////////////////////////////////////////////////
SharedMemRingBuffer::SharedMemRingBuffer(ENUM_WAIT_STRATEGY wait_strategy)
{
    max_data_size_ = 0;
    buffer_size_ = 0;
    total_mem_size_ = 0;
    ring_buffer_status_on_shared_mem_ = NULL;
    wait_strategy_type_ = wait_strategy ;
    wait_strategy_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
SharedMemRingBuffer::~SharedMemRingBuffer()
{
    if(wait_strategy_) {
        delete wait_strategy_;
        wait_strategy_ = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::Terminate()
{
    if(! shared_index_buffer_.DetachShMem()) {
        DEBUG_ELOG("Error"); 
        return false;
    }
    if(! shared_index_buffer_.RemoveShMem()) {
        DEBUG_ELOG("Error"); 
        return false;
    }
    if(! shared_raw_memory_buffer_.DetachShMem()) {
        DEBUG_ELOG("Error"); 
        return false;
    }
    if(! shared_raw_memory_buffer_.RemoveShMem()) {
        DEBUG_ELOG("Error"); 
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::RegisterConsumer (int id, int64_t* index_for_customer)
{
    if(ring_buffer_status_on_shared_mem_->array_consumer_indexes[id] == -1 ) {
        //처음 등록 
        ring_buffer_status_on_shared_mem_->registered_consumer_count++;
        if( ring_buffer_status_on_shared_mem_->registered_consumer_count >= MAX_CONSUMER) {
            DEBUG_ELOG("Error: Exceeds MAX_CONSUMER : " << MAX_CONSUMER); 
            return false;
        }

        if(ring_buffer_status_on_shared_mem_->cursor >= 0 ) {
            DEBUG_LOG("cursor >= 0"); 
            //데이터 전달 중이데 새로운 소비자가 추가
            ring_buffer_status_on_shared_mem_->array_consumer_indexes[id] = 
                ring_buffer_status_on_shared_mem_->cursor.load() ; 
        } else {
            DEBUG_LOG("set 0 "); 
            ring_buffer_status_on_shared_mem_->array_consumer_indexes[id] = 0; 
        }

        *index_for_customer = ring_buffer_status_on_shared_mem_->array_consumer_indexes[id];
    } else {
        //last read message index 
        //기존 최종 업데이트 했던 인덱스 + 1 돌려준다. consumer가 호출할 인덱스 이므로 ..
        *index_for_customer = ring_buffer_status_on_shared_mem_->array_consumer_indexes[id] + 1;
    }
    DEBUG_LOG("USAGE_CONSUMER ID : " << id<< " / index : "<< *index_for_customer); 
    return true;
}

///////////////////////////////////////////////////////////////////////////////
void SharedMemRingBuffer::ResetRingBufferState() 
{
    if(ring_buffer_status_on_shared_mem_ == NULL ) {
        DEBUG_LOG("call InitIndexBuffer first !"); 
        return;
    }
    DEBUG_LOG("---"); 

    ring_buffer_status_on_shared_mem_->cursor.store(-1);
    ring_buffer_status_on_shared_mem_->next.store(-1);
    ring_buffer_status_on_shared_mem_->registered_producer_count.store(0);
    ring_buffer_status_on_shared_mem_->registered_consumer_count.store(0);

    total_mem_size_ = 0;

    for(int i = 0; i < MAX_CONSUMER; i++) {
        ring_buffer_status_on_shared_mem_->array_consumer_indexes[i] = -1;
    }
    //for blocking wait strategy : shared mutex, shared cond var
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init( & ring_buffer_status_on_shared_mem_->mutex_lock, &mutex_attr);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init( & ring_buffer_status_on_shared_mem_->cond_var, &cond_attr);
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::Init(key_t key_index_buffer, size_t size_index_buffer , 
                               key_t key_data_buffer,  size_t size_data_buffer  )
{
    if(!InitIndexBuffer(key_index_buffer, size_index_buffer)) {
        return false;
    }
    if(!InitRawMemBuffer(key_data_buffer, size_data_buffer)) {
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::InitRawMemBuffer(key_t key_data_buffer, size_t size )
{
    if(size == 0) {
        std::cout << "Ln[" << __LINE__ << "] " << "Error: Invalid size " << '\n'; 
        DEBUG_ELOG("Error: Invalid size : " << size); 
        return false;
    }

    max_data_size_ = size ;
    raw_mem_buffer_capacity_ = size;
    bool bSharedMemFirstCreated = false;
    if(! shared_raw_memory_buffer_.CreateShMem(key_data_buffer, size, &bSharedMemFirstCreated )) {
        DEBUG_ELOG("Error: shared memory failed "); 
        return false;
    }
    if(! shared_raw_memory_buffer_.AttachShMem()) {
        DEBUG_ELOG("Error: shared memory failed "); 
        return false;
    }
    raw_mem_buffer_ = (char*) shared_raw_memory_buffer_. GetShMemStartAddr(); 
    return true;
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::InitIndexBuffer(key_t key_index_buffer, size_t size )
{
    if(size == 0) {
        DEBUG_ELOG("Error: Invalid size : " << size); 
        return false;
    }
    buffer_size_ = size;

    if(!ring_buffer_.SetCapacity(size) ) {
        DEBUG_ELOG("Error: Invalid size : " << size); 
        return false;
    }
    //shared memory consists of : StatusOnSharedMem + actual data
    total_mem_size_ = sizeof(_StatusOnSharedMem_)  + (sizeof(PositionInfo) * size) ;

    bool bSharedMemFirstCreated = false;
    if(! shared_index_buffer_.CreateShMem(key_index_buffer, total_mem_size_, &bSharedMemFirstCreated )) {
        DEBUG_ELOG("Error: shared memory failed "); 
        return false;
    }
    if(! shared_index_buffer_.AttachShMem()) {
        DEBUG_ELOG("Error: shared memory failed "); 
        return false;
    }

    ring_buffer_status_on_shared_mem_ = (StatusOnSharedMem*) shared_index_buffer_. GetShMemStartAddr(); 
    if(bSharedMemFirstCreated) {
        ResetRingBufferState();
    }
    char* buffer_start = (char*)shared_index_buffer_.GetShMemStartAddr() + sizeof(_StatusOnSharedMem_) ; 

    for(size_t i = 0; i < size; i++) {
        ring_buffer_[i] = (PositionInfo*) ( (char*)buffer_start + (sizeof(PositionInfo)*i) ) ;
    }

    ring_buffer_status_on_shared_mem_->nBufferSize = size;
    ring_buffer_status_on_shared_mem_->nTotalMemSize = total_mem_size_;

    //---------------------------------------------
    //wait strategy
    if(wait_strategy_type_ == BLOCKING_WAIT ) {
        DEBUG_LOG ("Wait Strategy :BLOCKING_WAIT" ); 
        wait_strategy_ = new BlockingWaitStrategy(ring_buffer_status_on_shared_mem_);
    } else if(wait_strategy_type_ == YIELDING_WAIT ) {
        DEBUG_LOG( "Wait Strategy :YIELDING_WAIT" ); 
        wait_strategy_ = new YieldingWaitStrategy(ring_buffer_status_on_shared_mem_);
    } else if(wait_strategy_type_ == SLEEPING_WAIT ) {
        DEBUG_LOG( "Wait Strategy :SLEEPING_WAIT" ); 
        wait_strategy_ = new SleepingWaitStrategy(ring_buffer_status_on_shared_mem_);
    } else {
        DEBUG_ELOG( "Invalid Wait Strategy :" << wait_strategy_type_); 
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
const char*  SharedMemRingBuffer::GetData(int64_t index, size_t* out_len)
{
    *out_len = ring_buffer_[index]->len;
    return raw_mem_buffer_ + ring_buffer_[index]->start_position; 
}

///////////////////////////////////////////////////////////////////////////////
int64_t SharedMemRingBuffer::GetTranslatedIndex( int64_t sequence)
{
    return ring_buffer_.GetTranslatedIndex(sequence);
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::SetData( int64_t index, PositionInfo* pos_info, size_t write_position, char* raw_data )
{
    //claim 으로 얻은 position을 가지고 계산해서 저장  
    //update usage info. 
    memcpy( ring_buffer_[index], pos_info, LEN_ONE_BUFFER_DATA ); 

    //actual data of arbitrary length
    memcpy( raw_mem_buffer_ + write_position, raw_data, pos_info->len);

    return true;
}

///////////////////////////////////////////////////////////////////////////////
int64_t SharedMemRingBuffer::GetMinIndexOfConsumers()
{
    int64_t min_index = INT64_MAX ;
    bool is_found = false;
    for(int i = 0; i < ring_buffer_status_on_shared_mem_->registered_consumer_count; i++) {
        int64_t nIndex = ring_buffer_status_on_shared_mem_->array_consumer_indexes[i];
        if( nIndex < min_index ) {
            min_index = nIndex;
            is_found = true;
        }
    }
    if(!is_found) {
        return 0;
    }
    return min_index ;
}

///////////////////////////////////////////////////////////////////////////////
int64_t SharedMemRingBuffer::GetNextSequenceForClaim()
{
    return ring_buffer_status_on_shared_mem_->next.fetch_add(1) + 1;
}

///////////////////////////////////////////////////////////////////////////////
//[input ] want_len : 쓰기를 원하는 데이터 크기
//[output] out_position_to_write : 버퍼에 쓸 위치
int64_t SharedMemRingBuffer::ClaimIndex(size_t want_len, size_t * out_position_to_write )
{
    if( want_len > max_data_size_ ) {
        DEBUG_ELOG("Error: Invalid data length : exceeds MAX : " << max_data_size_); 
        return -1;
    }
    ENUM_DATA_STATUS data_status = DATA_EMPTY;
    int64_t next_seq_for_claim = GetNextSequenceForClaim() ;
    ring_buffer_[next_seq_for_claim]->status =DATA_EMPTY; //init
    //다음 쓰기 위치 - 링버퍼 크기 = 링버퍼 한번 순회 이전위치, 
    //데이터 쓰기 작업이 링버퍼를 완전히 한바퀴 순회한 경우
    //여기서 더 진행하면 데이터를 이전 덮어쓰게 된다.
    //만약 아직 데이터를 read하지 못한 상태
    //(min customer index가 wrap position보다 작거나 같은 경우)라면 쓰기 작업은 대기해야 한다
    int64_t wrap_point = next_seq_for_claim - buffer_size_;

    do {
        if(next_seq_for_claim>0) {
            data_status = ring_buffer_[next_seq_for_claim-1]->status ;
        }

        int64_t gating_seq = GetMinIndexOfConsumers();
        if  ( wrap_point >=  gating_seq ) {
            std::this_thread::yield();
            continue;
        } else if  ( next_seq_for_claim>0 && data_status == DATA_EMPTY ) {
            //because of arbitrary data length, we need to wait until previous data is set 

            std::this_thread::yield();
            DEBUG_LOG("continue , DATA_EMPTY / next_seq_for_claim:" << next_seq_for_claim); 
            continue;
        } else {
            //calculate out_position_to_write
            if(next_seq_for_claim==0) {
                *out_position_to_write = 0;
            } else {
                *out_position_to_write = ring_buffer_[next_seq_for_claim-1]->offset_position ;
                DEBUG_LOG("write pos: "<< *out_position_to_write); 
            }

            if( next_seq_for_claim>0 && 
                ring_buffer_[next_seq_for_claim-1]->offset_position + want_len > raw_mem_buffer_capacity_ ) {
                //last write position + wanted data length > total data buffer size 
                //--> start from 0 position  
                DEBUG_LOG( "prev_reset_pos : "<< ring_buffer_status_on_shared_mem_->prev_reset_pos); 
                if( gating_seq!=0 && 
                    gating_seq < ring_buffer_status_on_shared_mem_->prev_reset_pos ) {
                    //but don't over-write customer data 
                    //'gating_seq == 0' means a consumer read index 0, so it is OK to write at index 0
                    
                    std::this_thread::yield();
                    DEBUG_LOG( "spin... / gating_seq=" << gating_seq
                          << " / next_seq_for_claim= "<< next_seq_for_claim 
                          << " / ring_buffer_status_on_shared_mem_->prev_reset_pos=" 
                          << ring_buffer_status_on_shared_mem_->prev_reset_pos ); 
                    continue;
                }
                ring_buffer_status_on_shared_mem_->prev_reset_pos = next_seq_for_claim ;
                DEBUG_LOG("raw_mem_buffer_capacity_ :"<< raw_mem_buffer_capacity_ 
                          << " / gating_seq: "<< gating_seq << " / want_len:"
                          << want_len <<" / ring_buffer_[gating_seq]->start_position: "
                          << ring_buffer_[gating_seq]->start_position );
                DEBUG_LOG( "reset pos 0 "); 
                *out_position_to_write = 0;
            }
            break;
        }
    } while (true);

    DEBUG_LOG("RETURN next_seq_for_claim:" << next_seq_for_claim ); 
    return next_seq_for_claim;
}

///////////////////////////////////////////////////////////////////////////////
bool SharedMemRingBuffer::Commit(int64_t index)
{
    //cursor 가 index 바로 앞인 경우만 성공한다.
    int64_t expected = index -1 ;

    while (true) {
        //if ( cursor_.compare_exchange_strong(expected , index ))
        if ( ring_buffer_status_on_shared_mem_->cursor == expected ) {
            ring_buffer_status_on_shared_mem_->cursor = index;

            break;
        }
        std::this_thread::yield();
    }
    wait_strategy_->SignalAllWhenBlocking(); //blocking wait strategy only.
    return true;
}

///////////////////////////////////////////////////////////////////////////////
void SharedMemRingBuffer::SignalAll()
{
    wait_strategy_->SignalAllWhenBlocking(); //blocking wait strategy only.
}

///////////////////////////////////////////////////////////////////////////////
int64_t SharedMemRingBuffer::WaitFor(size_t user_id, int64_t index)
{
    int64_t current_cursor = ring_buffer_status_on_shared_mem_->cursor.load() ;

    if( index > current_cursor ) {
        
#if 0
        char szMsg[100];
        snprintf(szMsg, sizeof(szMsg), 
                "[id:%d]    \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d] index [%" PRId64 " - trans : %" PRId64 "] no data, wait for :current_cursor[%" PRId64 "]", 
                user_id, __func__, __LINE__, index, GetTranslatedIndex(index),current_cursor  );
        {AtomicPrint atomicPrint(szMsg);}
#endif
        //wait strategy
        return wait_strategy_->Wait(index);
    } else {
#if 0
        char szMsg[100];
        snprintf(szMsg, sizeof(szMsg), "[id:%d]    \t\t\t\t\t\t\t\t\t\t\t\t [%s-%d] index[%" PRId64 "] returns [%" PRId64 "] ",
                user_id, __func__, __LINE__, index, ring_buffer_status_on_shared_mem_->cursor.load() );
        {AtomicPrint atomicPrint(szMsg);}
#endif
        return current_cursor ;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
bool  SharedMemRingBuffer::CommitRead(size_t user_id, int64_t index)
{
    ring_buffer_status_on_shared_mem_->array_consumer_indexes[user_id] = index ; //update

#if 0
    char szMsg[100];
    snprintf(szMsg, sizeof(szMsg), 
            "[id:%d]     \t\t\t\t\t\t\t\t\t\t\t\t[%s-%d] index[%" PRId64 "] ", user_id, __func__, __LINE__, index );
    {AtomicPrint atomicPrint(szMsg);}
#endif
    return true;
}


