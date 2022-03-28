/*
 * This experiment is made for using perf utility.
 *
 * Usage: ./expt03_perf <method-number>
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

#define NFIB 92
#define REPEAT 2000000

int main(int argc, char *argv[])
{
    char buf[1] = {'\0'};
    if (argc < 2) {
        printf("Usage: ./expt03_perf <method-number>\n");
        exit(1);
    }
    int method = atoi(argv[1]);

    int fd_fib = open(FIB_DEV, O_RDWR);
    if (fd_fib < 0) {
        perror("Failed to open character device");
        exit(2);
    }

    lseek(fd_fib, NFIB, SEEK_SET);
    for (int i = 0; i < REPEAT; ++i)
        write(fd_fib, buf, method);

    close(fd_fib);
    return 0;
}
