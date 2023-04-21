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

// hold counter values for proc file
// run utils/read_proc_message before unloading module to get results
// written to /tmp/cache_kmv2_message.txt
#define BUFFER_SIZE 8192

static char data_buffer[BUFFER_SIZE];
static size_t buffer_pos = 0;
static DEFINE_MUTEX(buffer_mutex);




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


// Modify the sample_perf_counters function
static enum hrtimer_restart sample_perf_counters(struct hrtimer *timer)
{
    u64 enabled, running, new_value, delta;
    u64 counter_values[NUM_EVENTS];
    int i;
    u64 program_counter;

    if (monitored_task && pid_alive(monitored_task)) {

        printk(KERN_INFO "(sample_perf_counters) Monitored task is alive (PID: %d), reading the counter values\n", monitored_task->pid);

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

        // Write the counter values to the buffer
        mutex_lock(&buffer_mutex);
        int bytes_written = snprintf(data_buffer + buffer_pos, BUFFER_SIZE - buffer_pos,
                                      "%d,%llu", target_pid, program_counter);

        for (i = 0; i < NUM_EVENTS; i++) {
            bytes_written += snprintf(data_buffer + buffer_pos + bytes_written, BUFFER_SIZE - buffer_pos - bytes_written, ",%llu", counter_values[i]);
        }
        bytes_written += snprintf(data_buffer + buffer_pos + bytes_written, BUFFER_SIZE - buffer_pos - bytes_written, "\n");

        if (bytes_written > 0 && bytes_written < BUFFER_SIZE - buffer_pos) {
            buffer_pos += bytes_written;
        } else {
            printk(KERN_ERR "Buffer overflow or error in writing counter values");
        }

        mutex_unlock(&buffer_mutex);

    // Call the function to write the counter values to a file
        //write_counters_to_file(counter_values);

        hrtimer_forward_now(timer, sample_interval);
        return HRTIMER_RESTART;
    } else {
        printk(KERN_INFO "Monitored task terminated (PID: %d), stopping the timer\n", monitored_task->pid);
        return HRTIMER_NORESTART;
    }
}



// static enum hrtimer_restart dummy_callback(struct hrtimer *timer)
// {
//     printk(KERN_INFO "Dummy timer callback executed\n");
//     hrtimer_forward_now(timer, sample_interval);
//     return HRTIMER_RESTART;
// }


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

    //timeout_interval = ktime_set(3, 0); // Set the timeout interval to 3 seconds for testing
    //hrtimer_init(&timeout_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    //timeout_timer.function = timeout_callback;
    //hrtimer_start(&timeout_timer, timeout_interval, HRTIMER_MODE_REL);
}

static void stop_monitoring(void)
{
    //hrtimer_cancel(&timeout_timer);
    hrtimer_cancel(&sample_timer);
    cleanup_perf_counters(perf_data);
    put_task_struct(monitored_task);
    monitored_task = NULL;
}

static enum hrtimer_restart timeout_callback(struct hrtimer *timer) {
    printk(KERN_INFO "Timeout reached, stopping the performance counter sampling\n");
    stop_monitoring();
    return HRTIMER_NORESTART;
}


/*

PID and file handling


*/


// dummy function to test kprobe behavior
// static int hello_switch(struct kprobe *p, struct pt_regs *regs)
// {
//     struct task_struct *next_task = current;

//     if (next_task && next_task->pid == target_pid)
//     {
//         //printk(KERN_INFO "hello world %d\n", target_pid);

//         spin_lock(&buffer_lock);
//         snprintf(message_buffer, sizeof(message_buffer), "hello world %d\n", target_pid);
//         spin_unlock(&buffer_lock);

//     }

//     return 0;
// }


// static struct kprobe kp = {
//     .symbol_name = "finish_task_switch",
//     .pre_handler = hello_switch,
// };


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

    // stop monitoring of previous task
    if (monitored_task) {
        printk(KERN_INFO "Stopping monitoring of previous process (PID: %d)\n", monitored_task->pid);
        stop_monitoring();
    }

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

    // ret = register_kprobe(&kp);
    // if (ret < 0)
    // {
    //     printk(KERN_INFO "register_kprobe failed, returned %d\n", ret);
    //     return ret;
    // }

    // was hello_switch_pid
    proc_entry_pid = proc_create("cache_kmv2_pid", 0200, NULL, &pid_fops);
    if (!proc_entry_pid)
    {
        printk(KERN_ERR "Error creating proc entry, exiting");
        //unregister_kprobe(&kp);
        proc_remove(proc_entry_pid);
        return -ENOMEM;
    }

    // was hello_switch_message
    proc_entry_message = proc_create("cache_kmv2_message", 0400, NULL, &message_fops);
    if (!proc_entry_message)
    {
        printk(KERN_ERR "Error creating proc entry for message, exiting");
        //unregister_kprobe(&kp);
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
    //unregister_kprobe(&kp);
    printk(KERN_INFO "unloaded cache_kprobe_monitor_v2 module\n");
}

module_init(cache_kprobe_monitor_v2_init);
module_exit(cache_kprobe_monitor_v2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sandbom");
MODULE_DESCRIPTION("perf monitor on context switch by PID sampled every second");
MODULE_VERSION("1.0");