#!/bin/bash

. .root

# Check if rooted
if [ "$(id -u)" != "0" ]; then
    echo "Must be root"
    exit 1
fi

START_THREADS=$1
END_THREADS=$2
DIR=$3

BASEDIR=/home/zmz/ipads/saml_cloud1/rcl/phoenix

if [ -z "$START_THREADS" ]; then START_THREADS=1; fi
if [ -z "$END_THREADS" ]; then END_THREADS=39; fi
if [ -z "$DIR" ]; then DIR=phoenix-rcl; fi

INTERVAL=`seq $START_THREADS $END_THREADS`
#INTERVAL="2 6 10 14 18 20 24 32 40 47"

CODEDIR=$BASEDIR/$DIR/tests
DATASETS=$BASEDIR/datasets
LIB_PATH='LD_LIBRARY_PATH=/home/zmz/ipads/saml_cloud1/rcl/liblock'

run_histogram() {
	if [ -f $CODEDIR/histogram/histogram$EXT ]; then
	        $CODEDIR/histogram/histogram$EXT $1
	fi
}

run_linear_regression() {
	if [ -f $CODEDIR/linear_regression/linear_regression$EXT ]; then
	        $CODEDIR/linear_regression/linear_regression$EXT $1
	fi
}

run_kmeans() {
	if [ -f $CODEDIR/kmeans/kmeans$EXT ]; then
	        $CODEDIR/kmeans/kmeans$EXT $1 $2
	fi
}

run_pca() {
	if [ -f $CODEDIR/pca/pca$EXT ]; then
	        $CODEDIR/pca/pca$EXT $1 $2 $3 $4
	fi
}

run_matrix_multiply() {
	if [ -f $CODEDIR/matrix_multiply/matrix_multiply$EXT ]; then
	        $CODEDIR/matrix_multiply/matrix_multiply$EXT $1
	fi
}

run_word_count() {
	if [ -f $CODEDIR/word_count/word_count$EXT ]; then
	        $CODEDIR/word_count/word_count$EXT $1
	fi
}

run_string_match() {
	if [ -f $CODEDIR/string_match/string_match$EXT ]; then
	        $CODEDIR/string_match/string_match$EXT $1
	fi
}


eval_exp() {
		RUNSS=$1
		shift;
		FILE=$1
		shift;

		#echo "-- Running: $@ with '$LIBLOCK_LOCK_NAME' lock and $MR_NULTHREADS cores --"
		for F in $(seq 1 $RUNSS); do
				eval $@ > $FILE-run$F.csv
		done
}

moyenne() {
		let TOT=0
		let N=0
		while read line; do
				let TOT=$TOT+$line
				let N=$N+1
		done
		let TOT=$TOT/$N
		echo "$TOT"
}


eval_all_of() {
		NAME=$1
		RUNS=$2
		shift; shift

		# Warmup run
		echo "Warmup $NAME"
 		EXT=-rcl MR_NUMTHREADS=20 eval $@ > /dev/null

		# Sequential run
		echo "Run sequential $@"
		EXT=-seq eval_exp $RUNS results/phoenix-$NAME-sequential "$@"

		# Runs with no liblock infrastructure
 		for y in 1; do ##$INTERVAL; do
		                echo "Run $@ with no liblock infrastructure $y threads."
 				MR_NUMTHREADS=$y eval_exp $RUNS results/phoenix-$NAME-baseline-$y "$@"
 		done

		# # Lock runs
 		# for y in $INTERVAL; do
		#                 echo "Run $@ with lock base-posix and $y threads."
 		# 		EXT=-rcl MR_NUMTHREADS=$y eval_exp $RUNS results/phoenix-$NAME-liblock-posix-$y "$@"
 		# done


		for x in saml posix rcl flat spinlock mcs; do
				for y in $INTERVAL; do
				                echo "Run $@ with lock $x and $y threads."
						EXT=-rcl MR_NUMTHREADS=$y LIBLOCK_LOCK_NAME=$x eval_exp $RUNS results/phoenix-$NAME-$x-$y "$@"
				done
		done
}


eval_all() {
		export LD_LIBRARY_PATH=$BASEDIR/../liblock

		RUNS=5
		echo "RUNS: $RUNS - THREADS: from $START_THREADS to $END_THREADS"
		#\rm -Rf results
		mkdir results

                # Linear_regression
                #ARG_LIST="key_file_100MB.txt"
		ARG_LIST="key_file_50MB.txt key_file_100MB.txt key_file_500MB.txt" # lr_4GB.txt"
		for ARG in $ARG_LIST; do
                    eval_all_of linear_regression-$ARG $RUNS "run_linear_regression $DATASETS/linear_regression_datafiles/$ARG 2>&1 >&-"
                done

		# PCA
                #ARG_LIST="1000"
                ARG_LIST="500 1000 1500"
                for ARG in $ARG_LIST; do
		    eval_all_of pca-$ARG $RUNS "run_pca -r $ARG -c $ARG 2>&1 >&-"
                done

                # Kmeans
                #ARG_LIST="50000"
                ARG_LIST="10000 50000 100000"
                for ARG in $ARG_LIST; do
		    eval_all_of kmeans-$ARG $RUNS "run_kmeans -p $ARG 2>&1 >&-"
                done

		# String_match
                #ARG_LIST="key_file_100MB.txt"
                ARG_LIST="key_file_50MB.txt key_file_100MB.txt key_file_500MB.txt"
		for ARG in $ARG_LIST; do
                    eval_all_of string_match-$ARG $RUNS "run_string_match $DATASETS/string_match_datafiles/$ARG 2>&1 >&-"
                done

                # Word_count
                #ARG_LIST="word_50MB.txt"
                ARG_LIST="word_10MB.txt word_50MB.txt word_100MB.txt"
		for ARG in $ARG_LIST; do
                    eval_all_of word_count-$ARG $RUNS "run_word_count $DATASETS/word_count_datafiles/$ARG 2>&1 >&-"
		done

                # Histogram
                #ARG_LIST="med.bmp"
                ARG_LIST="small.bmp med.bmp large.bmp" # hist-2.6g.bmp"
		for ARG in $ARG_LIST; do
                    eval_all_of histogram-$ARG $RUNS "run_histogram $DATASETS/histogram_datafiles/$ARG 2>&1 >&-"
		done

		# Matrix_multiply
                #ARG_LIST="500"
                ARG_LIST="100 500 1000"
		for ARG in $ARG_LIST; do
		    # Matric creation
		    MR_NUMTHREADS=20 $CODEDIR/matrix_multiply/matrix_multiply-rcl $ARG 1 1 2>&1 >&/dev/null # matrix_side row_block_length create_files
                    eval_all_of matrix_multiply-$ARG $RUNS "run_matrix_multiply $ARG 2>&1 >&-"
		    rm -f matrix_file_A.txt matrix_file_B.txt
                done


		chown -R zmz:zmz results
		date
}


eval_all
