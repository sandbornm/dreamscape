#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_FILE_SIZE 10485760  // 10 MB

int main() {
    FILE* fp = tmpfile();
    if (fp == NULL) {
        perror("Failed to create temporary file");
        exit(EXIT_FAILURE);
    }

    int count[256] = {0};
    int unique_count = 0;
    int byte_count = 0;
    unsigned char byte;

    while (byte_count < MAX_FILE_SIZE && fread(&byte, 1, 1, fp)) {
        count[byte]++;
        if (count[byte] == 1) {
            unique_count++;
        }
        byte_count++;
    }

    fclose(fp);
    printf("Number of unique bytes in file: %d\n", unique_count);

    return 0;
}
