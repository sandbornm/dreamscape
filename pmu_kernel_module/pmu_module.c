#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>


// ARM Cortex-A53 PMCR register
//#define ARMV8_PMCR_MASK 0x3f
#define ARMV8_PMCR_E    (1 << 0) // Enable all counters
//#define ARMV8_PMCR_P    (1 << 1) // Reset all counters
//#define ARMV8_PMCR_C    (1 << 2) // Clock counter reset
//#define ARMV8_PMCR_D    (1 << 3) // Count divider
//#define ARMV8_PMCR_X    (1 << 4) // Export of events
//#define ARMV8_PMCR_DP   (1 << 5) // Disable CCNT if non-invasive debug is prohibited
//#define ARMV8_PMCR_LC   (1 << 6) // Long cycle count enable
//#define ARMV8_PMCR_N_SHIFT 11 // Number of counters implemented
//#define ARMV8_PMCR_N_MASK 0x1f

// ARM Cortex-A53 event codes
#define ARMV8_EVENT_L1D_CACHE_ACCESS 0x04
#define ARMV8_EVENT_L1D_CACHE_REFILL 0x03 // aka miss
#define ARMV8_EVENT_L1I_CACHE_REFILL 0x01
#define ARMV8_EVENT_L1I_CACHE_ACCESS 0x14

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sandbom");
MODULE_DESCRIPTION("A kernel module for directly accessing PMU on ARM Cortex-A53");
MODULE_VERSION("1.0");

struct pmu_counters {
    u64 l1_data_cache_access;
    u64 l1_data_cache_refill;
    u64 l1_inst_cache_access;
    u64 l1_inst_cache_refill;

} pmu_counters;

static int pmu_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "L1 data cache access: %llu\n", pmu_counters.l1_data_cache_access);
    seq_printf(m, "L1 data cache refill: %llu\n", pmu_counters.l1_data_cache_refill);
    seq_printf(m, "L1 instruction cache access: %llu\n", pmu_counters.l1_inst_cache_access);
    seq_printf(m, "L1 instruction cache refill: %llu\n", pmu_counters.l1_inst_cache_refill);
    
    return 0;
}

static int pmu_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, pmu_proc_show, NULL);
}

static const struct file_operations pmu_proc_fops = {
    .owner      = THIS_MODULE,
    .open       = pmu_proc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};


static void configure_event_counter(u32 counter_index, u32 event_code)
{
    // Select the counter
    asm volatile("msr pmselr_el0, %0" ::"r"(counter_index));

    // Set the event type
    asm volatile("msr pmxevtyper_el0, %0" ::"r"(event_code));

    // Reset the event counter
    asm volatile("msr pmxevcntr_el0, %0" ::"r"(0));

    // Enable the event counter
    u64 counter_set_bit = 1ULL << counter_index;
    asm volatile("msr pmcntenset_el0, %0" : : "r" (counter_set_bit));
}


static int __init pmu_module_init(void)
{
    // Performance Monitors User Enable Register
    asm volatile("msr pmuserenr_el0, %0" : : "r"((u64)SYS_PMUSERENR_EL0));

    // Performance Monitors Count Enable Set register
    asm volatile("msr pmcntenset_el0, %0" : : "r" (SYS_PMCNTENSET_EL0));

    
    u64 val=0;
    // Read Performance Monitor Control Register 
    asm volatile("mrs %0, pmcr_el0" : "=r" (val));
    // Enable all counters and write configuration to PMCR
    asm volatile("msr pmcr_el0, %0" : : "r" (val|ARMV8_PMCR_E));


    // Configure the event counter 0 for level 1 data cache access
    configure_event_counter(0, ARMV8_EVENT_L1D_CACHE_ACCESS);

    // Configure the event counter 1 for level 1 data cache refill (miss)
    configure_event_counter(1, ARMV8_EVENT_L1D_CACHE_REFILL);

    // Configure the event counter 2 for level 1 instruction cache access
    configure_event_counter(2, ARMV8_EVENT_L1I_CACHE_ACCESS);

    // Configure the event counter 3 for level 1 instruction cache refill (miss)
    configure_event_counter(3, ARMV8_EVENT_L1I_CACHE_REFILL);


    // Create a proc entry
    struct proc_dir_entry *pmu_proc_entry;
    pmu_proc_entry = proc_create("pmu_counters", 0, NULL, &pmu_proc_fops);
    if (!pmu_proc_entry) {
        return -ENOMEM;
    }

    printk(KERN_INFO "PMU kernel module loaded\n");
    return 0;
}

static void __exit pmu_module_exit(void)
{
    
    u32 pmcr_val = 0;
    u64 counter_value_daccess = 0;
    u64 counter_value_drefill = 0;
    u64 counter_value_iaccess = 0;
    u64 counter_value_irefill = 0;

    // Read the value of the PMU counter 0 (level 1 data cache access)
    asm volatile("msr pmselr_el0, %0" ::"r"(0));
    asm volatile("mrs %0, pmxevcntr_el0" : "=r"(counter_value_daccess));

    // Read the value of the PMU counter 1 (level 1 data cache refill)
    asm volatile("msr pmselr_el0, %0" ::"r"(1));
    asm volatile("mrs %0, pmxevcntr_el0" : "=r"(counter_value_drefill));

    // Read the value of the PMU counter 0 (level 1 data cache access)
    asm volatile("msr pmselr_el0, %0" ::"r"(2));
    asm volatile("mrs %0, pmxevcntr_el0" : "=r"(counter_value_iaccess));

    // Read the value of the PMU counter 1 (level 1 data cache refill)
    asm volatile("msr pmselr_el0, %0" ::"r"(3));
    asm volatile("mrs %0, pmxevcntr_el0" : "=r"(counter_value_irefill));


    printk(KERN_INFO "PMU counter value (L1 data cache access): %llu\n", counter_value_daccess);
    printk(KERN_INFO "PMU counter value (L1 data cache refill): %llu\n", counter_value_drefill);
    printk(KERN_INFO "PMU counter value (L1 instruction cache access): %llu\n", counter_value_iaccess);
    printk(KERN_INFO "PMU counter value (L1 instruction cache refill): %llu\n", counter_value_irefill);

    // Read the PMU counter value
    asm volatile("mrs %0, pmcr_el0" : "=r"(pmcr_val));

    // Disable the PMU
    pmcr_val &= ~ARMV8_PMCR_E;
    asm volatile("msr pmcr_el0, %0" ::"r"(pmcr_val));

    pmu_counters.l1_data_cache_access = counter_value_daccess;
    pmu_counters.l1_data_cache_refill = counter_value_drefill;
    pmu_counters.l1_inst_cache_access = counter_value_iaccess;
    pmu_counters.l1_inst_cache_refill = counter_value_irefill;

    remove_proc_entry("pmu_counters", NULL);

    printk(KERN_INFO "PMU kernel module unloaded\n");
}

module_init(pmu_module_init);
module_exit(pmu_module_exit);
