#include "../include/scull_pipe.h"

#include <linux/fs.h>
#include <linux/poll.h>

#define SCULL_P_BUFFER 4000

int scull_p_buffer = SCULL_P_BUFFER;

static int scull_pipe_fasync(int fd, struct file* filp, int mode)
{
    pr_info("scull_pipe_fasync called\n");
    struct scull_pipe* dev = filp->private_data;
    return fasync_helper(fd, filp, mode, &dev->async_queue);
}

/*
 * Open and close
 */
static int scull_pipe_open(struct inode* inode, struct file* filp)
{
    pr_info("scull_pipe_open called\n");

    struct scull_pipe* dev;

    dev = container_of(inode->i_cdev, struct scull_pipe, cdev);
    filp->private_data = dev;

    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }
    if (!dev->buffer) {
        /* allocate the buffer */
        dev->buffer = kmalloc(scull_p_buffer, GFP_KERNEL);
        if (!dev->buffer) {
            mutex_unlock(&dev->mutex);
            return -ENOMEM;
        }
    }
    dev->buffersize = scull_p_buffer;
    dev->end = dev->buffer + dev->buffersize;
    dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */

    /* use f_mode, not f_flags: it's cleaner (fs/open.c tells why) */
    if (filp->f_mode & FMODE_READ) {
        ++dev->nreaders;
    }
    if (filp->f_mode & FMODE_WRITE) {
        ++dev->nwriters;
    }

    pr_info("nreaders: %d, nwriters: %d \n", dev->nreaders, dev->nwriters);

    mutex_unlock(&dev->mutex);
    return nonseekable_open(inode, filp);
}

static int scull_pipe_release(struct inode* inode, struct file* filp)
{
    pr_info("scull_pipe_release called\n");

    struct scull_pipe* dev = filp->private_data;

    /* remove this filp from the asynchronously notified filp's */
    scull_pipe_fasync(-1, filp, 0);

    mutex_lock(&dev->mutex);

    if (filp->f_mode & FMODE_READ) {
        --dev->nreaders;
    }
    if (filp->f_mode & FMODE_WRITE) {
        --dev->nwriters;
    }

    if (dev->nreaders == 0 && dev->nwriters == 0) {
        kfree(dev->buffer);
        dev->buffer = NULL; /* the other fields are not checked on open */
    }

    pr_info("nreaders: %d, nwriters: %d \n", dev->nreaders, dev->nwriters);

    mutex_unlock(&dev->mutex);
    return 0;
}

/* How much space is free? */
static int spacefree(struct scull_pipe* dev)
{
    if (dev->rp == dev->wp) {
        return dev->buffersize - 1;
    }

    return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

/* Wait for space for writing; caller must hold device semaphore.  On
 * error the semaphore will be released before returning. */
static int scull_getwritespace(struct scull_pipe* dev, struct file* filp)
{
    while (spacefree(dev) == 0) { /* full */
        DEFINE_WAIT(wait);

        mutex_unlock(&dev->mutex);

        if (filp->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }

        prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);

        if (spacefree(dev) == 0) {
            schedule();
        }

        finish_wait(&dev->outq, &wait);

        if (signal_pending(current)) {
            return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
        }

        if (mutex_lock_interruptible(&dev->mutex)) {
            return -ERESTARTSYS;
        }
    }

    return 0;
}

/*
 * Data management: read and write
 */
static ssize_t scull_pipe_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_pipe_read called\n");
    struct scull_pipe* dev = filp->private_data;
    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }

    while (dev->rp == dev->wp) { /* nothing to read */
        mutex_unlock(&dev->mutex); /* release the lock */

        if (filp->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }

        if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp))) {
            return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
        }

        /* otherwise loop, but first reacquire the lock */
        if (mutex_lock_interruptible(&dev->mutex)) {
            return -ERESTARTSYS;
        }
    }

    /* ok, data is there, return something */
    if (dev->wp > dev->rp) {
        count = min(count, (size_t)(dev->wp - dev->rp));
    } else { /* the write pointer has wrapped, return data up to dev->end */
        count = min(count, (size_t)(dev->end - dev->rp));
    }

    if (copy_to_user(buf, dev->rp, count)) {
        mutex_unlock(&dev->mutex);
        return -EFAULT;
    }

    dev->rp += count;
    if (dev->rp == dev->end) {
        dev->rp = dev->buffer; /* wrapped */
    }

    mutex_unlock(&dev->mutex);
    /* finally, awake any writers and return */
    wake_up_interruptible(&dev->outq);

    return count;
}

static ssize_t scull_pipe_write(
    struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
    pr_info("scull_pipe_write called\n");

    struct scull_pipe* dev = filp->private_data;
    int result;

    if (mutex_lock_interruptible(&dev->mutex)) {
        return -ERESTARTSYS;
    }

    /* Make sure there's space to write */
    result = scull_getwritespace(dev, filp);
    if (result) {
        return result; /* scull_getwritespace called up(&dev->sem) */
    }

    /* ok, space is there, accept something */
    count = min(count, (size_t)spacefree(dev));

    if (dev->wp >= dev->rp) {
        count = min(count, (size_t)(dev->end - dev->wp)); /* to end-of-buf */
    } else { /* the write pointer has wrapped, fill up to rp-1 */
        count = min(count, (size_t)(dev->rp - dev->wp - 1));
    }

    if (copy_from_user(dev->wp, buf, count)) {
        mutex_unlock(&dev->mutex);
        return -EFAULT;
    }

    dev->wp += count;
    if (dev->wp == dev->end) {
        dev->wp = dev->buffer; /* wrapped */
    }

    mutex_unlock(&dev->mutex);
    /* finally, awake any reader */
    wake_up_interruptible(&dev->inq); /* blocked in read() and select() */

    /* and signal asynchronous readers, explained late in chapter 5 */
    if (dev->async_queue) {
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
    }

    return count;
}

static __poll_t scull_pipe_poll(struct file* filp, struct poll_table_struct* wait)
{
    pr_info("scull_pipe_poll called\n");

    struct scull_pipe* dev = filp->private_data;
    unsigned int mask = 0;

    /*
     * The buffer is circular; it is considered full
     * if "wp" is right behind "rp" and empty if the
     * two are equal.
     */
    mutex_lock(&dev->mutex);
    poll_wait(filp, &dev->inq, wait);
    poll_wait(filp, &dev->outq, wait);

    if (dev->rp != dev->wp) {
        mask |= POLLIN | POLLRDNORM; /* readable */
    }

    if (spacefree(dev)) {
        mask |= POLLOUT | POLLWRNORM; /* writable */
    }

    mutex_unlock(&dev->mutex);
    return mask;
}

static struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .open = scull_pipe_open,
    .read = scull_pipe_read,
    .write = scull_pipe_write,
    .release = scull_pipe_release,
    .fasync = scull_pipe_fasync,
    .poll = scull_pipe_poll,
};

void init_scull_pipe(struct scull_pipe* scull_p_device)
{
    memset(scull_p_device, 0, sizeof(struct scull_pipe));
    init_waitqueue_head(&scull_p_device->inq);
    init_waitqueue_head(&scull_p_device->outq);
    mutex_init(&scull_p_device->mutex);

    cdev_init(&scull_p_device->cdev, &scull_fops);
    scull_p_device->cdev.owner = THIS_MODULE;
}

void clean_scull_pipe(struct scull_pipe* scull_p_device)
{
    cdev_del(&scull_p_device->cdev);
}
