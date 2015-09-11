
reset;
set ytics __ytics;
set xtics auto;
#set terminal aqua dashed enhanced font "Helvetica,18";
set terminal pdf dashed enhanced font "Helvetica,8";
set key default;
set key above;
set key outside;
#set key center;
set datafile separator ",";
		
set tmargin 3.7

set size ratio 0.5
#set lmargin 8
#set bmargin 2.8
#set rmargin 1.2
set xrange[1:22];
#set yrange[1:20];

set xlabel "Number of cores";
set ylabel "Speedup";
set y2label "";
set output "lat.pdf";

plot "results/acc-posix-get.csv" using 1:2:3 axes x1y1 title "POSIX"\
    with yerrorlines lc rgb "#4a004a" lt 4 pt 12 pointsize 1.4,\
     "results/acc-spinlock-get.csv" using 1:2:3 axes x1y1 title "SL"\
   with yerrorlines lc rgb "#000094" lt 3 pt 1 pointsize 1.5,\
     "results/acc-mcs-get.csv" using 1:2:3 axes x1y1 title "MCS"\
   with yerrorlines lc rgb "#004a4a" lt 6 pt 9 pointsize 1.4,\
     "results/acc-saml-get.csv" using 1:2:3 axes x1y1 title "SAML"\
   with yerrorlines lc rgb "#008b4a" lt 5 pt 8 pointsize 1.2,\
	 "results/acc-rcl-get.csv" using 1:2:3 axes x1y1 title "RCL"\
   with yerrorlines lc rgb "#4a0000" lw 2 lt 1 pt 7 pointsize 1.3

set output;
