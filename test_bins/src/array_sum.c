#include <stdio.h>

#define ARRAY_SIZE 10000000

int main() {
    int arr[ARRAY_SIZE];
    int sum = 0;

    // Fill the array with random values
    for (int i = 0; i < ARRAY_SIZE; i++) {
        arr[i] = i;
    }

    // Sum up the values in the array
    for (int i = 0; i < ARRAY_SIZE; i++) {
        sum += arr[i];
    }

    printf("Sum of array elements: %d\n", sum);

    return 0;
}
