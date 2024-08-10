#pragma once

#include <linux/mutex.h>

/*
 * Representation of scullc quantum sets.
 */
struct scullc_qset {
    void** data;
    struct scullc_qset* next;
};

struct scullc_dev {
    struct scullc_qset* data; /* Pointer to first quantum set */
    int quantum; /* The current quantum size */
    int qset; /* The current array size */
    unsigned long size; /* Amount of data stored here */
    struct mutex mutex; /* Mutual exclusion */
    struct kmem_cache* cache; /* Cache */
};
