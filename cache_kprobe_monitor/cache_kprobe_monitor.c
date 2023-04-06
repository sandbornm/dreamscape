// cache_kprobe_monitor.c
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/hashtable.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/signal.h>
#include <linux/cred.h>


static LIST_HEAD(counter_list);
static int target_pid = -1;
static char target_prog_path[256] = "";
static struct proc_dir_entry *proc_entry;

// static u32 event_codes[] = {
//     0x04, // L1 data cache misses
//     0x03, // Line fills
//     0x12, // Branch instructions
//     0x08  // Cache writes
// };

enum event_code {
    L1_DATA_CACHE_MISSES = 0x04,
    LINE_FILLS = 0x03,
    BRANCH_INSTRUCTIONS = 0x12,
    CACHE_WRITES = 0x08
};

static u32 event_codes[] = {
    L1_DATA_CACHE_MISSES,
    LINE_FILLS,
    BRANCH_INSTRUCTIONS,
    CACHE_WRITES
};

struct counter_data {
    unsigned long pc;
    u64 counters[4];
    struct list_head list;
};


static pid_t run_program(const char *path)
{
    printk("(run_program) starting %s", path);
    struct subprocess_info *sub_info;
    pid_t pid;

    sub_info = call_usermodehelper_setup(path, NULL, NULL, GFP_KERNEL, NULL, NULL, NULL);
    if (!sub_info) {
        printk(KERN_ERR "Failed to set up call to usermode helper for: %s\n", path);
        return -ENOMEM;
    }

    pid = call_usermodehelper_exec(sub_info, UMH_WAIT_PROC);
    if (pid < 0) {
        printk(KERN_ERR "Failed to execute usermode helper for: %s\n", path);
        return pid;
    }

    return pid;
}


