#pragma once

#include <linux/mutex.h>

/*
 * Representation of scullp quantum sets.
 */
struct scullp_qset {
    void** data;
    struct scullp_qset* next;
};

struct scullp_dev {
    struct scullp_qset* data; /* Pointer to first quantum set */
    int order; /* The current page order */
    int qset; /* The current array size */
    unsigned long size; /* Amount of data stored here */
    struct mutex mutex; /* Mutual exclusion */
};
