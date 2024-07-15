#include "linux/printk.h"
#include "linux/wait.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/preempt.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");

/*
 * The delay for the delayed workqueue timer file.
 */
static long delay = 1;
module_param(delay, long, 0);

/*
 * This module is a silly one: it only embeds short code fragments
 * that show how enqueued tasks `feel' the environment
 */

#define LIMIT (512) /* don't print any more after this size */

/*
 * Print information about the current environment. This is called from
 * within the task queues. If the limit is reched, awake the reading
 * process.
 */
static DECLARE_WAIT_QUEUE_HEAD(jiq_wait);

/*
 * Keep track of info we need between task queue runs.
 */
static struct clientdata {
    struct work_struct jiq_work;
    struct delayed_work jiq_delayed_work;
    size_t len;
    char* buf;
    unsigned long jiffies;
    long delay;
} jiq_data;

#define SCHEDULER_QUEUE ((task_queue*)1)

static void jiq_print_tasklet(struct tasklet_struct* tasklet);
static DECLARE_TASKLET(jiq_tasklet, jiq_print_tasklet);

/*
 * Do the printing; return non-zero if the task should be rescheduled.
 */
static int jiq_print(struct clientdata* data)
{
    int len = data->len;
    char* buf = data->buf;
    unsigned long j = jiffies;

    if (len > LIMIT) {
        wake_up_interruptible(&jiq_wait);
        return 0;
    }

    if (len == 0) {
        len = sprintf(buf, "    time  delta preempt   pid cpu command\n");
    } else {
        len = 0;
    }

    /* intr_count is only exported since 1.3.5, but 1.99.4 is needed anyways */
    len += sprintf(
        buf + len,
        "%9li  %4li     %3i %5i %3i %s\n",
        j,
        j - data->jiffies,
        preempt_count(),
        current->pid,
        smp_processor_id(),
        current->comm);

    data->len += len;
    data->buf += len;
    data->jiffies = j;
    return 1;
}

/*
 * Call jiq_print from a work queue
 */
static void jiq_print_wq(struct work_struct* work)
{
    struct clientdata* data = container_of(work, struct clientdata, jiq_work);

    if (!jiq_print(data)) {
        return;
    }

    schedule_work(&jiq_data.jiq_work);
}

static void jiq_print_wq_delayed(struct work_struct* work)
{
    struct clientdata* data = container_of(work, struct clientdata, jiq_delayed_work.work);

    if (!jiq_print(data)) {
        return;
    }

    schedule_delayed_work(&jiq_data.jiq_delayed_work, data->delay);
}

static ssize_t jiq_read_wq(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    DEFINE_WAIT(wait);

    char tmp[768];
    jiq_data.len = 0; /* nothing printed, yet */
    jiq_data.buf = tmp; /* print in this place */
    jiq_data.jiffies = jiffies; /* initial time */
    jiq_data.delay = 0;

    prepare_to_wait(&jiq_wait, &wait, TASK_INTERRUPTIBLE);
    schedule_work(&jiq_data.jiq_work);
    schedule();
    finish_wait(&jiq_wait, &wait);

    if (copy_to_user(buf, tmp, jiq_data.len)) {
        return -EFAULT;
    }
    *f_pos = jiq_data.len;

    return jiq_data.len;
}

static ssize_t jiq_read_wq_delayed(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    DEFINE_WAIT(wait);

    char tmp[768];
    jiq_data.len = 0; /* nothing printed, yet */
    jiq_data.buf = tmp; /* print in this place */
    jiq_data.jiffies = jiffies; /* initial time */
    jiq_data.delay = delay;

    prepare_to_wait(&jiq_wait, &wait, TASK_INTERRUPTIBLE);
    schedule_delayed_work(&jiq_data.jiq_delayed_work, delay);
    schedule();
    finish_wait(&jiq_wait, &wait);

    if (copy_to_user(buf, tmp, jiq_data.len)) {
        return -EFAULT;
    }
    *f_pos = jiq_data.len;

    return jiq_data.len;
}

static int cond = 0;

/*
 * Call jiq_print from a tasklet
 */
