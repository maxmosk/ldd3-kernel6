#include "../include/scullc.h"

#include "../include/scullc_dev_common.h"

#include <linux/fs.h>

static int scullc_open(struct inode* inode, struct file* filp)
{
    pr_info("scullc_open called\n");
    struct scullc* dev;

    dev = container_of(inode->i_cdev, struct scullc, cdev);
    filp->private_data = dev;

    return scullc_dev_open(&dev->dev, filp);
}

static int scullc_release(struct inode* inode, struct file* filp)
{
    pr_info("scullc_release called\n");
    struct scullc* dev = filp->private_data;
    return scullc_dev_release(&dev->dev, filp);
}

static ssize_t scullc_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullc_read called\n");
    struct scullc* dev = filp->private_data;
    return scullc_dev_read(&dev->dev, buf, count, f_pos);
}

static ssize_t scullc_write(struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullc_write called\n");
    struct scullc* dev = filp->private_data;
    return scullc_dev_write(&dev->dev, buf, count, f_pos);
}

static loff_t scullc_llseek(struct file* filp, loff_t off, int whence)
{
    pr_info("scullc_llseek called\n");
    struct scullc* dev = filp->private_data;
    return scullc_dev_llseek(&dev->dev, filp, off, whence);
}

static struct file_operations scullc_fops = {
    .owner = THIS_MODULE,
    .llseek = scullc_llseek,
    .read = scullc_read,
    .write = scullc_write,
    .open = scullc_open,
    .release = scullc_release,
};

void init_scullc(struct scullc* scullc_device, int quantum_size, struct kmem_cache* cache)
{
    construct_scullc_dev(&scullc_device->dev, quantum_size, cache);

    cdev_init(&scullc_device->cdev, &scullc_fops);
    scullc_device->cdev.owner = THIS_MODULE;
}

void clean_scullc(struct scullc* scullc_device)
{
    scullc_trim(&scullc_device->dev);
    cdev_del(&scullc_device->cdev);
}
