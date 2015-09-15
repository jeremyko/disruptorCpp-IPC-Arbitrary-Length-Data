# disruptorCpp-IPC for arbitrary length of data 
##slightly modified version of [disruptorCpp-IPC](https://github.com/jeremyko/disruptorCpp-IPC)

###inter thread test 

    cd tests/inter_thread 
    make clean all
    ./inter_thread_test 

###inter process test 

    cd tests/inter_process 
    make -f make_procucer.mk clean all
    make -f make_consumer.mk clean all
    ./inter_thread_test

    //whenever you change number of cunsumer, producer or memory size, 
    //clear shared memory first.
    //ipcrm -M 0x000e1740
    //ipcrm -M 0x0001e240
    
    //run 2 consumer, then 1 producer 
    ./consumer 0 
    ./consumer 1
    ./producer
