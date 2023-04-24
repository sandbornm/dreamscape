import json
import os
import plotly.graph_objs as go
from collections import defaultdict

# import sys

# if len(sys.argv) != 2:
#     print("Usage: python draw_figs.py [json_path]")
#     sys.exit(1)

# json_filename = sys.argv[1]

# Step 1: Read the JSON file and extract the data
json_filename = "./dmesg_04_24_04_54_48.json"  # Replace with the name of your JSON file
with open(json_filename, "r") as json_file:
    data = json.load(json_file)

# Step 2: Prepare the data for plotting
event_data = defaultdict(lambda: defaultdict(list))
for pid, events in data.items():
    for event in events:
        event_name = event["event_name"]
        timestamp = event["timestamp"]
        counter_value = event["counter_value"]
        event_data[pid][event_name].append({"timestamp": timestamp, "counter_value": counter_value})

# Step 3: Create a Plotly chart for each event with the corresponding data
figs_folder = "figs"
if not os.path.exists(figs_folder):
    os.mkdir(figs_folder)

for pid, events in event_data.items():
    for event_name, event_values in events.items():
        timestamps = [event["timestamp"] for event in event_values]
        counter_values = [event["counter_value"] for event in event_values]
        delta_values = [counter_values[i + 1] - counter_values[i] for i in range(len(counter_values) - 1)]

        fig = go.Figure()
        fig.add_trace(go.Scatter(x=timestamps, y=counter_values, mode='lines+markers', name=event_name))

        for i, (timestamp, delta) in enumerate(zip(timestamps[:-1], delta_values)):
            fig.add_annotation(
                x=timestamp,
                y=counter_values[i],
                text=f"{delta}",
                showarrow=False,
                font=dict(size=10),
                bgcolor="rgba(255, 255, 255, 0.7)"
            )

        fig.update_layout(
            title=f"Performance Counter Values for {event_name} (PID: {pid})",
            xaxis_title="Timestamp",
            yaxis_title="Counter Value"
        )

        # Step 4: Save the resulting figures to the "figs" folder
        fig_filename = os.path.join(figs_folder, f"{pid}_{event_name}.html")
        fig.write_html(fig_filename)
