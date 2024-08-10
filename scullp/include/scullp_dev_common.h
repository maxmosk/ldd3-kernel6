#pragma once

#include "scullp_dev.h"

int scullp_trim(struct scullp_dev* dev);

int scullp_dev_open(struct scullp_dev* dev, struct file* filp);

int scullp_dev_release(struct scullp_dev* dev, struct file* filp);

ssize_t scullp_dev_read(struct scullp_dev* dev, char __user* buf, size_t count, loff_t* f_pos);

ssize_t scullp_dev_write(
    struct scullp_dev* dev, const char __user* buf, size_t count, loff_t* f_pos);

loff_t scullp_dev_llseek(struct scullp_dev* dev, struct file* filp, loff_t off, int whence);

void construct_scullp_dev(struct scullp_dev* dev, int order);
