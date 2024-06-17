#pragma once

#include "scull_dev.h"

#include <linux/cdev.h>

struct scull_uid {
    struct scull_dev dev;
    struct cdev cdev; /* Char device structure */
    size_t count;
    kuid_t owner;
    spinlock_t lock;
};

void init_scull_uid(struct scull_uid* scull_device);

void clean_scull_uid(struct scull_uid* scull_device);
