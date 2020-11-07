add_executable (consumer consumer.cpp)
add_executable (producer producer.cpp)
 
target_link_libraries (consumer LINK_PUBLIC DisruptorCppIpcArbitraryLenLib)
target_link_libraries (producer LINK_PUBLIC DisruptorCppIpcArbitraryLenLib)



