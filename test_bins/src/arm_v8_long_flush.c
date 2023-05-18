#include <stdio.h>
#include <time.h>

static inline void arm_v8_flush(void* address)
{
    asm volatile ("DC CIVAC, %0" :: "r"(address));
    asm volatile ("DSB ISH");
    asm volatile ("ISB");
}

static inline void arm_v8_reload(void* address)
{
    asm volatile ("LDR %0, [%1]" : "=r" (address) : "r" (address));
}

#define NUM_ELEMENTS 1000000
#define NUM_ITERATIONS 1000 // Added this line

int main()
{
    int data[NUM_ELEMENTS];
    clock_t start, end;
    double cpu_time_used;
    int i, j;

    // initialize data
    for (i = 0; i < NUM_ELEMENTS; i++) {
        data[i] = i;
    }

    // flush data
    start = clock();
    for (j = 0; j < NUM_ITERATIONS; j++) { // Added this line
        for (i = 0; i < NUM_ELEMENTS; i++) {
            arm_v8_flush((void*) &data[i]);
        }
    } // Added this line
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Flush time: %f seconds\n", cpu_time_used);

    // reload data
    start = clock();
    for (j = 0; j < NUM_ITERATIONS; j++) { // Added this line
        for (i = 0; i < NUM_ELEMENTS; i++) {
            arm_v8_reload((void*) &data[i]);
        }
    } // Added this line
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Reload time: %f seconds\n", cpu_time_used);

    return 0;
}

