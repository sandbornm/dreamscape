#!/bin/bash

base=/home/ubuntu/dreamscape/test_bins/bins/arm64/

# read name of binary from command line arg
# if binary is missing, indicate it must be provided
if [ -z "$1" ]
  then
    echo "usage: ./test_mod.sh <binary_name>"
    echo "options: binaries at $base"
    exit 1
fi
binary=$1

# run the program on cpu 0
taskset -c 0 $base$binary &
pgm_pid=$!

# append binary, pgm_pid, current time to pid_bin_record
echo "$binary,$pgm_pid,$(date)" >> ./pid_bin_record

# echo to the proc file for module to start monitoring
echo $pgm_pid | sudo tee /proc/cache_kmv3_pid

# when the process exits call ../data/dmesg_dumps/dmesg_dump.sh
wait $pgm_pid
echo "Process $pgm_pid exited, getting data from dmesg"
/home/ubuntu/dreamscape/data/dmesg_dumps/dmesg_dump.sh