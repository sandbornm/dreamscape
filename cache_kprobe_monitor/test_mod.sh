#!/bin/bash

# Run stress on CPU for 20s and obtain the PID of the process
stress --cpu 1 --timeout 20s &
stress_pid=$!

# Monitor the stress process using the kernel module
echo "start_pid $stress_pid" | sudo tee /proc/cache_kprobe_monitor

# Wait for 5 seconds
sleep 3

# Stop the monitoring
echo "stop_pid" | sudo tee /proc/cache_kprobe_monitor

# Wait for the stress process to finish
#wait $stress_pid
