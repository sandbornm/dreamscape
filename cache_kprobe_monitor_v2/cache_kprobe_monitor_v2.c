#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/kprobes.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include <linux/perf_event.h>
#include <linux/slab.h>
#include <linux/pid.h>

#include <linux/hrtimer.h>


static int target_pid = -1;

static struct perf_counter_data *perf_data;
static struct task_struct *monitored_task = NULL;
static struct hrtimer sample_timer;
static ktime_t sample_interval;

static struct proc_dir_entry *proc_entry_pid;
static struct proc_dir_entry *proc_entry_message;

static char message_buffer[512];
static DEFINE_SPINLOCK(buffer_lock);



/*

Perf counter handling


*/


// https://developer.arm.com/documentation/ddi0500/latest/ p.12-36
enum event_code {
    L1I_CACHE_REFILL = 0x01,
    L1I_TLB_REFILL = 0x02,
    L1D_CACHE_REFILL = 0x03,
    L1D_CACHE_ACCESS = 0x04,
    L1D_TLB_REFILL = 0x05,
    LD_RETIRED = 0x06,
    ST_RETIRED = 0x07,
    INST_RETIRED = 0x08,
    CPU_CYCLES = 0x11,
    BR_PRED = 0x12,
    MEM_ACCESS = 0x13,
    L1I_CACHE = 0x14,
    L1D_CACHE_WB = 0x15
};

static u32 event_codes[] = {
    L1I_CACHE_REFILL,
    L1I_TLB_REFILL,
    L1D_CACHE_REFILL,
    L1D_CACHE_ACCESS,
    L1D_TLB_REFILL,
    LD_RETIRED,
    ST_RETIRED,
    INST_RETIRED,
    CPU_CYCLES,
    BR_PRED,
    MEM_ACCESS,
    L1I_CACHE,
    L1D_CACHE_WB
};

#define NUM_EVENTS (sizeof(event_codes) / sizeof(event_codes[0]))

struct perf_counter_data {
    struct perf_event *events[NUM_EVENTS];
    u64 prev_values[NUM_EVENTS];
};



// static struct perf_counter_data *init_perf_counters(pid_t pid)
// {
//     struct perf_counter_data *perf_data;
//     struct perf_event_attr attr;
//     int cpu, i;

//     printk(KERN_INFO " (init_perf_counters) Initializing performance counters\n");
//     perf_data = kzalloc(sizeof(struct perf_counter_data), GFP_KERNEL);
//     if (!perf_data) {
//         printk(KERN_ERR "kzalloc failed\n");
//         return NULL;
//     }
//     printk(KERN_INFO "kzalloc succeeded\n");

//     // need to add a for loop for all of the event codes

//     printk(KERN_INFO "creating perf_event_attr\n");
//     memset(&attr, 0, sizeof(struct perf_event_attr));
//     attr.type = PERF_TYPE_RAW; // PERF_TYPE_HARDWARE;
//     attr.size = sizeof(struct perf_event_attr);
//     attr.config = 0x04; // Replace with the desired event code
//     attr.exclude_kernel = 1;

//     printk(KERN_INFO "perf_event_attr created\n");

//     printk(KERN_INFO "getting cpu");
//     cpu = get_cpu();
//     printk(KERN_INFO "calling perf_event_create_kernel_counter\n");

//     perf_data->event = perf_event_create_kernel_counter(&attr, cpu, NULL, NULL, NULL);

//     printk(KERN_INFO "perf_event_create_kernel_counter returned\n");

//     if (IS_ERR(perf_data->event)) {
//         kfree(perf_data);
//         return NULL;
//     }

//     printk(KERN_INFO "perf_event_create_kernel_counter succeeded\n");

//     printk(KERN_INFO "putting cpu");
//     put_cpu();

//     perf_data->prev_value = 0;

//     return perf_data;
// }

static struct perf_counter_data *init_perf_counters(pid_t pid)
{
    struct perf_counter_data *perf_data;
    struct perf_event_attr attr;
    int cpu;
    size_t i;

