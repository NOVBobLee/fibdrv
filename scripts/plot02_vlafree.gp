#!/usr/bin/gnuplot

reset
set output 'data/02_vlafree_pic.png'
set title 'Fibonacci execution time'
set term png enhanced font 'Helvetica,10'

set xlabel 'F(n)'
set ylabel 'time (ns)'
set grid

plot [0:100][0:300] \
'data/02_vlafree_data.out' using 1:2 with linespoints pt 7 ps .5 title "vla", \
'' using 1:3 with linespoints pt 7 ps .5 title "kmalloc", \
'' using 1:4 with linespoints pt 7 ps .5 title "fixed-la"
