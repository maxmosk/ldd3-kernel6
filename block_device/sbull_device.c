#include "sbull_device.h"

static inline int process_request(struct request *rq, unsigned int *nr_bytes)
{
	int ret = 0;
	struct bio_vec bvec;
	struct req_iterator iter;
	sbull_dev_t *dev = rq->q->queuedata;
	loff_t pos = blk_rq_pos(rq) << SECTOR_SHIFT;
	loff_t dev_size = (dev->capacity << SECTOR_SHIFT);

	rq_for_each_segment(bvec, rq, iter) {
		unsigned long len = bvec.bv_len;
		void *buf = page_address(bvec.bv_page) + bvec.bv_offset;

		if ((pos + len) > dev_size)
			len = (unsigned long)(dev_size - pos);

		if (rq_data_dir(rq))
			memcpy(dev->data + pos, buf, len); /* WRITE */
		else
			memcpy(buf, dev->data + pos, len); /* READ */

		pos += len;
		*nr_bytes += len;
	}

	return ret;
}

static blk_status_t _queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
	unsigned int nr_bytes = 0;
	blk_status_t status = BLK_STS_OK;
	struct request *rq = bd->rq;

	//might_sleep();
	cant_sleep(); /* cannot use any locks that make the thread sleep */

	blk_mq_start_request(rq);

	if (process_request(rq, &nr_bytes))
		status = BLK_STS_IOERR;

	pr_info("SBULL: request %llu:%d processed\n", blk_rq_pos(rq), nr_bytes);

	blk_mq_end_request(rq, status);

	return status;
}

static struct blk_mq_ops mq_ops = {
    .queue_rq = _queue_rq,
};

static inline int init_tag_set(struct blk_mq_tag_set *set, void *data)
{
	set->ops = &mq_ops;
	set->nr_hw_queues = 1;
	set->nr_maps = 1;
	set->queue_depth = 128;
	set->numa_node = NUMA_NO_NODE;
	set->flags = BLK_MQ_F_SHOULD_MERGE | BLK_MQ_F_STACKING;

	set->cmd_size = 0;
	set->driver_data = data;

	return blk_mq_alloc_tag_set(set);
}

static int _sbull_open(struct gendisk *disk, blk_mode_t mode)
{
	sbull_dev_t *dev = disk->private_data;

	if (!dev) {
		pr_err("SBULL: Invalid disk private_data\n");
		return -ENXIO;
	}

    atomic_inc(&dev->open_counter);
	pr_info("SBULL: Device was opened. There is %d users\n", atomic_read(&dev->open_counter));

	return 0;
}

static void _sbull_release(struct gendisk *disk)
{
	sbull_dev_t *dev = disk->private_data;

	if (!dev) {
		pr_err("SBULL: Invalid disk private_data\n");
		return;
	}

    atomic_dec(&dev->open_counter);
	pr_info("SBULL: Device was closed. There is %d users\n", atomic_read(&dev->open_counter));
}

int _sbull_ioctl(struct block_device *bdev, blk_mode_t mode,
			unsigned cmd, unsigned long arg)
{
    pr_info("SBULL: ioctl was called");
    return -ENOTTY;
}

static struct block_device_operations sbull_fops = {
    .owner = THIS_MODULE,
    .open = _sbull_open,
    .release = _sbull_release,
    .ioctl = _sbull_ioctl,
};

sbull_dev_t* sbull_add_device(int major)
{
    sbull_dev_t *dev = NULL;
    int ret = 0;
    struct gendisk *disk;

    pr_info("SBULL: add device '%s' capacity %d sectors\n", DEVICE_NAME, DEVICE_CAPACITY);

    dev = kzalloc(sizeof(sbull_dev_t), GFP_KERNEL);
    if (!dev) {
        ret = -ENOMEM;
        goto fail;
    }

    atomic_set(&dev->open_counter, 0);

    dev->capacity = DEVICE_CAPACITY;
    dev->data = vmalloc(DEVICE_CAPACITY << SECTOR_SHIFT);
    if (!dev->data) {
        ret = -ENOMEM;
        goto fail_kfree;
    }

	ret = init_tag_set(&dev->tag_set, dev);
	if (ret) {
		pr_err("SBULL: Failed to allocate tag set\n");
		goto fail_vfree;
	}

	disk = blk_mq_alloc_disk(&dev->tag_set, dev);
	if (unlikely(!disk)) {
		ret = -ENOMEM;
		pr_err("SBULL: Failed to allocate disk\n");
		goto fail_free_tag_set;
	}
	if (IS_ERR(disk)) {
		ret = PTR_ERR(disk);
		pr_err("SBULL: Failed to allocate disk\n");
		goto fail_free_tag_set;
	}

    dev->disk = disk;

	disk->flags |= GENHD_FL_NO_PART;

	disk->major = major;
	disk->first_minor = 0;
	disk->minors = 1;

	disk->fops = &sbull_fops;

	disk->private_data = dev;

	sprintf(disk->disk_name, DEVICE_NAME);
	set_capacity(disk, dev->capacity);

	blk_queue_physical_block_size(disk->queue, SECTOR_SIZE);
	blk_queue_logical_block_size(disk->queue, SECTOR_SIZE);
	blk_queue_max_hw_sectors(disk->queue, BLK_DEF_MAX_SECTORS);
	blk_queue_flag_set(QUEUE_FLAG_NOMERGES, disk->queue);

	ret = add_disk(disk);
	if (ret) {
		pr_err("SBULL: Failed to add disk '%s'\n", disk->disk_name);
		goto fail_put_disk;
	}

	pr_info("SBULL: Simple block device [%d:%d] was added\n", major, 0);

	return dev;

fail_put_disk:
	put_disk(dev->disk);
fail_free_tag_set:
	blk_mq_free_tag_set(&dev->tag_set);
fail_vfree:
	vfree(dev->data);
fail_kfree:
	kfree(dev);
fail:
	pr_err("SBULL: Failed to add block device\n");

    return ERR_PTR(ret);
}

void sbull_remove_device(sbull_dev_t* dev)
{
	del_gendisk(dev->disk);

    put_disk(dev->disk);

    blk_mq_free_tag_set(&dev->tag_set);

    vfree(dev->data);

    kfree(dev);

    pr_info("SBULL: device removed");
}
