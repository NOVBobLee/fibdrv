#include "bn_fib.h"

/* Swap two fbn pointers */
#define fbn_swap(a, b) \
    ({                 \
        fbn *tmp = a;  \
        a = b;         \
        b = tmp;       \
    })
/* Modulo 32 */
#define MOD32(x) ((x) & ((1U << 5) - 1))
/* Round-Up 32 bits */
#define ROUNDUP32(x) (((x) + (1U << 5) - 1) & ~((1U << 5) - 1))

/*
 * Allocate fbn.
 * @len: the length of fbn's num
 * Return fbn with length @len and num is all zeros.
 */
fbn *fbn_alloc(int len)
{
    fbn *new = kmalloc(sizeof(fbn), GFP_KERNEL);
    if (unlikely(!new))
        goto fail_fbn_alloc;
    new->num = kcalloc(len, sizeof(u32), GFP_KERNEL);
    if (unlikely(!new->num))
        goto fail_num_alloc;
    new->len = len;
    return new;

fail_num_alloc:
    kfree(new);
fail_fbn_alloc:
    return NULL;
}

/*
 * Free fbn.
 * @obj: fbn object
 * Return 0 on success and -1 on failure.
 */
int fbn_free(fbn *obj)
{
    if (unlikely(!obj))
        return -1;
    kfree(obj->num);
    kfree(obj);
    return 0;
}

/*
 * Resize fbn.
 * @obj: fbn object
 * @len: new length to @obj's num
 * Return 0 on success and -1 on failure.
 */
static int fbn_resize(fbn *obj, int len)
{
    if (obj->len == len)
        return 0;
    obj->num = krealloc_array(obj->num, len, sizeof(u32), GFP_KERNEL);
    if (unlikely(!obj->num))
        return -1; /* fail to realloc */
    if (len > obj->len)
        memset(obj->num + obj->len, 0, sizeof(u32) * (len - obj->len));
    obj->len = len;
    return 0;
}

/*
 * Copy fbn to another fbn.
 * @des: the copy destination of fbn
 * @src: the copy source of fbn
 * Return 0 on success and -1 on failure.
 */
int fbn_copy(fbn *des, fbn *src)
{
    int res = fbn_resize(des, src->len);
    if (unlikely(res < 0))
        return -1;
    memcpy(des->num, src->num, sizeof(u32) * src->len);
    return 0;
}

/* Swap two fbn contents. */
static void fbn_swap_content(fbn *a, fbn *b)
{
    u32 *num = a->num;
    a->num = b->num;
    b->num = num;
    int len = a->len;
    a->len = b->len;
    b->len = len;
}

/* Print fbn in hex (Debug: dmesg) */
void fbn_printhex(fbn *obj)
{
    for (int i = obj->len - 1; i >= 0; --i)
        pr_info("fibdrv: %d %#010x\n", i, obj->num[i]);
}

/* Print fbn into a string (decimal), need kfree to free this string */
char *fbn_print(const fbn *obj)
{
    size_t slen = (sizeof(int) * 8 * obj->len) / 3 + 2;
    char *str = kmalloc(slen, GFP_KERNEL), *p = str;
    memset(str, '0', slen - 1);
    str[slen - 1] = '\0';

    for (int i = obj->len - 1; i >= 0; --i) {
        for (unsigned mask = 1U << 31; mask; mask >>= 1) {
            int carry = !!(mask & obj->num[i]);
            for (int j = slen - 2; j >= 0; --j) {
                str[j] += str[j] - '0' + carry;
                carry = (str[j] > '9');
                if (carry)
                    str[j] -= 10;
            }
        }
    }

    while (p[0] == '0' && p[1] != '\0')
        ++p;
    memmove(str, p, strlen(p) + 1);
    return str;
}

/*
 * Left-shift fbn's num in 32 bits.
 * @obj: fbn object
 * @k: shift @k bits, k <= 32, take modulus 32
 */
void fbn_lshift32(fbn *obj, int k)
{
    if (unlikely(!k))
        return;
    /* 1 <= k <= 32 */
    u32 kmod32 = MOD32(k);
    k = kmod32 + (!kmod32 << 5);

    /* shift and combine the carry bits */
    u64 bcabinet = 0;
    for (int i = 0; i < obj->len; ++i) {
        bcabinet = (u64) obj->num[i] << k | bcabinet;
        obj->num[i] = bcabinet;
        bcabinet >>= 32;
    }
    /* no enough length for storing the carry bits */
    if (bcabinet) {
        fbn_resize(obj, obj->len + 1);
        obj->num[obj->len - 1] = bcabinet;
    }
}

/*
 * Left-shift fbn's num (general).
 * @obj: fbn object, num cannot be 0
 * @k: shift @k bits (no limit)
 */
