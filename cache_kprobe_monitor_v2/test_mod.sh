#!/bin/bash

# Run stress on CPU for 20s and obtain the PID of the process
#stress --cpu 1 --timeout 20s &

/home/ubuntu/dreamscape/test_bins/bins/arm64/onebil_memacc &
#cache_intensive &
pgm_pid=$!

# Monitor the stress process using the kernel module
echo $pgm_pid | sudo tee /proc/cache_kmv2_pid