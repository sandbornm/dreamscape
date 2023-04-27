#!/bin/bash
timestamp=$(date +"%Y%m%d_%H%M%S")
output_file="./dmesg_output_${timestamp}.txt"

echo "Dumping dmesg to ${output_file}"
dmesg > "${output_file}"
sudo dmesg -C

# invoke with watch -n 60 ./dmesg_dump.sh to run during program analysis
