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
/* Divide 32 */
#define DIV32(x) ((x) >> 5)
/* Round up 4 */
#define ROUNDUP4(x) (((x) + (1U << 2) - 1) & ~((1U << 2) - 1))
/* Round up 32 */
#define ROUNDUP32(x) (((x) + (1U << 5) - 1) & ~((1U << 5) - 1))
/* Round up 32 and then divide 32 */
#define DIV_ROUNDUP32(x) DIV32((x) + (1U << 5) - 1)
/* Check fbn number is zero or not */
#define fbn_iszero(x) (!(x)->len)
/* fbn last element, need to be used without zero fbn */
#define fbn_lastelmt(x) ((x)->num[(x)->len - 1 + !(x)->len])
/*
 * Assign a value to fbn's n-th num element.
 * @obj: fbn object
 * @n: @n-th element of @obj->num
 * @value: assigning value, only accept 32-bits unsigned integer
 */
#define fbn_assign(obj, n, value) ((obj)->num[(n)] = (value))
/* Set one fbn as zero */
#define fbn_setzero(obj) ((obj)->len = 0)

/*
 * Allocate fbn.
 * @cap: the length of fbn's num alloc
 * Return fbn with length round-up4(@cap) and num is all zeros.
 */
fbn *fbn_alloc(int cap)
{
    if (unlikely(cap <= 0))
        goto inval_or_fail_fbnalloc;
    fbn *new = kmalloc(sizeof(fbn), GFP_KERNEL);
    if (unlikely(!new))
        goto inval_or_fail_fbnalloc;

    /* round up 4 for lazy allocation */
    cap = ROUNDUP4(cap);
    new->num = kcalloc(cap, sizeof(u32), GFP_KERNEL);
    if (unlikely(!new->num))
        goto fail_num_alloc;
    new->cap = cap;
    new->len = 0;
    return new;
fail_num_alloc:
    kfree(new);
inval_or_fail_fbnalloc:
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
 * Resize fbn, realloc if needed (lazy alloc).
 * @obj: fbn object
 * @len: new length to @obj's num
 * Return 0 on success and -1 on failure.
 */
static int fbn_resize(fbn *obj, int len)
{
    obj->len = len;
    if (likely(len <= obj->cap))
        return 0;
    int new_cap = ROUNDUP4(len);
    obj->num = krealloc_array(obj->num, new_cap, sizeof(u32), GFP_KERNEL);
    if (unlikely(!obj->num))
        return -1; /* fail to realloc */
    if (new_cap > obj->cap)
        memset(obj->num + obj->cap, 0, sizeof(u32) * (new_cap - obj->cap));
    obj->cap = new_cap;
    return 0;
}

/*
 * Assign a 32-bits value to fbn.
 * @obj: fbn object
 * @value: a 32-bits value
 */
void fbn_set_u32(fbn *obj, u32 value)
{
    if (value > 0) {
        fbn_resize(obj, 1);
        fbn_assign(obj, 0, value);
    } else {
        fbn_setzero(obj);
    }
}

/*
 * Truncate the leading zero elements of fbn->num, adjust the length of valid
 * value elements.
 * @obj: fbn object
 */
static void fbn_trunclz(fbn *obj)
{
    if (likely(fbn_iszero(obj) || fbn_lastelmt(obj)))
        return;
    int i = obj->len - 1;
    for (; i >= 0 && !obj->num[i]; --i)
        ;
    fbn_resize(obj, i + 1);
}

/*
 * Copy fbn to another fbn.
 * @des: the copy destination of fbn
 * @src: the copy source of fbn
 * Return 0 on success and -1 on failure.
 */
int fbn_copy(fbn *des, const fbn *src)
{
    if (des == src)
        return 0;
    int res = fbn_resize(des, src->len);
    if (unlikely(res < 0))
        return -1;
    memcpy(des->num, src->num, sizeof(u32) * src->len);
    des->len = src->len;
    des->cap = src->cap;
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
    int cap = a->cap;
    a->cap = b->cap;
    b->cap = cap;
}

#ifdef _FBN_DEBUG
/* Print fbn in hex (Debug: use dmesg) */
void fbndebug_printhex(const fbn *obj)
{
    for (int i = obj->cap - 1; i >= 0; --i)
        pr_info("fibdrv_debug: %d %#010x", i, obj->num[i]);
    pr_info("fibdrv_debug: - ---------- len %d", obj->len);
}
#endif /* _FBN_DEBUG */

/* Print fbn into a string (decimal), need kfree to free this string */
char *fbn_print(const fbn *obj)
{
    size_t slen = (sizeof(int) * 8 * obj->len) / 3 + 2;
    char *str = kmalloc(slen, GFP_KERNEL), *p = str;
    memset(str, '0', slen - 1);
    str[slen - 1] = '\0';
    if (unlikely(fbn_iszero(obj))) {
        str[0] = '0';
        return str;
    }

    for (int i = obj->len - 1; i >= 0; --i) {
        for (u32 mask = 1U << 31; mask; mask >>= 1) {
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

/* (high * 2^32 + low) = q * d + r, be aware of overflow of q */
#define divl(high, low, d, q, r) \
    __asm__("divl %4" : "=a"(q), "=d"(r) : "0"(low), "1"(high), "rm"(d))

/*
 * Divide obj by 10^9.
 * @obj: fbn object which is dividend in the beginning and quotient in the end
 * Return the remainder.
 */
static u32 fbn_divten9(fbn *obj)
{
    u32 high_r = 0, divisor = 1000000000U;
    /* start from the leading non-zero element */
    u32 *nump = obj->num + obj->len - 1;

    for (int i = obj->len - 1; i >= 0; --i) {
        u32 cur = *nump, q, r;
        divl(high_r, cur, divisor, q, r);
        *nump = q; /* store the quotient */

        high_r = r; /* update the remainder */
        --nump;     /* move to the lower num */
    }
    return high_r;
}

static u32 put_dec_helper4(char *end, u32 x)
{
    /* q = x / 10^4
     *   = (x * (2^43 / 10^4)) * 2^(-43)
     * require: x < 1,128,869,999 */
    u32 q = (x * 0x346DC5D7ULL) >> 43;
    u32 r = x - q * 10000;

    for (int i = 0; i < 3; ++i) {
        /* q2 = r / 10
         *    = (r * (2^15 / 10)) * 2^(-15)
         * require: r < 16,389 */
        u32 q2 = (r * 0xccd) >> 15;
        *--end = '0' + (r - q2 * 10);
        r = q2;
    }
    *--end = '0' + r;

    return q;
}

/* modified from put_dec() in drivers/firmware/efi/libstub/vsprintf.c */
static char *put_dec(char *end, u32 n)
{
    u32 high, q;
    char *p = end;
    high = n >> 16; /* low = (n & 0xffff) */

    /* n = high * 2^16 + low
     *   = 65536 * high + low
     *   = (6 * 10^4 + 5536) * high + low
     *   = (6 * high) * 10^4 + (5536 * high + low) */

    /* 10^0 part */
    q = 5536 * high + (n & 0xffff);
    q = put_dec_helper4(p, q);
    p -= 4; /* 4 digits */

    /* 10^4 part */
    q += 6 * high;
    q = put_dec_helper4(p, q);
    p -= 4; /* 4 digits */

    /* 10^8 part, q < 10 */
    *--p = '0' + q; /* the 9-th digit */

    return p;
}

/* Print fbn into string (version 1), need kfree to free the string */
char *fbn_printv1(const fbn *obj)
{
    if (unlikely(fbn_iszero(obj))) {
        char *str = kmalloc(2, GFP_KERNEL);
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    fbn *obj2 = fbn_alloc(obj->len); /* alloc fbn */
    if (unlikely(!obj2))
        goto fail_to_alloc;
    int res = fbn_copy(obj2, obj); /* copy fbn */
    if (unlikely(res))
        goto fail_to_copy_or_creatstr;
    /* almost 10 digits per 32 bits */
    size_t str_len = (obj2->len + 1) * 10;
    char *str = kmalloc(str_len, GFP_KERNEL); /* alloc string */
    if (unlikely(!str))
        goto fail_to_copy_or_creatstr;
    str[str_len - 1] = '\0';
    char *str_end = str + str_len - 1, *head = str_end - 1;

    /* short division, print decimal string */
    do {
        /* divided by 10^9, obj2 will become the quotient */
        u32 r_ten9 = fbn_divten9(obj2);
        /* print r_ten9 in str (9 digits) */
        head = put_dec(head, r_ten9);

        /* decrease when a new leading zero element appears */
        obj2->len -= !fbn_lastelmt(obj2);
    } while (obj2->len);

    /* strip off the leading 0's */
    while (head < str_end && *head == '0')
        ++head;
    memmove(str, head, strlen(head) + 1);

    fbn_free(obj2);
    return str;
fail_to_copy_or_creatstr:
    fbn_free(obj2);
fail_to_alloc:
    return NULL;
}

/*
 * Left-shift under 31 bits: b = a << k. a <<= k is also acceptable.
 * @b: fbn object to store the result
 * @a: fbn object to be shifted
 * @k: shift @k bits, k %= 32
 */
void fbn_lshift31(fbn *b, fbn *a, int k)
{
    /* shift 0 bit or a is zero fbn */
    if (unlikely(!k || fbn_iszero(a))) {
        fbn_copy(b, a);
        return;
    }
    /* take modulus 32 and resize b */
    int new_len = a->len - 1 + DIV_ROUNDUP32(fls(fbn_lastelmt(a)) + MOD32(k));
    fbn_resize(b, new_len);

    /* shift and combine carry bits */
    u64 bcabinet = 0;
    for (int i = 0; i < a->len; ++i) {
        bcabinet = (u64) a->num[i] << k | bcabinet;
        b->num[i] = bcabinet;
        bcabinet >>= 32;
    }
    /* remaining part */
    if (bcabinet)  // TODO: TEST [likely or unlikely] in fast doubling method
        fbn_lastelmt(b) = bcabinet;
}

/*
 * Left-shift (general): obj->num <<= k
 * @obj: fbn object, num cannot be 0
 * @k: shift @k bits (no limit)
 */
void fbn_lshift(fbn *obj, int k)
{
    if (unlikely(!k || fbn_iszero(obj)))
        return;
    int shift_bit = MOD32(k);
    int shift_elmt = DIV32(k);
    int new_elmt = DIV32(k + fls(fbn_lastelmt(obj)) - 1);
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
    /* trivial case: a or b is zero */
    int a_iszero = fbn_iszero(a);
    if (unlikely(a_iszero || fbn_iszero(b))) {
        if (a_iszero)
            fbn_copy(c, b);
        else
            fbn_copy(c, a);
        return;
    }

    /* a->num is always the longest one */
    if (a->len < b->len)
        fbn_swap(a, b);
    int b_len = b->len;
    fbn_resize(c, a->len);

    /* addition operation (same length part) */
    int i;
    u64 bcabinet = 0;
    for (i = 0; i < b_len; ++i) {
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
        fbn_lastelmt(c) = bcabinet; /* bcabinet = 1 */
    }
}

/* c = a - b, where a >= b. a -= b is also acceptable */
void fbn_sub(fbn *c, fbn *a, fbn *b)
{
    /* trivial case: a or b is zero */
    int a_iszero = fbn_iszero(a);
    if (unlikely(a_iszero || fbn_iszero(b))) {
        if (a_iszero)
            fbn_set_u32(c, 0);
        else
            fbn_copy(c, a);
        return;
    }
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
    fbn_trunclz(c);
}

/* c = a * b (long multiplication). a *= b is also acceptable */
void fbn_mul(fbn *c, fbn *a, fbn *b)
{
    /* trivial case */
    if (unlikely(fbn_iszero(a) || fbn_iszero(b))) {
        fbn_set_u32(c, 0); /* c = 0 */
        return;
    }

    int new_len = a->len + b->len - 2 +
                  DIV_ROUNDUP32(fls(fbn_lastelmt(a)) + fls(fbn_lastelmt(b)));
    fbn *pseudo_c = fbn_alloc(new_len); /* need an all zero array */
    fbn_resize(pseudo_c, new_len);

    /* long multiplication */
    for (int offset = 0; offset < b->len; ++offset) {
        int pc_idx = 0; /* 0 for suppressing cppcheck */
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
    if (!fbn_lastelmt(pseudo_c))
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
    if (unlikely(n <= 2)) {
        if (n > 0)
            fbn_set_u32(des, 1); /* des = 1 */
        else
            fbn_set_u32(des, 0); /* des = 0 */
        return;
    }

    /* Fibonacci definition */
    fbn *arr[2];
    arr[0] = fbn_alloc(1);
    arr[1] = fbn_alloc(1);
    fbn_set_u32(arr[0], 1); /* arr[0] = 1 (F_1) */
    fbn_set_u32(arr[1], 1); /* arr[1] = 1 (F_2) */
    for (int i = 3; i <= n; ++i)
        fbn_add(arr[i & 1], arr[i & 1], arr[(i - 1) & 1]);

    fbn_swap_content(des, arr[n & 1]);
    fbn_free(arr[0]);
    fbn_free(arr[1]);
}

/*
 * Calculate the nth Fibonacci number with fast doubling method.
 * @des: fbn object to store @n-th Fibonacci number
 * @n: @n-th Fibonacci number
 */
void fbn_fib_fastdoubling(fbn *des, int n)
{
    /* trivial case */
    if (unlikely(n <= 2)) {
        if (n > 0)
            fbn_set_u32(des, 1); /* des = 1 */
        else
            fbn_set_u32(des, 0); /* des = 0 */
        return;
    }

    /* fast doubling method */
    u32 mask = 1U << (fls((u32) n) - 1);
    fbn *a = des; /* a will be the result */
    fbn *b = fbn_alloc(1);
    fbn *tmp = fbn_alloc(1);
    fbn_set_u32(a, 0); /* a = 0 */
    fbn_set_u32(b, 1); /* b = 1 */
    while (mask) {
        /* times 2 */
        fbn_lshift31(tmp, b, 1);  /* tmp = ((b << 1) */
        fbn_sub(tmp, tmp, a);     /*        - a) */
        fbn_mul(tmp, tmp, a);     /*        * a */
        fbn_mul(a, a, a);         /* a^2 */
        fbn_mul(b, b, b);         /* b^2 */
        fbn_add(b, b, a);         /* b = a^2 + b^2 */
        fbn_swap_content(a, tmp); /* a <-> tmp */

        /* plus 1 */
        if (mask & n) {
            fbn_swap_content(a, b); /* a <-> b */
            fbn_add(b, b, a);       /* b += a */
        }
        mask >>= 1;
    }

    fbn_free(b);
    fbn_free(tmp);
}

/*
 * Calculate the nth Fibonacci number with fast doubling method.
 * Version 1: without subtraction
 * @des: fbn object to store @n-th Fibonacci number
 * @n: @n-th Fibonacci number
 */
void fbn_fib_fastdoublingv1(fbn *des, int n)
{
    /* trivial case */
    if (unlikely(n <= 2)) {
        if (n > 0)
            fbn_set_u32(des, 1); /* des = 1 */
        else
            fbn_set_u32(des, 0); /* des = 0 */
        return;
    }

    /* fast doubling method */
    u32 mask = 1U << (fls((u32) n) - 1 - 1);
    fbn *a = fbn_alloc(1);
    fbn *b = des; /* b will be the result */
    fbn *tmp = fbn_alloc(1);
    fbn_set_u32(a, 0); /* a = 0 */
    fbn_set_u32(b, 1); /* b = 1 */
    while (mask) {
        /* times 2 */
        fbn_lshift31(tmp, a, 1);  /* tmp = ((a << 1) */
        fbn_add(tmp, tmp, b);     /*        + b) */
        fbn_mul(tmp, tmp, b);     /*        * b */
        fbn_mul(a, a, a);         /* a^2 */
        fbn_mul(b, b, b);         /* b^2 */
        fbn_add(a, a, b);         /* b = a^2 + b^2 */
        fbn_swap_content(b, tmp); /* a <-> tmp */

        /* plus 1 */
        if (mask & n) {
            fbn_swap_content(a, b); /* a <-> b */
            fbn_add(b, b, a);       /* b += a */
        }
        mask >>= 1;
    }

    fbn_free(a);
    fbn_free(tmp);
}
