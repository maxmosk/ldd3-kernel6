#include "../include/scullp.h"

#include "../include/scullp_dev_common.h"

#include <linux/fs.h>

static int scullp_open(struct inode* inode, struct file* filp)
{
    pr_info("scullp_open called\n");
    struct scullp* dev;

    dev = container_of(inode->i_cdev, struct scullp, cdev);
    filp->private_data = dev;

    return scullp_dev_open(&dev->dev, filp);
}

static int scullp_release(struct inode* inode, struct file* filp)
{
    pr_info("scullp_release called\n");
    struct scullp* dev = filp->private_data;
    return scullp_dev_release(&dev->dev, filp);
}

static ssize_t scullp_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullp_read called\n");
    struct scullp* dev = filp->private_data;
    return scullp_dev_read(&dev->dev, buf, count, f_pos);
}

static ssize_t scullp_write(struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullp_write called\n");
    struct scullp* dev = filp->private_data;
    return scullp_dev_write(&dev->dev, buf, count, f_pos);
}

static loff_t scullp_llseek(struct file* filp, loff_t off, int whence)
{
    pr_info("scullp_llseek called\n");
    struct scullp* dev = filp->private_data;
    return scullp_dev_llseek(&dev->dev, filp, off, whence);
}

static struct file_operations scullp_fops = {
    .owner = THIS_MODULE,
    .llseek = scullp_llseek,
    .read = scullp_read,
    .write = scullp_write,
    .open = scullp_open,
    .release = scullp_release,
};

void init_scullp(struct scullp* scullp_device, int order)
{
    construct_scullp_dev(&scullp_device->dev, order);

    cdev_init(&scullp_device->cdev, &scullp_fops);
    scullp_device->cdev.owner = THIS_MODULE;
}

void clean_scullp(struct scullp* scullp_device)
{
    scullp_trim(&scullp_device->dev);
    cdev_del(&scullp_device->cdev);
}
