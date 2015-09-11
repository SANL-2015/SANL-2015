
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
		
set tmargin 1.7

set size ratio 0.5
#set lmargin 8
#set bmargin 2.8
#set rmargin 1.2
set xrange[1:22];
set yrange[0.9:5.1];

set xlabel "Number of cores";
set ylabel "Speedup";
set y2label "";

plot "results/acc-posix-set.csv" using 1:2:3 axes x1y1 title "POSIX"\
    with yerrorlines lc rgb "#9BBB59" lt 1 lw 5 pt 12 pointsize 0.5,\
     "results/acc-spinlock-set.csv" using 1:2:3 axes x1y1 title "CAS spinlock"\
   with yerrorlines lc rgb "#1F497D" lt 1 lw 5 pt 1 pointsize 0.5,\
     "results/acc-mcs-set.csv" using 1:2:3 axes x1y1 title "MCS"\
   with yerrorlines lc rgb "#4BACC6" lt 1 lw 5 pt 9 pointsize 0.5,\
     "results/acc-saml-set.csv" using 1:2:3 axes x1y1 title "SAML"\
   with yerrorlines lc rgb "#D58AF8" lt 1 lw 5 pt 6 pointsize 0.5,\
	 "results/acc-rcl-set.csv" using 1:2:3 axes x1y1 title "RCL"\
   with yerrorlines lc rgb "#C0504D" lw 5 lt 1 pt 7 pointsize 0.5

