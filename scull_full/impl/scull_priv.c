#include "../include/scull_priv.h"

#include "../include/scull_dev_common.h"

#include <linux/fs.h>
#include <linux/tty.h>

struct scull_listitem {
    struct scull_dev dev;
    dev_t key;
    struct list_head list;
};

/* Look for a device or create one if missing */
static struct scull_dev* scull_priv_lookfor_device(struct scull_priv* scull_device, dev_t key)
{
    struct scull_listitem* lptr;

    list_for_each_entry(lptr, &scull_device->list, list)
    {
        if (lptr->key == key) {
            return &(lptr->dev);
        }
    }

    /* not found */
    lptr = kmalloc(sizeof(struct scull_listitem), GFP_KERNEL);
    if (!lptr) {
        return NULL;
    }

    /* initialize the device */
    memset(lptr, 0, sizeof(struct scull_listitem));
    lptr->key = key;
    scull_trim(&(lptr->dev)); /* initialize it */
    mutex_init(&(lptr->dev.mutex));

    /* place it in the list */
    list_add(&lptr->list, &scull_device->list);

    return &(lptr->dev);
}

static int scull_priv_open(struct inode* inode, struct file* filp)
{
    pr_info("scull_priv_open called\n");
    struct scull_priv* priv;
    priv = container_of(inode->i_cdev, struct scull_priv, cdev);

    struct scull_dev* dev;
    dev_t key;

    if (!current->signal->tty) {
        pr_debug("Process \"%s\" has no ctl tty\n", current->comm);
        return -EINVAL;
    }
    key = tty_devnum(current->signal->tty);

    /* look for a scull device in the list */
    spin_lock(&priv->lock);
    dev = scull_priv_lookfor_device(priv, key);
    spin_unlock(&priv->lock);

    if (!dev) {
        return -ENOMEM;
    }

    filp->private_data = dev;
    return scull_dev_open(dev, filp);
}

static int scull_priv_release(struct inode* inode, struct file* filp)
{
    pr_info("scull_priv_release called\n");
    return 0;
}

static ssize_t scull_priv_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_priv_read called\n");
    struct scull_dev* dev = filp->private_data;
    return scull_dev_read(dev, buf, count, f_pos);
}

static ssize_t scull_priv_write(
    struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_priv_write called\n");
    struct scull_dev* dev = filp->private_data;
    return scull_dev_write(dev, buf, count, f_pos);
}

static long scull_priv_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    pr_info("scull_priv_ioctl called\n");
    struct scull_dev* dev = filp->private_data;
    return scull_dev_ioctl(dev, cmd, arg);
}

static loff_t scull_priv_llseek(struct file* filp, loff_t off, int whence)
{
    pr_info("scull_priv_llseek called\n");
    struct scull_dev* dev = filp->private_data;
    return scull_dev_llseek(dev, filp, off, whence);
}

static struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .llseek = scull_priv_llseek,
    .read = scull_priv_read,
    .write = scull_priv_write,
    .unlocked_ioctl = scull_priv_ioctl,
    .open = scull_priv_open,
    .release = scull_priv_release,
};

void init_scull_priv(struct scull_priv* scull_device)
{
    memset(scull_device, 0, sizeof(struct scull_priv));
    spin_lock_init(&scull_device->lock);
    INIT_LIST_HEAD(&scull_device->list);

    cdev_init(&scull_device->cdev, &scull_fops);
    scull_device->cdev.owner = THIS_MODULE;
}

void clean_scull_priv(struct scull_priv* scull_device)
{
    struct scull_listitem *lptr, *next;
    list_for_each_entry_safe(lptr, next, &scull_device->list, list)
    {
        list_del(&lptr->list);
        scull_trim(&(lptr->dev));
        kfree(lptr);
    }
    cdev_del(&scull_device->cdev);
}
