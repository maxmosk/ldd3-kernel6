#pragma once

#include <linux/mutex.h>

/*
 * Representation of scullp quantum sets.
 */
struct scullv_qset {
    void** data;
    struct scullv_qset* next;
};

struct scullv_dev {
    struct scullv_qset* data; /* Pointer to first quantum set */
    int order; /* The current page order */
    int qset; /* The current array size */
    unsigned long size; /* Amount of data stored here */
    struct mutex mutex; /* Mutual exclusion */
};
