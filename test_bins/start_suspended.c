#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    pid_t pid;
    int status;

    // Fork the current process
    pid = fork();

    if (pid == 0) {
        // Child process: execute the target program
        execvp(argv[1], argv+1);
        perror("execvp");
        exit(1);
    } else if (pid < 0) {
        // Error: fork failed
        perror("fork");
        exit(1);
    } else {
        // Parent process: send SIGSTOP to the child process
        if (kill(pid, SIGSTOP) != 0) {
            perror("kill");
            exit(1);
        }

        // Wait for the child process to stop
        if (waitpid(pid, &status, WUNTRACED) < 0) {
            perror("waitpid");
            exit(1);
        }

        // Output the child process PID
        printf("Child process started in suspended state with PID: %d\n", pid);
    }

    return 0;
}
