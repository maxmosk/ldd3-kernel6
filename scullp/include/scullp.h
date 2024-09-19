#pragma once

#include "scullp_dev.h"

#include <linux/cdev.h>

struct scullp {
    struct scullp_dev dev;
    struct cdev cdev; /* Char device structure */
};

void init_scullp(struct scullp* scullp_device, int order);

void clean_scullp(struct scullp* scullp_device);
