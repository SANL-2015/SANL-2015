
reset;
set ytics __ytics;
set xtics ("1" 1, "5" 5, "10" 10, "15" 15, "20" 20, "22" 22);
#set terminal aqua dashed enhanced font "Helvetica,18";
set terminal pdf dashed enhanced font "Helvetica,10";
set key default;
set key above;
set key right;
#set key inside;
#set key center;
set datafile separator ",";
		
set tmargin 0.7

set size ratio 0.5
#set lmargin 8
#set bmargin 2.8
#set rmargin 1.2
set xrange[1:22];
set yrange[0.9:5];

set xlabel "Number of cores";
set ylabel "Speedup";
set y2label "";

plot "results/acc-posix-get.csv" using 1:2:3 axes x1y1 title "Memcached/GET"\
    with yerrorlines lc rgb "#94004a" lt 1 lw 5 pt 1 pointsize 2,\
     "results/acc-posix-set.csv" using 1:2:3 axes x1y1 title "     Memcached/SET"\
    with yerrorlines lc rgb "#000094" lt 1 lw 5 pt 2 pointsize 2 #,\
#     "results/acc-spinlock-get.csv" using 1:2:3 axes x1y1 title "SL"\
#   with yerrorlines lc rgb "#000094" lt 3 pt 1 pointsize 1.5,\
#     "results/acc-mcs-get.csv" using 1:2:3 axes x1y1 title "MCS"\
#   with yerrorlines lc rgb "#004a4a" lt 6 pt 9 pointsize 1.4,\
#	 "results/acc-rcl-get.csv" using 1:2:3 axes x1y1 title "RCL"\
#   with yerrorlines lc rgb "#4a0000" lw 2 lt 1 pt 7 pointsize 1.3

