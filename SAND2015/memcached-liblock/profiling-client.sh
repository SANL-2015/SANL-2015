#!/bin/bash

file_prefix=memcached_1_4_6

function benchmark {
    sudo killall -q memcached

    for m in 22; do
        echo "Experiment "$1" starting with "$m" cores."

        sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"../liblock/"  \
             LIBLOCK_LOCK_NAME="rcl" \
             memcached-1.4.6-patched/memcached -t $m -u root&

        sleep 1
               
        libmemcached-1.0.2/clients/memslap-profiling \
            --flush --servers=127.0.0.1:11211 --concurrency=460 \
            --test=$1
       
        sudo killall -9 -w -v memcached
        echo
        echo
    done
}

benchmark get
benchmark set

