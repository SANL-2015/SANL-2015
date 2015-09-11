#!/bin/bash

. ../figures/bench.inc.sh

FFF=0

plot_latencies() {
		color=$5
		title=$2
		id=$4

		if [ $FFF = 0 ]; then
				echo -n "plot "
				FFF=1
		else
				echo ",\\"
				echo -n "     "
		fi

 		echo -n "\"results/left_latencies_"$id"_1_0_dependent.csv\" using 1:2 axes x1y1 with linespoints $color title \"$title\""
}

plot_dcm() {
		col=$5
		name=$2
		id=$4

		if [ $FFF = 0 ]; then
				echo -n "plot "
				FFF=1
		else
				echo ",\\"
				echo -n "     "
		fi

    if [ "$id" != "gl_rcl" ]; then
		    file="results/left_dcm_clients_"$id"_1_0_dependent.csv"
    else
				server_file="results/left_dcm_server_"$id"_1_0_dependent.csv"
        cat results/left_dcm_clients_"$id"_1_0_dependent.csv | \
						LC_ALL="C" awk -F, '{ n=$2; getline <"'"$server_file"'"; printf("%d, %f\n", $1, n+$2); }' > \
						results/left_dcm_all_"$id"_1_0_dependent.csv
        file="results/left_dcm_all_"$id"_1_0_dependent.csv"
    fi

		echo -n "\"$file\" using 1:2 axes x1y1 with linespoints $col title \"$name\""
}

echo "reset;"
echo 'set terminal pdf dashed enhanced font "Helvetica, 8" size 4.5,3.2;'
echo 'set datafile separator ",";'

echo 'set multiplot'

echo 'set log xy;'
echo 'unset y2tics;'
echo 'set ytics mirror'
echo 'set xtics mirror'

echo 'set xrange[101:2000000];'
echo 'set yrange[0.9:200];'
echo 'set lmargin 7.5;'

echo 'set orig 0,0.6'
echo 'set size 1,0.4'

echo 'set xtics'
echo 'set format x ""'
echo 'set log x2'
echo 'set key default above center horizontal;'

echo 'set tmargin 1.5;'
echo 'set bmargin 0'
echo 'set y2label "# L2 Cache Misses";'
echo 'set border 14'

for b in $RCL mcs spinlock flat posix; do
		on_bench $b plot_dcm
done
echo

echo 'set border 15'
echo 'set format x "%g"'
echo 'set yrange[40:900000];'
echo 'set tmargin 0'
echo 'set bmargin 3'
echo 'set orig 0,0'
echo 'set size 1.0,0.6'
echo 'unset key;'
echo 'set xlabel "Delay (cycles)"'
echo 'set y2label "Execution time (cycles)";'

FFF=0
for b in $RCL flat mcs posix spinlock; do
		on_bench $b plot_latencies
done
echo
