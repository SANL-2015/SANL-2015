#!/bin/bash
# Previous experiment flawed: MCS bound to wrong core (numa_alloc_local BEFORE affinity
# Using -O0 for this one.

. ../figures/bench.inc.sh

SCRIPT_NAME=`basename $0`
EXPERIMENT_NAME=left_${SCRIPT_NAME%.*}
NUMBER_OF_RUNS=25
NUMBER_OF_SAMPLES=50
NUMBER_OF_ITERATIONS=1000
MIN=100
MAX=2000000
BENCHMARK=./benchmark

rm -Rf results/$SCRIPT_NAME* >/dev/null 1>&0
mkdir results

echo "Experiment no." $EXPERIMENT_NAME

function measure_full_latencies {
	step=`awk "BEGIN {printf(\"%.12g\", $MAX ** (1/($NUMBER_OF_SAMPLES)))}";`
	delay=$step
	c=$1;g=$2;l=$3
	shift 3
	for i in `seq 1 $NUMBER_OF_SAMPLES`; do
#	for intdelay in 0 50; do
		delay=`echo "$delay * $step" | bc -q`

		if [ $(echo "$delay > $MIN"| bc) -eq 1 ]; then
				intdelay=`echo "$delay / 1" | bc -q`
				echo -n $intdelay
				echo -n ","
				echo -n "scale=6;(" > results/sum
				echo -n > results/list
				for j in `seq 1 $NUMBER_OF_RUNS`
				do
						sudo $BENCHMARK -e 0x80000002 -1 -n $NUMBER_OF_ITERATIONS -d $intdelay -A 1 -c $c -s 1 -F $type -g $g -l $l $@ | tr -d ',\n' \
								| tee -a results/sum results/list >> /dev/null
						if [ $j -lt $NUMBER_OF_RUNS ]; then
								echo -n '+' >> results/sum
								echo -n ',' >> results/list
						fi
				done
				echo ")/$NUMBER_OF_RUNS" >> results/sum
				cat results/sum | bc | tr -d '\n'
				echo -n ","
				cat results/list
				echo
		fi
	done
}

LOOPINFO=dependent

function apply {
		i=$1
		n=$3
		f=$5
		shift 7
		echo "$n, $i shared variables"
		echo "========================"
		measure_full_latencies 47 $i 0 $@ -o -N -x dependent | tee results/${EXPERIMENT_NAME}_${f}_${i}_0_${LOOPINFO}.csv
}

# Interdependent accesses
for i in 1 5
do
		for b in $RCL; do
				on_bench $b apply $i
		done
done
#echo -e "Subject: amd48 results\n\nLoop ${LOOPINFO} done." | /usr/sbin/ssmtp jp.lozi@gmail.com
