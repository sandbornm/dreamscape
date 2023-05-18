This repository contains code and data for DREAMSCAPE: Dynamic cache Replacement policy for Evading ARM Side Channels by Aggregating Performance Events

# Context
- `gcc --version = gcc (Ubuntu 9.4.0-1ubuntu1~20.04.1) 9.4.0`

- `uname -a = Linux ubuntu 5.4.0-1069-raspi #79-Ubuntu SMP PREEMPT Thu Aug 18 18:15:22 UTC 2022 aarch64 aarch64 aarch64 GNU/Linux`

- `cat /proc/cpuinfo`
```
processor       : 0
BogoMIPS        : 38.40
Features        : fp asimd evtstrm crc32 cpuid
CPU implementer : 0x41
CPU architecture: 8
CPU variant     : 0x0
CPU part        : 0xd03
CPU revision    : 4

processor       : 1
BogoMIPS        : 38.40
Features        : fp asimd evtstrm crc32 cpuid
CPU implementer : 0x41
CPU architecture: 8
CPU variant     : 0x0
CPU part        : 0xd03
CPU revision    : 4

processor       : 2
BogoMIPS        : 38.40
Features        : fp asimd evtstrm crc32 cpuid
CPU implementer : 0x41
CPU architecture: 8
CPU variant     : 0x0
CPU part        : 0xd03
CPU revision    : 4

processor       : 3
BogoMIPS        : 38.40
Features        : fp asimd evtstrm crc32 cpuid
CPU implementer : 0x41
CPU architecture: 8
CPU variant     : 0x0
CPU part        : 0xd03
CPU revision    : 4

Hardware        : BCM2835
Revision        : a020d3
Serial          : 000000004fea7bcb
Model           : Raspberry Pi 3 Model B Plus Rev 1.3
```

- `file arm_pmu_experiments/pmu_cycle_count`
```
arm_pmu_experiments/pmu_cycle_count: ELF 64-bit LSB shared object, ARM aarch64, version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-aarch64.so.1, BuildID[sha1]=0b3ef6ba583860f3c65cb89c2f0ca24073623194, for GNU/Linux 3.7.0, not stripped
```

## arm_pmu_experiments
contains example c code and binaries for enabling PMU for cache miss and cycle counts

## pmu_kernel_module
kernel module to track L1 I and D caches for misses and refills

## pmu_utils
basic examples of enabling and reading from PMU registers

# Dataset
![JPL ALLSTAR Binaries](https://allstar.jhuapl.edu/)

# QEMU setup

to test the kernel module, QEMU was used based partly on this tutorial:

https://translatedcode.wordpress.com/2017/07/24/installing-debian-on-qemus-64-bit-arm-virt-board/

the command to run the setup is:

```
qemu-system-aarch64 -M virt -m 1024 -cpu cortex-a53 \
  -kernel vmlinuz-4.19.0-23-arm64 \
  -initrd initrd.img-4.19.0-23-arm64 \
  -append 'root=/dev/vda2' \
  -drive if=none,file=hda.qcow2,format=qcow2,id=hd \
  -device virtio-blk-pci,drive=hd \
  -netdev user,id=mynet \
  -device virtio-net-pci,netdev=mynet \
  -nographic
  ```

The QEMU image emulates a single core ARM Cortex A53:

`$ uname -a`
`Linux debian 4.19.0-23-arm64 #1 SMP Debian 4.19.269-1 (2022-12-20) aarch64 GNU/Linux`

`$ cat /proc/cpuinfo`

```
processor	: 0
BogoMIPS	: 125.00
Features	: fp asimd evtstrm aes pmull sha1 sha2 crc32 cpuid
CPU implementer	: 0x41
CPU architecture: 8
CPU variant	: 0x0
CPU part	: 0xd03
CPU revision	: 4
```

View [module commands](./module_commands.md) for instructions on running this module

## Usage: 

The `test_mod.sh` script takes the name of a binary under `dreamscape/test_bins/bins/arm64`; the name of a binary in this folder is sufficient. This script will run the binary on cpu core 0 and wait for it to terminate. During execution, the delta of the performance counter values for a number of hardware events will be recorded, keyed by the program counter value. Upon program termination, these counter values which were printk'd to dmesg, will be written to a text file with the name of the program by its pid indicated in the filename. Additionally, this script will write the name of the program along with its pid to the file called `pid_bin_record` in the `...monitor_v3` folder.

The `data/dmesg_dumps/dmesg_parser.py` is a python script that will convert all .txt files without corresponding .json files into a file with the following structure:

```

 "2973": [
        {
            "timestamp": 401.755036,
            "event_name": "L1D_CACHE_REFILL",
            "counter_value": 2387,
            "delta": 2387,
            "program_counter": 281473327092708
        },
        {
            "timestamp": 401.755048,
            "event_name": "L1D_CACHE_ACCESS",
            "counter_value": 28499087,
            "delta": 28499087,
            "program_counter": 281473327092708
        },

```

with the pid as the key and the timestamp, event name, current counter value, change from last reading, and program counter value including in the keyed dictionary.

the script `data/dmesg_dumps/draw_figs_all.py` will produce html figures depicting the program counter changes in an interactive plot that can be viewed in browser. These figures are in the `figs/` directory of the dmesg\_dumps subdirectory.

 
