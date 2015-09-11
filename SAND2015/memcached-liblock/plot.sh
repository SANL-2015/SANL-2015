#!/bin/bash

RESULTS_FOLDER=$1
PREFIX=$2

compute_acceleration() {
	BASE=$1
	N=0
	while read line; do
		let N=N+1
        I=0
		VALS=$(echo $line | tr -s ',' ' ')
		M=$(echo $line | cut -d"," -f2) 
		exp=0
		for v in $VALS; do
            if [[ I -ge 2 ]]; then
			    exp="$exp + ($BASE/$v - $BASE/$M)^2"
            fi
		    I=$I+1
        done
		if [ $lock = "rcl" ]; then
			echo $(echo $N+1|bc),$(echo $BASE/$M | bc -l),$(echo $exp | bc -l)
		else
 			echo $N,$(echo $BASE/$M | bc -l),$(echo $exp | bc -l)
		fi
	done < $RESULTS_FOLDER/$PREFIX\_$lock\_$exp.csv
}

build_data() {
	#for exp in get set; do
	for exp in set; do
		BASE=$(cat $RESULTS_FOLDER/$PREFIX\_posix_$exp.csv | \
                   head -n 1 | cut -d"," -f2)
 		for lock in posix mcs rcl spinlock saml; do
			cat $RESULTS_FOLDER/$PREFIX\_$lock\_$exp.csv  | \
                compute_acceleration $BASE > $RESULTS_FOLDER/acc-$lock-$exp.csv
 		done
	done
}

build_data

for n in "set"; do
#for n in "get" "set"; do
    if [[ $n == "get" ]]; then
        YTICS=2
    else
        YTICS=1
    fi
  	cat model.plot | sed -e "s/results/$RESULTS_FOLDER/" \
                   | sed -e "s/__ytics/$YTICS/"  \
                   | sed -e "s/get/$n/" > $n.plot
  	gnuplot $n.plot > $RESULTS_FOLDER/memcached-$n.pdf
    rm -f $n.plot
done

#                   | sed -e "s/title \"[^\"]*\"/notitle/"  \
#cat model.plot | sed -e "s/__ytics/1/" \
#               | sed -e "s/results/$RESULTS_FOLDER/" \
#               | sed -e "s/get/$n/" \
#               | grep -v margin \
#               | gnuplot > $RESULTS_FOLDER/memcached-labels-precut.pdf

