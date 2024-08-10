#include "../include/scullv.h"

#include "../include/scullv_dev_common.h"

#include <linux/fs.h>

static int scullv_open(struct inode* inode, struct file* filp)
{
    pr_info("scullv_open called\n");
    struct scullv* dev;

    dev = container_of(inode->i_cdev, struct scullv, cdev);
    filp->private_data = dev;

    return scullv_dev_open(&dev->dev, filp);
}

static int scullv_release(struct inode* inode, struct file* filp)
{
    pr_info("scullv_release called\n");
    struct scullv* dev = filp->private_data;
    return scullv_dev_release(&dev->dev, filp);
}

static ssize_t scullv_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullv_read called\n");
    struct scullv* dev = filp->private_data;
    return scullv_dev_read(&dev->dev, buf, count, f_pos);
}

static ssize_t scullv_write(struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullv_write called\n");
    struct scullv* dev = filp->private_data;
    return scullv_dev_write(&dev->dev, buf, count, f_pos);
}

static loff_t scullv_llseek(struct file* filp, loff_t off, int whence)
{
    pr_info("scullv_llseek called\n");
    struct scullv* dev = filp->private_data;
    return scullv_dev_llseek(&dev->dev, filp, off, whence);
}

static struct file_operations scullv_fops = {
    .owner = THIS_MODULE,
    .llseek = scullv_llseek,
    .read = scullv_read,
    .write = scullv_write,
    .open = scullv_open,
    .release = scullv_release,
};

void init_scullv(struct scullv* scullv_device, int order)
{
    construct_scullv_dev(&scullv_device->dev, order);

    cdev_init(&scullv_device->cdev, &scullv_fops);
    scullv_device->cdev.owner = THIS_MODULE;
}

void clean_scullv(struct scullv* scullv_device)
{
    scullv_trim(&scullv_device->dev);
    cdev_del(&scullv_device->cdev);
}
