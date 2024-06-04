#include "../include/scull.h"
#include "../include/scull_pipe.h"
#include "../include/scull_priv.h"
#include "../include/scull_single.h"
#include "../include/scull_uid.h"
#include "../include/scull_wuid.h"

#include <linux/device/class.h>
#include <linux/fs.h>
#include <linux/init.h>

#include <linux/version.h>

#define CLASS_NAME "scull"

#define DEVICE_FILE_NAME "scull"
#define DEVICE_PIPE_NAME "scullpipe"
#define DEVICE_SINGLE_NAME "scullsingle"
#define DEVICE_UID_NAME "sculluid"
#define DEVICE_WUID_NAME "scullwuid"
#define DEVICE_PRIV_NAME "scullpriv"

#define DEVICE_FILE_COUNT 4 /* number of bare scull devices */
#define DEVICE_PIPE_COUNT 4 /* number of pipe scull devices */

MODULE_LICENSE("GPL");

static int major;
static struct class* cls;

static struct scull scull_devices[DEVICE_FILE_COUNT];
static struct scull_pipe scull_pipes[DEVICE_PIPE_COUNT];
static struct scull_single scull_single;
static struct scull_uid scull_uid;
static struct scull_wuid scull_wuid;
static struct scull_priv scull_priv;

void cleanup(void)
{
    size_t device_num = 0;
    for (size_t i = 0; i < DEVICE_FILE_COUNT; ++i) {
        clean_scull(&scull_devices[i]);
        device_destroy(cls, MKDEV(major, device_num++));
        pr_info("Device %s%li destroyed\n", DEVICE_FILE_NAME, i);
    }
    for (size_t i = 0; i < DEVICE_PIPE_COUNT; ++i) {
        clean_scull_pipe(&scull_pipes[i]);
        device_destroy(cls, MKDEV(major, device_num++));
        pr_info("Device %s%li destroyed\n", DEVICE_PIPE_NAME, i);
    }
    clean_scull_single(&scull_single);
    device_destroy(cls, MKDEV(major, device_num++));
    pr_info("Device %s destroyed\n", DEVICE_SINGLE_NAME);

    clean_scull_uid(&scull_uid);
    device_destroy(cls, MKDEV(major, device_num++));
    pr_info("Device %s destroyed\n", DEVICE_UID_NAME);

    clean_scull_wuid(&scull_wuid);
    device_destroy(cls, MKDEV(major, device_num++));
    pr_info("Device %s destroyed\n", DEVICE_WUID_NAME);

    clean_scull_priv(&scull_priv);
    device_destroy(cls, MKDEV(major, device_num++));
    pr_info("Device %s destroyed\n", DEVICE_PRIV_NAME);

    class_destroy(cls);
    pr_info("Class destroyed\n");
    unregister_chrdev_region(MKDEV(major, 0), DEVICE_FILE_COUNT);
    pr_info("Char device unregistered\n");
}

static int __init init(void)
{
    dev_t dev;
    int res = alloc_chrdev_region(
        &dev, 0, DEVICE_FILE_COUNT + DEVICE_PIPE_COUNT + 4 /* single, uid, wuid, priv */, "scull");

    if (res < 0) {
        pr_alert("Cannot register char device\n");
        return res;
    }

    major = MAJOR(dev);
    pr_info("Assigned major number: %d\n", major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(CLASS_NAME);
#else
    cls = class_create(THIS_MODULE, CLASS_NAME);
#endif
    pr_info("Class created\n");

    size_t device_num = 0;

    for (size_t i = 0; i < DEVICE_FILE_COUNT; ++i) {
        device_create(cls, NULL, MKDEV(major, device_num++), NULL, "%s%li", DEVICE_FILE_NAME, i);
        pr_info("Device %s%li created\n", DEVICE_FILE_NAME, i);
        init_scull(&scull_devices[i]);
    }

    for (size_t i = 0; i < DEVICE_PIPE_COUNT; ++i) {
        device_create(cls, NULL, MKDEV(major, device_num++), NULL, "%s%li", DEVICE_PIPE_NAME, i);
        pr_info("Device %s%li created\n", DEVICE_PIPE_NAME, i);
        init_scull_pipe(&scull_pipes[i]);
    }

    device_create(cls, NULL, MKDEV(major, device_num++), NULL, "%s", DEVICE_SINGLE_NAME);
    pr_info("Device %s created\n", DEVICE_SINGLE_NAME);
    init_scull_single(&scull_single);

    device_create(cls, NULL, MKDEV(major, device_num++), NULL, "%s", DEVICE_UID_NAME);
    pr_info("Device %s created\n", DEVICE_UID_NAME);
    init_scull_uid(&scull_uid);

    device_create(cls, NULL, MKDEV(major, device_num++), NULL, "%s", DEVICE_WUID_NAME);
    pr_info("Device %s created\n", DEVICE_WUID_NAME);
    init_scull_wuid(&scull_wuid);

    device_create(cls, NULL, MKDEV(major, device_num++), NULL, "%s", DEVICE_PRIV_NAME);
    pr_info("Device %s created\n", DEVICE_PRIV_NAME);
    init_scull_priv(&scull_priv);

    device_num = 0;

    for (size_t i = 0; i < DEVICE_FILE_COUNT; ++i) {
        res = cdev_add(&scull_devices[i].cdev, MKDEV(major, device_num++), 1);
        if (res) {
            pr_notice("Can't add %s%li", DEVICE_FILE_NAME, i);
        }
    }

    for (size_t i = 0; i < DEVICE_PIPE_COUNT; ++i) {
        res = cdev_add(&scull_pipes[i].cdev, MKDEV(major, device_num++), 1);
        if (res) {
            pr_notice("Can't add %s%li", DEVICE_PIPE_NAME, i);
        }
    }

    res = cdev_add(&scull_single.cdev, MKDEV(major, device_num++), 1);
    if (res) {
        pr_notice("Can't add %s", DEVICE_SINGLE_NAME);
    }

    res = cdev_add(&scull_uid.cdev, MKDEV(major, device_num++), 1);
    if (res) {
        pr_notice("Can't add %s", DEVICE_UID_NAME);
    }

    res = cdev_add(&scull_wuid.cdev, MKDEV(major, device_num++), 1);
    if (res) {
        pr_notice("Can't add %s", DEVICE_WUID_NAME);
    }

    res = cdev_add(&scull_priv.cdev, MKDEV(major, device_num++), 1);
    if (res) {
        pr_notice("Can't add %s", DEVICE_PRIV_NAME);
    }

    return 0;
}

module_init(init);
module_exit(cleanup);
