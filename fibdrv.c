#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static const long long BASE = 100000000;  // = 10^8,bits per part = 7

#define part_num 4
typedef struct bigNum {
    long long part[part_num];
} bigNum;
bigNum *big_add(bigNum a, bigNum b)
{
    bigNum *result = kmalloc(sizeof(bigNum), GFP_KERNEL);
    long long carry = 0;
    for (int i = 0; i < part_num; i++) {
        long long tmp = carry + a.part[i] + b.part[i];
        result->part[i] = tmp % BASE;
        carry = tmp / BASE;
    }
    return result;
}


bigNum *mul(bigNum a, bigNum b)
{
    bigNum *result = kmalloc(sizeof(bigNum), GFP_KERNEL);
    for (int i = 0; i < part_num; i++) {
        result->part[i] = 0;
    }
    for (int i = 0; i < part_num; i++) {
        long long carry = 0;
        for (int j = 0; i + j < part_num; j++) {
            long long tmp = a.part[i] * b.part[j] + carry + result->part[i + j];
            result->part[i + j] = tmp % BASE;
            carry = tmp / BASE;
        }
    }
    return result;
}

static void matrix_mult(long long m[2][2], long long n[2][2])
{
    long long m00 = m[0][0] * n[0][0] + m[0][1] * n[1][0];
    long long m01 = m[0][0] * n[0][1] + m[0][1] * n[1][1];
    long long m10 = m[1][0] * n[0][0] + m[1][1] * n[1][0];
    long long m11 = m[1][0] * n[0][1] + m[1][1] * n[1][1];
    m[0][0] = m00;
    m[0][1] = m01;
    m[1][0] = m10;
    m[1][1] = m11;
}


static long long fib_sequence_qmatrix(long long k)  // qmatrix
{
    if (k == 0)
        return 0;

    long long fn[2][2] = {{1, 1}, {1, 0}};
    long long f1[2][2] = {{1, 1}, {1, 0}};
    int log = 100;
    int stack[log];
    int n = -1;

    while (k > 1) {
        if (k % 2 == 1) {
            stack[++n] = 0;
        }
        stack[++n] = 1;
        k /= 2;
    }
    for (int i = n; i >= 0; i--) {
        if (!stack[i]) {
            matrix_mult(fn, f1);
        } else {
            matrix_mult(fn, fn);
        }
    }
    return fn[0][1];
}

static long long fib_sequence_fb(long long n)  // Fast doubling
{
    if (n == 0)
        return 0;
    long long t0 = 1;  // F(n)
    long long t1 = 1;  // F(n + 1)
    long long t3 = 1;  // F(2n)
    long long t4;      // F(2n+1)
    int i = 1;
    while (i < n) {
        if ((i << 1) <= n) {
            t4 = t1 * t1 + t0 * t0;
            t3 = t0 * (2 * t1 - t0);
            t0 = t3;
            t1 = t4;
            i = i << 1;
        } else {
            t0 = t3;
            t3 = t4;
            t4 = t0 + t4;
            i++;
        }
    }
    return t3;
}

static long long fib_sequence(long long k)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
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

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    ktime_t start, end;
    ssize_t fibval;
    char tmpbuf[20];
    start = ktime_get();
    // fibval = fib_sequence(*offset);
    fibval = fib_sequence_qmatrix(*offset);
    end = ktime_get();

    sprintf(tmpbuf, "%llu", ktime_to_ns(ktime_sub(end, start)));
    copy_to_user(buf, tmpbuf, 20);
    return fibval;
    // return (ssize_t) fib_sequence(*offset);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
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