    printk(KERN_INFO " (init_perf_counters) Initializing multi performance counters\n");

    // Allocate memory for the perf_counter_data array
    perf_data = kzalloc(NUM_EVENTS * sizeof(struct perf_counter_data), GFP_KERNEL);
    if (!perf_data) {
        printk(KERN_ERR "kzalloc failed\n");
        return NULL;
    }
    printk(KERN_INFO "kzalloc succeeded\n");

    cpu = get_cpu();
    for (i = 0; i < NUM_EVENTS; ++i) {
        printk(KERN_INFO "creating perf_event_attr for event code %u\n", event_codes[i]);

        // Initialize the perf_event_attr structure for the current event code
        memset(&attr, 0, sizeof(struct perf_event_attr));
        attr.type = PERF_TYPE_RAW;
        attr.size = sizeof(struct perf_event_attr);
        attr.config = event_codes[i];  // Set the event code
        attr.exclude_kernel = 1;

        printk(KERN_INFO "perf_event_attr created\n");

        printk(KERN_INFO "calling perf_event_create_kernel_counter for event code %u\n", event_codes[i]);
        perf_data->events[i] = perf_event_create_kernel_counter(&attr, cpu, NULL, NULL, NULL);

        if (IS_ERR(perf_data->events[i])) {
            // Release resources and return NULL if there is an error
            while (i > 0) {
                --i;
                perf_event_release_kernel(perf_data->events[i]);
            }
            kfree(perf_data);
            put_cpu();
            return NULL;
        }

        printk(KERN_INFO "perf_event_create_kernel_counter succeeded for event code %u\n", event_codes[i]);
        perf_data->prev_values[i] = 0;
    }
    put_cpu();

    return perf_data;
}

