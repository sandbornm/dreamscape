import json
import os
import plotly.graph_objs as go
from collections import defaultdict
import glob

# Step 1: Read the JSON file and extract the data
# read all json files from the current directory using glob
#json_filename = "data/dmesg_dumps/dmesg_output_20230501_033047.json"

# absolute path of data/dmesg_dumps/*.json

json_files = glob.glob("/home/ubuntu/dreamscape/data/dmesg_dumps/*.json")
print(json_files)
for json_file in json_files:

    print(f"processing {json_file}")

    with open(json_file, "r") as jf:
        data = json.load(jf)

    # Step 2: Prepare the data for plotting
    event_data = defaultdict(lambda: defaultdict(list))
    for pid, events in data.items():
        if pid != "null": # partial dumps
            for event in events:
                event_name = event["event_name"]
                timestamp = event["timestamp"]
                delta = event["delta"]
                program_counter = event["program_counter"]  # Extract the program counter values
                event_data[pid][event_name].append({"timestamp": timestamp, "delta": delta, "program_counter": program_counter})

    # Step 3: Create a Plotly chart for each PID with all events
    figs_folder = "figs"
    if not os.path.exists(figs_folder):
        os.mkdir(figs_folder)

    for pid, events in event_data.items():
        fig = go.Figure()
        
        for event_name, event_values in events.items():
            timestamps = [event["timestamp"] for event in event_values]
            deltas = [event["delta"] for event in event_values]
            program_counters = [event["program_counter"] for event in event_values]  # Extract the program counter values
            fig.add_trace(go.Scatter(x=timestamps, y=deltas, mode='lines+markers', name=event_name, text=program_counters, hovertemplate='Timestamp: %{x}<br>Counter Value Change: %{y}<br>Program Counter: %{text}<extra></extra>'))  # Include the text and hovertemplate parameters

        fig.update_layout(
            title=f"Performance Counter Value Changes for PID: {pid}",
            xaxis_title="Timestamp",
            yaxis_title="Counter Value Change"
        )

        # Step 4: Save the resulting figures to the "figs" folder
        fig_filename = os.path.join(figs_folder, f"{pid}_all_events.html")
        print(f"saving {fig_filename}")
        fig.write_html(fig_filename)
