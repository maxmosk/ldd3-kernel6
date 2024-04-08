#ifndef SCULL_FOPS_H
#define SCULL_FOPS_H

#include <linux/fs.h>

extern ssize_t scull_read (struct file *, char __user *, size_t, loff_t *);
extern ssize_t scull_write (struct file *, const char __user *, size_t, loff_t *);
extern int scull_open (struct inode *, struct file *);
extern int scull_release (struct inode *, struct file *);
extern int scull_trim(struct scull_dev*);

#endif