#!/bin/bash

RESULTS_FOLDER=$1
PREFIX=$2

compute_average() {
	N=0
	while read line; do
        I=0
		VALS=$(echo $line | tr -s ',' ' ')
		M="scale=6;(0"
	
		for v in $VALS; do
           if [[ $I -ge 1 ]]; then
				M="$M + $v"
			else
				N=$v
           fi
		    I=$I+1
        done
		M="$M)/"$((I-1))
		
	    echo $N,`echo $M | bc -l`,$M
	done < $RESULTS_FOLDER/$lock.csv
}

build_data() {
 	#for lock in orig posix mcs mcstp rcl rcl-yield spinlock flat; do
 	for lock in orig posix mcs rcl spinlock flat saml; do
		cat $RESULTS_FOLDER/$lock.csv  | \
        compute_average > $RESULTS_FOLDER/avg-$lock.csv
	done
}

build_data

cat model.plot | sed -e "s/results/$RESULTS_FOLDER/" > tmp.plot
gnuplot tmp.plot > $RESULTS_FOLDER/berkeleydb.pdf

cat model.plot | sed -e "s/results/$RESULTS_FOLDER/" \
			   | sed -e "s/pdf dashed/png/" \
			   | sed -e "s/Helvetica/Monaco/" > tmp.plot
gnuplot tmp.plot > $RESULTS_FOLDER/berkeleydb.png

rm -f tmp.plot

#cat model.plot | sed -e "s/__ytics/1/" \
#               | sed -e "s/results/$RESULTS_FOLDER/" \
#               | grep -v margin \
#               | gnuplot > $RESULTS_FOLDER/berkeleydb-labels-precut.pdf

