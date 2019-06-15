#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "big.h"

#define FIB_DEV "/dev/fibonacci"
void big_print(bigNum buf)
{
    int i = part_num - 1;
    while ((i >= 0) && (buf.part[i] == 0))
        i--;
    if (i < 0) {
        printf("0");
        return;
    }
    printf("%lld", buf.part[i--]);
    while (i >= 0) {
        printf("%08lld", buf.part[i]);
        i--;
    }
}

int main()
{
    int fd;
    long long sz;

    bigNum buf;
    char write_buf[] = "testing writing";
    int offset = 100;  // TODO: test something bigger than the limit
    int i = 0;
    long int ns;
    struct timespec start, end;
    FILE *ft = fopen("time", "w");

    fd = open(FIB_DEV, O_RDWR);

    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (i = 0; i <= offset; i++) {
        for (int j = 0; j < part_num; j++) {
            buf.part[j] = 0;
        }
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &start);
        sz = read(fd, &buf, sizeof(bigNum));
        clock_gettime(CLOCK_MONOTONIC, &end);

        printf("Reading from " FIB_DEV " at offset %d, returned the sequence ",
               i);
        big_print(buf);
        printf(".\n");
    }

    for (i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, &buf, sizeof(bigNum));
        printf("Reading from " FIB_DEV " at offset %d, returned the sequence ",
               i);
        big_print(buf);
        printf(".\n");
    }
    close(ft);
    close(fd);
    return 0;
}
