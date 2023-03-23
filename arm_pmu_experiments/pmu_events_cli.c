#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <getopt.h>
#include <string.h>


static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    int ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    return ret;
}

int is_valid_perf_event(unsigned int event)
{
    switch (event)
    {
    case 0x03: // L1D_CACHE_REFILL
    case 0x04: // L1D_CACHE
    case 0x05: // L1D_TLB_REFILL
        return 1;
    default:
        return 0;
    }
}

// char * get_event_name_from_code(const char * event_code)
// {
//     if (strcmp(event_code, "0x03") == 0)
//     {
//         return "L1D_CACHE_REFILL";
//     }
//     else {
//         return "unknown event";
//     }
// }

int main(int argc, char **argv)
{
    struct perf_event_attr pe;
    long long count;
    int fd;
    int option;
    unsigned int perf_event = 0;
    char *binary_path = NULL;

    while ((option = getopt(argc, argv, "p:e:")) != -1)
    {
        switch (option)
        {
        case 'p':
            binary_path = optarg;
            break;
        case 'e':
            perf_event = (unsigned int)strtoul(optarg, NULL, 0);
            break;
        default:
            fprintf(stderr, "Usage: %s -p /path/to/binary -e perf_event\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (!binary_path || !perf_event || !is_valid_perf_event(perf_event))
    {
        fprintf(stderr, "Usage: %s -p /path/to/binary -e perf_event\n", argv[0]);
        fprintf(stderr, "Valid perf_event values (hexadecimal) for ARM Cortex A53:\n");
        fprintf(stderr, "  0x03 : L1D_CACHE_REFILL\n");
        fprintf(stderr, "  0x04 : L1D_CACHE\n");
        fprintf(stderr, "  0x05 : L1D_TLB_REFILL\n");
        exit(EXIT_FAILURE);
    }

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_RAW;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = perf_event;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1)
        {
            fprintf(stderr, "Error opening perf event\n");
            exit(EXIT_FAILURE);
        }
    pid_t child_pid = fork();

    if (child_pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (child_pid == 0) // Child process
    {
        printf("in child process\n");

        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

    }
    else // Parent process
    {
        printf("in parent process\n");
        int status;
        waitpid(child_pid, &status, 0);

        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        read(fd, &count, sizeof(long long));

        printf("event code %u cache event count: %lld\n", perf_event, count);

        close(fd);
    }

    return 0;
}