void fbn_lshift(fbn *obj, int k)
{
    if (unlikely(!k))
        return;
    int shift_bit = MOD32(k);
    int shift_elmt = k >> 5;
    /* Be careful, obj->num[obj->len - 1] cannot be zero. If met, it means
     * there's zero elements in the high position of num didn't truncated */
    int new_elmt = (k + fls(obj->num[obj->len - 1]) - 1) >> 5;
    fbn_resize(obj, obj->len + new_elmt);

    /*               0     1       (len - 1)
     * obj->num = | xxx | xxx | ... | xxx |
     *               ^     ^  ^  ^     ^
     *              low     middle    high
     */
    int shift_back = 32 - shift_bit;
    u32 mask = -(1U << shift_back);
    /* high part */
    int i = obj->len - 1;
    if (new_elmt > shift_elmt) {
        obj->num[i] = (obj->num[i - shift_elmt - 1] & mask) >> shift_back;
        --i;
    }
    /* middle part */
    for (; i > shift_elmt; --i) { /* i >= shift_elmt + 1 */
        obj->num[i] = (obj->num[i - shift_elmt] << shift_bit) |
                      (obj->num[i - 1 - shift_elmt] & mask) >> shift_back;
    }
    /* low part */
    if (i == shift_elmt) {
        obj->num[i] = obj->num[i - shift_elmt] << shift_bit;
        --i;
    }
    /* remaining zeros part */
    for (; i >= 0; --i)
        obj->num[i] = 0;
}

/* c = a + b, addition assignment (a += b) is also acceptable */
void fbn_add(fbn *c, fbn *a, fbn *b)
{
    /* a->num is always the longest one */
    if (a->len < b->len)
        fbn_swap(a, b);
    fbn_resize(c, a->len);

    /* addition operation (same length part) */
    int i;
    u64 bcabinet = 0;
    for (i = 0; i < b->len; ++i) {
        bcabinet += (u64) a->num[i] + b->num[i];
        c->num[i] = bcabinet;
        bcabinet >>= 32;
    }
    /* addition operation (remaining part) */
    for (; i < a->len; ++i) {
        bcabinet += (u64) a->num[i];
        c->num[i] = bcabinet;
        bcabinet >>= 32;
    }
    /* if the carry is still remained */
    if (unlikely(bcabinet)) {
        fbn_resize(c, a->len + 1);
        c->num[c->len - 1] = bcabinet; /* bcabinet = 1 */
    }
}

/* c = a - b, where a >= b. a -= b is also acceptable */
void fbn_sub(fbn *c, fbn *a, fbn *b)
{
    fbn_resize(c, a->len);

    int i;
    u32 borrow = 0;
    u64 subtrahend;
    for (i = 0; i < b->len; ++i) {
        subtrahend = (u64) b->num[i] + borrow;
        borrow = subtrahend > a->num[i];
        c->num[i] = a->num[i] - (u32) subtrahend;
    }
    for (; i < a->len; ++i) {
        subtrahend = (u64) borrow;
        borrow = subtrahend > a->num[i];
        c->num[i] = a->num[i] - (u32) subtrahend;
    }
    /* truncate the leading zero elements */
    for (i = c->len - 1; i >= 0 && !c->num[i]; --i)
        ;
    fbn_resize(c, i + 1 + (i == -1)); /* avoid i == -1, happens when num
                                         is 0, although fbn won't happen */
}

/* c = a * b (long multiplication). a *= b is also acceptable */
void fbn_mul(fbn *c, fbn *a, fbn *b)
{
    /* Be careful, num[obj->len - 1] cannot be zero. If met, it means
     * there's zero elements in the high position of num didn't truncated */
    int new_len =
        a->len + b->len - 2 +
        (ROUNDUP32(fls(a->num[a->len - 1]) + fls(b->num[b->len - 1])) >> 5);
    fbn *pseudo_c = fbn_alloc(new_len); /* need an all zero array */

    /* long multiplication */
    for (int offset = 0; offset < b->len; ++offset) {
        int pc_idx = 0; /* for suppressing cppcheck */
        u64 bcabinet = 0;
        /* c += a * (b->num[offset]) */
        for (int i = 0; i < a->len; ++i) {
            pc_idx = i + offset;
            bcabinet +=
                (u64) a->num[i] * b->num[offset] + pseudo_c->num[pc_idx];
            pseudo_c->num[pc_idx] = bcabinet;
            bcabinet >>= 32;
        }
        pseudo_c->num[pc_idx + 1] = bcabinet; /* maybe it's 0 */
    }
    /* truncate the leading zero element */
    if (!pseudo_c->num[new_len - 1])
        fbn_resize(pseudo_c, new_len - 1);

    /* pass the content to c */
    fbn_swap_content(pseudo_c, c);
    fbn_free(pseudo_c);
}

/*
 * Calculate the nth Fibonacci number with definition.
 * @des: fbn object to store @n-th Fibonacci number
 * @n: @n-th Fibonacci number
 */
void fbn_fib_defi(fbn *des, int n)
{
    /* trivial case */
    if (unlikely(n < 2)) {
        fbn_resize(des, 1);
        des->num[0] = n;
        return;
    }

    /* Fibonacci definition */
    fbn *arr[2];
    arr[0] = fbn_alloc(1);
    arr[1] = fbn_alloc(1);
    arr[1]->num[0] = 1;
    for (int i = 2; i <= n; ++i)
        fbn_add(arr[i & 1], arr[i & 1], arr[(i - 1) & 1]);
    fbn_swap_content(des, arr[n & 1]);

    fbn_free(arr[0]);
    fbn_free(arr[1]);
}
