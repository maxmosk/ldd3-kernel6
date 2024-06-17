#pragma once

#include <linux/cdev.h>
#include <linux/mutex.h>

struct scull_pipe {
    wait_queue_head_t inq, outq; /* Read and write queues */
    char *buffer, *end; /* Begin of buf, end of buf */
    int buffersize; /* Used in pointer arithmetic */
    char *rp, *wp; /* Where to read, where to write */
    int nreaders, nwriters; /* Number of openings for r/w */
    struct fasync_struct* async_queue; /* Asynchronous readers */
    struct mutex mutex; /* Mutual exclusion */
    struct cdev cdev; /* Char device structure */
};

void init_scull_pipe(struct scull_pipe* scull_p_device);

void clean_scull_pipe(struct scull_pipe* scull_p_device);
