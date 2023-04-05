#!/bin/bash

# Start the target program in the suspended state
(./$1 &); pkill -STOP $1

# Wait for a short time to allow the child process to start
sleep 1

echo "last process is $!"
# Get the PID of the child process
CHILD_PID=$!

# Debugging output
echo "Program path: $1"
echo "Child process PID: $CHILD_PID"

# Output the child process PID
echo "Child process started in suspended state with PID: $CHILD_PID"
