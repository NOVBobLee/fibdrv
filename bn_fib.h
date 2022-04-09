#ifndef __FBN_H_
#define __FBN_H_

#include <linux/slab.h>
#include <linux/string.h> /* memset() */
#include <linux/types.h>

/*
 * [num] points to an array, every elements are 4-byte,
 *       so storing a big number larger than 4 bytes will
 *       be like bellow:
 *
 * 0xba98'7654'3210 => | 76543210 | 0000ba98 | ... |
 *                           ^          ^
 *                         num[0]     num[1]
 *
 * [len] is the length of the array
 */
typedef struct {
    u32 *num;
    int len;
} fbn;

/*
 * Allocate fbn.
 * @len: the length of fbn's num
 * Return fbn with length @len and num is all zeros.
 */
fbn *fbn_alloc(int len);
/* Free fbn, return 0 on success and -1 on failure */
int fbn_free(fbn *obj);

/*
 * Assign a value to fbn's n-th num element.
 * @obj: fbn object
 * @n: @n-th element of @obj->num
 * @value: assigning value, only accept 32-bits unsigned integer
 */
#define fbn_assign(obj, n, value) ((obj)->num[(n)] = (value))

/*
 * Copy fbn to another fbn.
 * @des: the copy destination of fbn
 * @src: the copy source of fbn
 * Return 0 on success and -1 on failure.
 */
int fbn_copy(fbn *des, fbn *src);
/* Print fbn in hex (Debug: dmesg) */
void fbn_printhex(fbn *obj);
/* Print fbn to string (decimal), need kfree to free this string */
char *fbn_print(const fbn *obj);

/*
 * Left-shift fbn's num in 32 bits.
 * @obj: fbn object
 * @k: shift @k bits, k <= 32
 */
void fbn_lshift32(fbn *obj, int k);
/*
 * Left-shift fbn's num (general).
 * @obj: fbn object, num cannot be 0
 * @k: shift @k bits (no limit)
 */
void fbn_lshift(fbn *obj, int k);
/* c = a + b, addition assignment (c += a) is also acceptable */
void fbn_add(fbn *c, fbn *a, fbn *b);
/* c = a - b, where a >= b. a -= b is also acceptable */
void fbn_sub(fbn *c, fbn *a, fbn *b);
/* c = a * b (long multiplication). a *= b is also acceptable */
void fbn_mul(fbn *c, fbn *a, fbn *b);

/*
 * Calculate the nth Fibonacci number with definition.
 * @des: fbn object to store @n-th Fibonacci number
 * @n: @n-th Fibonacci number
 */
void fbn_fib_defi(fbn *des, int n);

#endif /* __FBN_H_ */
