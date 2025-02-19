#!/usr/bin/gnuplot

reset
set output 'data/00_checkvalues92_pic.png'
set title 'Test vs. correct Fibonacci numbers'
set term png enhanced font 'Helvetica,10'

set xlabel 'F(n)'
set ylabel 'abs(diff)'
set grid

plot [0:100][0:23000] \
'data/00_checkvalues92_data.out' using 1:2 with linespoints pt 7 ps .5 \
	title "abs(test - fibonacci)"
