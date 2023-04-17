## Usage

Commands for the `cache_kprobe_monitor_v2` module:

0. Load the module after compilation with `make`: `sudo insmod cache_kprobe_monitor_v2.ko`

1. Start performance counting for a program that will be started by the module (can bash script this by starting process in background with `/cmd/ &` and then get its pid with `just_started_pid=$!`):

`echo "start <PID>" | sudo tee /proc/cache_kprobe_monitor`

2. The performance counter values will be read and updated every 1 second (can tweak this later) and then written to a /tmp/ text file named after the pid with the values of the performance counters indicated.

3. Wait until the program finishes execution (need to add command to stop monitoring for a long-running program)

4. remove the module with: `sudo rmmod cache_kprobe_monitor_v2`


### COptional:
- One option that must be changed in the source is the `ktime_set(s, ns)` call which dictates how much time should pass between performance counter samples. Default is 1s 0ns, 5s was used for testing.


### TODO
- [X] Multi-counter values
- [X] Include current program counter value
- [X] Sample on an interval, currently every 1 second
- [ ] Write results to a userspace file or save dmesg slices
- [ ] Add a stop command for long-running programs e.g. daemons
- [ ] Parsing and visualization

