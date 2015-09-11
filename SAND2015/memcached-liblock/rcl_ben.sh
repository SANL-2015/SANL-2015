#!/bin/bash

file_prefix=memcached_1_4_6

NUMBER_OF_RUNS=1
MIN_THREADS=21
MEMCACHED_VERSION=1.4.6
#LIBMEMCACHED_VERSION=0.49
LIBMEMCACHED_VERSION=1.0.2

trap "cleanup; exit" SIGHUP SIGINT SIGTERM

function cleanup {
    rm -f results/sum
    rm -f results/list
    rm -f results/out
}

function benchmark {
    killall -9 -q memcached
   
    cleanup 
    rm -f results/$2.csv

    echo "Starting generating file results/"$2".csv..."
    date

    if [[ "$1" == "rcl" ]]; then
        MAX_THREADS=21
    else
        MAX_THREADS=22
    fi

    for n in `seq $MIN_THREADS $MAX_THREADS`; do
        echo -n "$n," >> results/$2.csv
           
        echo -n "scale=6;(" > results/sum
        echo -n > results/list

        for m in `seq 1 $NUMBER_OF_RUNS`; do
            echo "Lock: "$1", experiment: "$3", cores : "$n", iteration : "$m"."

            LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"../liblock/" \
                 LIBLOCK_LOCK_NAME=$1 \
                 memcached-$MEMCACHED_VERSION-patched/memcached -t $n -u root &

            sleep 0.5
                
            libmemcached-$LIBMEMCACHED_VERSION/clients/memslap \
                --flush --servers=127.0.0.1:11211 --concurrency=460 \
                --test=$3 $4 > results/out #2> /dev/null
            
            echo "Elapsed time : "
            cat results/out
            grep "Took" results/out | awk '{ printf "%s",$2; }' \
                                    | tee -a results/sum results/list
            echo
            
            if [ $m -lt $NUMBER_OF_RUNS ]; then
                echo -n '+' >> results/sum
                echo -n ',' >> results/list
            fi

            #killall -9 -w -v memcached
            killall -SIGINT -w -v memcached
        done

        echo ")/$NUMBER_OF_RUNS" >> results/sum
        cat results/sum | bc | tr -d '\n' >> results/$2.csv
        echo -n "," >> results/$2.csv
        cat results/list >> results/$2.csv
        echo >> results/$2.csv
    done

    echo "File results/"$2".csv contents:"
    cat results/$2.csv

    cleanup

    echo "Done."
    date
}

benchmark rcl ${file_prefix}_rcl_get get
#benchmark saml ${file_prefix}_saml_get get
#benchmark mcs ${file_prefix}_mcs_get get
#benchmark spinlock ${file_prefix}_spinlock_get get
#benchmark posix ${file_prefix}_posix_get get

benchmark rcl ${file_prefix}_rcl_set set
#benchmark saml ${file_prefix}_saml_set set
#benchmark mcs ${file_prefix}_mcs_set set
#benchmark spinlock ${file_prefix}_spinlock_set set
#benchmark posix ${file_prefix}_posix_set set

#benchmark rcl ${file_prefix}_rcl_get mget --binary
#benchmark mcs ${file_prefix}_mcs_get mget --binary
#benchmark spinlock ${file_prefix}_rcl_get mget --binary
#benchmark posix ${file_prefix}_mcs_get mget --binary

