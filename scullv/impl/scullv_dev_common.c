#include "../include/scullv_dev_common.h"

#include <linux/vmalloc.h>

#include <linux/fs.h>

#define DEFAULT_SCULL_QSET 1000

/*
 * Follow the list
 */
static struct scullv_qset* scullv_follow(struct scullv_dev* scullv_device, int n)
{
    pr_info("scullv_follow called\n");
    struct scullv_qset* qs = scullv_device->data;

    /* Allocate first qset explicitly if need be */
    if (!qs) {
        qs = scullv_device->data = kmalloc(sizeof(struct scullv_qset), GFP_KERNEL);
        if (qs == NULL) {
            return NULL; /* Never mind */
        }
        memset(qs, 0, sizeof(struct scullv_qset));
    }

    /* Then follow the list */
    while (n--) {
        if (!qs->next) {
            qs->next = kmalloc(sizeof(struct scullv_qset), GFP_KERNEL);
            if (qs->next == NULL) {
                return NULL; /* Never mind */
            }
            memset(qs->next, 0, sizeof(struct scullv_qset));
        }
        qs = qs->next;
        continue;
    }
    return qs;
}

/*
 * Empty out the scullv device; must be called with the device
 * semaphore held.
 */
int scullv_trim(struct scullv_dev* dev)
{
    pr_info("scullv_trim called\n");
    struct scullv_qset *dptr, *next;
    int qset = dev->qset; /* "dev" is not-null */
    int i;

    for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
        if (dptr->data) {
            for (i = 0; i < qset; ++i) {
                if (dptr->data[i]) {
                    vfree(dptr->data[i]);
                }
            }
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }

    dev->size = 0;
    dev->data = NULL;
    return 0;
}

/*
 * Open and close
 */
int scullv_dev_open(struct scullv_dev* dev, struct file* filp)
{
    pr_info("scullv_dev_open called\n");
    /* now trim to 0 the length of the device if open was write-only */
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (mutex_lock_interruptible(&dev->mutex)) {
            return -ERESTARTSYS;
        }

        scullv_trim(dev); /* ignore errors */
        mutex_unlock(&dev->mutex);
    }
    return 0; /* success */
}

int scullv_dev_release(struct scullv_dev* dev, struct file* filp)
{
    return 0;
}

/*
 * Data management: read and write
 */
ssize_t scullv_dev_read(struct scullv_dev* dev, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullv_dev_read called\n");

    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }

    struct scullv_qset* dptr; /* the first listitem */
    int quantum = PAGE_SIZE << dev->order;
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
    dptr = scullv_follow(dev, item);

    if (dptr == NULL || !dptr->data || !dptr->data[s_pos]) {
        goto out; /* don't fill holes */
    }

    /* read only up to the end of this quantum */
    if (count > quantum - q_pos) {
        count = quantum - q_pos;
    }

    char tmpBuf[968];
    memcpy(tmpBuf, dptr->data[s_pos] + q_pos, count);
    if (copy_to_user(buf, tmpBuf, count)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;

out:
    mutex_unlock(&dev->mutex);
    return retval;
}

ssize_t scullv_dev_write(
    struct scullv_dev* dev, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullv_dev_write called\n");

    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }

    struct scullv_qset* dptr;
    int quantum = PAGE_SIZE << dev->order;
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
    dptr = scullv_follow(dev, item);

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
        dptr->data[s_pos] = (void*)vmalloc(quantum);
        if (!dptr->data[s_pos]) {
            goto out;
        }

        memset(dptr->data[s_pos], 0, quantum);
    }

    /* write only up to the end of this quantum */
    if (count > quantum - q_pos) {
        count = quantum - q_pos;
    }

    char tmpBuf[968];
    if (copy_from_user(tmpBuf, buf, count)) {
        retval = -EFAULT;
        goto out;
    }
    memcpy(dptr->data[s_pos] + q_pos, tmpBuf, count);

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
 * The "extended" operations -- only seek
 */
loff_t scullv_dev_llseek(struct scullv_dev* dev, struct file* filp, loff_t off, int whence)
{
    pr_info("scullv_llseek called\n");

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

void construct_scullv_dev(struct scullv_dev* scullv_device, int order)
{
    memset(scullv_device, 0, sizeof(struct scullv_dev));
    scullv_device->order = order;
    scullv_device->qset = DEFAULT_SCULL_QSET;
    mutex_init(&scullv_device->mutex);
}
