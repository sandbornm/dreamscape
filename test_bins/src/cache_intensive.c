#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ARRAY_SIZE 1000000
#define ITERATIONS 1000

void cache_intensive_task() {
    int i, j;
    int *data = (int *)malloc(ARRAY_SIZE * sizeof(int));

    for (i = 0; i < ARRAY_SIZE; i++) {
        data[i] = i;
    }

    for (j = 0; j < ITERATIONS; j++) {
        for (i = 0; i < ARRAY_SIZE; i++) {
            data[i] = data[i] * 2 - i;
        }
    }

    free(data);
}

int main() {
    clock_t start, end;
    double duration;

    start = clock();

    cache_intensive_task();

    end = clock();
    duration = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Duration: %.2f seconds\n", duration);
    return 0;
}
