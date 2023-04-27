import json
import os
import plotly.graph_objs as go
from collections import defaultdict

# # Step 1: Read the JSON file and extract the data
json_filename = "./dmesg_output_20230427_052318.json"
#"./dmesg_04_24_04_54_48.json"  # Replace with the name of your JSON file
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

# Compute the differences in counter values
for pid, events in event_data.items():
    for event_name, event_values in events.items():
        for i in range(1, len(event_values)):
            event_values[i]["counter_value_diff"] = event_values[i]["counter_value"] - event_values[i - 1]["counter_value"]

# Step 3: Create a Plotly chart for each PID with all events
figs_folder = "figs"
if not os.path.exists(figs_folder):
    os.mkdir(figs_folder)

for pid, events in event_data.items():
    fig = go.Figure()
    
    for event_name, event_values in events.items():
        timestamps = [event["timestamp"] for event in event_values]
        counter_value_diffs = [event.get("counter_value_diff", 0) for event in event_values]  # Use 0 for the first value in the list
        fig.add_trace(go.Scatter(x=timestamps, y=counter_value_diffs, mode='lines+markers', name=event_name))

    fig.update_layout(
        title=f"Performance Counter Value Changes for PID: {pid}",
        xaxis_title="Timestamp",
        yaxis_title="Counter Value Change"
    )

    # Step 4: Save the resulting figures to the "figs" folder
    fig_filename = os.path.join(figs_folder, f"{pid}_all_events.html")
    fig.write_html(fig_filename)

# # Step 2: Prepare the data for plotting
# event_data = defaultdict(lambda: defaultdict(list))
# for pid, events in data.items():
#     for event in events:
#         event_name = event["event_name"]
#         timestamp = event["timestamp"]
#         counter_value = event["counter_value"]
#         event_data[pid][event_name].append({"timestamp": timestamp, "counter_value": counter_value})

# # Step 3: Create a Plotly chart for each PID with all events
# figs_folder = "figs"
# if not os.path.exists(figs_folder):
#     os.mkdir(figs_folder)

# for pid, events in event_data.items():
#     fig = go.Figure()
    
#     for event_name, event_values in events.items():
#         timestamps = [event["timestamp"] for event in event_values]
#         counter_values = [event["counter_value"] for event in event_values]
#         fig.add_trace(go.Scatter(x=timestamps, y=counter_values, mode='lines+markers', name=event_name))

#     fig.update_layout(
#         title=f"Performance Counter Values for PID: {pid}",
#         xaxis_title="Timestamp",
#         yaxis_title="Counter Value"
#     )

#     # Step 4: Save the resulting figures to the "figs" folder
#     fig_filename = os.path.join(figs_folder, f"{pid}_all_events.html")
#     fig.write_html(fig_filename)