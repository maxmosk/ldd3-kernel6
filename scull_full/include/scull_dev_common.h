#pragma once

#include "scull_dev.h"

int scull_trim(struct scull_dev* dev);

int scull_dev_open(struct scull_dev* dev, struct file* filp);

int scull_dev_release(struct scull_dev* dev, struct file* filp);

ssize_t scull_dev_read(
    struct scull_dev* dev, char __user* buf, size_t count, loff_t* f_pos);

ssize_t scull_dev_write(
    struct scull_dev* dev, const char __user* buf, size_t count, loff_t* f_pos);

long scull_dev_ioctl(struct scull_dev* dev, unsigned int cmd, unsigned long arg);

loff_t scull_dev_llseek(struct scull_dev* dev, struct file* filp, loff_t off, int whence);

void construct_scull_dev(struct scull_dev* dev);
