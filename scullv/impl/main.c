#include "../include/scullv.h"

#include <linux/device/class.h>
#include <linux/fs.h>
#include <linux/init.h>

#include <linux/version.h>

#define CLASS_NAME "scullv"
#define DEVICE_FILE_NAME "scullv"

#define DEVICE_FILE_COUNT 4 /* number of bare scull devices */

#define DEFAULT_SCULLV_ORDER 4 /* one page at a time */
int scullv_order = DEFAULT_SCULLV_ORDER;
module_param(scullv_order, int, 0);

MODULE_LICENSE("GPL");

static int major;
static struct class* cls;

static struct scullv scullv_devices[DEVICE_FILE_COUNT];

void cleanup(void)
{
    size_t device_num = 0;
    for (size_t i = 0; i < DEVICE_FILE_COUNT; ++i) {
        clean_scullv(&scullv_devices[i]);
        device_destroy(cls, MKDEV(major, device_num++));
        pr_info("Device %s%li destroyed\n", DEVICE_FILE_NAME, i);
    }

    class_destroy(cls);
    pr_info("Class destroyed\n");
    unregister_chrdev_region(MKDEV(major, 0), DEVICE_FILE_COUNT);
    pr_info("Char device unregistered\n");
}

static int __init init(void)
{
    dev_t dev;
    int res = alloc_chrdev_region(&dev, 0, DEVICE_FILE_COUNT, "scullv");

    if (res < 0) {
        pr_alert("Cannot register char device\n");
        return res;
    }

    major = MAJOR(dev);
    pr_info("Assigned major number: %d\n", major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(CLASS_NAME);
#else
    cls = class_create(THIS_MODULE, CLASS_NAME);
#endif
    pr_info("Class created\n");

    size_t device_num = 0;

    for (size_t i = 0; i < DEVICE_FILE_COUNT; ++i) {
        device_create(cls, NULL, MKDEV(major, device_num++), NULL, "%s%li", DEVICE_FILE_NAME, i);
        pr_info("Device %s%li created\n", DEVICE_FILE_NAME, i);
        init_scullv(&scullv_devices[i], scullv_order);
    }

    device_num = 0;

    for (size_t i = 0; i < DEVICE_FILE_COUNT; ++i) {
        res = cdev_add(&scullv_devices[i].cdev, MKDEV(major, device_num++), 1);
        if (res) {
            pr_notice("Can't add %s%li", DEVICE_FILE_NAME, i);
        }
    }

    return 0;
}

module_init(init);
module_exit(cleanup);