static void write_kinfo_to_file(int pid, const char *prog_path)
{
    struct file *file;
    mm_segment_t old_fs;
    struct counter_data *entry;
    char buffer[256];
    char filename[256];
    const char *prog_name = kbasename(prog_path); // Extract the basename from the program path

    snprintf(filename, sizeof(filename), "/tmp/kinfo_output_pid_%d_%s.txt", pid, prog_name);

    file = filp_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (IS_ERR(file)) {
        printk(KERN_ERR "Failed to open file: %s (error%ld)\n", filename, PTR_ERR(file));
        return;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    list_for_each_entry(entry, &counter_list, list) {
        snprintf(buffer, sizeof(buffer), "PC: 0x%lx, L1 misses: %llu, Line fills: %llu, Branches: %llu, Cache writes: %llu\n",
            entry->pc, entry->counters[0], entry->counters[1], entry->counters[2], entry->counters[3]);
        file->f_op->write(file, buffer, strlen(buffer), &file->f_pos);
    }

    set_fs(old_fs);
    filp_close(file, NULL);
}


int is_target_pid(pid_t pid, const char *prog_path) {
    //printk(KERN_INFO "(is_target_pid) called with pid: %d and prog_path: %s\n", pid, prog_path);
    struct task_struct *task = pid_task(find_vpid(pid), PIDTYPE_PID);
    const char *task_comm;

    if (!task)
        return 0;

    task_comm = kbasename(task->comm);

    // printk(KERN_INFO " (is_target_pid) base name is: %s\n", task_comm);

    if (target_pid > 0 && pid == target_pid) {
        printk(KERN_INFO " (is_target_pid) target pid hit with value: %d\n", target_pid);
        return 1;
    }

    if (target_prog_path[0] != '\0' && strcmp(task_comm, prog_path) == 0) {
        printk(KERN_INFO " (is_target_pid) program name hit with value: %s\n", prog_path);
        return 1;
    }

    return 0;
}



static ssize_t command_handler(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
    char buf[256];
    ssize_t buf_size;
    char command[32];
    int ret;

    buf_size = min(count, (sizeof(buf) - 1));
    if (copy_from_user(buf, user_buf, buf_size))
        return -EFAULT;

    buf[buf_size] = 0;

    if (sscanf(buf, "%31s", command) != 1)
        return -EINVAL;

    if (strcmp(command, "start") == 0) {
        char path[256];
        if (sscanf(buf, "start %255s", path) == 1) {
            target_pid = run_program(path);
        } else {
            return -EINVAL;
        }
    } else if (strcmp(command, "start_pid") == 0) {
        int pid;
        if (sscanf(buf, "start_pid %d", &pid) == 1) {
            target_pid = pid;
        } else {
            return -EINVAL;
        }
    } else if (strcmp(command, "stop") == 0) {
        char prog_name[256];
        if (sscanf(buf, "stop %255s", prog_name) == 1) {
            write_kinfo_to_file(target_pid, prog_name);
        } else {
            return -EINVAL;
        }
        target_pid = -1;
    } else if (strcmp(command, "stop_pid") == 0) {
        write_kinfo_to_file(target_pid, "");
        target_pid = -1;
    } else {
        return -EINVAL;
    }

    return count;
}





static const struct file_operations target_pid_fops = {
    .write = command_handler
};






static void configure_perf_event(struct perf_event_attr *attr, u32 config)
{
    memset(attr, 0, sizeof(struct perf_event_attr));
    attr->type = PERF_TYPE_RAW;
    attr->size = sizeof(struct perf_event_attr);
    attr->config = config;
    attr->disabled = 1;
    attr->exclude_kernel = 1;
    attr->exclude_hv = 1;
    attr->exclude_idle = 1;
}

u64 read_perf_counter(struct perf_event *event) {
    u64 value, enabled, running;
    value = perf_event_read_value(event, &enabled, &running);

    printk(KERN_INFO "(read_perf_counter) counter value is: %llu\n", value);

    return value;
}

static void update_counters(unsigned long pc, u64 counter_values[])
{
    struct counter_data *entry;

    list_for_each_entry(entry, &counter_list, list) {
        if (entry->pc == pc) {
            printk("(update_counters) pc match with value: %llu", pc);
            int i;
            for (i = 0; i < 4; ++i) {
                entry->counters[i] += counter_values[i];
            }
            return;
        }
    }

    entry = kmalloc(sizeof(struct counter_data), GFP_KERNEL);
    entry->pc = pc;
    memcpy(entry->counters, counter_values, sizeof(u64) * 4);
    list_add(&entry->list, &counter_list);
}



static int pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    struct task_struct *task = current;

    if (is_target_pid(task->pid, target_prog_path)) {
        unsigned long pc = instruction_pointer(regs);
        printk(KERN_INFO "target pid match, pc value is %llu", pc);
        struct perf_event *events[4];
        u64 counter_values[64];
        int i;
        for (i = 0; i < 4; ++i) {
            struct perf_event_attr attr;
            configure_perf_event(&attr, event_codes[i]);
            events[i] = perf_event_create_kernel_counter(&attr, task->pid, NULL, NULL, NULL);
            printk(KERN_INFO "perf_event_create_kernel_counter return value is %s", events[i]);
        
            if (IS_ERR(events[i])) {
                printk(KERN_ERR "Failed to create perf_event for event_code %d\n", event_codes[i]);
                return 0;
            }
        
        }

        

        printk(KERN_DEBUG "Before perf count read loop\n");
        for (i = 0; i < 4; ++i) {
            printk(KERN_DEBUG "Event[%d]: %px\n", i, events[i]);
            counter_values[i] = read_perf_counter(events[i]);
            printk(KERN_DEBUG "counter value at i is %llu", counter_values[i]);
            perf_event_release_kernel(events[i]);
        }
        printk(KERN_INFO "After perf count read loop\n");
        update_counters(pc, counter_values);
    }

    return 0;
}

static struct kprobe kp = {
    .symbol_name = "finish_task_switch",
    .pre_handler = pre_handler,
};


static int __init cache_kprobe_module_init(void)
{
    printk(KERN_INFO "Initializing cache_kprobe_monitor\n");

    int ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "register_kprobe failed, returned %d\n", ret);
        printk(KERN_DEBUG "Failed to register kprobe for symbol: %s\n", kp.symbol_name);
        return ret;
    }

    proc_entry = proc_create("cache_kprobe_monitor", 0666, NULL, &target_pid_fops);
    if (!proc_entry) {
        printk(KERN_ERR "Failed to create proc file\n");
        unregister_kprobe(&kp);
        return -ENOMEM;
    }

    printk(KERN_INFO "Registered kprobe at %p\n", kp.addr);
    printk(KERN_INFO "Target PID: %d\n", target_pid);

    return 0;
}


static void __exit cache_kprobe_module_exit(void)
{
    unregister_kprobe(&kp);
    printk(KERN_INFO "Unregistered kprobe at %p\n", kp.addr);

    proc_remove(proc_entry);

    struct counter_data *entry, *temp;
    list_for_each_entry_safe(entry, temp, &counter_list, list) {
        printk(KERN_INFO "PC: 0x%lx, L1 misses: %llu, Line fills: %llu, Branches: %llu, Cache writes: %llu\n",
            entry->pc, entry->counters[0], entry->counters[1], entry->counters[2], entry->counters[3]);
        list_del(&entry->list);
        kfree(entry);
    }
}


module_init(cache_kprobe_module_init);
module_exit(cache_kprobe_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sandbom");
MODULE_DESCRIPTION("Performance counter cache event Kprobe kernel module");