#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1000000

int main() {
    // Create 5 random files in /tmp/
    char filename[20];
    FILE* fp;
    for (int i = 0; i < 5; i++) {
        sprintf(filename, "/tmp/file%d.txt", i);
        fp = fopen(filename, "w");
        if (fp == NULL) {
            perror("Failed to create file");
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < BUF_SIZE; j++) {
            fputc(rand() % 256, fp);
        }
        fclose(fp);
    }

    // Read from each file into a single buffer
    char buffer[BUF_SIZE * 5];
    int total_bytes = 0;
    for (int i = 0; i < 5; i++) {
        sprintf(filename, "/tmp/file%d.txt", i);
        fp = fopen(filename, "r");
        if (fp == NULL) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }
        int bytes_read = fread(buffer + total_bytes, 1, BUF_SIZE, fp);
        total_bytes += bytes_read;
        fclose(fp);
    }

    // Delete all the files
    for (int i = 0; i < 5; i++) {
        sprintf(filename, "/tmp/file%d.txt", i);
        if (unlink(filename) == -1) {
            perror("Failed to delete file");
            exit(EXIT_FAILURE);
        }
    }

    // Print out the contents of the buffer
    //printf("%s\n", buffer);

    return 0;
}