static void jiq_print_tasklet(struct tasklet_struct* tasklet)
{
    if (jiq_print(&jiq_data)) {
        tasklet_schedule(&jiq_tasklet);
    } else {
        cond = 1;
    }
}

static ssize_t jiq_read_tasklet(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    char tmp[768];
    jiq_data.len = 0; /* nothing printed, yet */
    jiq_data.buf = tmp; /* print in this place */
    jiq_data.jiffies = jiffies; /* initial time */

    cond = 0;
    tasklet_schedule(&jiq_tasklet);
    wait_event_interruptible(jiq_wait, cond); /* sleep till completion */

    if (copy_to_user(buf, tmp, jiq_data.len)) {
        return -EFAULT;
    }
    *f_pos = jiq_data.len;

    return jiq_data.len;
}

/*
 * This one, instead, tests out the timers.
 */

static struct timer_list jiq_timer;

static void jiq_timedout(struct timer_list* ptr)
{
    jiq_print(&jiq_data); /* print a line */
    cond = 1;
    wake_up_interruptible(&jiq_wait); /* awake the process */
}

static ssize_t jiq_read_run_timer(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    char tmp[768];
    jiq_data.len = 0; /* prepare the argument for jiq_print() */
    jiq_data.buf = tmp; /* print in this place */
    jiq_data.jiffies = jiffies;

    jiq_timer.expires = jiffies + HZ; /* one second */
    timer_setup(&jiq_timer, jiq_timedout, TIMER_INIT_FLAGS); /* init the timer structure */

    cond = 0;
    jiq_print(&jiq_data); /* print and go to sleep */
    add_timer(&jiq_timer);
    wait_event_interruptible(jiq_wait, cond); /* RACE */
    del_timer_sync(&jiq_timer); /* in case a signal woke us up */

    if (copy_to_user(buf, tmp, jiq_data.len)) {
        return -EFAULT;
    }
    *f_pos = jiq_data.len;

    return jiq_data.len;
}

#define PROC_FILES_COUNT 4

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define OPS_TYPE proc_ops
#define OPS_READ proc_read
#else
#define OPS_TYPE file_operations
#define OPS_READ read
#endif
static const struct proc_info {
    const char* name;
    struct OPS_TYPE file_ops;
} proc_files_info[PROC_FILES_COUNT] = {
    {.name = "jiqwq", .file_ops = {.OPS_READ = jiq_read_wq}},
    {.name = "jiqwqdelay", .file_ops = {.OPS_READ = jiq_read_wq_delayed}},
    {.name = "jiqtimer", .file_ops = {.OPS_READ = jiq_read_run_timer}},
    {.name = "jiqtasklet", .file_ops = {.OPS_READ = jiq_read_tasklet}}};

static struct proc_dir_entry* proc_files[PROC_FILES_COUNT];

int create_proc(const struct proc_info* info, struct proc_dir_entry** proc_file)
{
    *proc_file = proc_create(info->name, 0644, NULL, &info->file_ops);
    if (NULL == *proc_file) {
        pr_alert("Could not initialize /proc/%s\n", info->name);
        return -ENOMEM;
    }

    pr_info("/proc/%s created\n", info->name);
    return 0;
}

int __init jiq_init(void)
{
    INIT_WORK(&jiq_data.jiq_work, jiq_print_wq);
    INIT_DELAYED_WORK(&jiq_data.jiq_delayed_work, jiq_print_wq_delayed);
    for (size_t i = 0; i < PROC_FILES_COUNT; ++i) {
        int res = create_proc(&proc_files_info[i], &proc_files[i]);
        if (res != 0) {
            for (size_t j = 0; j <= i; ++j) {
                proc_remove(proc_files[j]);
                pr_info("/proc/%s removed\n", proc_files_info[j].name);
            }
            return res;
        }
    }
    return 0;
}

void __exit jiq_cleanup(void)
{
    for (size_t i = 0; i < PROC_FILES_COUNT; ++i) {
        proc_remove(proc_files[i]);
        pr_info("/proc/%s removed\n", proc_files_info[i].name);
    }
}

module_init(jiq_init);
module_exit(jiq_cleanup);
