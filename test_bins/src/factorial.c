#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    uint64_t result = 1;

    for (int i = 1; i <= 100; i++) {
        result *= i;
    }

    printf("Factorial of 100: %llu\n", result);

    return 0;
}
