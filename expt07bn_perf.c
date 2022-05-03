#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

enum {
    BNFIB_DEFI,
    BNFIB_FASTDBL,
    BNFIB_FASTDBLv1,
};
#define METHOD BNFIB_FASTDBLv1
#define NFIB 1000 /* Be ware of the buf size */

int main(void)
{
    char buf[1] = {'\0'};

    int fd_fib = open(FIB_DEV, O_RDWR);
    if (fd_fib < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    lseek(fd_fib, NFIB, SEEK_SET);
    /* perf main part is written in fibdrv.c - fib_read() */
    read(fd_fib, buf, METHOD);

    close(fd_fib);
    return 0;
}
