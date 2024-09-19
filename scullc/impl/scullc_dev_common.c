#include "../include/scullc_dev_common.h"
#include "linux/types.h"

#include <linux/fs.h>

#define DEFAULT_SCULL_QSET 1000

/*
 * Follow the list
 */
static struct scullc_qset* scullc_follow(struct scullc_dev* scullc_device, int n)
{
    pr_info("scullc_follow called\n");
    struct scullc_qset* qs = scullc_device->data;

    /* Allocate first qset explicitly if need be */
    if (!qs) {
        qs = scullc_device->data = kmalloc(sizeof(struct scullc_qset), GFP_KERNEL);
        if (qs == NULL) {
            return NULL; /* Never mind */
        }
        memset(qs, 0, sizeof(struct scullc_qset));
    }

    /* Then follow the list */
    while (n--) {
        if (!qs->next) {
            qs->next = kmalloc(sizeof(struct scullc_qset), GFP_KERNEL);
            if (qs->next == NULL) {
                return NULL; /* Never mind */
            }
            memset(qs->next, 0, sizeof(struct scullc_qset));
        }
        qs = qs->next;
        continue;
    }
    return qs;
}

/*
 * Empty out the scullc device; must be called with the device
 * semaphore held.
 */
int scullc_trim(struct scullc_dev* dev)
{
    pr_info("scullc_trim called\n");
    struct scullc_qset *dptr, *next;
    int qset = dev->qset; /* "dev" is not-null */
    int i;

    for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
        if (dptr->data) {
            for (i = 0; i < qset; ++i) {
                if (dptr->data[i]) {
                    kmem_cache_free(dev->cache, dptr->data[i]);
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
int scullc_dev_open(struct scullc_dev* dev, struct file* filp)
{
    pr_info("scullc_dev_open called\n");
    /* now trim to 0 the length of the device if open was write-only */
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (mutex_lock_interruptible(&dev->mutex)) {
            return -ERESTARTSYS;
        }

        scullc_trim(dev); /* ignore errors */
        mutex_unlock(&dev->mutex);
    }
    return 0; /* success */
}

int scullc_dev_release(struct scullc_dev* dev, struct file* filp)
{
    return 0;
}

/*
 * Data management: read and write
 */
ssize_t scullc_dev_read(struct scullc_dev* dev, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullc_dev_read called\n");

    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }

    struct scullc_qset* dptr; /* the first listitem */
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
    dptr = scullc_follow(dev, item);

    if (dptr == NULL || !dptr->data || !dptr->data[s_pos]) {
        goto out; /* don't fill holes */
    }

    /* read only up to the end of this quantum */
    if (count > quantum - q_pos) {
        count = quantum - q_pos;
    }

    char tmpBuf[128];
    size_t readBytes = 0;
    while (readBytes != count) {
        size_t readCount = count - readBytes > 128 ? 128 : count - readBytes;
        memcpy(tmpBuf, dptr->data[s_pos] + q_pos + readBytes, readCount);
        if (copy_to_user(buf + readBytes, tmpBuf, readCount)) {
            retval = -EFAULT;
            goto out;
        }
        readBytes += readCount;
    }

    *f_pos += count;
    retval = count;

out:
    mutex_unlock(&dev->mutex);
    return retval;
}

ssize_t scullc_dev_write(
    struct scullc_dev* dev, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scullc_dev_write called\n");

    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }

    struct scullc_qset* dptr;
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
    dptr = scullc_follow(dev, item);

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
        dptr->data[s_pos] = kmem_cache_alloc(dev->cache, GFP_KERNEL);
        if (!dptr->data[s_pos]) {
            goto out;
        }

        memset(dptr->data[s_pos], 0, quantum);
    }

    /* write only up to the end of this quantum */
    if (count > quantum - q_pos) {
        count = quantum - q_pos;
    }

    char tmpBuf[128];
    size_t writenBytes = 0;
    while (writenBytes != count) {
        size_t writeCount = count - writenBytes > 128 ? 128 : count - writenBytes;
        if (copy_from_user(tmpBuf, buf + writenBytes, writeCount)) {
            retval = -EFAULT;
            goto out;
        }
        memcpy(dptr->data[s_pos] + q_pos + writenBytes, tmpBuf, writeCount);
        writenBytes += writeCount;
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
 * The "extended" operations -- only seek
 */
loff_t scullc_dev_llseek(struct scullc_dev* dev, struct file* filp, loff_t off, int whence)
{
    pr_info("scullc_llseek called\n");

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

void construct_scullc_dev(
    struct scullc_dev* scullc_device, int quantum_size, struct kmem_cache* cache)
{
    memset(scullc_device, 0, sizeof(struct scullc_dev));
    scullc_device->quantum = quantum_size;
    scullc_device->qset = DEFAULT_SCULL_QSET;
    scullc_device->cache = cache;
    mutex_init(&scullc_device->mutex);
}
