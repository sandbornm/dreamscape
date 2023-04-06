#include <stdio.h>
#include <time.h>

#define ARRAY_SIZE (1 << 20) // 1 MB
#define LOOP_COUNT 1000000000 // 1 billion

int main() {
    int arr[ARRAY_SIZE] = {0}; // create an array in memory
    int sum = 0;
    clock_t start_time = clock(); // get the current time
    for (int i = 0; i < LOOP_COUNT; i++) {
        sum += arr[i % ARRAY_SIZE]; // access a different element of the array each time
    }
    clock_t end_time = clock(); // get the current time
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC; // calculate the elapsed time
    printf("Sum: %d\nElapsed time: %.2f seconds\n", sum, elapsed_time);
    return 0;
}
