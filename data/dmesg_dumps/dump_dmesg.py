import subprocess
import time

# Get current date and time
current_time = time.strftime("%m_%d_%H_%M_%S")

# Generate filename with current time stamp
filename = "./dmesg_" + current_time + ".txt"

print(filename)

# Run dmesg command and output to file
subprocess.call(f"dmesg > {filename}", shell=True)
