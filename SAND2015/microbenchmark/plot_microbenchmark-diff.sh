#!/bin/bash

. ../figures/bench.inc.sh

plot_latencies() {
		color=$5
		title=$2
		id=$4

		bn=results/left_latencies_$id
		f1="$bn"_1_0_dependent.csv
		f2="$bn"_5_0_dependent.csv
		f3="$bn"_diff.csv
		
		cat $f1 | gawk -F',' -v f2=$f2 '{ y=$2; getline < f2; v=$2-y; if(v<0) {v=1;} printf("%10s, %f\n", $1, v); }' > $f3

		if [ $FFF = 0 ]; then
				echo -n "plot "
				FFF=1
		else
				echo ",\\"
				echo -n "     "
		fi

		echo -n "\"$f3\" using 1:2 axes x1y1 with linespoints $color title \"$title\""
}

plot_dcm() {
		col=$5
		name=$2
		id=$4

		c="results/left_dcm_clients_"$id
		c1="$c"_1_0_dependent.csv
		c5="$c"_5_0_dependent.csv
		r="$c"_diff.csv

		if [ $FFF = 0 ]; then
				echo -n "plot "
				FFF=1
		else
				echo ",\\"
				echo -n "     "
		fi

    if [ "$id" != "gl_rcl" ]; then
				cat $c1 | gawk -F',' '{ y=$2; getline < "'"$c5"'"; v=$2-y; printf("%10s, %f\n", $1, v); }' > $r
    else
				s="results/left_dcm_server_"$id
				s1="$s"_1_0_dependent.csv
				s5="$s"_5_0_dependent.csv

				cat $c5 | gawk -F',' '{ c5=$2; getline < "'"$s5"'"; s5=$2; getline < "'"$c1"'"; c1=$2; getline < "'"$s1"'"; s1=$2; printf("%10s, %f\n", $1, c5+s5-c1-s1); }' > $r
    fi

		echo -n "\"$r\" using 1:2 axes x1y1 with linespoints $col title \"$name\""
}

echo "reset;"
echo 'set terminal pdf dashed enhanced font "Helvetica, 8" size 4.5,3.2;'
echo 'set datafile separator ",";'

echo 'set multiplot'

echo 'set log xy;'
echo 'unset y2tics;'
echo 'set ytics mirror (10,20,50)'
echo 'set xtics mirror'
echo 'set border 14'

echo 'set xrange[101:2000000];'
echo 'set yrange[5:66];'
echo 'set lmargin 7.5;'

echo 'set orig 0,0.8'
echo 'set size 1,0.2'

echo 'set format x ""'
echo 'set log x2'
echo 'set key default above center horizontal;'

echo 'set tmargin 1.5;'
echo 'set bmargin 0'
echo 'set y2label " "'
echo 'unset xtics'

FFF=0
for b in $RCL mcs spinlock flat posix; do
		on_bench $b plot_dcm
done
echo

echo 'set border 10'
echo 'set yrange[-2:5];'
echo 'set ytics autofreq 2'
#echo 'set mytics (-1,1)'
echo "unset log y"
echo 'set xtics nomirror'
echo 'unset x2tics'
echo 'unset key;'
echo 'set y2label "             Diff # L2 Cache Misses";'
echo 'set tmargin 0'
echo 'set orig 0,0.6'
echo 'set size 1,0.2'

FFF=0
for b in $RCL mcs spinlock flat posix; do
		on_bench $b plot_dcm
done
echo

echo 'set border 15'
echo "set log y"
echo 'set xtics mirror'
echo 'set format x "%g"'
echo 'set yrange[40:900000];'
echo 'set bmargin 3'
echo 'set orig 0,0'
echo 'set size 1.0,0.6'
echo 'set ytics autofreq'
echo 'set xlabel "Delay (cycles)"'
echo 'set y2label "Diff time (cycles)";'

FFF=0
for b in $RCL flat mcs posix spinlock; do
		on_bench $b plot_latencies
done
echo
