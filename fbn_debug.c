#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

/* n-th Fibonacci number
 * Be careful of the buf size, 10000-th Fibonacci needs 2901 digits */
#define NFIB 10000

enum {
    BNFIB_DEFI,
    BNFIB_FASTD,
};
#define METHOD BNFIB_FASTD

int main()
{
    char buf[5000];
    buf[4999] = '\0';

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    lseek(fd, NFIB, SEEK_SET);
    long long ktime = read(fd, buf, METHOD);
    printf("Fibonacci(%d) = %s ktime: %lld\n", NFIB, buf, ktime);

    /* test write ktime macro */
    lseek(fd, 5, SEEK_SET);
    ktime = write(fd, buf, 8); /* fls */
    printf("TEST ktime macro: kt = %lld\n", ktime);

    close(fd);
    return 0;
}
