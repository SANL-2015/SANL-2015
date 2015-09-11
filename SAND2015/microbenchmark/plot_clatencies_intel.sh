#!/bin/bash

. ../figures/bench.inc.sh

FFF=0

plot() {
		nb_lines=$1
		first=$2
		if [ $nb_lines = 1 ]; then
				col=$7
		else
				col=$8
		fi
		name=$4
		id=$6

		if [ $FFF = 0 ]; then
				echo -n "plot "
				FFF=1
		else
				echo ",\\"
				echo -n "     "
		fi

		file="results/left_latencies_"$id"_"$nb_lines"_0_dependent.csv"
		ppp="\"$file\" using 1:2 axes x1y1 with linespoints $col"

 		if [ $first = 1 ]; then
				agreement=""
				if [ $nb_lines -gt 1 ]; then
					agreement="es"
				fi
 				echo -n $ppp" title \"$nb_lines access$agreement:                $name\""
		else
 				echo -n $ppp" title \"$name\""
 		fi
}

cat ../figures/model.plot

echo 'set log xy;'
echo 'unset y2tics;'
echo 'set ylabel "";'
echo 'set log y2;'

echo 'set key outside;'
echo 'set key above;'
echo 'set key right;'
echo 'set key font ",4";'
echo 'set key spacing 1;'
echo 'unset key;'

#echo 'set xrange[101:2000000];'
echo 'set xrange[101:10000];'
echo 'set yrange[10000:100000];'
echo 'set y2range[10000:100000];'

echo 'set xlabel "Delay (cycles)";'
echo 'set y2label "Execution time (cycles)";'

echo 'unset ytics;'
echo 'set y2tics;'

echo 'set terminal pdf;'

echo 'set output "lat.pdf";'
#for i in 1 5; do
for i in 5; do
		Z=1
		for b in $benchs; do 
				on_bench $b plot $i $Z
				Z=0
		done
done

echo

echo 'set output;'
