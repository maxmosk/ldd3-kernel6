#include "../include/scullc.h"

#include <linux/device/class.h>
#include <linux/fs.h>
#include <linux/init.h>

#include <linux/version.h>

#define CLASS_NAME "scullc"
#define CACHE_NAME "scullc"
#define DEVICE_FILE_NAME "scullc"

#define DEVICE_FILE_COUNT 4 /* number of bare scull devices */

#define DEFAULT_SCULLC_QUANTUM 4000
int scullc_quantum = DEFAULT_SCULLC_QUANTUM;
module_param(scullc_quantum, int, 0);

MODULE_LICENSE("GPL");

static int major;
static struct class* cls;

/* declare one cache pointer: use it for all devices */
struct kmem_cache* scullc_cache;

static struct scullc scullc_devices[DEVICE_FILE_COUNT];

void cleanup(void)
{
    size_t device_num = 0;
    for (size_t i = 0; i < DEVICE_FILE_COUNT; ++i) {
        clean_scullc(&scullc_devices[i]);
        device_destroy(cls, MKDEV(major, device_num++));
        pr_info("Device %s%li destroyed\n", DEVICE_FILE_NAME, i);
    }

    if (scullc_cache) {
        kmem_cache_destroy(scullc_cache);
    }

    class_destroy(cls);
    pr_info("Class destroyed\n");
    unregister_chrdev_region(MKDEV(major, 0), DEVICE_FILE_COUNT);
    pr_info("Char device unregistered\n");
}

static int __init init(void)
{
    dev_t dev;
    int res = alloc_chrdev_region(&dev, 0, DEVICE_FILE_COUNT, "scullc");

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

    scullc_cache =
        kmem_cache_create(CACHE_NAME, scullc_quantum, 0, SLAB_HWCACHE_ALIGN, NULL); /* no ctor */
    if (!scullc_cache) {
        cleanup();
        return -ENOMEM;
    }

    for (size_t i = 0; i < DEVICE_FILE_COUNT; ++i) {
        device_create(cls, NULL, MKDEV(major, device_num++), NULL, "%s%li", DEVICE_FILE_NAME, i);
        pr_info("Device %s%li created\n", DEVICE_FILE_NAME, i);
        init_scullc(&scullc_devices[i], scullc_quantum, scullc_cache);
    }

    device_num = 0;

    for (size_t i = 0; i < DEVICE_FILE_COUNT; ++i) {
        res = cdev_add(&scullc_devices[i].cdev, MKDEV(major, device_num++), 1);
        if (res) {
            pr_notice("Can't add %s%li", DEVICE_FILE_NAME, i);
        }
    }

    return 0;
}

module_init(init);
module_exit(cleanup);
