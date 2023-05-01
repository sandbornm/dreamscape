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


//https://elixir.bootlin.com/linux/v5.4/source/arch/arm64/include/asm/perf_event.h
// https://developer.arm.com/documentation/ddi0500/latest/ p.12-36
enum event_code {
    //L1I_CACHE_REFILL = 0x01,
    //L1I_TLB_REFILL = 0x02,
    L1D_CACHE_REFILL = 0x03,
    L1D_CACHE_ACCESS = 0x04,
    L1D_TLB_REFILL = 0x05,
    LD_RETIRED = 0x06,
    ST_RETIRED = 0x07,
    INST_RETIRED = 0x08,
    CPU_CYCLES = 0x11,
    BR_PRED = 0x12,
    MEM_ACCESS = 0x13,
    //L1I_CACHE = 0x14,
    L1D_CACHE_WB = 0x15,
    BUS_ACCESS = 0x19,
};

static u32 event_codes[] = {
    //L1I_CACHE_REFILL,
    //L1I_TLB_REFILL,
    L1D_CACHE_REFILL,
    L1D_CACHE_ACCESS,
    L1D_TLB_REFILL,
    LD_RETIRED,
    ST_RETIRED,
    INST_RETIRED,
    CPU_CYCLES,
    BR_PRED,
    MEM_ACCESS,
    //L1I_CACHE,
    L1D_CACHE_WB,
    BUS_ACCESS,
};

#define INVALID_PID (-1)

static int target_pid = INVALID_PID;
static struct perf_counter_data *perf_data;
static struct task_struct *monitored_task = NULL;
static struct proc_dir_entry *proc_entry_pid;

#define NUM_EVENTS (sizeof(event_codes) / sizeof(event_codes[0]))

struct perf_counter_data {
    struct perf_event *events[NUM_EVENTS];
    u64 prev_values[NUM_EVENTS];
};

static struct perf_counter_data *init_perf_counters(pid_t pid)
{
    struct perf_counter_data *perf_data;
    struct perf_event_attr attr;
    int cpu = 0; // hardcode cpu 0 for now
    size_t i;

    // Allocate memory for the perf_counter_data array
    perf_data = kzalloc(NUM_EVENTS * sizeof(struct perf_counter_data), GFP_KERNEL);
    if (!perf_data) {
        printk(KERN_ERR "(init_perf_counters) kzalloc failed\n");
        return NULL;
    }

    printk(KERN_INFO "(init_perf_counters) kzalloc success\n");

    //cpu = get_cpu();
    printk(KERN_INFO "(init_perf_counters) creating perf counters\n");
    for (i = 0; i < NUM_EVENTS; ++i) {

        // Initialize the perf_event_attr structure for the current event code
        memset(&attr, 0, sizeof(struct perf_event_attr));
        attr.type = PERF_TYPE_RAW;
        attr.size = sizeof(struct perf_event_attr);
        attr.config = event_codes[i];  // Set the event code
        attr.exclude_kernel = 1;
        attr.inherit = 0;

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

        printk(KERN_INFO "(init_perf_counters) perf_event_create_kernel_counter succeeded for event code %u\n", event_codes[i]);
        perf_data->prev_values[i] = 0;
    }
    //put_cpu();

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

ssize_t pid_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    int new_pid;
    char temp[32];

    if (count > sizeof(temp) - 1)
        return -EINVAL;

    if (copy_from_user(temp, buf, count))
        return -EFAULT;

    temp[count] = '\0';
    sscanf(temp, "%d", &new_pid);

    // If there is an existing target PID, clean up its perf_data
    if (target_pid != INVALID_PID && perf_data) {
        cleanup_perf_counters(perf_data);
        perf_data = NULL;
    }

    // Set the new target_pid
    target_pid = new_pid;

    printk(KERN_INFO " (pid_proc_write) new target_pid: %d\n", target_pid);

