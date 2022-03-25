#!/usr/bin/gnuplot

reset
set output 'data/04_exactsol_pic.png'
set title 'Exact-solution method vs. correct Fibonacci numbers'
set term png enhanced font 'Helvetica,10'

set xlabel 'F(n)'
set ylabel 'abs(diff)'
set grid

plot [0:100][0:23000] \
'data/04_exactsol_data.out' using 1:2 with linespoints pt 7 ps .5 \
	title "abs(exact-fibonacci)"
