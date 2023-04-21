#!/usr/bin/env python3

import r2pipe
import json

def generate_cfg_json(binary_path):
    # Open the binary in radare2
    r2 = r2pipe.open(binary_path)

    # Analyze the binary
    r2.cmd("aa")

    # Install the r2dec plugin
    r2.cmd("r2pm -i r2dec")

    # Generate the CFG in JSON format
    cfg_json = r2.cmd("r2dec -C -j")

    # Parse the JSON output
    try:
        cfg_json = json.loads(cfg_json)
    except json.JSONDecodeError:
        print("Error: Failed to parse JSON output")
        return None

    # Close the radare2 process
    r2.quit()

    return cfg_json

if __name__ == '__main__':
    binary_path = '/path/to/binary'
    cfg_json = generate_cfg_json(binary_path)

    if cfg_json:
        with open('cfg.json', 'w') as f:
            json.dump(cfg_json, f, indent=4)
