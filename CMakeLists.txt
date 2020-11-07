cmake_minimum_required (VERSION 2.8) 

project (DisruptorCppIpcArbitraryLen)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

SET(CMAKE_CXX_STANDARD 11) 
#SET(CMAKE_BUILD_TYPE Debug)
SET(CMAKE_BUILD_TYPE Release)

#XXX for terminal debugging
ADD_DEFINITIONS ( -DDEBUG_PRINT )

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_library (DisruptorCppIpcArbitraryLenLib ring_buffer_on_shmem.cpp shared_mem_manager.cpp )
add_definitions(-Wall)

add_subdirectory (tests/inter_thread)
add_subdirectory (tests/inter_process)
