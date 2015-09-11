#!/bin/bash

BENCH="mcs"

N_CS=100000 # Number of critical sections per thread

SHOW_BENCH_OUTPUT=0 # If set to 1, run the benchmark only once and show its
                    # output. Use -o to set this flag.

while getopts "o" OPTION
do
     case $OPTION in
         o)
            SHOW_BENCH_OUTPUT=1
     esac
done

if [[ $SHOW_BENCH_OUTPUT -eq 1 ]]; then
    sudo ./benchmark -m -1 -n $N_CS -d 1000 -A 1 -c 47 -s 1 -u \
                     -g 1 -l 0 -F $BENCH\_mon -o -N -x dependent
    exit
fi

TOT_CONTROL_CS_CYCLES=0
TOT_CS_CYCLES=0
TOT_MON_CYCLES=0

N_IT=3

for i in `seq 1 $N_IT`; do
    CONTROL_OUTPUT=`sudo ./benchmark -m -1 -n $N_CS -d 1000 -A 1 -c 47 -s 1 -u \
                    -g 1 -l 0 -F $BENCH -o -N -x dependent`
    OUTPUT=`sudo ./benchmark -m -1 -n $N_CS -d 1000 -A 1 -c 47 -s 1 -u -g 1 \
            -l 0 -F $BENCH\_mon -o -N -x dependent`

    CONTROL_CS_CYCLES=`echo "$CONTROL_OUTPUT" | tail -n 1 | tr -d ',' | cut -d '.' -f 1`
    CS_CYCLES=`echo "$OUTPUT" | tail -n 1 | tr -d ',' | cut -d '.' -f 1`

    #SUM=`echo "$OUTPUT" | cut -d' ' -f3 | head -n -1 | paste -sd+ | bc`

    VALID_LINES=`echo "$OUTPUT" | cut -d' ' -f3 | head -n -1 | grep -v "\-1"`
    SUM=`echo "$VALID_LINES" | paste -sd+ | bc`
    NUM_LINES=`echo "$VALID_LINES" | sed '/^\s*$/d' | wc -l`

    if [[ $NUM_LINES -gt 0 ]]; then
        echo "Got results from "$NUM_LINES" threads."
    else
        echo "Got results from "$NUM_LINES" (<1) threads. Exiting."
        exit
    fi

    MON_CYCLES=`echo "scale=3;$SUM/$NUM_LINES/$N_CS" | bc | cut -d '.' -f 1`

    TOT_CONTROL_CS_CYCLES=$((TOT_CONTROL_CS_CYCLES + CONTROL_CS_CYCLES))
    TOT_CS_CYCLES=$((TOT_CS_CYCLES + CS_CYCLES))
    TOT_MON_CYCLES=$((TOT_MON_CYCLES + MON_CYCLES))

    echo "Iteration #$i done."
done

AVG_CONTROL_CS_CYCLES=`echo "scale=1;$TOT_CONTROL_CS_CYCLES/$N_IT" | bc`
AVG_CS_CYCLES=`echo "scale=1;$TOT_CS_CYCLES/$N_IT" | bc`
AVG_MON_CYCLES=`echo "scale=1;$TOT_MON_CYCLES/$N_IT" | bc`

echo "(1) Avg. CS execution time without monitoring (in cycles): "$AVG_CONTROL_CS_CYCLES
echo "(2) Avg. CS execution time (in cycles): "$AVG_CS_CYCLES
echo "If (1) and (2) differ by a lot, the monitoring might be too intrusive."
echo "Avg. result: $AVG_MON_CYCLES"

