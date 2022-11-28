#!/bin/bash -x

#example of running txrx_test using PlutoSDR as an RF frontend
sudo LD_LIBRARY_PATH=$(pwd)/../bin_test nice -20 ../bin_test/txrx_test -f 2400000000 -a n_prb=6,context="ip:192.168.2.16" -p 6 -g 50 -o test_txrx_pluto.bin
