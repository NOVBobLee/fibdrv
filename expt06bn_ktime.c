/*
 * This experiment test the execution time in the kernel space.
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
#define OUT_FILE "data/06bn_ktime_data.out"

#define NSAMPLE 1000

#define NFIB 1000 /* be aware of the buf size */
enum {
    BNFIB_DEFI,
    BNFIB_FASTDBL,
    BNFIB_FASTDBLv1,
};
#define METHOD BNFIB_FASTDBLv1


int main(void)
{
    char buf[5000] = {
        [4999] = '\0',
    };

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
        double times[NSAMPLE] = {0.};
        double mean = 0., sd = 0.;

        /* sampling */
        for (int n = 0; n < NSAMPLE; ++n) {
            times[n] = read(fd_fib, buf, METHOD);
            mean += times[n];
        }
        mean /= NSAMPLE;

        /* calculate the corrected standard deviation */
        for (int n = 0; n < NSAMPLE; ++n)
            sd += (times[n] - mean) * (times[n] - mean);
        sd = sqrt(sd / (NSAMPLE - 1));

        /* pick the samples in 95% confidence interval */
        double ci95_mean = 0.;
        int cnt = 0;
        double cibound95 = mean + 2 * sd;
        double cibound5 = mean - 2 * sd;
        for (int n = 0; n < NSAMPLE; ++n) {
            if (cibound5 < times[n] && times[n] < cibound95) {
                ci95_mean += times[n];
                ++cnt;
            }
        }
        ci95_mean /= cnt;

        /* write into OUT_FILE */
        fprintf(fp_out, "%d %.5lf\n", i, ci95_mean); /* fibonacci_i time */
    }

    fclose(fp_out);
    close(fd_fib);
    return 0;
}
