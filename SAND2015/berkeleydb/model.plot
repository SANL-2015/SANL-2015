
reset;
set xtics ("0" 0, "5" 5, "10" 10, "15" 15, "20" 20, "25" 25, "30" 30, "35" 35, "40" 40)
#		   "120" 120, "140" 140, "160" 160, "180" 180, "200" 200)
set log y
set terminal pdf dashed enhanced font "Helvetica,8";
set key default;
set key above;
#set key inside;
#unset key
set key top;
set key center;
set datafile separator ",";
		
#set size ratio 1
set size ratio 0.5
#set lmargin 8
#set bmargin 2.8
#set rmargin 1.2
#set tmargin 0.7

set yrange[0.7197:1000];
#set xrange[1:200];
set xrange[1:40];

set xlabel "Number of clients (1 client = 1 thread)";
set ylabel "Throughput (transactions/s)";
set y2label "";

set multiplot

plot  "results/avg-orig.csv" using 1:2 axes x1y1 title "Base"\
   with linespoints lc rgb "#4a0094" lt 7 pt 3 pointsize 1.4,\
	 "results/avg-posix.csv" using 1:2 axes x1y1 title "POSIX"\
   with linespoints lc rgb "#4a004a" lt 4 pt 12 pointsize 1.4,\
     "results/avg-spinlock.csv" using 1:2 axes x1y1 title "SL"\
   with linespoints lc rgb "#000094" lt 3 pt 1 pointsize 1.5,\
     "results/avg-flat.csv" using 1:2 axes x1y1 title "FC"\
   with linespoints lc rgb "#009400" lt 5 pt 4 pointsize 1,\
     "results/avg-mcs.csv" using 1:2 axes x1y1 title "MCS"\
   with linespoints lc rgb "#004a4a" lt 6 pt 9 pointsize 1.4,\
     "results/avg-saml.csv" using 1:2 axes x1y1 title "SAML"\
   with linespoints lc rgb "#009494" lt 2 pt 2 pointsize 1.4,\
     "results/avg-rcl.csv" using 1:2 axes x1y1 title "RCL"\
   with linespoints lc rgb "#4a0000" lt 1 pt 7 pointsize 1.3
#     "results/avg-mcstp.csv" using 1:2 axes x1y1 title "  MCS-TP"\
#   with linespoints lc rgb "#009494" lt 2 pt 2 pointsize 1.4,\
#     "results/avg-rcl-yield.csv" using 1:2 axes x1y1 title "RCL"\
#   with linespoints lc rgb "#4a0000" lt 1 pt 7 pointsize 1.3

#lc rgb "#000094" lt 3 pt 1 pointsize 1.5

#set parametric
#set trange [1:1000]
#replot 48,t lt 1 lc rgb "#949494"


