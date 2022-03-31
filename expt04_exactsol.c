/*
 * This experiment tests the error between the results of exact-solution method
 * and the correct Fibonacci numbers.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define IN_FILE "scripts/fib_to_92.txt"
#define OUT_FILE "data/00_checkvalues_92_data.out"

#define NFIB 92

#define phi_p 1.6180339887498948L
#define phi_n -0.6180339887498949L
#define inv_sqrt5 0.4472135954999579L

long long fib_exact(int k)
{
    double result = inv_sqrt5 * (pow(phi_p, k) - pow(phi_n, k));
    return (long long) result;
}

int main(void)
{
    char *linep = malloc(sizeof(char) * 20);
    unsigned long linepsize = 20;

    FILE *fp_in = fopen(IN_FILE, "r");
    if (fp_in == NULL) {
        perror("Failed to open input file");
        exit(1);
    }
    FILE *fp_out = fopen(OUT_FILE, "w");
    if (fp_out == NULL) {
        fclose(fp_in);
        perror("Failed to open output file");
        exit(2);
    }

    for (int fib_i = 0; fib_i <= NFIB; ++fib_i) {
        getline(&linep, &linepsize, fp_in);
        long long fib = atoll(linep);
        long long exact = fib_exact(fib_i);

        long long diff = exact - fib;
        fprintf(fp_out, "%d %lld %lld %lld\n", fib_i, diff, fib, exact);
    }
    free(linep);

    fclose(fp_out);
    fclose(fp_in);
    return 0;
}
