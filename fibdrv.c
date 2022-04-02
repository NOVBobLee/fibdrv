#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
//#define MAX_LENGTH 92
#define MAX_LENGTH 100

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

static inline ssize_t fibseq_vla_timer(long long k)
{
    ktime_t kt;
    kt = ktime_get();
    fib_sequence(k);
    kt = ktime_sub(ktime_get(), kt);
    return (ssize_t) ktime_to_ns(kt);
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

static inline ssize_t fibseq_kmalloc_timer(long long k)
{
    ktime_t kt;
    kt = ktime_get();
    fibseq_kmalloc(k);
    kt = ktime_sub(ktime_get(), kt);
    return (ssize_t) ktime_to_ns(kt);
}

static long long fibseq_fixedla(long long k)
{
    long long f[2] = {0, 1};

    for (int i = 2; i <= k; ++i)
        f[i & 0x1] += f[(i - 1) & 0x1];

    return f[k & 0x1];
}

static inline ssize_t fibseq_fixedla_timer(long long k)
{
    ktime_t kt;
    kt = ktime_get();
    fibseq_fixedla(k);
    kt = ktime_sub(ktime_get(), kt);
    return (ssize_t) ktime_to_ns(kt);
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

static inline ssize_t fibseq_exactsolv2_timer(long long k)
{
    ktime_t kt;
    kt = ktime_get();
    fibseq_exactsolv2(k);
    kt = ktime_sub(ktime_get(), kt);
    return (ssize_t) ktime_to_ns(kt);
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

static inline ssize_t fibseq_exactsolv3_timer(long long k)
{
    ktime_t kt;
    kt = ktime_get();
    fibseq_exactsolv3(k);
    kt = ktime_sub(ktime_get(), kt);
    return (ssize_t) ktime_to_ns(kt);
}

static long long fibseq_fastdoubling(long long k)
{
    if (unlikely(k < 2))
        return k;

    /* find the left-most bit */
    long long mask = 1ULL << __fls((unsigned long) k);

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

static inline ssize_t fibseq_fastdoubling_timer(long long k)
{
    ktime_t kt;
    kt = ktime_get();
    fibseq_fastdoubling(k);
    kt = ktime_sub(ktime_get(), kt);
    return (ssize_t) ktime_to_ns(kt);
}

#ifdef __TEST_KTIME
static ssize_t (*const fibonacci_seq[])(long long) = {
    fibseq_vla_timer,        fibseq_kmalloc_timer,    fibseq_fixedla_timer,
    fibseq_exactsolv2_timer, fibseq_exactsolv3_timer, fibseq_fastdoubling_timer,
};
#else
static long long (*const fibonacci_seq[])(long long) = {
    fib_sequence,      fibseq_kmalloc,    fibseq_fixedla,
    fibseq_exactsolv2, fibseq_exactsolv3, fibseq_fastdoubling,
};
#endif

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

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (ssize_t) fib_sequence(*offset);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t method,
                         loff_t *offset)
{
    return fibonacci_seq[method](*offset);
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
