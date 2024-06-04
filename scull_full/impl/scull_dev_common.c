#include "../include/scull_dev_common.h"

#include <linux/fs.h>

#define DEFAULT_SCULL_QUANTUM 4000
#define DEFAULT_SCULL_QSET 1000

#define SCULL_IOC_MAGIC 'k'
#define SCULL_IOCRESET _IO(SCULL_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */

#define SCULL_IOCSQUANTUM _IOW(SCULL_IOC_MAGIC, 1, int)
#define SCULL_IOCSQSET _IOW(SCULL_IOC_MAGIC, 2, int)
#define SCULL_IOCTQUANTUM _IO(SCULL_IOC_MAGIC, 3)
#define SCULL_IOCTQSET _IO(SCULL_IOC_MAGIC, 4)
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC, 5, int)
#define SCULL_IOCGQSET _IOR(SCULL_IOC_MAGIC, 6, int)
#define SCULL_IOCQQUANTUM _IO(SCULL_IOC_MAGIC, 7)
#define SCULL_IOCQQSET _IO(SCULL_IOC_MAGIC, 8)
#define SCULL_IOCXQUANTUM _IOWR(SCULL_IOC_MAGIC, 9, int)
#define SCULL_IOCXQSET _IOWR(SCULL_IOC_MAGIC, 10, int)
#define SCULL_IOCHQUANTUM _IO(SCULL_IOC_MAGIC, 11)
#define SCULL_IOCHQSET _IO(SCULL_IOC_MAGIC, 12)
#define SCULL_IOC_MAXNR 13

/*
 * Follow the list
 */
static struct scull_qset* scull_follow(struct scull_dev* scull_device, int n)
{
    pr_info("scull_follow called\n");
    struct scull_qset* qs = scull_device->data;

    /* Allocate first qset explicitly if need be */
    if (!qs) {
        qs = scull_device->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        if (qs == NULL)
            return NULL; /* Never mind */
        memset(qs, 0, sizeof(struct scull_qset));
    }

    /* Then follow the list */
    while (n--) {
        if (!qs->next) {
            qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (qs->next == NULL)
                return NULL; /* Never mind */
            memset(qs->next, 0, sizeof(struct scull_qset));
        }
        qs = qs->next;
        continue;
    }
    return qs;
}

/*
 * Empty out the scull device; must be called with the device
 * semaphore held.
 */
int scull_trim(struct scull_dev* dev)
{
    pr_info("scull_trim called\n");
    struct scull_qset *dptr, *next;
    int qset = dev->qset; /* "dev" is not-null */
    int i;

    for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
        if (dptr->data) {
            for (i = 0; i < qset; ++i) {
                kfree(dptr->data[i]);
            }
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }

    dev->size = 0;
    dev->quantum = dev->new_quantum;
    dev->qset = dev->new_qset;
    dev->data = NULL;
    return 0;
}

/*
 * Open and close
 */
int scull_dev_open(struct scull_dev* dev, struct file* filp)
{
    pr_info("scull_dev_open called\n");
    /* now trim to 0 the length of the device if open was write-only */
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (mutex_lock_interruptible(&dev->mutex)) {
            return -ERESTARTSYS;
        }

        scull_trim(dev); /* ignore errors */
        mutex_unlock(&dev->mutex);
    }
    return 0; /* success */
}

int scull_dev_release(struct scull_dev* dev, struct file* filp)
{
    return 0;
}

/*
 * Data management: read and write
 */
ssize_t scull_dev_read(struct scull_dev* dev, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_dev_read called\n");

    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }

    struct scull_qset* dptr; /* the first listitem */
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset; /* how many bytes in the listitem */
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;

    if (*f_pos >= dev->size) {
        goto out;
    }

    if (*f_pos + count > dev->size) {
        count = dev->size - *f_pos;
    }

    /* find listitem, qset index, and offset in the quantum */
    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    /* follow the list up to the right position (defined elsewhere) */
    dptr = scull_follow(dev, item);

    if (dptr == NULL || !dptr->data || !dptr->data[s_pos]) {
        goto out; /* don't fill holes */
    }

    /* read only up to the end of this quantum */
    if (count > quantum - q_pos) {
        count = quantum - q_pos;
    }

    if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;

out:
    mutex_unlock(&dev->mutex);
    return retval;
}

