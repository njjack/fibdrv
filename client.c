#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    int fd;
    long long sz;

    char buf[1];
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
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &start);
        sz = read(fd, buf, 100);
        clock_gettime(CLOCK_MONOTONIC, &end);
        fprintf(ft, "%d %llu %lu \n", i, atoll(buf),
                end.tv_nsec - start.tv_nsec);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }

    for (i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 100);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }
    close(ft);
    close(fd);
    return 0;
}
