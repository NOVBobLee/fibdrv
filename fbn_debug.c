#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[1000];
    buf[999] = '\0';

    int offset = 100;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    lseek(fd, offset, SEEK_SET);
    long long left = read(fd, buf, 1000);
    printf("Fibonacci(%d) = %s left: %lld\n", offset, buf, left);

    close(fd);
    return 0;
}
