#include <stdio.h>
#include <stdbool.h>

#define NUM_PRIMES 3000

int main() {
    int primes[NUM_PRIMES] = {2};
    int count = 1, i = 3;

    while (count < NUM_PRIMES) {
        bool is_prime = true;
        for (int j = 0; j < count; j++) {
            if (i % primes[j] == 0) {
                is_prime = false;
                break;
            }
        }
        if (is_prime) {
            primes[count] = i;
            count++;
        }
        i += 2;
    }

    // Print out the first 3000 prime numbers
    for (int i = 0; i < NUM_PRIMES; i++) {
        printf("%d ", primes[i]);
    }

    return 0;
}
