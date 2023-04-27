#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUF_SIZE 8192

int main(void) {
    int fd;
    ssize_t count;
    char buffer[BUF_SIZE];
    char *filename = "/proc/cache_kmv2_message";
    char *output_filename = "/tmp/cache_kmv2_message.txt";
    FILE *output_file;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Error opening proc file");
        exit(EXIT_FAILURE);
    }

    count = read(fd, buffer, BUF_SIZE - 1);
    if (count < 0) {
        perror("Error reading proc file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    buffer[count] = '\0';

    output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("Error opening output file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    fwrite(buffer, sizeof(char), count, output_file);

    fclose(output_file);
    close(fd);

    printf("Contents of %s written to %s\n", filename, output_filename);
    return 0;
}
