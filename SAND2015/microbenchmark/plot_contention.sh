#!/bin/bash

. ../figures/bench.inc.sh

FFF=0

plot() {
		nb_lines=$1
		first=$2
		col=$7
		name=$3
		id=$5

		if [ $FFF = 0 ]; then
				FFF=1
		else
				echo ",\\"
				echo -n "     "
		fi

		file="results/left_latencies_"$id"_"$nb_lines"_0_dependent.csv"
		ppp="\"$file\" using 1:2 axes x1y1 with linespoints $col"

 		echo -n $ppp" title \"$name\""
}


echo 'reset;'
# Used in USENIX'12
echo 'set terminal pdf dashed enhanced font "Helvetica, 10" size 5,1.7;'
# Used in the midterm evaluation
#echo 'set terminal pdf dashed enhanced font "Helvetica, 10" size 5,2.35;'
echo 'set datafile separator ",";'

echo 'set log x;'
#echo 'set log y;'

echo 'set key default;'

#echo 'set lmargin 3.5;'
#echo 'set bmargin 5;'
echo 'set tmargin 0.5;'
echo 'set rmargin 0.5;'

echo 'set xlabel "Delay (cycles)";'
#echo 'set ylabel "Execution time (cycles)";'
echo 'set ylabel "% of time in CS";'

echo 'set xrange[101:2000000];'
#echo 'set yrange[1000:500000];'
echo 'set yrange[0:100] noreverse nowriteback;'

echo "set ytics nomirror"
echo "set xtics nomirror"
#echo 'set y2tics (2.3,10,20,30,40,50,58,70,80,90,100)'
#echo 'set y2tics'
echo "unset my2tics;"

echo
#echo "set arrow from 14500,93 to 100,93 nohead lw 2 lt 1;"
#echo "set label \"Collapse of RCL (14500 cycles): 93%\"  at 200,87 font \"Helvetica,8\";"
echo
echo "set arrow from 60000,70 to 100,70 nohead lw 2 lt 1;"
echo "set label \"Collapse of MCS (60000 cycles): 70%\"  at 200,75 font \"Helvetica,8\";"
echo
echo "set arrow from 120000,20 to 100,20 nohead lw 2 lt 1;"
echo "set label \"Collapse of POSIX (120000 cycles): 20%\"  at 200,25 font \"Helvetica,8\";"
echo

echo -n "plot "
#on_bench $RCL  plot 1 $Z
#on_bench mcs   plot 1 $Z
#on_bench posix plot 1 $Z
#echo ",\\"
#echo -n "     "
echo -n "\"results/left_contention_gl_posix_1_0_dependent.csv\" using 1:2 axes x1y2 with linespoint lw 2 lc rgb \"#009400\" lt 4 pt 13 title \"% of time in CS\""

echo

