#include <linux/init.h>
#include <linux/module.h>

#include "sbull_device.h"

static int sbull_major;
sbull_dev_t* sbull_device;


static int __init sbull_init(void)
{
#ifndef BIO_BASED_SBULL
    pr_info("SBULL: init sbull in request mode\n");
#else
    pr_info("SBULL: init sbull in bio mode\n");
#endif

#ifdef PRINT_INFO
    pr_info("SBULL: print info when fucntions called\n");
#endif

    int ret = 0;
    sbull_major = register_blkdev(sbull_major, DEVICE_NAME);
    if (sbull_major <= 0) {
        pr_warn("SBULL: unable to get major number\n");
        return -EBUSY;
    }

    sbull_device = sbull_add_device(sbull_major);
    if (IS_ERR(sbull_device))
        ret = PTR_ERR(sbull_device);

    if (ret != 0)
        unregister_blkdev(sbull_major, DEVICE_NAME);

    return ret;
}

static void __exit sbull_exit(void)
{
    sbull_remove_device(sbull_device);

    if (sbull_major > 0)
        unregister_blkdev(sbull_major, DEVICE_NAME);
    pr_info("SBULL: has been deleted");
}

MODULE_LICENSE("GPL");

module_init(sbull_init);
module_exit(sbull_exit);
