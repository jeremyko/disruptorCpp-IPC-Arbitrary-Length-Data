# disruptorCpp-IPC for arbitrary length of data 
## slightly modified version of [disruptorCpp-IPC](https://github.com/jeremyko/disruptorCpp-IPC)

### compile

    mkdir build; cd build; cmake ..; make

### inter thread test 

    cd build/tests/inter_thread 
    ./inter_thread_test 

### inter process test 

    cd build/tests/inter_process 

    # whenever you change number of cunsumer, producer or memory size, 
    # clear shared memory first using ipcrm.
    # ex) ipcrm -M 0x000e1740
    #     ipcrm -M 0x0001e240
    
    # run 2 consumer, then 1 producer 
    ./consumer 0 
    ./consumer 1
    ./producer
    # make sure reset shared memory running 'ipcrm -M your_shmkey' 
    # if you have changed buffer size or number of producers/consumers.
