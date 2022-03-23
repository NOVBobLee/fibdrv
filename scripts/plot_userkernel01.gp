#!/usr/bin/gnuplot

reset
set output 'data/userkernel01-pic.png'
set title 'Fibonacci execution time'
set term png enhanced font 'Helvetica,10'

set xlabel 'F(n)'
set ylabel 'time (ns)'
set grid

plot [0:100][0:800] \
'data/userkernel01-data.out' using 1:2 with linespoints pt 7 ps .5 title "kernel", \
'' using 1:3 with linespoints pt 7 ps .5 title "user", \
'' using 1:4 with linespoints pt 7 ps .5 title "syscall"
