

import re
import sys
import json
import os
from collections import defaultdict

event_code_map = {
        1: "L1I_CACHE_REFILL",
        2: "L1I_TLB_REFILL",
        3: "L1D_CACHE_REFILL",
        4: "L1D_CACHE_ACCESS",
        5: "L1D_TLB_REFILL",
        6: "LD_RETIRED",
        7: "ST_RETIRED",
        8: "INST_RETIRED",
        17: "CPU_CYCLES",
        18: "BR_PRED",
        19: "MEM_ACCESS",
        20: "L1I_CACHE",
        21: "L1D_CACHE_WB",
        25: "BUS_ACCESS",
}


def parse_dmesg(filename):
    pid_regex = r"PID: (\d+)"
    timestamp_regex = r"\[(.*?)\]"
    event_code_counter_regex = r"event code: (\d+)\s+; counter value: (\d+)"
    program_counter_regex = r"current program counter: (\d+)"

    with open(filename, 'r') as file:
        data = file.read()

    pid_data = defaultdict(list)

    program_counter = None

    for line in data.split('\n'):
        pid_match = re.search(pid_regex, line)
        timestamp_match = re.search(timestamp_regex, line)
        event_code_counter_match = re.search(event_code_counter_regex, line)
        program_counter_match = re.search(program_counter_regex, line)

        if pid_match:
            pid = int(pid_match.group(1))
            if pid not in pid_data:
                pid_data[pid] = []

        if program_counter_match:
            program_counter = int(program_counter_match.group(1))

        if timestamp_match and event_code_counter_match:
            timestamp = float(timestamp_match.group(1))
            event_code = int(event_code_counter_match.group(1))
            event_name = event_code_map[event_code]
            counter_value = int(event_code_counter_match.group(2))

            event_info = {
                "timestamp": timestamp,
                "event_name": event_name,
                "counter_value": counter_value,
                "program_counter": program_counter
            }
            pid_data[pid].append(event_info)

    json_filename = os.path.splitext(filename)[0] + '.json'
    with open(json_filename, 'w') as json_file:
        json.dump(pid_data, json_file, indent=4)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python parser.py <filename>")
        sys.exit(1)
    
    input_filename = sys.argv[1]
    parse_dmesg(input_filename)


    