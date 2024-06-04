#include "../include/scull.h"

#include "../include/scull_dev_common.h"

#include <linux/fs.h>

static int scull_open(struct inode* inode, struct file* filp)
{
    pr_info("scull_open called\n");
    struct scull* dev;

    dev = container_of(inode->i_cdev, struct scull, cdev);
    filp->private_data = dev;

    return scull_dev_open(&dev->dev, filp);
}

static int scull_release(struct inode* inode, struct file* filp)
{
    pr_info("scull_release called\n");
    struct scull* dev = filp->private_data;
    return scull_dev_release(&dev->dev, filp);
}

static ssize_t scull_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_read called\n");
    struct scull* dev = filp->private_data;
    return scull_dev_read(&dev->dev, buf, count, f_pos);
}

static ssize_t scull_write(struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_write called\n");
    struct scull* dev = filp->private_data;
    return scull_dev_write(&dev->dev, buf, count, f_pos);
}

static long scull_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    pr_info("scull_ioctl called\n");
    struct scull* dev = filp->private_data;
    return scull_dev_ioctl(&dev->dev, cmd, arg);
}

static loff_t scull_llseek(struct file* filp, loff_t off, int whence)
{
    pr_info("scull_llseek called\n");
    struct scull* dev = filp->private_data;
    return scull_dev_llseek(&dev->dev, filp, off, whence);
}

static struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .llseek = scull_llseek,
    .read = scull_read,
    .write = scull_write,
    .unlocked_ioctl = scull_ioctl,
    .open = scull_open,
    .release = scull_release,
};

void init_scull(struct scull* scull_device)
{
    construct_scull_dev(&scull_device->dev);

    cdev_init(&scull_device->cdev, &scull_fops);
    scull_device->cdev.owner = THIS_MODULE;
}

void clean_scull(struct scull* scull_device)
{
    scull_trim(&scull_device->dev);
    cdev_del(&scull_device->cdev);
}
