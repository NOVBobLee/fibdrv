#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

/* n-th Fibonacci number
 * Be careful of the buf size, 10000-th Fibonacci needs 2901 digits */
#define NFIB 5

enum {
    BNFIB_DEFI,
    BNFIB_FASTD,
    BNFIB_FASTDv1,
};
#define METHOD BNFIB_FASTD

int main()
{
    char buf[5000] = {
        [4999] = '\0',
    };

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    lseek(fd, NFIB, SEEK_SET);
    long long ktime = read(fd, buf, METHOD);
    printf("Fibonacci(%d) = %s ktime: %lld\n", NFIB, buf, ktime);

    close(fd);
    return 0;
}
