#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>

#include <linux/device/class.h> //for class_create/class_destroy
#include <linux/device.h> // for device_create/device_destroy

#include <linux/version.h> // for kenel version

#include "main.h"
#include "fops.h"

#define DEVICE_NAME "sculldev" /* Dev name as it appears in /proc/devices   */

MODULE_LICENSE("GPL");

static int major; // device major number assigned to scull device;
static struct class *cls; // device class;
struct scull_dev *scull_device;

static struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .open = scull_open,
    .read = scull_read,
    .write = scull_write,
    .release = scull_release,
};

void scull_cleanup(void)
{
    if (scull_device) {
        scull_trim(scull_device);
        cdev_del(&scull_device->cdev);
        kfree(scull_device);
    }

    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);

    unregister_chrdev(major, DEVICE_NAME);
}

static int __init scull_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &scull_fops);

    if (major < 0) {
        pr_alert("Cannot register char device with %d\n", major);
        return major;
    }

    pr_info("Assigned major number: %d.\n", major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif

    device_create(cls, NULL,  MKDEV(major, 0), NULL, DEVICE_NAME);

    scull_device = kmalloc(sizeof(struct scull_dev), GFP_KERNEL);

    if (!scull_device) {
        scull_cleanup();
        return -ENOMEM;
    }

    memset(scull_device, 0, sizeof(struct scull_dev));

    scull_device->quantum = SCULL_QUANTUM;
    scull_device->qset = SCULL_QSET;
    sema_init(&scull_device->sem, 1);
    
    cdev_init(&scull_device->cdev, &scull_fops);
    scull_device->cdev.owner = THIS_MODULE;

    int err = cdev_add(&scull_device->cdev, MKDEV(major, 0), 1);
    
    if (err)
        pr_notice("Can't add scull");

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    return 0;
}

module_init(scull_init);
module_exit(scull_cleanup);
