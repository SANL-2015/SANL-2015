#!/bin/bash

./plot_dcm.sh | gnuplot > graphs/dcm.pdf
./plot_latencies_custom.sh | gnuplot > graphs/latencies.pdf
