#include "../include/scull_wuid.h"

#include "../include/scull_dev_common.h"

#include <linux/fs.h>
#include <linux/sched.h>

static inline int scull_w_available(struct scull_wuid* scull_device)
{
    return scull_device->count == 0 || uid_eq(scull_device->owner, current->cred->uid) ||
           uid_eq(scull_device->owner, current->cred->euid) || capable(CAP_DAC_OVERRIDE);
}

static int scull_wuid_open(struct inode* inode, struct file* filp)
{
    pr_info("scull_wuid_open called\n");
    struct scull_wuid* dev;

    dev = container_of(inode->i_cdev, struct scull_wuid, cdev);
    filp->private_data = dev;

    spin_lock(&dev->lock);
    while (!scull_w_available(dev)) {
        spin_unlock(&dev->lock);
        if (filp->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }
        if (wait_event_interruptible(dev->wait, scull_w_available(dev))) {
            return -ERESTARTSYS; /* tell the fs layer to handle it */
        }
        spin_lock(&dev->lock);
    }
    if (dev->count == 0) {
        dev->owner = current->cred->uid; /* grab it */
    }
    ++dev->count;
    spin_unlock(&dev->lock);

    int res = scull_dev_open(&dev->dev, filp);
    if (res != 0) {
        spin_lock(&dev->lock);
        --dev->count;
        spin_unlock(&dev->lock);
    }
    return res;
}

static int scull_wuid_release(struct inode* inode, struct file* filp)
{
    pr_info("scull_wuid_release called\n");
    struct scull_wuid* dev = filp->private_data;
    int temp;

    spin_lock(&dev->lock);
    --dev->count;
    temp = dev->count;
    spin_unlock(&dev->lock);

    if (temp == 0) {
        wake_up_interruptible_sync(&dev->wait); /* awake other uid's */
    }

    return scull_dev_release(&dev->dev, filp);
}

static ssize_t scull_wuid_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_wuid_read called\n");
    struct scull_wuid* dev = filp->private_data;
    return scull_dev_read(&dev->dev, buf, count, f_pos);
}

static ssize_t scull_wuid_write(
    struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_wuid_write called\n");
    struct scull_wuid* dev = filp->private_data;
    return scull_dev_write(&dev->dev, buf, count, f_pos);
}

static long scull_wuid_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    pr_info("scull_wuid_ioctl called\n");
    struct scull_wuid* dev = filp->private_data;
    return scull_dev_ioctl(&dev->dev, cmd, arg);
}

static loff_t scull_wuid_llseek(struct file* filp, loff_t off, int whence)
{
    pr_info("scull_wuid_llseek called\n");
    struct scull_wuid* dev = filp->private_data;
    return scull_dev_llseek(&dev->dev, filp, off, whence);
}

static struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .llseek = scull_wuid_llseek,
    .read = scull_wuid_read,
    .write = scull_wuid_write,
    .unlocked_ioctl = scull_wuid_ioctl,
    .open = scull_wuid_open,
    .release = scull_wuid_release,
};

void init_scull_wuid(struct scull_wuid* scull_device)
{
    memset(scull_device, 0, sizeof(struct scull_wuid));
    construct_scull_dev(&scull_device->dev);
    spin_lock_init(&scull_device->lock);
    init_waitqueue_head(&scull_device->wait);

    cdev_init(&scull_device->cdev, &scull_fops);
    scull_device->cdev.owner = THIS_MODULE;
}

void clean_scull_wuid(struct scull_wuid* scull_device)
{
    scull_trim(&scull_device->dev);
    cdev_del(&scull_device->cdev);
}