static void write_counters_to_file(u64 *counter_values)
{
    printk(KERN_INFO " (write_counters_to_file) Writing counter values to file\n");
    struct file *file;
    char buf[1024]; // TODO figure out optimal size buffer
    int i;
    loff_t pos = 0;

    file = filp_open("/tmp/perf_counters.txt", O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (IS_ERR(file)) {
        printk(KERN_ERR "Error opening /tmp/perf_counters.txt\n");
        return;
    }

    for (i = 0; i < NUM_EVENTS; i++) {
        snprintf(buf, sizeof(buf), "Event %d: %llu\n", event_codes[i], counter_values[i]);
        kernel_write(file, buf, strlen(buf), &pos);
    }

    filp_close(file, NULL);

    printk(KERN_INFO " (write_counters_to_file) Finished writing counter values to file\n");
}

static void cleanup_perf_counters(struct perf_counter_data *perf_data)
{
    int i;

    printk(KERN_INFO " (cleanup_perf_counters) Cleaning up multi performance counters\n");

    if (!perf_data) {
        return;
    }

    for (i = 0; i < NUM_EVENTS; i++) {
        if (perf_data->events[i]) {
            perf_event_release_kernel(perf_data->events[i]);
        }
    }

    kfree(perf_data);

    printk(KERN_INFO " (cleanup_perf_counters) Finished cleaning up multi performance counters\n");
}


// static void cleanup_perf_counters(struct perf_counter_data *perf_data)
// {
//     if (!perf_data) {
//         return;
//     }

//     if (perf_data->event) {
//         perf_event_release_kernel(perf_data->event);
//     }

//     kfree(perf_data);
// }




// static enum hrtimer_restart sample_perf_counters(struct hrtimer *timer)
// {
//     u64 enabled, running, new_value, delta;

//     printk(KERN_INFO "(sample_perf_counters) timer callback, Monitoring a task\n");

//     // TODO: add a for loop for all of the event codes and also consider a non null param to perf_event_read_value if the timestamp is important

//     if (monitored_task && pid_alive(monitored_task)) {
//         printk(KERN_INFO "(sample_perf_counters) Monitored task is alive, reading the counter value\n");
//         new_value = perf_event_read_value(perf_data->event, &enabled, &running);
//         printk(KERN_INFO "(sample_perf_counters) read counter value\n");
//         delta = new_value - perf_data->prev_value;
//         printk(KERN_INFO "(sample_perf_counters) L1D_CACHE_ACCESS counter value: %llu, delta: %llu\n", new_value, delta);
//         perf_data->prev_value = new_value;
//         printk(KERN_INFO "(sample_perf_counters) hrtimer_forward_now\n");
//         hrtimer_forward_now(timer, sample_interval);
//         return HRTIMER_RESTART;
//     } else {
//         printk(KERN_INFO "Monitored task terminated, stopping the timer\n");
//         return HRTIMER_NORESTART;
//     }
// }

// static void write_counters_to_file(u64 *counter_values)
// {
//     struct file *file;
//     char buf[256];
//     int i;
//     loff_t pos = 0;

//     file = filp_open("/tmp/perf_counters.txt", O_CREAT | O_WRONLY | O_APPEND, 0644);
//     if (IS_ERR(file)) {
//         printk(KERN_ERR "Error opening /tmp/perf_counters.txt\n");
//         return;
//     }

//     for (i = 0; i < NUM_EVENTS; i++) {
//         snprintf(buf, sizeof(buf), "Event %d: %llu\n", event_codes[i], counter_values[i]);
//         kernel_write(file, buf, strlen(buf), &pos);
//     }

//     filp_close(file, NULL);
// }

// Modify the sample_perf_counters function
static enum hrtimer_restart sample_perf_counters(struct hrtimer *timer)
{
    u64 enabled, running, new_value, delta;
    u64 counter_values[NUM_EVENTS];
    int i;
    u64 program_counter;

    if (monitored_task && pid_alive(monitored_task)) {

        printk(KERN_INFO "(sample_perf_counters) Monitored task is alive, reading the counter values\n");

        struct pt_regs *regs = task_pt_regs(monitored_task);
        program_counter = regs->pc;

        printk(KERN_INFO "(sample_perf_counters) current program counter: %llu\n", program_counter);

        for (i = 0; i < NUM_EVENTS; i++) {
            new_value = perf_event_read_value(perf_data->events[i], &enabled, &running);
            delta = new_value - perf_data->prev_values[i];
            printk(KERN_INFO "(sample_perf_counters) event code: %u  ; counter value: %llu, delta: %llu\n", event_codes[i], new_value, delta);
            counter_values[i] = delta;
            perf_data->prev_values[i] = new_value;
        }
    // Call the function to write the counter values to a file
        //write_counters_to_file(counter_values);

        hrtimer_forward_now(timer, sample_interval);
        return HRTIMER_RESTART;
    } else {
        printk(KERN_INFO "Monitored task terminated, stopping the timer\n");
        return HRTIMER_NORESTART;
    }
}



static enum hrtimer_restart dummy_callback(struct hrtimer *timer)
{
    printk(KERN_INFO "Dummy timer callback executed\n");
    hrtimer_forward_now(timer, sample_interval);
    return HRTIMER_RESTART;
}


static void start_monitoring(pid_t pid)
{
    monitored_task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!monitored_task) {
        printk(KERN_ERR "Failed to find task for PID %d\n", pid);
        return;
    }
    printk(KERN_INFO "Found task for PID %d\n", pid);

    get_task_struct(monitored_task);
    printk(KERN_INFO "Got task struct for PID %d\n", pid);

    printk(KERN_INFO "Initializing performance counters\n");
    perf_data = init_perf_counters(pid);
    if (!perf_data) {
        printk(KERN_ERR "Failed to initialize performance counters\n");
        return;
    }

    printk(KERN_INFO "Starting the timer for 5 second sampling of counters\n");
    sample_interval = ktime_set(1, 0); // ktime(second, nanosecond) intervals to collect data
    printk(KERN_INFO "Sample interval set for 5 seconds intervals\n");

    printk(KERN_INFO "Initializing the timer\n");
    hrtimer_init(&sample_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    printk(KERN_INFO "Timer initialized\n");

    sample_timer.function = sample_perf_counters; // dummy_callback; //

    printk(KERN_INFO "Starting the timer\n");
    hrtimer_start(&sample_timer, sample_interval, HRTIMER_MODE_REL);
    printk(KERN_INFO "Timer started\n");
}

static void stop_monitoring(void)
{
    hrtimer_cancel(&sample_timer);
    cleanup_perf_counters(perf_data);
    put_task_struct(monitored_task);
    monitored_task = NULL;
}




/*

PID and file handling


*/


static int hello_switch(struct kprobe *p, struct pt_regs *regs)
{
    struct task_struct *next_task = current;

    if (next_task && next_task->pid == target_pid)
    {
        //printk(KERN_INFO "hello world %d\n", target_pid);

        spin_lock(&buffer_lock);
        snprintf(message_buffer, sizeof(message_buffer), "hello world %d\n", target_pid);
        spin_unlock(&buffer_lock);

    }

    return 0;
}


static struct kprobe kp = {
    .symbol_name = "finish_task_switch",
    .pre_handler = hello_switch,
};


static ssize_t pid_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "Reading PID from proc file\n");
    char str[12];
    if (count >= sizeof(str)) {
        return -EINVAL;
    }

    if (copy_from_user(str, buffer, count)) {
        return -EFAULT;
    }

    str[count] = '\0';
    if (kstrtoint(str, 10, &target_pid)) {
        return -EINVAL;
    }
    printk(KERN_INFO "PID set to %d\n", target_pid); // Added for debugging

    // begin perf monitoring
    printk(KERN_INFO "beginning perf monitoring\n");
    start_monitoring(target_pid);

    return count;
}

