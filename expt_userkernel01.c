/*
 * This program tests the time spent of Fibonacci sequence computing in
 * different spaces (kernel space, user space).
 *
 * The method of computing the Fibonacci seq. is the default one (using the
 * definition of Fibonacci seq. and VLA).
 *
 * It generates 1000 samples and then only pick the samples in 95% confidence
 * interval.
 */
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define OUT_FILE "data/userkernel01-data.out"

#define NFIB 92
#define NSAMPLE 1000
#define NSPACE 2
enum { KERNEL, USER };

int main(void)
{
    char buf[1] = {'\0'};

    int fd_fib = open(FIB_DEV, O_RDWR);
    if (fd_fib < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    FILE *fp_out = fopen(OUT_FILE, "w");
    if (fp_out == NULL) {
        close(fd_fib);
        perror("Failed to open output file");
        exit(2);
    }

    /* start testing time */
    for (int i = 0; i <= NFIB; ++i) {
        lseek(fd_fib, i, SEEK_SET);
        double times[NSPACE][NSAMPLE] = {0.};
        double mean[NSPACE] = {0.}, sd[NSPACE] = {0.};

        /* sampling */
        for (int n = 0; n < NSAMPLE; ++n) {
            struct timespec t1, t2;
            clock_gettime(CLOCK_MONOTONIC, &t1);
            times[KERNEL][n] = write(fd_fib, buf, 1);
            clock_gettime(CLOCK_MONOTONIC, &t2);
            times[USER][n] = (double) (t2.tv_sec * 1e9 + t2.tv_nsec -
                                       t1.tv_sec * 1e9 - t1.tv_nsec);
            /* sum for mean */
            mean[KERNEL] += times[KERNEL][n];
            mean[USER] += times[USER][n];
        }
        /* calculate the mean */
        for (int space = 0; space < NSPACE; ++space)
            mean[space] /= NSAMPLE;

        /* calculate the corrected standard deviation */
        for (int space = 0; space < NSPACE; ++space) {
            for (int n = 0; n < NSAMPLE; ++n)
                sd[space] += (times[space][n] - mean[space]) *
                             (times[space][n] - mean[space]);
            sd[space] = sqrt(sd[space] / (NSAMPLE - 1));
        }

        /* pick the samples in 95% confidence interval */
        double ci95_mean[NSPACE] = {0.};
        int cnt[NSPACE] = {0};
        for (int space = 0; space < NSPACE; ++space) {
            double cibound95 = mean[space] + 2 * sd[space];
            double cibound5 = mean[space] - 2 * sd[space];
            for (int n = 0; n < NSAMPLE; ++n) {
                if (cibound5 < times[space][n] && times[space][n] < cibound95) {
                    ci95_mean[space] += times[space][n];
                    ++cnt[space];
                }
            }
            ci95_mean[space] /= cnt[space];
        }

        /* write into OUT_FILE */
        fprintf(fp_out, "%d", i); /* fibonacci_i */
        for (int space = 0; space < NSPACE; ++space)
            fprintf(fp_out, " %.5lf", ci95_mean[space]); /* time */
        fprintf(fp_out, " %.5lf", ci95_mean[USER] - ci95_mean[KERNEL]);
        fprintf(fp_out, "\n");
    }

    fclose(fp_out);
    close(fd_fib);
    return 0;
}
