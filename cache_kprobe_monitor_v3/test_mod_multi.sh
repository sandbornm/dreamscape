#!/bin/bash

base="/home/ubuntu/dreamscape/test_bins/bins/arm64/"

# Read the name of the binary from the command line argument
# If the binary is missing, indicate that it must be provided
#if [ -z "$1" ]; then
#  echo "usage: ./test_mod.sh <binary_name>"
#  echo "options: binaries at $base"
#  exit 1
#fi

# Iterate through each binary in the base directory
for binary in "$base"/*; do
  # Check if the item is a file, executable, and not a directory
  if [[ -f "$binary" ]] && [[ -x "$binary" ]]; then
    # Run the program on CPU 0
    taskset -c 0 "$binary" &
    pgm_pid=$!

    # Append binary, pgm_pid, current time to pid_bin_record
    echo "$(basename "$binary"),$pgm_pid,$(date)" >> ./pid_bin_record

    # Echo to the proc file for the module to start monitoring
    echo "$pgm_pid" | sudo tee /proc/cache_kmv3_pid

    wait "$pgm_pid"
    echo "Process $pgm_pid exited, getting data from dmesg"
    /home/ubuntu/dreamscape/data/dmesg_dumps/dmesg_dump.sh

    # Execute the desired commands after each iteration
    python3 /home/ubuntu/dreamscape/data/dmesg_dumps/dmesg_parser.py && python3 /home/ubuntu/dreamscape/data/dmesg_dumps/draw_figs_all.py
  fi

done

