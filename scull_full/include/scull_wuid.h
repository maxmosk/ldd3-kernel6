#pragma once

#include "scull_dev.h"

#include <linux/cdev.h>

struct scull_wuid {
    struct scull_dev dev;
    struct cdev cdev; /* Char device structure */
    size_t count;
    kuid_t owner;
    spinlock_t lock;
    wait_queue_head_t wait;
};

void init_scull_wuid(struct scull_wuid* scull_device);

void clean_scull_wuid(struct scull_wuid* scull_device);
