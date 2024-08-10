#pragma once

#include "scullc_dev.h"

#include <linux/cdev.h>

struct scullc {
    struct scullc_dev dev;
    struct cdev cdev; /* Char device structure */
};

void init_scullc(struct scullc* scullc_device, int quantum_size, struct kmem_cache* cache);

void clean_scullc(struct scullc* scullc_device);
