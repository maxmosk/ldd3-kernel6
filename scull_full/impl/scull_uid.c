#include "../include/scull_uid.h"

#include "../include/scull_dev_common.h"

#include <linux/fs.h>
#include <linux/sched.h>

static int scull_uid_open(struct inode* inode, struct file* filp)
{
    pr_info("scull_uid_open called\n");
    struct scull_uid* dev;

    dev = container_of(inode->i_cdev, struct scull_uid, cdev);
    filp->private_data = dev;

    spin_lock(&dev->lock);
    if (dev->count && !uid_eq(dev->owner, current->cred->uid) && /* allow user */
        !uid_eq(dev->owner, current->cred->euid) && /* allow whoever did su */
        !capable(CAP_DAC_OVERRIDE)) { /* still allow root */
        spin_unlock(&dev->lock);
        return -EBUSY; /* -EPERM would confuse the user */
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

static int scull_uid_release(struct inode* inode, struct file* filp)
{
    pr_info("scull_uid_release called\n");
    struct scull_uid* dev = filp->private_data;
    spin_lock(&dev->lock);
    --dev->count; /* nothing else */
    spin_unlock(&dev->lock);
    return scull_dev_release(&dev->dev, filp);
}

static ssize_t scull_uid_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_uid_read called\n");
    struct scull_uid* dev = filp->private_data;
    return scull_dev_read(&dev->dev, buf, count, f_pos);
}

static ssize_t scull_uid_write(
    struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_uid_write called\n");
    struct scull_uid* dev = filp->private_data;
    return scull_dev_write(&dev->dev, buf, count, f_pos);
}

static long scull_uid_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    pr_info("scull_uid_ioctl called\n");
    struct scull_uid* dev = filp->private_data;
    return scull_dev_ioctl(&dev->dev, cmd, arg);
}

static loff_t scull_uid_llseek(struct file* filp, loff_t off, int whence)
{
    pr_info("scull_uid_llseek called\n");
    struct scull_uid* dev = filp->private_data;
    return scull_dev_llseek(&dev->dev, filp, off, whence);
}

static struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .llseek = scull_uid_llseek,
    .read = scull_uid_read,
    .write = scull_uid_write,
    .unlocked_ioctl = scull_uid_ioctl,
    .open = scull_uid_open,
    .release = scull_uid_release,
};

void init_scull_uid(struct scull_uid* scull_device)
{
    memset(scull_device, 0, sizeof(struct scull_uid));
    construct_scull_dev(&scull_device->dev);
    spin_lock_init(&scull_device->lock);

    cdev_init(&scull_device->cdev, &scull_fops);
    scull_device->cdev.owner = THIS_MODULE;
}

void clean_scull_uid(struct scull_uid* scull_device)
{
    scull_trim(&scull_device->dev);
    cdev_del(&scull_device->cdev);
}
