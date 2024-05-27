#ifndef SBULL_DEV_H
#define SBULL_DEV_H

#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/device.h>
#include <linux/blk-mq.h>
#include <linux/types.h>

#define DEVICE_NAME "sbulldev"
#define DEVICE_CAPACITY 4096

typedef struct sbull_dev_t
{
    sector_t capacity;			    // Device size in bytes
    u8* data;			    		// The data aray. u8 - 8 bytes
    struct blk_mq_tag_set tag_set;
    struct gendisk *disk;
    atomic_t open_counter;
} sbull_dev_t;

sbull_dev_t* sbull_add_device(int major);
void sbull_remove_device(sbull_dev_t* dev);

#endif