set term png size 800, 500 font "Times-Roman,"
set output 'CasHMC_plot_no0.png'
set title "CasHMC bandwidth graph" font ",18"
set autoscale
set grid
set xlabel "Simulation plot epoch"
set ylabel "Bandwidth [GB/s]"
set key left box
plot  "CasHMC_plot_No0.dat" using 1:4 title 'HMC' with lines lw 2 , \
      "CasHMC_plot_No0.dat" using 1:2 title 'Link[0]' with lines lw 1 , \
      "CasHMC_plot_No0.dat" using 1:3 title 'Link[1]' with lines lw 1 , \
