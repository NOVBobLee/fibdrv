#!/usr/bin/gnuplot

reset
set output 'data/print_v0v1_pic.png'
set title 'Print decimal string execution time (kernel)'
set term png enhanced font 'Helvetica,10'

set xlabel 'F(n)'
set ylabel 'time (ns)'
set grid

plot [0:1000][0:230000] \
'data/06bn_ktimes_printv0_data.out' using 1:2 with linespoints pt 7 ps .5 title "printv0", \
'data/06bn_ktimes_printv1_data.out' using 1:2 with linespoints pt 7 ps .5 title "printv1"
