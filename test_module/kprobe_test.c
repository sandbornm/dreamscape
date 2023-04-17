#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/kprobes.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

// #define TARGET_PID 2081

static int target_pid = -1;

static struct proc_dir_entry *proc_entry_pid;
static struct proc_dir_entry *proc_entry_message;

static char message_buffer[512];
static DEFINE_SPINLOCK(buffer_lock);



static int hello_switch(struct kprobe *p, struct pt_regs *regs)
{
    struct task_struct *next_task = current;

    if (next_task && next_task->pid == target_pid)
    {
        printk(KERN_INFO "hello world %d\n", target_pid);

        spin_lock(&buffer_lock);
        snprintf(message_buffer, sizeof(message_buffer), "hello world %d\n", target_pid);
        spin_unlock(&buffer_lock);

        // char filename[64];
        // snprintf(filename, sizeof(filename), "/tmp/hello_%d.txt", target_pid);
        // struct file *file = filp_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        // if (IS_ERR(file))
        // {
        //     printk(KERN_ERR "Error opening file: %ld\n", PTR_ERR(file));
        //     return 0;
        // }

        // char buffer[64];
        // int len = snprintf(buffer, sizeof(buffer), "hello world %d\n", target_pid);
        // mm_segment_t oldfs = get_fs();
        // set_fs(KERNEL_DS);
        // file->f_op->write(file, buffer, len, &file->f_pos);
        // set_fs(oldfs);

        // filp_close(file, NULL);

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


static int __init hello_switch_init(void)
{
    int ret;

    ret = register_kprobe(&kp);
    if (ret < 0)
    {
        printk(KERN_INFO "register_kprobe failed, returned %d\n", ret);
        return ret;
    }

    proc_entry_pid = proc_create("hello_switch_pid", 0200, NULL, &pid_fops);
    if (!proc_entry_pid)
    {
        printk(KERN_ERR "Error creating proc entry, exiting");
        unregister_kprobe(&kp);
        return -ENOMEM;
    }

    proc_entry_message = proc_create("hello_switch_message", 0400, NULL, &message_fops);
    if (!proc_entry_message)
    {
        printk(KERN_ERR "Error creating proc entry for message, exiting");
        unregister_kprobe(&kp);
        proc_remove(proc_entry_message);
        return -ENOMEM;
    }

    printk(KERN_INFO "Proc entries created, write PID to /proc/hello_switch_pid to start\n");
    printk(KERN_INFO "hello_switch module loaded\n");
    return 0;
}

static void __exit hello_switch_exit(void)
{
    proc_remove(proc_entry_message);
    proc_remove(proc_entry_pid);
    unregister_kprobe(&kp);
    printk(KERN_INFO "hello_switch module unloaded\n");
}

module_init(hello_switch_init);
module_exit(hello_switch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Hello world on context switch for PID specified by user in proc file");
MODULE_VERSION("1.0");