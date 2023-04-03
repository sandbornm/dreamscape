// cache_kprobe_monitor.c
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/hashtable.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/signal.h>
#include <linux/cred.h>


static LIST_HEAD(counter_list);
static int target_pid = -1;
static struct dentry *debugfs_entry;

static ssize_t target_pid_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
    char buf[32];
    ssize_t buf_size;
    struct task_struct *task;
    struct pid *pid_struct;

    buf_size = min(count, (sizeof(buf) - 1));
    if (copy_from_user(buf, user_buf, buf_size))
        return -EFAULT;

    buf[buf_size] = 0;

    if (kstrtoint(buf, 10, &target_pid) != 0)
        return -EINVAL;

    pid_struct = find_get_pid(target_pid);
    task = get_pid_task(pid_struct, PIDTYPE_PID);

    if (task) {
        send_sig_info(SIGCONT, SEND_SIG_FORCED, task); // Resume the process
        put_task_struct(task);
    } else {
        return -ESRCH;
    }

    return count;
}

static const struct file_operations target_pid_fops = {
    .write = target_pid_write,
};

static bool is_target_pid(pid_t pid)
{
    return pid == target_pid;
}

static u32 event_codes[] = {
    0x04, // L1 data cache misses
    0x03, // Line fills
    0x12, // Branch instructions
    0x08  // Cache writes
};

struct counter_data {
    unsigned long pc;
    u64 counters[4];
    struct list_head list;
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

    return value;
}

static void update_counters(unsigned long pc, u64 counter_values[])
{
    struct counter_data *entry;

    list_for_each_entry(entry, &counter_list, list) {
        if (entry->pc == pc) {
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

    if (is_target_pid(task->pid)) {
        unsigned long pc = instruction_pointer(regs);
        struct perf_event *events[4];
        struct perf_event_attr attr;
        u64 counter_values[64];
        int i;
        for (i = 0; i < 4; ++i) {
            configure_perf_event(&attr, event_codes[i]);
            events[i] = perf_event_create_kernel_counter(&attr, task->pid, NULL, NULL, NULL);
        }

        for (i = 0; i < 4; ++i) {
            counter_values[i] = read_perf_counter(events[i]);
            perf_event_release_kernel(events[i]);
        }

        update_counters(pc, counter_values);
    }

    return 0;
}

static void write_kinfo_to_file(const char *filename)
{
    struct file *file;
    mm_segment_t old_fs;
    struct counter_data *entry;
    char buffer[256];

    file = filp_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (IS_ERR(file)) {
        printk(KERN_ERR "Failed to open file: %s\n", filename);
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

static struct kprobe kp = {
    .symbol_name = "finish_task_switch",
    .pre_handler = pre_handler,
};


static int __init perf_cache_kprobe_module_init(void)
{
    printk(KERN_INFO "Initializing perf_cache_kprobe_module\n");

    int ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "register_kprobe failed, returned %d\n", ret);
        printk(KERN_INFO "Failed to register kprobe for symbol: %s\n", kp.symbol_name);
        return ret;
    }

    printk(KERN_INFO "Registered kprobe at %p\n", kp.addr);
    printk(KERN_INFO "Target PID: %d\n", target_pid);

    return 0;
}


static void __exit perf_cache_kprobe_module_exit(void)
{
    unregister_kprobe(&kp);
    printk(KERN_INFO "Unregistered kprobe at %p\n", kp.addr);

    debugfs_remove(debugfs_entry);

    struct counter_data *entry, *temp;
    list_for_each_entry_safe(entry, temp, &counter_list, list) {
        printk(KERN_INFO "PC: 0x%lx, L1 misses: %llu, Line fills: %llu, Branches: %llu, Cache writes: %llu\n",
            entry->pc, entry->counters[0], entry->counters[1], entry->counters[2], entry->counters[3]);
        list_del(&entry->list);
        kfree(entry);
    }

    write_kinfo_to_file("/tmp/kinfo_output.txt");
}


module_init(perf_cache_kprobe_module_init);
module_exit(perf_cache_kprobe_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sandbom");
MODULE_DESCRIPTION("Performance counter cache event Kprobe kernel module");
