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
				echo -n "plot "
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
echo 'set terminal pdf dashed enhanced font "Helvetica, 8";'
echo 'set datafile separator ",";'

echo 'set log x;'
echo 'set log y;'

echo 'set key default above right Right width 0 noreverse horizontal samplen 2;'

#echo 'set lmargin 3.5;'
#echo 'set bmargin 5;'
echo 'set tmargin 1.5;'

echo 'set xlabel "Delay (cycles)";'
echo 'set ylabel "Execution time (cycles)";'
echo 'set y2label "% in CS";'

echo 'set xrange[101:2000000];'
echo 'set yrange[1000:500000];'
echo 'set y2range[0:100] noreverse nowriteback;'

echo "set ytics nomirror"
#echo "set xtics nomirror"
echo 'set y2tics (0,10,20,30,40,50,58,70,80,93,100)'
echo 'set y2tics'
echo "unset my2tics;"

echo
#echo "set arrow from 82000,1400 to 82000, 37000 nohead lw 2 lt 1;"
#echo "set arrow from 82000,37000 to 2000000, 37000 nohead lw 2 lt 1;"
#echo "set arrow from 150000,1150 to 150000, 4000 nohead lw 2 lt 1;"
#echo "set arrow from 150000,1150 to 2000000, 1150 nohead lw 2 lt 1;"

echo "set arrow from 60000,1400 to 60000, 77500 nohead lw 2 lt 1;"
echo "set arrow from 60000,77500 to 2000000, 77500 nohead lw 2 lt 1;"

echo "set arrow from 120000,3450 to 120000, 50000 nohead lw 2 lt 1;"
echo "set arrow from 120000,3450 to 2000000, 3450 nohead lw 2 lt 1;"

echo "set arrow from 14500,2000 to 14500, 325000 nohead lw 2 lt 1;"
echo "set arrow from 14500,325000 to 2000000, 325000 nohead lw 2 lt 1;"

#  lc rgb \"#949494\";"
echo
echo "set label \"93%\" at 300000,370000;"
echo "set label \"70%\" at 300000,90000;"
echo "set label \"20%\" at 300000,4000 ;"
echo

on_bench $RCL  plot 1 $Z
on_bench mcs   plot 1 $Z
on_bench posix plot 1 $Z
echo ",\\"
echo -n "     "
echo -n "\"results/left_contention_gl_posix_1_0_dependent.csv\" using 1:2 axes x1y2 with linespoint lw 2 lc rgb \"#009400\" lt 4 pt 13 title \"        % in CS\""

echo