ssize_t scull_dev_write(struct scull_dev* dev, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_dev_write called\n");

    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }

    struct scull_qset* dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t retval = -ENOMEM; /* value used in "goto out" statements */

    /* find listitem, qset index and offset in the quantum */
    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    /* follow the list up to the right position */
    dptr = scull_follow(dev, item);

    if (dptr == NULL) {
        goto out;
    }

    if (!dptr->data) {
        dptr->data = kmalloc(qset * sizeof(char*), GFP_KERNEL);
        if (!dptr->data) {
            goto out;
        }

        memset(dptr->data, 0, qset * sizeof(char*));
    }

    if (!dptr->data[s_pos]) {
        dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
        if (!dptr->data[s_pos]) {
            goto out;
        }
    }

    /* write only up to the end of this quantum */
    if (count > quantum - q_pos) {
        count = quantum - q_pos;
    }

    if (copy_from_user(dptr->data[s_pos] + q_pos, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;
    retval = count;

    /* update the size */
    if (dev->size < *f_pos) {
        dev->size = *f_pos;
    }

out:
    mutex_unlock(&dev->mutex);
    return retval;
}

/*
 * The ioctl() implementation
 */
long scull_dev_ioctl(struct scull_dev* dev, unsigned int cmd, unsigned long arg)
{
    pr_info("scull_dev_ioctl called\n");

    int retval = 0;
    int err = 0, tmp;

    /*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
    if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) {
        return -ENOTTY;
    }
    if (_IOC_NR(cmd) >= SCULL_IOC_MAXNR) {
        return -ENOTTY;
    }

    /*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
    if (_IOC_DIR(cmd) & _IOC_READ) {
        err = !access_ok((void __user*)arg, _IOC_SIZE(cmd));
    }

    else if (_IOC_DIR(cmd) & _IOC_WRITE) {
        err = !access_ok((void __user*)arg, _IOC_SIZE(cmd));
    }

    if (err) {
        return -EFAULT;
    }

    if (!capable(CAP_SYS_ADMIN)) {
        return -EPERM;
    }

    switch (cmd) {
        case SCULL_IOCRESET:
            pr_info("IOCTL reset called\n");
            dev->new_quantum = DEFAULT_SCULL_QUANTUM;
            dev->new_qset = DEFAULT_SCULL_QSET;
            break;

        case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
            if (!capable(CAP_SYS_ADMIN)) {
                return -EPERM;
            }
            retval = __get_user(dev->new_quantum, (int __user*)arg);
            break;

        case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
            pr_info("IOCTL tell quantum called\n");
            if (!capable(CAP_SYS_ADMIN)) {
                return -EPERM;
            }
            dev->new_quantum = arg;
            break;

        case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
            retval = __put_user(dev->new_quantum, (int __user*)arg);
            break;

        case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
            pr_info("IOCTL query quantum called, qunatum is: %d", dev->new_quantum);
            return dev->new_quantum;

        case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */
            if (!capable(CAP_SYS_ADMIN)) {
                return -EPERM;
            }
            tmp = dev->new_quantum;
            retval = __get_user(dev->new_quantum, (int __user*)arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user*)arg);
            break;

        case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */
            if (!capable(CAP_SYS_ADMIN)) {
                return -EPERM;
            }
            tmp = dev->new_quantum;
            dev->new_quantum = arg;
            return tmp;

        default: /* redundant, as cmd was checked against MAXNR */
            return -ENOTTY;
    }

    return retval;
}

/*
 * The "extended" operations -- only seek
 */
loff_t scull_dev_llseek(struct scull_dev* dev, struct file* filp, loff_t off, int whence)
{
    pr_info("scull_llseek called\n");

    loff_t newpos;

    switch (whence) {
        case 0: /* SEEK_SET */
            newpos = off;
            break;
        case 1: /* SEEK_CUR */
            newpos = filp->f_pos + off;
            break;
        case 2: /* SEEK_END */
            newpos = dev->size + off;
            break;
        default: /* can't be happened */
            return -EINVAL;
    }

    if (newpos < 0)
        return -EINVAL;

    filp->f_pos = newpos;
    return newpos;
}

void construct_scull_dev(struct scull_dev* scull_device)
{
    memset(scull_device, 0, sizeof(struct scull_dev));
    scull_device->new_quantum = DEFAULT_SCULL_QUANTUM;
    scull_device->new_qset = DEFAULT_SCULL_QSET;
    scull_device->quantum = DEFAULT_SCULL_QUANTUM;
    scull_device->qset = DEFAULT_SCULL_QSET;
    mutex_init(&scull_device->mutex);
}
