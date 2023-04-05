#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_POINTS 1000000

int main() {
    srand(time(NULL));
    int count = 0;

    for (int i = 0; i < NUM_POINTS; i++) {
        double x = (double) rand() / RAND_MAX;
        double y = (double) rand() / RAND_MAX;
        if (x * x + y * y <= 1) {
            count++;
        }
    }

    double pi = 4.0 * count / NUM_POINTS;
    printf("Estimated value of Pi: %f\n", pi);

    return 0;
}
