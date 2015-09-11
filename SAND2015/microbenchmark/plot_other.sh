#!/bin/bash

. ../figures/bench.inc.sh

server_file=results/left_latencies_gl_rcl_1_0_dependent.csv
F="use rate"

cat other.txt | grep "$F" | cut -d':' -f2 | \
		LC_ALL="C" awk -F',' '{ n=$1; getline <"'"$server_file"'"; print $1", "n; }' > results/full_other.csv

cat ../figures/model.plot

echo 'set log x;'
echo 'unset y2tics;'
echo 'set ylabel "";'
echo 'set log y;'

echo 'unset key;'
echo 'set key default;'
echo 'set key above;'
echo 'set key right;'

echo 'set xrange[100:2000000];'
#echo 'set yrange[1300:20000];'
#echo 'set y2range[-0.01:0.20];'

echo 'set xlabel "Delay (cycles)";'
echo 'set ylabel "Latency (cycles)";'
echo 'set y2label "'"$F"'";'

echo "set xtics 10"
echo 'set mxtics 5;'
echo "set ytics (1500,2000,3000,5000,10000,15000,20000)"
echo 'set y2tics;'

echo "plot \"$server_file\" axis x1y1 title \"RCL latency\" with linespoints, \\"
#echo "     \"results/left_latencies_gl_mcsl_1_0_dependent.csv\" axis x1y1 title \"MCS latency\" with linespoints, \\"
echo "    \"results/full_other.csv\" axis x1y2 title \"$F\" with linespoints"


