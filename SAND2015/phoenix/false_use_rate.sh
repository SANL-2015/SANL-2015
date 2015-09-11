#!/bin/bash

# You must define MMM and EEE in the liblock before running this script.


. .root


CODEDIR=$BASEDIR/phoenix-rcl/tests
DATASETS=$BASEDIR/datasets

run_histogram() {
	if [ -f $CODEDIR/histogram/histogram-rcl ]; then
	        $CODEDIR/histogram/histogram-rcl $1
	fi
}

run_linear_regression() {
	if [ -f $CODEDIR/linear_regression/linear_regression-rcl ]; then
	        $CODEDIR/linear_regression/linear_regression-rcl $1
	fi
}

run_kmeans() {
	if [ -f $CODEDIR/kmeans/kmeans-rcl ]; then
	        $CODEDIR/kmeans/kmeans-rcl $1 $2
	fi
}

run_pca() {
	if [ -f $CODEDIR/pca/pca-rcl ]; then
	        $CODEDIR/pca/pca-rcl $1 $2 $3 $4
	fi
}

run_matrix_multiply() {
	if [ -f $CODEDIR/matrix_multiply/matrix_multiply-rcl ]; then
	        $CODEDIR/matrix_multiply/matrix_multiply-rcl $1
	fi
}

run_word_count() {
	if [ -f $CODEDIR/word_count/word_count-rcl ]; then
	        $CODEDIR/word_count/word_count-rcl $1
	fi
}

run_string_match() {
	if [ -f $CODEDIR/string_match/string_match-rcl ]; then
	        $CODEDIR/string_match/string_match-rcl $1
	fi
}

export LD_LIBRARY_PATH=$BASEDIR/../liblock
export MR_NUMTHREADS=47
export LIBLOCK_LOCK_NAME=rcl

# Linear_regression
ARG_LIST="key_file_50MB.txt key_file_100MB.txt key_file_500MB.txt"
for ARG in $ARG_LIST; do
    echo "Linear_regression with $ARG"
    run_linear_regression $DATASETS/linear_regression_datafiles/$ARG 2>&- | awk '/manager/,/PAPI_L2_DCM/ {print $0}'
done

# PCA
echo ""
ARG_LIST="500 1000 1500"
for ARG in $ARG_LIST; do
    echo "PCA with $ARG"
    run_pca -r $ARG -c $ARG 2>&- | awk '/manager/,/PAPI_L2_DCM/ {print $0}'
done

# Kmeans
echo ""
ARG_LIST="10000 50000 100000"
for ARG in $ARG_LIST; do
    echo "Kmeans with $ARG"
    run_kmeans -p $ARG 2>&- | awk '/manager/,/PAPI_L2_DCM/ {print $0}'
done

# String_match
echo ""
ARG_LIST="key_file_50MB.txt key_file_100MB.txt key_file_500MB.txt"
for ARG in $ARG_LIST; do
    echo "String_match with $ARG"
    run_string_match $DATASETS/string_match_datafiles/$ARG 2>&- | awk '/manager/,/PAPI_L2_DCM/ {print $0}'
done

# Word_count
echo ""
ARG_LIST="word_10MB.txt word_50MB.txt word_100MB.txt"
for ARG in $ARG_LIST; do
    echo "Word_count with $ARG"
    run_word_count $DATASETS/word_count_datafiles/$ARG 2>&- | awk '/manager/,/PAPI_L2_DCM/ {print $0}'
done

# Histogram
echo ""
ARG_LIST="small.bmp med.bmp large.bmp"
for ARG in $ARG_LIST; do
    echo "Histogram with $ARG"
    run_histogram $DATASETS/histogram_datafiles/$ARG 2>&- | awk '/manager/,/PAPI_L2_DCM/ {print $0}'
done

# Matrix_multiply
echo ""
ARG_LIST="100 500 1000"
for ARG in $ARG_LIST; do
    echo "Matrix_multiply with $ARG"
    # Matric creation
    MR_NUMTHREADS=20 $CODEDIR/matrix_multiply/matrix_multiply-rcl $ARG 1 1 2>&1 >&/dev/null # matrix_side row_block_length create_files
    run_matrix_multiply $ARG 2>&- | awk '/manager/,/PAPI_L2_DCM/ {print $0}'
    rm -f matrix_file_A.txt matrix_file_B.txt
done

