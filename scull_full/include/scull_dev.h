#pragma once

#include <linux/mutex.h>

/*
 * Representation of scull quantum sets.
 */
struct scull_qset {
    void** data;
    struct scull_qset* next;
};

struct scull_dev {
    struct scull_qset* data; /* Pointer to first quantum set */
    int quantum; /* The current quantum size */
    int qset; /* The current array size */
    int new_quantum; /* The new quantum size */
    int new_qset; /* The new array size */
    unsigned long size; /* Amount of data stored here */
    struct mutex mutex; /* Mutual exclusion */
};
