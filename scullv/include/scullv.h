#pragma once

#include "scullv_dev.h"

#include <linux/cdev.h>

struct scullv {
    struct scullv_dev dev;
    struct cdev cdev; /* Char device structure */
};

void init_scullv(struct scullv* scullv_device, int order);

void clean_scullv(struct scullv* scullv_device);
