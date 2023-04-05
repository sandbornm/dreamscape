#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
    for (int i = 0; i < 10; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Failed to fork process");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            printf("Child process %d\n", getpid());
            sleep(10);
            printf("Orphaned child process %d\n", getpid());
            exit(EXIT_SUCCESS);
        } else {
            printf("Parent process %d forked child %d\n", getpid(), pid);
        }
    }

    return 0;
}
