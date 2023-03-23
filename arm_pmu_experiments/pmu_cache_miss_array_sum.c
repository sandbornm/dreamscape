#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    int ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    return ret;
}

int main(int argc, char **argv)
{
    struct perf_event_attr pe;
    long long count;
    int fd;

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_RAW;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = 0x03;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1)
    {
        fprintf(stderr, "Error opening perf event\n");
        exit(EXIT_FAILURE);
    }

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

    /* Toy example: Sum the elements of an array */
    int array_size = 100000;
    int *array = (int *)malloc(array_size * sizeof(int));
    int sum = 0;

    for (int i = 0; i < array_size; i++)
    {
        array[i] = i;
    }

    for (int i = 0; i < array_size; i++)
    {
        sum += array[i];
    }

    free(array);

    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    read(fd, &count, sizeof(long long));

    printf("Cache misses: %lld\n", count);
    printf("Sum: %d\n", sum);

    close(fd);
    return 0;
}

