#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "bn_fib.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
//#define MAX_LENGTH 92
#define MAX_LENGTH 10000

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static long long fib_sequence(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

static long long fibseq_kmalloc(long long k)
{
    long long result, *f = kmalloc_array(k + 2, sizeof(long long), GFP_KERNEL);

    if (unlikely(!f))
        return -1;

    f[0] = 0;
    f[1] = 1;
    for (int i = 2; i <= k; ++i)
        f[i] = f[i - 1] + f[i - 2];

    result = f[k];
    kfree(f);
    return result;
}

static long long fibseq_fixedla(long long k)
{
    long long f[2] = {0, 1};

    for (int i = 2; i <= k; ++i)
        f[i & 0x1] += f[(i - 1) & 0x1];

    return f[k & 0x1];
}

static long long fibseq_exactsolv2(long long k)
{
    if (unlikely(k < 2))
        return k;

    long long a = 1, b = 1;
    for (int i = 2; i <= k; ++i) {
        long long tmp_a = (a + 5 * b) >> 1;
        b = (a + b) >> 1;
        a = tmp_a;
    }

    return b;
}

static long long fibseq_exactsolv3(long long k)
{
    if (unlikely(k < 2))
        return k;

    long long a = 1, b = 1;
    for (int i = 2; i <= k; ++i) {
        long long old_b = b;
        b = (a & b) + ((a ^ b) >> 1);
        a = (old_b << 1) + b;
    }

    return b;
}

static long long fibseq_fastdoubling_loop62(long long k)
{
    if (unlikely(k < 2))
        return k;

    /* find the left-most bit */
    unsigned long long mask = 1ULL << 62;
    while (!(k & mask))
        mask >>= 1;

    /* fast doubling */
    long long a = 0, b = 1;
    while (mask) {
        /* times 2 */
        long long tmp = a;
        a = a * ((b << 1) - a);
        b = tmp * tmp + b * b;

        /* plus 1 */
        if (k & mask) {
            tmp = b;
            b += a;
            a = tmp;
        }
        mask >>= 1;
    }
    return a;
}

#define DECLARE_FASTDBL_LOOP(NLOOP)                               \
    static long long fibseq_fastdoubling_loop##NLOOP(long long k) \
    {                                                             \
        if (unlikely(k < 2))                                      \
            return k;                                             \
        unsigned mask = 1U << NLOOP;                              \
        while (!(k & mask))                                       \
            mask >>= 1;                                           \
        long long a = 0, b = 1;                                   \
        while (mask) {                                            \
            long long tmp = a;                                    \
            a = a * ((b << 1) - a);                               \
            b = tmp * tmp + b * b;                                \
            if (k & mask) {                                       \
                tmp = b;                                          \
                b += a;                                           \
                a = tmp;                                          \
            }                                                     \
            mask >>= 1;                                           \
        }                                                         \
        return a;                                                 \
    }

DECLARE_FASTDBL_LOOP(31);
DECLARE_FASTDBL_LOOP(16);
DECLARE_FASTDBL_LOOP(6);

static long long fibseq_fastdoubling_fls(long long k)
{
    if (unlikely(k < 2))
        return k;

    /* find the left-most bit */
    unsigned mask = 1U << (fls((unsigned) k) - 1);

    /* fast doubling */
    long long a = 0, b = 1;
    while (mask) {
        /* times 2 */
        long long tmp = a;
        a = a * ((b << 1) - a);
        b = tmp * tmp + b * b;

        /* plus 1 */
        if (k & mask) {
            tmp = b;
            b += a;
            a = tmp;
        }
        mask >>= 1;
    }
    return a;
}

static long long fibseq_fastdoubling_clz(long long k)
{
    if (unlikely(k < 2))
        return k;

    /* find the left-most bit */
    unsigned mask = 1U << (31 - __builtin_clz((unsigned) k));

    /* fast doubling */
    long long a = 0, b = 1;
    while (mask) {
        /* times 2 */
        long long tmp = a;
        a = a * ((b << 1) - a);
        b = tmp * tmp + b * b;

        /* plus 1 */
        if (k & mask) {
            tmp = b;
            b += a;
            a = tmp;
        }
        mask >>= 1;
    }
    return a;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

#if 0
/* For escaping the warning in macro BNFIB_KTIME.
 * warning: ignoring return value of ‘copy_to_user’, declared with attribute
 * warn_unused_result [-Wunused-result] */
__attribute__((always_inline)) static inline void escape(void *p)
{
    __asm__ volatile ("" : : "g"(p) : "memory");
}
#endif

static char *(*const bn_print[])(const fbn *) = {
    fbn_print,   /* 0 */
    fbn_printv1, /* 1 */
};
#define BN_PRINT 1

static void (*const bn_fibonacci_seq[])(fbn *, int) = {
    fbn_fib_defi,
    fbn_fib_fastdoubling,
    fbn_fib_fastdoublingv1,
};

#define BNFIB_KTIME(bnfib_method, k, buf)       \
    ({                                          \
        fbn *fib = fbn_alloc(1);                \
        ktime_t kt = ktime_get();               \
        bn_fibonacci_seq[bnfib_method](fib, k); \
        kt = ktime_sub(ktime_get(), kt);        \
        fbn_free(fib);                          \
        ktime_to_ns(kt);                        \
    })

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t method,
                        loff_t *offset)
{
#ifndef _FBN_DEBUG
#ifdef _TEST_KTIME
    return (ssize_t) BNFIB_KTIME(method, *offset, buf);
#else
    fbn *fib = fbn_alloc(1);
    bn_fibonacci_seq[method](fib, *offset);
    char *str = bn_print[BN_PRINT](fib);
    ssize_t left = copy_to_user(buf, str, strlen(str) + 1);
    kfree(str);
    fbn_free(fib);
    return left;
#endif /* _TEST_KTIME */
#else  /* defined(_FBN_DEBUG) */
    pr_info("fibdrv_debug: <test bn_fib>");
    fbn *a = fbn_alloc(1);
    char *str = NULL;

    for (int i = 0; i < 5; ++i) {
        fbn_fib_fastdoublingv1(a, i);
        str = fbn_printv1(a);
        pr_info("fibdrv_debug: str %s\n", str);
        kfree(str);
    }
    fbn_lshift(a, 32);
    fbndebug_printhex(a);

    fbn_free(a);
    return 0;
#endif /* _FBN_DEBUG */
}

static long long (*const fibonacci_seq[])(long long) = {
    fib_sequence,               /* 0 */
    fibseq_kmalloc,             /* 1 */
    fibseq_fixedla,             /* 2 */
    fibseq_exactsolv2,          /* 3 */
    fibseq_exactsolv3,          /* 4 */
    fibseq_fastdoubling_loop62, /* 5 */
    fibseq_fastdoubling_loop31, /* 5 */
    fibseq_fastdoubling_loop16, /* 6 */
    fibseq_fastdoubling_loop6,  /* 7 */
    fibseq_fastdoubling_fls,    /* 8 */
    fibseq_fastdoubling_clz,    /* 9 */
};

#define FIB_KTIME(fib_method, k)         \
    ({                                   \
        ktime_t kt = ktime_get();        \
        fibonacci_seq[fib_method](k);    \
        kt = ktime_sub(ktime_get(), kt); \
        ktime_to_ns(kt);                 \
    })

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t method,
                         loff_t *offset)
{
#ifdef _TEST_KTIME
    return (ssize_t) FIB_KTIME(method, *offset);
#else
    return fibonacci_seq[method](*offset);
#endif /* _TEST_KTIME */
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    cdev_init(fib_cdev, &fib_fops);
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
