#!/bin/bash
# Previous experiment flawed: MCS bound to wrong core (numa_alloc_local BEFORE affinity
# Using -O0 for this one.

. ../figures/bench.inc.sh
 
SCRIPT_NAME=`basename $0`
EXPERIMENT_NAME=${SCRIPT_NAME%.*}
NUMBER_OF_RUNS=1
NUMBER_OF_SAMPLES=50
NUMBER_OF_ITERATIONS=5000
MIN=100
MAX=2000000
BENCHMARK=./benchmark

export LC_ALL=""

rm -Rf results/$SCRIPT_NAME* >/dev/null 1>&0
mkdir -p results

echo "Experiment name: " $EXPERIMENT_NAME

function measure_full_latencies {
	step=`awk "BEGIN {printf(\"%.12g\", $MAX ** (1/($NUMBER_OF_SAMPLES)))}";`
	delay=$step
	c=$1;g=$2;l=$3
	shift 3
	for i in `seq 1 $NUMBER_OF_SAMPLES`; do
		delay=`echo "$delay * $step" | bc -q`
		if [ $(echo "$delay > $MIN"| bc) -eq 1 ]; then
			intdelay=`echo "$delay / 1" | bc -q`
			echo -n $intdelay
			echo -n ","
			echo -n "scale=6;(" | tee results/sum results/tlksum \
                                      results/lksum results/cssum \
                                      results/ulksum results/osum \
                                      > /dev/null
			echo -n > results/list
			for j in `seq 1 $NUMBER_OF_RUNS`
			do
				sudo LIBLOCK_LOCK_NAME="posix" \
                     ../mutrace-rcl/mutrace-rcl --n-locks-to-log 0 \
                        $BENCHMARK -m -1 -n $NUMBER_OF_ITERATIONS -d $intdelay \
                            -A 1 -c $c -s 1 -u -g $g -l $l $@ \
                            2> results/mutrace \
                            | tr -d ',\n' \
                            | tee -a results/sum results/list > /dev/null

				cat results/mutrace | grep avg | tr -d '%' | tr -s " " " " \
                                    | cut -d" " -f3,4,5,6,7 > results/stats
                cat results/stats | cut -d" " -f1 | tr -d '\n' >> results/tlksum
                cat results/stats | cut -d" " -f2 | tr -d '\n' >> results/lksum
                cat results/stats | cut -d" " -f3 | tr -d '\n' >> results/cssum
                cat results/stats | cut -d" " -f4 | tr -d '\n' >> results/ulksum
                cat results/stats | cut -d" " -f5 | tr -d '\n' \
                                  | sed "s/[^0-9.]//g;" >> results/osum

                rm results/mutrace results/stats

                if [ $j -lt $NUMBER_OF_RUNS ]; then
					echo -n '+' | tee -a results/sum results/tlksum \
                                      results/lksum results/cssum \
                                      results/ulksum results/osum \
                                      > /dev/null
					echo -n ',' >> results/list
				fi
			done
			echo ")/$NUMBER_OF_RUNS" | tee -a results/sum results/tlksum \
                                              results/lksum results/cssum \
                                              results/ulksum results/osum \
                                              > /dev/null

#echo ">"
#cat results/osum
#cat results/osum | bc
#echo "<"
			cat results/sum | bc | tr -d '\n'
			echo -n ","
			cat results/tlksum | bc | tr -d '\n'
			echo -n ","
			cat results/lksum | bc | tr -d '\n'
			echo -n ","
    		cat results/cssum | bc | tr -d '\n'
			echo -n ","
			cat results/ulksum | bc | tr -d '\n'
			echo -n ","
			cat results/osum | bc  | tr -d '\n'
			echo
		fi
	done

    rm results/*sum
    rm results/list
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

benchs="posix"

# Interdependent accesses
for i in 1 5
do
    for b in $benchs; do
		on_bench $b apply $i
	done
done
#echo -e "Subject: amd48 results\n\nLoop ${LOOPINFO} done." | /usr/sbin/ssmtp jp.lozi@gmail.com
