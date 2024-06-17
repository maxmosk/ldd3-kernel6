#pragma once

#include "scull_dev.h"

#include <linux/cdev.h>

struct scull_priv {
    struct cdev cdev; /* Char device structure */
    spinlock_t lock;
    struct list_head list; /* The list of devices, and a lock to protect it */
};

void init_scull_priv(struct scull_priv* scull_device);

void clean_scull_priv(struct scull_priv* scull_device);
