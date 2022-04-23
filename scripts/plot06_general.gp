#!/usr/bin/gnuplot

reset
set output 'data/fastdbl_lazyrealloc.png'
set title 'Execution time (fast doubling kernel)'
set term png enhanced font 'Helvetica,10'

set xlabel 'F(n)'
set ylabel 'time (ns)'
set grid

plot [0:1000][0:5000] \
'data/0417bn_ktime_divroundup32_data.out' using 1:2 with linespoints pt 7 ps .5 title "v2 divroundup32", \
'data/06bn_ktime_data.out' using 1:2 with linespoints pt 7 ps .5 title "v3 lazy realloc", \
