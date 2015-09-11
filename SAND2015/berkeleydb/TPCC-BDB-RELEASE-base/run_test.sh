#!/bin/bash
#
# run_test.sh
# ===========
# (C) Jean-Pierre Lozi, 2011
#
# Example use:
# ./run_test.sh none 0 300 46 rcl 2 
#

# Only use this if you have a valid database in the home-base/ directory (it 
# makes the benchmark faster, just use tpcc_load to generate a database in
# home-base/.
# USE_HOME_BASE=1

source "../../inc/color-echo.sh"

N_CORES=`grep -c ^processor /proc/cpuinfo`
TARGET=amd48 # Default target
if [[ $N_CORES -eq 4 ]]; then
	TARGET=quad
fi

if [[ "$TARGET" == "quad" ]]; then
	CORES=( 0 1 2 3 )
	N_THREADS=2
else
	CORES=( 0  4  8 12 16 20 24 28 32 36
			3  7 11 15 19 23 27 31 35 39
			2  6 10 14 18 22 26 30 34 38
			1  5  9 13 17 21 25 29 33 37)
	N_THREADS=38
fi

PROFILER="none"
if [[ $# -ge 1 ]]; then
    PROFILER=$1
fi

REQUEST_TYPE=0
if [[ $# -ge 2 ]]; then
	REQUEST_TYPE=$2
fi

N_ITERATIONS=300
if [[ $# -ge 3 ]]; then
	N_ITERATIONS=$3
fi

if [[ $# -ge 4 ]]; then
	N_THREADS=$4
fi

LOCK_NAME="posix"
if [[ $# -ge 5 ]]; then
    LOCK_NAME=$5
fi

N_SERVERS=1
if [[ $# -ge 6 ]]; then
    N_SERVERS=$6
fi

generate_input() {
	for i in `seq 1 $N_THREADS`; do
		echo $1
        if [[ $1 == "rcl" ]]; then
		    echo ${CORES[$((i % (40 - N_SERVERS) + N_SERVERS))]}
        else
            echo ${CORES[$((i % 40))]}
		fi
        echo $N_ITERATIONS
		echo 0
	done
	echo -1
}

clock() {
	STEP=5
	NOW=0

	cecho "[start]" $red

	while true; do
		sleep $STEP
		NOW=$(($NOW+5))
		cecho "[${NOW}s]" $red
	done
}

generate_input $REQUEST_TYPE > input

if [[ ! -d ../home ]]; then

    mkdir ../home
fi
    if [[ $USE_HOME_BASE -eq 1 ]]; then
        sudo cp ../home-base/* ../home/
    else
        rm -f ../home/*

        sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/../../liblock/ \
             LIBLOCK_LOCK_NAME="posix" \
                ./tpcc_load -h /home/zmz/ipads/saml_cloud1/rcl/berkeleydb/home/ -w 1
    fi
#fi

case $PROFILER in

    "none")

    sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/../../liblock/ \
         LIBLOCK_LOCK_NAME=$LOCK_NAME NUM_SERVERS=$N_SERVERS \
             ./tpcc_xact_server -b -h ../home -i < input

    ;;

    "none-gdb")

    sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/../../liblock/ \
         LIBLOCK_LOCK_NAME=$LOCK_NAME NUM_SERVERS=$N_SERVERS \
             gdb ./tpcc_xact_server
             # Once in gdb:
             # run -b -h /home/jp/svn-margaux/margaux/rcl/berkeleydb/home -i < /home/jp/svn-margaux/margaux/rcl/berkeleydb/TPCC-BDB-RELEASE-patched/input
    ;;

    "mutrace-rcl")

    sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/../../liblock/ \
         LIBLOCK_LOCK_NAME=$LOCK_NAME NUM_SERVERS=$N_SERVERS \
             mutrace-rcl -d ./tpcc_xact_server -b  -h ../home -i < input

    ;;

    "lock-profiler")

    # Quick hack
    sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/../../liblock/ \
         LIBLOCK_LOCK_NAME=$LOCK_NAME NUM_SERVERS=$N_SERVERS \
         ../../lock-profiler/lock-profiler ./tpcc_xact_server \
              -b  -h ../home -i < input

    ;;

    *)

    cecho "First argument is invalid." $red

    ;;

esac

#rm -f ../home