static const struct file_operations pid_fops = {
    .write = pid_write,
};


static ssize_t message_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    ssize_t len;

    spin_lock(&buffer_lock);
    len = simple_read_from_buffer(buffer, count, ppos, message_buffer, strlen(message_buffer));
    spin_unlock(&buffer_lock);

    return len;
}

static const struct file_operations message_fops = {
    .read = message_read,
};



static int __init cache_kprobe_monitor_v2_init(void)
{

    console_loglevel = 8;

    printk(KERN_INFO "loading cache_kprobe_monitor_v2 module\n");

    int ret;

    ret = register_kprobe(&kp);
    if (ret < 0)
    {
        printk(KERN_INFO "register_kprobe failed, returned %d\n", ret);
        return ret;
    }

    // was hello_switch_pid
    proc_entry_pid = proc_create("cache_kmv2_pid", 0200, NULL, &pid_fops);
    if (!proc_entry_pid)
    {
        printk(KERN_ERR "Error creating proc entry, exiting");
        unregister_kprobe(&kp);
        proc_remove(proc_entry_pid);
        return -ENOMEM;
    }

    // was hello_switch_message
    proc_entry_message = proc_create("cache_kmv2_message", 0400, NULL, &message_fops);
    if (!proc_entry_message)
    {
        printk(KERN_ERR "Error creating proc entry for message, exiting");
        unregister_kprobe(&kp);
        proc_remove(proc_entry_message);
        return -ENOMEM;
    }

    printk(KERN_INFO "Proc entries created, write PID to /proc/cache_kmv2_pid to start\n");    
    printk(KERN_INFO "loaded cache_kprobe_monitor_v2 module\n");

    return 0;
}

static void __exit cache_kprobe_monitor_v2_exit(void)
{
    printk(KERN_INFO "unloading cache_kprobe_monitor_v2 module\n");
    stop_monitoring();
    proc_remove(proc_entry_message);
    proc_remove(proc_entry_pid);
    unregister_kprobe(&kp);
    printk(KERN_INFO "unloaded cache_kprobe_monitor_v2 module\n");
}

module_init(cache_kprobe_monitor_v2_init);
module_exit(cache_kprobe_monitor_v2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sandbom");
MODULE_DESCRIPTION("perf monitor on context switch by PID sampled every second");
MODULE_VERSION("1.0");