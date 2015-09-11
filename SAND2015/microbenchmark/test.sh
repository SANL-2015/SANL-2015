#!/bin/bash

VERBOSE=1
BENCHMARK=./benchmark
CLIENTS=47

other_args=$@
declare -a benchs
benchs=(
		'[posix] liblock: '           'posix'         '-F posix'
		'[spinlock] liblock: '        'spinlock'      '-F spinlock'
		'[spinlock] bench: '          'spinlockb'     '-L'
		'[mcsm] liblock: '            'mcsmit'        '-F mcsmit'
		'[mcs] liblock: '             'mcs'           '-F mcs'
		'[mcs] bench: '               'mcsb'          '-M'
		'[flat combining] liblock: '  'flat'          '-F flat'
		'[rclb] bench: '              'rclb'          '-m -o'
		'[rcl] liblock: '             'rcl'           '-F rcl'
		'[mwait] liblock: '           'mwait'         '-F mwait'
#		'[adapt] liblock: '           'adapt'         '-F adapt'
)

NITERATIONS=5000

on_benchs() {
		cmd=$1
		shift
		let N=0
		while [ $N -lt ${#benchs[*]} ]; do
				$cmd "${benchs[$N]}" "${benchs[$N+1]}" $@ ${benchs[$N+2]}
				let N=$N+3
		done
}

bench_run() {
		name=$1
		id=$2
		delay=$3
		globals=$4
		locals=$5
		shift; shift; shift; shift; shift;
		echo -n "$name"
		CMD="../mutrace-rcl/mutrace-rcl $BENCHMARK -1 -c $CLIENTS -n $NITERATIONS -d $delay -A 1 -s 0 -m -u -g $globals -l $locals $@"
		CMD="$BENCHMARK -1 -c $CLIENTS -n $NITERATIONS -d $delay -A 1 -s 0 -m -u -g $globals -l $locals $@"
		[ -z "$VERBOSE" ] || echo $CMD
		eval $CMD
}

run() {
		delay=$1
		globals=$2
		locals=$3

		echo "---- delay $delay - $globals globals - $locals locals ----"
		on_benchs bench_run $delay $globals $locals $clients
		echo
}

if [ -z "$1" ]; then delay=100; else delay=$1;   fi
if [ -z "$2" ]; then globals=1; else globals=$2; fi
if [ -z "$3" ]; then locals=0;  else locals=$3;  fi
if [ -z "$4" ]; then 
		run $delay $globals $locals
else
		let N=0
		while [ $N -lt ${#benchs[*]} ]; do
				if [ "${benchs[$N+1]}" = $4 ]; then
						bench_run "${benchs[$N]}" "${benchs[$N+1]}" $delay $globals $locals ${benchs[$N+2]}
				fi
				let N=$N+3
		done
fi
