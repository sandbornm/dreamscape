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

#include <linux/mutex.h>

static DEFINE_MUTEX(target_pid_mutex);


#define num_events 13

static LIST_HEAD(counter_list);
static int target_pid = -1;
//static char target_prog_path[256] = "";
static struct proc_dir_entry *proc_entry;

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

struct counter_data {
    unsigned long pc;
    u64 counters[num_events];
    struct list_head list;
};


// static pid_t run_program(const char *path)
// {
//     struct subprocess_info *sub_info;
//     pid_t pid;

//     sub_info = call_usermodehelper_setup(path, NULL, NULL, GFP_KERNEL, NULL, NULL, NULL);
//     if (!sub_info) {
//         printk(KERN_ERR "Failed to set up call to usermode helper for: %s\n", path);
//         return -ENOMEM;
//     }

//     pid = call_usermodehelper_exec(sub_info, UMH_WAIT_PROC);
//     if (pid < 0) {
//         printk(KERN_ERR "Failed to execute usermode helper for: %s\n", path);
//         return pid;
//     }

//     return pid;
// }


static void write_kinfo_to_file(int pid)
{
    struct file *file;
    mm_segment_t old_fs;
    struct counter_data *entry;
    char buffer[256];
    char filename[256];

    snprintf(filename, sizeof(filename), "/tmp/cache_kprobe_monitor_pid_%d.txt", pid);

    printk(KERN_INFO "(write_kinfo_to_file) Writing kinfo to %s for pid %d\n", filename, pid); 

    file = filp_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (IS_ERR(file)) {
        printk(KERN_ERR "Failed to open file: %s (error%ld)\n", filename, PTR_ERR(file));
        return;
    }

    // if (list_empty(&counter_list)) {
    //     printk(KERN_WARNING "(write_kinfo_to_file) counter_list is empty, no data to write\n");

    //     filp_close(file, NULL);
    //     return;
    // }

    // enable access to user-space memory
    old_fs = get_fs();
    set_fs(KERNEL_DS);

    // ensure we start at beginning of file
    file->f_pos = 0;


    // takes pointer to structure being iterated over, pointer to head of list, and name of struct list_head field in counter_data struct
    list_for_each_entry(entry, &counter_list, list) {
        int i;
        char temp_buffer[64];

        snprintf(buffer, sizeof(buffer), "PC: 0x%lx", entry->pc);

        // check if file->f_op->write function is null
        if (file->f_op->write) {
            file->f_op->write(file, buffer, strlen(buffer), &file->f_pos);
        } else {
            printk(KERN_ERR "(write_kinfo_to_file) File operation write is NULL\n");
        }

        for (i = 0; i < num_events; ++i) {
            snprintf(temp_buffer, sizeof(temp_buffer), ", Event %d: %llu", i, entry->counters[i]);
            file->f_op->write(file, temp_buffer, strlen(temp_buffer), &file->f_pos);
        }

        snprintf(temp_buffer, sizeof(temp_buffer), "\n");
        file->f_op->write(file, temp_buffer, strlen(temp_buffer), &file->f_pos);
    }

    // restore fs segment 
    set_fs(old_fs);
    filp_close(file, NULL);
}

// int is_target_pid(pid_t pid) {
// //const char *prog_path) {
//     struct task_struct *task = pid_task(find_vpid(pid), PIDTYPE_PID);
//     const char *task_comm;

//     if (!task) {
//         //printk(KERN_INFO "(is_target_pid) no task found!");
//         return 0;
//     }

//     task_comm = kbasename(task->comm);
//     //printk(KERN_INFO "(is_target_pid) task_comm is: %s", task_comm);

//     // cast target_pid to pid_t for comparison
//     mutex_lock(&target_pid_mutex);
//     if (target_pid > 0 && task->pid == (pid_t)target_pid) {
//         printk(KERN_INFO "(is_target_pid) pid match for %d\n", task->pid);
//         return 1;
//     }
//     mutex_unlock(&target_pid_mutex);

//     // if (target_prog_path[0] != '\0' && strcmp(task_comm, prog_path) == 0)
//     //     return 1;

//     return 0;
// }



