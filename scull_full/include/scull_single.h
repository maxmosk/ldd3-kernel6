#pragma once

#include "scull_dev.h"

#include <linux/cdev.h>

struct scull_single {
    struct scull_dev dev;
    struct cdev cdev; /* Char device structure */
    atomic_t available;
};

void init_scull_single(struct scull_single* scull_device);

void clean_scull_single(struct scull_single* scull_device);
