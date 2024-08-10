#pragma once

#include "scullc_dev.h"

int scullc_trim(struct scullc_dev* dev);

int scullc_dev_open(struct scullc_dev* dev, struct file* filp);

int scullc_dev_release(struct scullc_dev* dev, struct file* filp);

ssize_t scullc_dev_read(
    struct scullc_dev* dev, char __user* buf, size_t count, loff_t* f_pos);

ssize_t scullc_dev_write(
    struct scullc_dev* dev, const char __user* buf, size_t count, loff_t* f_pos);

loff_t scullc_dev_llseek(struct scullc_dev* dev, struct file* filp, loff_t off, int whence);

void construct_scullc_dev(struct scullc_dev* dev, int quantum_size, struct kmem_cache* cache);
