/*
 * This experiment checks the result output from the Fibonacci module.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define IN_FILE "scripts/fib_to_92.txt"
#define OUT_FILE "data/00_checkvalues92_data.out"

#define NFIB 92

enum {
    VLA,
    KMALLOC,
    FLA,
    EXACTSOL2,
    EXACTSOL3,
    FASTDBL_L62,
    FASTDBL_L31,
    FASTDBL_L16,
    FASTDBL_L6,
    FASTDBL_FLS,
    FASTDBL_CLZ,
};
#define METHOD FASTDBL_FLS

int main(void)
{
    char *linep = malloc(sizeof(char) * 20);
    unsigned long linepsize = 20;
    char buf[1] = {'\0'};

    int fd_fib = open(FIB_DEV, O_RDWR);
    if (fd_fib < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    FILE *fp_in = fopen(IN_FILE, "r");
    if (fp_in == NULL) {
        close(fd_fib);
        perror("Failed to open input file");
        exit(2);
    }
    FILE *fp_out = fopen(OUT_FILE, "w");
    if (fp_out == NULL) {
        close(fd_fib);
        fclose(fp_in);
        perror("Failed to open output file");
        exit(3);
    }

    for (int fib_i = 0; fib_i <= NFIB; ++fib_i) {
        getline(&linep, &linepsize, fp_in);
        long long fib = atoll(linep);
        lseek(fd_fib, fib_i, SEEK_SET);
        long long result = write(fd_fib, buf, METHOD);

        long long diff = llabs(result - fib);
        fprintf(fp_out, "%d %lld %lld %lld\n", fib_i, diff, fib, result);
    }
    free(linep);

    fclose(fp_out);
    fclose(fp_in);
    close(fd_fib);
    return 0;
}
