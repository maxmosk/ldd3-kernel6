#include <linux/cdev.h>
#include <linux/device/class.h>
#include <linux/fs.h>
#include <linux/init.h>

#include <linux/version.h>

MODULE_LICENSE("GPL");

static int major;
static struct class* cls;
static struct cdev cdev;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag = 0;
ssize_t sleepy_read(struct file* filp, char __user* buf, size_t count, loff_t* pos)
{
    printk(KERN_DEBUG "process %i (%s) going to sleep\n", current->pid, current->comm);
    wait_event_interruptible(wq, flag != 0);
    flag = 0;
    printk(KERN_DEBUG "awoken %i (%s)\n", current->pid, current->comm);
    return 0; /* EOF */
}

ssize_t sleepy_write(struct file* filp, const char __user* buf, size_t count, loff_t* pos)
{
    printk(KERN_DEBUG "process %i (%s) awakening the readers...\n", current->pid, current->comm);
    flag = 1;
    wake_up_interruptible(&wq);
    return count; /* succeed, to avoid retrial */
}

static struct file_operations sleepy_file_fops = {
    .owner = THIS_MODULE,
    .read = sleepy_read,
    .write = sleepy_write,
};

void cleanup(void)
{
    cdev_del(&cdev);
    device_destroy(cls, MKDEV(major, 0));
    pr_info("Device sleepy destroyed\n");
    class_destroy(cls);
    pr_info("Class destroyed\n");
    unregister_chrdev_region(MKDEV(major, 0), 1);
    pr_info("Sleepy device unregistered\n");
}

static int __init init(void)
{
    dev_t dev;
    int res = alloc_chrdev_region(&dev, 0, 1, "sleepy");

    if (res < 0) {
        pr_alert("Cannot register sleepy device\n");
        return res;
    }

    major = MAJOR(dev);
    pr_info("Assigned major number: %d\n", major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create("sleepy");
#else
    cls = class_create(THIS_MODULE, "sleepy");
#endif
    pr_info("Class created\n");

    device_create(cls, NULL, MKDEV(major, 0), NULL, "sleepy");
    pr_info("Device sleepy created\n");
    cdev_init(&cdev, &sleepy_file_fops);
    cdev.owner = THIS_MODULE;

    res = cdev_add(&cdev, MKDEV(major, 0), 1);
    if (res) {
        pr_notice("Can't add sleepy");
    }

    return 0;
}

module_init(init);
module_exit(cleanup);
