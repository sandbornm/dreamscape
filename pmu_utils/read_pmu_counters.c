#include <stdio.h>
#include <stdint.h>

int main() {
    FILE *f = fopen("/proc/pmu_counters", "r");
    if (!f) {
        perror("Error opening /proc/pmu_counters");
        return 1;
    }

    uint64_t l1_data_cache_access, l1_data_cache_refill;
    fscanf(f, "L1 data cache access: %llu\n", &l1_data_cache_access);
    fscanf(f, "L1 data cache refill: %llu\n", &l1_data_cache_refill);
    fclose(f);

    printf("L1 data cache access: %llu\n", l1_data_cache_access);
    printf("L1 data cache refill: %llu\n", l1_data_cache_refill);

    return 0;
}
