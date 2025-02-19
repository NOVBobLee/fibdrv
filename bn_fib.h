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
 * [len] is the length of array with valid value elements
 *       i.e. allocated array length - #(leading zero elements)
 * [cap] is the allocated array length
 */
typedef struct {
    u32 *num;
    int len;
    int cap;
} fbn;

/*
 * Allocate fbn.
 * @len: the length of fbn's num
 * Return fbn with length @len and num is all zeros.
 */
fbn *fbn_alloc(int cap);
/* Free fbn, return 0 on success and -1 on failure */
int fbn_free(fbn *obj);

/*
 * Assign a 32-bits value to fbn.
 * @obj: fbn object
 * @value: 32-bits value
 */
void fbn_set_u32(fbn *obj, u32 value);

/*
 * Copy fbn to another fbn.
 * @des: the copy destination of fbn
 * @src: the copy source of fbn
 * Return 0 on success and -1 on failure.
 */
int fbn_copy(fbn *des, const fbn *src);
#ifdef _FBN_DEBUG
/* Print fbn in hex (Debug: use dmesg) */
void fbndebug_printhex(const fbn *obj);
#endif /* _FBN_DEBUG */
/* Print fbn to string (decimal), need kfree to free this string */
char *fbn_print(const fbn *obj);
/* Print fbn into string (version 1), need kfree to free the string */
char *fbn_printv1(const fbn *obj);

/*
 * Left-shift under 31 bits: b = a << k. a <<= k is also acceptable.
 * @b: fbn object to store the result
 * @a: fbn object to be shifted
 * @k: shift @k bits, k %= 32
 */
void fbn_lshift31(fbn *b, fbn *a, int k);
/*
 * Left-shift (general): obj->num <<= k
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
/*
 * Calculate the nth Fibonacci number with fast doubling method.
 * @des: fbn object to store @n-th Fibonacci number
 * @n: @n-th Fibonacci number
 */
void fbn_fib_fastdoubling(fbn *des, int n);
/*
 * Calculate the nth Fibonacci number with fast doubling method.
 * Version 1: without subtraction
 * @des: fbn object to store @n-th Fibonacci number
 * @n: @n-th Fibonacci number
 */
void fbn_fib_fastdoublingv1(fbn *des, int n);

#endif /* __FBN_H_ */
