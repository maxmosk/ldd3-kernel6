#include "main.h"
#include "fops.h"

int scull_quantum = (int)SCULL_QUANTUM;
int scull_qset = (int)SCULL_QSET;

int scull_trim(struct scull_dev* dev)
{
    pr_info("scull_trim called\n");
    struct scull_qset *dptr, *next;
    int qset = dev->qset;
    int i;

    for (dptr = dev->data; dptr; dptr = next) {
        if (dptr->data) {
            for (i = 0; i < qset; ++i)
                kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }

    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;
    return 0;
}

static struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
    pr_info("scull_follow called\n");
    struct scull_qset *qs = dev->data;  
    /* Allocate first qset explicitly if need be */
    if (!qs) {
        qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        if (qs == NULL)
            return NULL;  /* Never mind */  
        memset(qs, 0, sizeof(struct scull_qset));
    }   
    while (n--) {
        if (!qs->next) {
    	    qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
    	    if (qs->next == NULL)
                return NULL;
            memset(qs->next, 0, sizeof(struct scull_qset));
        }
        qs = qs->next;
        continue;
    }
    return qs;
}

ssize_t scull_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    pr_info("scull_read called\n");
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr; /* the first listitem */
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset; /* how many bytes in the listitem */
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    if (*f_pos >= dev->size) {
        up(&dev->sem);
        return retval;
    }

    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    dptr = scull_follow(dev, item);

    if (dptr == NULL || !dptr->data || !dptr->data[s_pos]) {
        up(&dev->sem);
        return retval;
    }

    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
        retval = -EFAULT;
        up(&dev->sem);
        return retval;
    }
    *f_pos += count;
    retval = count;

    up(&dev->sem);
    return retval;
}

ssize_t scull_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    pr_info("scull_write called\n");
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t retval = -ENOMEM;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    dptr = scull_follow(dev, item);

    if (dptr == NULL) {
        up(&dev->sem);
        return retval;
    }

    if (!dptr->data) {
        dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (!dptr->data) {
            up(&dev->sem);
            return retval;
        }

        memset(dptr->data, 0, qset * sizeof(char *));
    }    

    if (!dptr->data[s_pos]) {
        dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
        if (!dptr->data[s_pos]) {
            up(&dev->sem);
            return retval;
        }
    }

    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_from_user(dptr->data[s_pos]+q_pos, buf, count)) {
        retval = -EFAULT;
        up(&dev->sem);
        return retval;
    }

    *f_pos += count;
    retval = count;

    if (dev->size < *f_pos)
        dev->size = *f_pos;

    up(&dev->sem);
    return retval;
}

int scull_open (struct inode *inode, struct file *filp)
{
    pr_info("scull_open called\n");
    struct scull_dev *dev;

    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev;

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;

        scull_trim(dev);
        up(&dev->sem);
    }

    return 0;
}

int scull_release (struct inode *, struct file *)
{
    pr_info("scull_release called\n");
    return 0;
}

long scull_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
    pr_info("scull_ioctl called\n");

    int retval = 0;
    int err = 0, tmp;

    if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC)
        return -ENOTTY;

    if (_IOC_NR(cmd) > SCULL_IOC_MAXNR)
        return -ENOTTY;

    //access_ok is changed look at uaccess.h

    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));

    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));

    if (err)
        return -EFAULT;

    if (! capable (CAP_SYS_ADMIN))
        return -EPERM;

    switch (cmd) {

        case SCULL_IOCRESET:
            pr_info("IOCTL reset called\n");
            scull_quantum = SCULL_QUANTUM;
            scull_qset = SCULL_QSET;
            break;

        case SCULL_IOCSQUANTUM:
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            retval = __get_user(scull_quantum, (int __user *)arg);
            break;

        case SCULL_IOCTQUANTUM:
            pr_info("IOCTL tell quantum called\n");
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            scull_quantum = arg;
            break;

        case SCULL_IOCGQUANTUM:
            retval = __put_user(scull_quantum, (int __user *)arg);
            break;

        case SCULL_IOCQQUANTUM:
            pr_info("IOCTL query quantum called, qunatum is: %d", scull_quantum);
            return scull_quantum;

        case SCULL_IOCXQUANTUM:
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_quantum;
            retval = __get_user(scull_quantum, (int __user *)arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user *)arg);
            break;

        case SCULL_IOCHQUANTUM:
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_quantum;
            scull_quantum = arg;
            return tmp;

        default:
            return -ENOTTY;
    }

    return retval;
}