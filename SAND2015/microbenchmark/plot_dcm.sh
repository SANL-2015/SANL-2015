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

    if [ "$id" != "gl_rcl" ]; then
		    file="results/left_dcm_clients_"$id"_"$nb_lines"_0_dependent.csv"
    else
				server_file="results/left_dcm_server_"$id"_"$nb_lines"_0_dependent.csv"
        cat results/left_dcm_clients_"$id"_"$nb_lines"_0_dependent.csv | \
						LC_ALL="C" awk -F, '{ n=$2; getline <"'"$server_file"'"; printf("%d, %f\n", $1, n+$2); }' > \
						results/left_dcm_all_"$id"_"$nb_lines"_0_dependent.csv
        file="results/left_dcm_all_"$id"_"$nb_lines"_0_dependent.csv"
    fi

		ppp="\"$file\" using 1:2 axes x1y1 with linespoints $col"

    if [ $nb_lines = 5 ]; then
        ppp=$ppp" lw 3"
    fi
		
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

echo 'unset key;'
echo 'set key default;'
echo 'set key above;'
echo 'set key right;'

echo 'set xrange[101:2000000];'
echo 'set yrange[1:200];'

echo 'set xlabel "Delay (cycles)";'
echo 'set y2label "# L2 Cache Misses";'

echo 'unset ytics;'
echo 'set y2tics;'

#echo 'set size 1.0,0.72;'

for i in 1 5; do
		Z=1
		for b in $benchs; do 
				on_bench $b plot $i $Z
				Z=0
		done
done

echo

