#!/usr/bin/gnuplot

reset
set output 'data/05bn_userkernel_fastdbl_pic.png'
set title 'Fibonacci execution time'
set term png enhanced font 'Helvetica,10'

set xlabel 'F(n)'
set ylabel 'time (ns)'
set grid

plot [0:1000][0:230000] \
'data/05bn_userkernel_data.out' using 1:2 with linespoints pt 7 ps .5 title "fastdoubling(kernel)", \
'' using 1:3 with linespoints pt 7 ps .5 title "user", \
'' using 1:4 with linespoints pt 7 ps .5 title "syscall + copy2user + fbnprint"
