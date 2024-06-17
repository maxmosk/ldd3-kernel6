#pragma once

#include "scull_dev.h"

#include <linux/cdev.h>

struct scull {
    struct scull_dev dev;
    struct cdev cdev; /* Char device structure */
};

void init_scull(struct scull* scull_device);

void clean_scull(struct scull* scull_device);
