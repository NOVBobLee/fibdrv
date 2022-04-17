#!/usr/bin/gnuplot

reset
set output 'data/fastdbl_v1v2.png'
set title 'Execution time (kernel)'
set term png enhanced font 'Helvetica,10'

set xlabel 'F(n)'
set ylabel 'time (ns)'
set grid

plot [0:1000][0:5000] \
'data/0415bn_ktime_fastdblv1_data.out' using 1:2 with linespoints pt 7 ps .5 title "fastdbl v1", \
'data/06bn_ktime_data.out' using 1:2 with linespoints pt 7 ps .5 title "fastdbl v2", \