    // If the new target_pid is not -1, initialize the perf_data for it
    if (target_pid != INVALID_PID) {

        monitored_task = get_pid_task(find_vpid(target_pid), PIDTYPE_PID);
        if (!monitored_task) {
            printk(KERN_ERR "get_pid_task failed\n");
            target_pid = INVALID_PID;
            return -EFAULT;
        }

        perf_data = init_perf_counters(target_pid);
        if (!perf_data) {
            printk(KERN_ERR "init_perf_counters failed\n");
            target_pid = INVALID_PID;
            return -EFAULT;
        }
    }

    return count;
}

int finish_task_switch_handler(struct kprobe *p, struct pt_regs *regs) {

    u64 enabled, running, new_value, delta;
    u64 counter_values[NUM_EVENTS];
    int i;
    u64 program_counter;

    struct task_struct *next = container_of((void *)regs->regs[0], struct task_struct, thread.cpu_context);

    if (current->pid == target_pid || next->pid == target_pid) {

        printk(KERN_INFO "(finish_task_switch_handler) target program hit (PID: %d), reading counter values\n", monitored_task->pid);

        //struct pt_regs *regs = task_pt_regs(monitored_task);
        struct task_struct *target_task = (current->pid == target_pid) ? current : next;
        struct pt_regs *target_regs = task_pt_regs(target_task);
        program_counter = target_regs->pc;

        printk(KERN_INFO "(finish_task_switch_handler) current program counter: %llu\n", program_counter);

        // print whether the target pid is next or prev
        if (next->pid == target_pid) {
            printk(KERN_INFO "(finish_task_switch_handler) target pid is next\n");
        } else {
            printk(KERN_INFO "(finish_task_switch_handler) target pid is prev\n");
        }

        for (i = 0; i < NUM_EVENTS; i++) {
            new_value = perf_event_read_value(perf_data->events[i], &enabled, &running);
            
            if (new_value < 0) {
                printk(KERN_ERR "(finish_task_switch_handler) perf_event_read_value failed for event code: %u\n", event_codes[i]);
                return -EFAULT;
            } else {
                delta = new_value - perf_data->prev_values[i];
                printk(KERN_INFO "(finish_task_switch_handler) event code: %u  ; counter value: %llu, delta: %llu\n", event_codes[i], new_value, delta);
                counter_values[i] = delta;
                perf_data->prev_values[i] = new_value;
            }
        }
    }
    return 0;
}

static const struct file_operations pid_proc_fops = {
    .write = pid_proc_write,
};

 // Setup kprobe
static struct kprobe kp = {
    .symbol_name = "finish_task_switch",
    .pre_handler = finish_task_switch_handler,
};

// Initialize the kernel module
static int __init cache_kprobe_monitor_v3_init(void) {
    // Create the proc file
    proc_create("cache_kmv3_pid", 0644, NULL, &pid_proc_fops);

    printk(KERN_INFO " (cache_kprobe_monitor_v3_init) Initializing cache_kprobe_monitor_v3\n");

    // Register kprobe
    int ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("register_kprobe failed, returned %d\n", ret);
        return ret;
    }
    pr_info("Kprobe registered\n");

    return 0;
}

// Cleanup the kernel module
static void __exit cache_kprobe_monitor_v3_exit(void) {
    // Remove the proc file
    remove_proc_entry("cache_kmv3_pid", NULL);

    printk(KERN_INFO " (cache_kprobe_monitor_v3_exit) Cleaning up cache_kprobe_monitor_v3\n");
    
    // Unregister kprobe
    unregister_kprobe(&kp);
    pr_info("Kprobe unregistered\n");

    // Clean up perf_data if it exists
    if (perf_data) {
        cleanup_perf_counters(perf_data);
        perf_data = NULL;
    }
}

module_init(cache_kprobe_monitor_v3_init);
module_exit(cache_kprobe_monitor_v3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sandbom");
MODULE_DESCRIPTION("perf monitor on context switch by PID sample deltas");
MODULE_VERSION("3.0");



