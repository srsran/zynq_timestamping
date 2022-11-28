#!/bin/bash -x

#example of running txrx_test from the ZCU boards
sudo LD_LIBRARY_PATH=$(pwd)
nice -20 ./txrx_test -f 2400000000 -a n_prb=6 -p 6 -g 40 -o test_txrx_zcu.bin
