/*
 * This program tests the time spent of Fibonacci sequence computing in
 * different methods (VLA/kmalloc/fixed-length array)
 *
 * All methods uses the definition of Fibonacci seq.
 *
 * It generates 1000 samples and then only pick the samples in 95% confidence
 * interval.
 */
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define OUT_FILE "data/02_vlafree_data.out"

#define NFIB 92
#define NSAMPLE 1000
#define NMETHOD 3
enum { VLA, KMALLOC, FLA };

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
        double times[NMETHOD][NSAMPLE] = {0.};
        double mean[NMETHOD] = {0.}, sd[NMETHOD] = {0.};

        /* sampling */
        for (int method = 0; method < NMETHOD; ++method) {
            for (int n = 0; n < NSAMPLE; ++n) {
                times[method][n] = write(fd_fib, buf, method);
                mean[method] += times[method][n];
            }
            mean[method] /= NSAMPLE;
        }

        /* calculate the corrected standard deviation */
        for (int method = 0; method < NMETHOD; ++method) {
            for (int n = 0; n < NSAMPLE; ++n)
                sd[method] += (times[method][n] - mean[method]) *
                              (times[method][n] - mean[method]);
            sd[method] = sqrt(sd[method] / (NSAMPLE - 1));
        }

        /* pick the samples in 95% confidence interval */
        double ci95_mean[NMETHOD] = {0.};
        int cnt[NMETHOD] = {0};
        for (int method = 0; method < NMETHOD; ++method) {
            double cibound95 = mean[method] + 2 * sd[method];
            double cibound5 = mean[method] - 2 * sd[method];
            for (int n = 0; n < NSAMPLE; ++n) {
                if (cibound5 < times[method][n] &&
                    times[method][n] < cibound95) {
                    ci95_mean[method] += times[method][n];
                    ++cnt[method];
                }
            }
            ci95_mean[method] /= cnt[method];
        }

        /* write into OUT_FILE */
        fprintf(fp_out, "%d", i); /* fibonacci_i */
        for (int method = 0; method < NMETHOD; ++method)
            fprintf(fp_out, " %.5lf", ci95_mean[method]); /* time */
        fprintf(fp_out, "\n");
    }

    fclose(fp_out);
    close(fd_fib);
    return 0;
}
