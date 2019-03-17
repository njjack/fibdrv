reset
set ylabel 'time(ns)'
set xlabel 'nth fibonacci number'
set key right top
set title 'costing time in kernel and userspace'
set term png enhanced font 'Verdana,10'
set output 'runtime.png'

plot [:][:2400]'time' \
   using 1:2 with linespoints linewidth 3  title 'kernel', \
'' using 1:3 with linespoints linewidth 3 title 'user' , \
