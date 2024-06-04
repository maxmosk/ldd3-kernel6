#include "../include/scull_single.h"

#include "../include/scull_dev_common.h"

#include <linux/fs.h>

static int scull_single_open(struct inode* inode, struct file* filp)
{
    pr_info("scull_single_open called\n");
    struct scull_single* dev;

    dev = container_of(inode->i_cdev, struct scull_single, cdev);
    filp->private_data = dev;

    if (!atomic_dec_and_test(&dev->available)) {
        atomic_inc(&dev->available);
        return -EBUSY; /* already open */
    }

    int res = scull_dev_open(&dev->dev, filp);
    if (res != 0) {
        atomic_inc(&dev->available);
    }
    return res;
}

static int scull_single_release(struct inode* inode, struct file* filp)
{
    pr_info("scull_single_release called\n");
    struct scull_single* dev = filp->private_data;
    atomic_inc(&dev->available); /* release the device */
    return scull_dev_release(&dev->dev, filp);
}

static ssize_t scull_single_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_single_read called\n");
    struct scull_single* dev = filp->private_data;
    return scull_dev_read(&dev->dev, buf, count, f_pos);
}

static ssize_t scull_single_write(
    struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_single_write called\n");
    struct scull_single* dev = filp->private_data;
    return scull_dev_write(&dev->dev, buf, count, f_pos);
}

static long scull_single_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    pr_info("scull_single_ioctl called\n");
    struct scull_single* dev = filp->private_data;
    return scull_dev_ioctl(&dev->dev, cmd, arg);
}

static loff_t scull_single_llseek(struct file* filp, loff_t off, int whence)
{
    pr_info("scull_single_llseek called\n");
    struct scull_single* dev = filp->private_data;
    return scull_dev_llseek(&dev->dev, filp, off, whence);
}

static struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .llseek = scull_single_llseek,
    .read = scull_single_read,
    .write = scull_single_write,
    .unlocked_ioctl = scull_single_ioctl,
    .open = scull_single_open,
    .release = scull_single_release,
};

void init_scull_single(struct scull_single* scull_device)
{
    memset(scull_device, 0, sizeof(struct scull_single));
    construct_scull_dev(&scull_device->dev);
    atomic_set(&scull_device->available, 1);

    cdev_init(&scull_device->cdev, &scull_fops);
    scull_device->cdev.owner = THIS_MODULE;
}

void clean_scull_single(struct scull_single* scull_device)
{
    scull_trim(&scull_device->dev);
    cdev_del(&scull_device->cdev);
}
