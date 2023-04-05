#include <stdio.h>
#include <stdlib.h>

#define NUM_PTRS 10000

int main() {
    void* ptrs[NUM_PTRS];

    // Allocate memory for pointers and temporary data structures
    for (int i = 0; i < NUM_PTRS; i++) {
        ptrs[i] = malloc(sizeof(void*));
        if (ptrs[i] == NULL) {
            perror("Failed to allocate memory");
            exit(EXIT_FAILURE);
        }
    }

    // Rewire pointers to point to each other and temporary data structures
    for (int i = 0; i < NUM_PTRS; i++) {
        if (i < NUM_PTRS - 1) {
            *(void**)ptrs[i] = ptrs[i+1];
        } else {
            *(void**)ptrs[i] = malloc(sizeof(int));
        }
    }

    // Free all the allocated memory
    for (int i = 0; i < NUM_PTRS; i++) {
        free(ptrs[i]);
    }

    return 0;
}
