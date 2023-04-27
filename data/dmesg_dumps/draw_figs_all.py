import json
import os
import plotly.graph_objs as go
from collections import defaultdict

# Step 1: Read the JSON file and extract the data
json_filename = "./dmesg_output_20230427_052318.json"
with open(json_filename, "r") as json_file:
    data = json.load(json_file)

# Step 2: Prepare the data for plotting
event_data = defaultdict(lambda: defaultdict(list))
for pid, events in data.items():
    for event in events:
        event_name = event["event_name"]
        timestamp = event["timestamp"]
        delta = event["delta"]
        event_data[pid][event_name].append({"timestamp": timestamp, "delta": delta})

# Step 3: Create a Plotly chart for each PID with all events
figs_folder = "figs"
if not os.path.exists(figs_folder):
    os.mkdir(figs_folder)

for pid, events in event_data.items():
    fig = go.Figure()
    
    for event_name, event_values in events.items():
        timestamps = [event["timestamp"] for event in event_values]
        deltas = [event["delta"] for event in event_values]
        fig.add_trace(go.Scatter(x=timestamps, y=deltas, mode='lines+markers', name=event_name))

    fig.update_layout(
        title=f"Performance Counter Value Changes for PID: {pid}",
        xaxis_title="Timestamp",
        yaxis_title="Counter Value Change"
    )

    # Step 4: Save the resulting figures to the "figs" folder
    fig_filename = os.path.join(figs_folder, f"{pid}_all_events.html")
    fig.write_html(fig_filename)
