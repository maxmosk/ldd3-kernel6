#include <linux/init.h>
#include <linux/module.h>

#include "snull_device.h"

static int __init snull_init(void)
{
    int ret = snull_add_device();
    if (ret)
        pr_info("SNULL: cannot create device\n");

    return ret;
}

static void __exit snull_exit(void)
{
    snull_remove_device();
}

MODULE_LICENSE("GPL");

module_init(snull_init);
module_exit(snull_exit);
