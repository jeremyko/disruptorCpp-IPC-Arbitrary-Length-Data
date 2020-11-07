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

#ifndef DISRUPTORCPP_RING_BUFFER_ON_SHM_HPP
#define DISRUPTORCPP_RING_BUFFER_ON_SHM_HPP

#include <iostream>
#include <atomic>
#include <vector>
#include <thread>
#include <inttypes.h>
#include "common_def.hpp"
#include "ring_buffer.hpp"
#include "shared_mem_manager.hpp" 
#include "wait_strategy.hpp"

///////////////////////////////////////////////////////////////////////////////
typedef enum _ENUM_DATA_STATUS_
{
    DATA_EMPTY,
    DATA_EXISTS
} ENUM_DATA_STATUS ;

typedef struct _PositionInfo_
{
    ENUM_DATA_STATUS  status;
    size_t  start_position; 
    size_t  offset_position;
    size_t  len;
} PositionInfo ;

///////////////////////////////////////////////////////////////////////////////
class SharedMemRingBuffer
{
    public:
        SharedMemRingBuffer();
        SharedMemRingBuffer(ENUM_WAIT_STRATEGY wait_strategy);
        ~SharedMemRingBuffer();

        bool     Init(key_t key_index_buffer, size_t size_index_buffer , 
                      key_t key_data_buffer , size_t size_data_buffer );

        void     ResetRingBufferState();
        bool     Terminate();
        bool     SetData( int64_t index, PositionInfo* pos_info, size_t write_position, char* raw_data );
        const char*  GetData(int64_t index, size_t* out_len);

        bool     RegisterConsumer (int id, int64_t* index_for_customer);
        int64_t  GetTranslatedIndex( int64_t sequence);
        void     SignalAll(); 
 
        //producer
        int64_t  ClaimIndex(size_t want_len, size_t* out_position_to_write);
        bool     Commit(int64_t index);
        
        //consumer
        int64_t  WaitFor(size_t user_id, int64_t index);
        bool     CommitRead(size_t user_id, int64_t index);

    private:
        size_t   raw_mem_buffer_capacity_ ; 
        char*    raw_mem_buffer_ ; 
        size_t   max_data_size_ ;
        size_t   buffer_size_;
        size_t   total_mem_size_ ;
        ENUM_WAIT_STRATEGY          wait_strategy_type_;
        WaitStrategyInterface*      wait_strategy_ ;
        RingBuffer< PositionInfo* > ring_buffer_ ; 
        SharedMemoryManager         shared_raw_memory_buffer_ ;
        SharedMemoryManager         shared_index_buffer_ ;
        StatusOnSharedMem*          ring_buffer_status_on_shared_mem_; 

        int64_t  GetMinIndexOfConsumers();
        int64_t  GetNextSequenceForClaim();
        bool     InitRawMemBuffer(key_t key, size_t size);
        bool     InitIndexBuffer (key_t key, size_t size);
    
        //no copy allowed
        SharedMemRingBuffer(SharedMemRingBuffer&) = delete;   
        void operator=(SharedMemRingBuffer) = delete;
};

#endif

