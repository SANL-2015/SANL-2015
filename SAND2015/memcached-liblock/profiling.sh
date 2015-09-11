#!/bin/bash

file_prefix=memcached_1_4_6

function benchmark {
    sudo killall -9 memcached

#   for m in 1 4 8 16 22; do
    for m in 22; do
        echo "Experiment "$1" starting with "$m" cores."

#        sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"../liblock/" \
#             LIBLOCK_LOCK_NAME="rcl" \
#            memcached-1.4.6-patched/memcached \
#                                                -t $m -u root&

        sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"../liblock/" \
             LIBLOCK_LOCK_NAME="rcl" \
             ../lock-profiler/lock-profiler -a \
            memcached-1.4.6-patched/memcached \
                                                -t $m -u root&

#        sudo ../lock-profiler/lock-profiler -f memcached-1.4.6/memcached \
#                                                -t $m -u root&
#        sudo ../mutrace-rcl/mutrace-rcl memcached-1.4.6/memcached \
#                                                -t $m -u root&

        sleep 0.1
        libmemcached-1.0.2/clients/memslap \
            --flush --servers=127.0.0.1:11211 --concurrency=460 \
            --test=$1 > results/out
       
        sudo killall -SIGINT -w -v memcached
        sudo killall -9 -v memcached
        sleep 5
        echo
        echo
    done
}

#benchmark get
#benchmark set

benchmark $1

