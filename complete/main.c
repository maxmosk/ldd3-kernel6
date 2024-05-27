#include <linux/cdev.h>
#include <linux/device/class.h>
#include <linux/fs.h>
#include <linux/init.h>

#include <linux/version.h>

MODULE_LICENSE("GPL");

static int major;
static struct class* cls;
static struct cdev cdev;

DECLARE_COMPLETION(comp);

ssize_t complete_read(struct file* filp, char __user* buf, size_t count, loff_t* pos)
{
    printk(KERN_DEBUG "process %i (%s) going to sleep\n", current->pid, current->comm);
    wait_for_completion(&comp);
    printk(KERN_DEBUG "awoken %i (%s)\n", current->pid, current->comm);
    return 0; /* EOF */
}

ssize_t complete_write(struct file* filp, const char __user* buf, size_t count, loff_t* pos)
{
    printk(KERN_DEBUG "process %i (%s) awakening the readers...\n", current->pid, current->comm);
    complete(&comp);
    return count; /* succeed, to avoid retrial */
}

static struct file_operations complete_file_fops = {
    .owner = THIS_MODULE,
    .read = complete_read,
    .write = complete_write,
};

void cleanup(void)
{
    cdev_del(&cdev);
    device_destroy(cls, MKDEV(major, 0));
    pr_info("Device complete destroyed\n");
    class_destroy(cls);
    pr_info("Class destroyed\n");
    unregister_chrdev_region(MKDEV(major, 0), 1);
    pr_info("Complete device unregistered\n");
}

static int __init init(void)
{
    dev_t dev;
    int res = alloc_chrdev_region(&dev, 0, 1, "complete");

    if (res < 0) {
        pr_alert("Cannot register complete device\n");
        return res;
    }

    major = MAJOR(dev);
    pr_info("Assigned major number: %d\n", major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create("complete");
#else
    cls = class_create(THIS_MODULE, "complete");
#endif
    pr_info("Class created\n");

    device_create(cls, NULL, MKDEV(major, 0), NULL, "complete");
    pr_info("Device complete created\n");
    cdev_init(&cdev, &complete_file_fops);
    cdev.owner = THIS_MODULE;

    res = cdev_add(&cdev, MKDEV(major, 0), 1);
    if (res) {
        pr_notice("Can't add complete");
    }

    return 0;
}

module_init(init);
module_exit(cleanup);
