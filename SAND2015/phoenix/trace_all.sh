
TOKEN=$1
START_THREADS=$2
END_THREADS=$3

if [ -z "$TOKEN" ]; then TOKEN="library: "; fi # and also "map phase: " and "reduce phase: "
if [ -z "$START_THREADS" ]; then START_THREADS=1; fi
if [ -z "$END_THREADS" ]; then END_THREADS=39; fi
TOKEN_FILE_SUFFIX=`echo $TOKEN | sed -e 's/:/ /' | sed -e 's/ //g'`

compute_var() {
		BASE=$1
		M=$2
		S=0
		N=0
		while read line; do
				let N=N+1
				S=$(echo "$S + ($BASE/$line - $M)^2" | bc -l)
		done
		echo "sqrt($S/$N)" | bc -l
}

moyenne() {
                let TOT=0
                while read line; do
		                let TOT=$TOT+$line
		done
                let TOT=$TOT/$1
		echo "$TOT"
}

compute_moyenne_run() {
    NB_RUNS=`ls $1-run*.csv | wc -l`
    for file in `ls $1-run*.csv`; do
	cat $file | grep "$TOKEN" | grep -v "inter library" | sed -e "s/$TOKEN//" | sed -e "s/ //g"
    done | moyenne $NB_RUNS
}

compute_acceleration() {
		BASE=$1
		BENCH=$2
		LOCK=$3
		while read line; do
				N=`echo $line | cut -d',' -f1`
				V=`echo $line | cut -d',' -f2`
				A=`echo $BASE/$V | bc -l`
                                E=`compute_moyenne_run results/phoenix-$BENCH-$LOCK-$N | compute_var $BASE $A`
				if [ $LOCK = "rcl" ]; then
						echo $(echo $N+1 | bc), $A, $E
				else
						echo $N, $A, $E
				fi
		done
}

build_data() {
    ARG_LIST=$1
    PROG=$2
    LOCKS="posix rcl mcs spinlock flat saml"

    INTERVAL=`seq $START_THREADS $END_THREADS`
    #INTERVAL="2 6 10 14 18 20 24 32 40 47"


    for arg in $ARG_LIST; do
	for n in ${PROG}-${arg}; do
	    echo $n

	    for lock in $LOCKS; do
		for y in $INTERVAL; do
 		    echo -n "$y, "
 		    compute_moyenne_run results/phoenix-$n-$lock-$y
 		done > results/phoenix-$n-$lock.csv
	    done

#            BASE=$(compute_moyenne_run results/phoenix-$n-sequential)
            BASE=$(compute_moyenne_run results/phoenix-$n-baseline-1)
 	    echo "BASE=$BASE"
  	    for t in $LOCKS; do
#		compute_moyenne $n $t > results/phoenix-$n-$t.csv
  		compute_acceleration $BASE $n $t < results/phoenix-$n-$t.csv > results/acceleration-$n-$TOKEN_FILE_SUFFIX-$t.csv
  	    done
	done
    done
}

build_plot() {
    ARG_LIST=$1
    PROG=$2
    for arg in $ARG_LIST; do
	for n in ${PROG}-${arg}; do
  	    #cat model.plot | sed -e "s/title \"[^\"]*\"/notitle/" | sed -e "s/radiosity/$n-$TOKEN_FILE_SUFFIX/" > $n.plot
	    cat model.plot | sed -e "s/radiosity/$n-$TOKEN_FILE_SUFFIX/" > $n.plot
  	    gnuplot $n.plot > $n-$TOKEN_FILE_SUFFIX.pdf
   	    rm -f $n.plot
	done
    done
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

#cat model.plot | sed -e "s/radiosity/histogram-small.bmp-$TOKEN_FILE_SUFFIX/" | grep -v margin | gnuplot > phoenix-labels.pdf
#cat model.plot | sed -e "s/title \"[^\"]*\"/notitle/" | sed -e "s/radiosity/histogram-small.bmp-$TOKEN_FILE_SUFFIX/" | grep -v margin | gnuplot > phoenix-labels.pdf
