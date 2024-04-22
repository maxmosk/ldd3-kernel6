#ifndef SCULL_FOPS_H
#define SCULL_FOPS_H

#include <linux/fs.h>

extern ssize_t scull_p_read (struct file *, char __user *, size_t, loff_t *);
extern ssize_t scull_p_write (struct file *, const char __user *, size_t, loff_t *);
extern int scull_p_open (struct inode *, struct file *);
extern int scull_p_release (struct inode *, struct file *);
extern int scull_p_fasync(int, struct file *, int);
extern __poll_t scull_p_poll (struct file *, struct poll_table_struct *);
#endif