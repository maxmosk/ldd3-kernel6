#pragma once

#include "scullv_dev.h"

int scullv_trim(struct scullv_dev* dev);

int scullv_dev_open(struct scullv_dev* dev, struct file* filp);

int scullv_dev_release(struct scullv_dev* dev, struct file* filp);

ssize_t scullv_dev_read(struct scullv_dev* dev, char __user* buf, size_t count, loff_t* f_pos);

ssize_t scullv_dev_write(
    struct scullv_dev* dev, const char __user* buf, size_t count, loff_t* f_pos);

loff_t scullv_dev_llseek(struct scullv_dev* dev, struct file* filp, loff_t off, int whence);

void construct_scullv_dev(struct scullv_dev* dev, int order);
