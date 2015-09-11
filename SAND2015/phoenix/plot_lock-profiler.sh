START_THREADS=$2
END_THREADS=$3

if [ -z "$START_THREADS" ]; then START_THREADS=1; fi
if [ -z "$END_THREADS" ]; then END_THREADS=47; fi

build_data() {
    ARG_LIST=$1
    PROG=$2
    INTERVAL=`seq $START_THREADS $END_THREADS`

    for arg in $ARG_LIST; do
	echo "$PROG-$arg"

	for y in $INTERVAL; do
 	    echo -n "$y, "
 	    cat results_lock-profiler/phoenix-mutrace-${PROG}-${arg}-$y.log | awk '/^Global statistics:/ {print $3}' | sed 's/%/ /'
  	done > results_lock-profiler/contention-${PROG}-${arg}.csv
    done
}

build_plot() {
    ARG_LIST=$1
    PROG=$2

    #cat model.plot | sed -e "s/title \"[^\"]*\"/$PROG/" | sed -e "s/radiosity/$n-$TOKEN_FILE_SUFFIX/" > $n.plot
    prog1=`echo $PROG | sed -e "s/_/ /"`
    cat model_mutrace.plot | sed -e 's/results/results_lock-profiler/' | sed -e "s/set title \"[^\"]*\"/set title \"$prog1\"/" > $PROG.plot

    i=1
    for arg in $ARG_LIST; do
	arg1=`echo $arg | sed -e "s/_/ /g"`
	cat $PROG.plot | sed -e "s/contention-arg$i/contention-${PROG}-${arg}/" | sed -e "s/arg$i/$arg1/" > tmp.plot
	mv tmp.plot $PROG.plot
	i=`expr $i + 1`
    done
    
    gnuplot $PROG.plot > contention-$PROG.pdf
    rm -f $PROG.plot
}

ARG_LIST="key_file_50MB.txt key_file_100MB.txt key_file_500MB.txt" # lr_4GB.txt"
build_data "$ARG_LIST" linear_regression
build_plot "$ARG_LIST" linear_regression

ARG_LIST="500 1000 1500"
build_data "$ARG_LIST" pca
build_plot "$ARG_LIST" pca

ARG_LIST="10000 50000 100000"
build_data "$ARG_LIST" kmeans
build_plot "$ARG_LIST" kmeans

ARG_LIST="key_file_50MB.txt key_file_100MB.txt key_file_500MB.txt"
build_data "$ARG_LIST" string_match
build_plot "$ARG_LIST" string_match

ARG_LIST="word_10MB.txt word_50MB.txt word_100MB.txt"
build_data "$ARG_LIST" word_count
build_plot "$ARG_LIST" word_count

ARG_LIST="small.bmp med.bmp large.bmp" # hist-2.6g.bmp"
build_data "$ARG_LIST" histogram
build_plot "$ARG_LIST" histogram

ARG_LIST="100 500 1000"
build_data "$ARG_LIST" matrix_multiply
build_plot "$ARG_LIST" matrix_multiply


# for f in `ls results`; do
#     echo $f
#     cat results/$f | awk '/^Global statistics:/ {print $0}'
#     cat results_mutrace/$f | awk '$2 == "avg",$1 == "Lock" && $3 == "Unlock" && $5 == "CS:" {print $0}'
#     echo ""
# done

# for f in `ls results_lock-profiler`; do
#     cat results_lock-profiler/$f | awk '/^Global statistics:/ {print $3}' | sed 's/%/ /'
# done

