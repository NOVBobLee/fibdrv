#include "bn_fib.h"

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

/* c = a + b, addition assignment (a += b) is also acceptable */
void fbn_add(fbn *c, fbn *a, fbn *b)
{
    /* a->num is always the longest one */
    if (a->len < b->len)
        fbn_swap(a, b);
    fbn_resize(c, a->len);

    /* addition operation (same length part) */
    int i, len = b->len;
    u64 bcabinet = 0;
    for (i = 0; i < len; ++i) {
        bcabinet += (u64) a->num[i] + b->num[i];
        c->num[i] = bcabinet;
        bcabinet >>= 32;
    }
    /* addition operation (remaining part) */
    len = a->len;
    for (; i < len; ++i) {
        bcabinet += (u64) a->num[i];
        c->num[i] = bcabinet;
        bcabinet >>= 32;
    }
    /* if the carry is still remained */
    if (unlikely(bcabinet)) {
        fbn_resize(c, len + 1);
        c->num[len] = bcabinet; /* bcabinet = 1 */
    }
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
    fbn_copy(des, arr[n & 1]);
}