static ssize_t command_handler(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{

    printk(KERN_INFO "(command_handler) called on proc write with count: %lu\n", (unsigned long)count);
 
    char buf[256];
    ssize_t buf_size;
    char command[32];
    int ret;

    buf_size = min(count, (sizeof(buf) - 1));
    if (copy_from_user(buf, user_buf, buf_size)) {
        return -EFAULT;
    }

    buf[buf_size] = 0;

    if (sscanf(buf, "%31s", command) != 1) {
        printk(KERN_INFO "(command_handler) EINVAL error");
        return -EINVAL;
    }

    // if (strcmp(command, "start") == 0) {
    //     char path[256];
    //     if (sscanf(buf, "start %255s", path) == 1) {
    //         target_pid = run_program(path);
    //     } else {
    //         return -EINVAL;
    //     }
    //} else 
    
    if (strcmp(command, "start_pid") == 0) {
        printk(KERN_INFO "(command_handler) start_pid command received");
        int pid;
        if (sscanf(buf, "start_pid %d", &pid) == 1) {
            mutex_lock(&target_pid_mutex);
            target_pid = pid;
            mutex_unlock(&target_pid_mutex);
            printk(KERN_INFO "(command_handler) target_pid updated to %d\n", target_pid);
        } else {
            return -EINVAL;
        }

        printk(KERN_INFO "(command_handler) start_pid command read for pid: %d\n", pid);
    // } else if (strcmp(command, "stop") == 0) {
    //     char prog_name[256];
    //     if (sscanf(buf, "stop %255s", prog_name) == 1) {
    //         write_kinfo_to_file(target_pid, prog_name);
    //     } else {
    //         return -EINVAL;
    //     }
    //     target_pid = -1;
    } else if (strcmp(command, "stop_pid") == 0) {
        printk(KERN_INFO "(command_handler) stop_pid command received for target %d\n", target_pid);
        write_kinfo_to_file(target_pid);
        mutex_lock(&target_pid_mutex);
        target_pid = -1;
        mutex_unlock(&target_pid_mutex);
    } else {
        return -EINVAL;
    }

    printk(KERN_DEBUG "(command_handler) target_pid set to %d\n", target_pid);

    return count;
}


static const struct file_operations target_pid_fops = {
    .write = command_handler,
};






static void configure_perf_event(struct perf_event_attr *attr, u32 config)
{
    printk(KERN_INFO "(configure_perf_event) with config %u\n", config);
    memset(attr, 0, sizeof(struct perf_event_attr));
    attr->type = PERF_TYPE_RAW;
    attr->size = sizeof(struct perf_event_attr);
    attr->config = config;
    attr->disabled = 1;
    attr->exclude_kernel = 1;
    attr->exclude_hv = 1;
    attr->exclude_idle = 1;

    printk(KERN_INFO "(configure_perf_event) complete with config %u\n", config);

}

u64 read_perf_counter(struct perf_event *event) {

    printk(KERN_INFO "(read_perf_counter) attempting perf_event_read_value\n");

    u64 value, enabled, running;
    value = perf_event_read_value(event, &enabled, &running);

    return value;
}

static void update_counters(unsigned long pc, u64 counter_values[])
{

    printk(KERN_INFO "(update_counters)");

    struct counter_data *entry;

    list_for_each_entry(entry, &counter_list, list) {
        if (entry->pc == pc) {
            int i;
            for (i = 0; i < num_events; ++i) {
                entry->counters[i] += counter_values[i];
            }
            return;
        }
    }

    entry = kmalloc(sizeof(struct counter_data), GFP_KERNEL);
    entry->pc = pc;
    memcpy(entry->counters, counter_values, sizeof(u64) * num_events);
    list_add(&entry->list, &counter_list);
}



static int pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    struct task_struct *task = current;
    pid_t current_pid = task_pid_nr(task); // task->pid;

    //printk(KERN_INFO "In pre-handler\n");

    // target_prog_path passed to is_target_pid previously
    //for_each_process(task) {

    mutex_lock(&target_pid_mutex);

    //printk(KERN_DEBUG "(pre_handler) current_pid is %d and target_pid is %d\n", current_pid, target_pid);

    // target_pid > 0 && 
    if (current_pid == (pid_t)target_pid) {

        printk(KERN_INFO "(pre_handler) target pid hit on %d\n", current_pid);
        unsigned long pc = regs_return_value(regs); //instruction_pointer(regs);

        printk(KERN_INFO "(pre_handler) current pc value: %llu\n", pc);
        struct perf_event *events[num_events];
        struct perf_event_attr attr;
        u64 counter_values[64];
        int i;

        printk(KERN_INFO "(pre_handler) configuring perf events...");
        for (i = 0; i < num_events; ++i) {
            configure_perf_event(&attr, event_codes[i]);
            struct perf_event * ev = perf_event_create_kernel_counter(&attr, current_pid, NULL, NULL, NULL);
            
            // ensure perf event was created 
            if (IS_ERR(ev)) {
                printk(KERN_INFO "(pre_handler) Failed to create perf event: %ld\n", PTR_ERR(ev));
                return PTR_ERR(ev);
            }
            events[i] = ev; 
        
        }

        for (i = 0; i < num_events; ++i) {
            counter_values[i] = read_perf_counter(events[i]);
            perf_event_release_kernel(events[i]);
        }

        update_counters(pc, counter_values);
    }
    mutex_unlock(&target_pid_mutex);


    return 0;
}

static struct kprobe kp = {
    .symbol_name = "finish_task_switch",
    .pre_handler = pre_handler,
};


static int __init perf_cache_kprobe_module_init(void)
{
    printk(KERN_INFO "Initializing cache_kprobe_monitor\n");

    int ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "register_kprobe failed, returned %d\n", ret);
        printk(KERN_INFO "Failed to register kprobe for symbol: %s\n", kp.symbol_name);
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


static void __exit perf_cache_kprobe_module_exit(void)
{
    unregister_kprobe(&kp);
    printk(KERN_INFO "Unregistered kprobe at %p\n", kp.addr);

    proc_remove(proc_entry);

    struct counter_data *entry, *temp;
    list_for_each_entry_safe(entry, temp, &counter_list, list) {
        // printk(KERN_INFO "PC: 0x%lx, L1 misses: %llu, Line fills: %llu, Branches: %llu, Cache writes: %llu\n",
        //     entry->pc, entry->counters[0], entry->counters[1], entry->counters[2], entry->counters[3]);
        list_del(&entry->list);
        kfree(entry);
    }
}


module_init(perf_cache_kprobe_module_init);
module_exit(perf_cache_kprobe_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sandbom");
MODULE_DESCRIPTION("Performance counter cache event Kprobe kernel module");
