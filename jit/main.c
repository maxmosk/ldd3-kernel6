#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/version.h>

#include <asm/hardirq.h>

MODULE_LICENSE("GPL");

static ssize_t jit_currentime(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    char tmp[256];

    struct timespec64 tv1;
    struct timespec64 tv2;
    unsigned long j1;
    u64 j2;

    /* get them four */
    j1 = jiffies;
    j2 = get_jiffies_64();
    ktime_get_real_ts64(&tv1);
    ktime_get_coarse_real_ts64(&tv2);

    /* print */
    len = 0;
    len += sprintf(
        tmp,
        "0x%08lx 0x%016Lx %40i.%09i\n"
        "%40i.%09i\n",
        j1,
        j2,
        (int)tv1.tv_sec,
        (int)tv1.tv_nsec,
        (int)tv2.tv_sec,
        (int)tv2.tv_nsec);
    if (copy_to_user(buf, tmp, len)) {
        return -EFAULT;
    }
    *f_pos = len;

    return len;
}

int delay = HZ; /* The default delay, expressed in jiffies. */
module_param(delay, int, 0);

static ssize_t jit_busy(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    char tmp[64];

    unsigned long j0, j1; /* Jiffies. */
    j0 = jiffies;
    j1 = j0 + delay;

    while (time_before(jiffies, j1)) {
        cpu_relax();
    }

    j1 = jiffies; /* Actual value after we delayed. */

    len = sprintf(tmp, "%9li %9li\n", j0, j1);
    if (copy_to_user(buf, tmp, len)) {
        return -EFAULT;
    }
    *f_pos = len;
    return len;
}

static ssize_t jit_sched(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    char tmp[64];

    unsigned long j0, j1; /* Jiffies. */
    j0 = jiffies;
    j1 = j0 + delay;

    while (time_before(jiffies, j1)) {
        schedule();
    }

    j1 = jiffies; /* Actual value after we delayed. */

    len = sprintf(tmp, "%9li %9li\n", j0, j1);
    if (copy_to_user(buf, tmp, len)) {
        return -EFAULT;
    }
    *f_pos = len;
    return len;
}

static ssize_t jit_queue(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    char tmp[64];

    unsigned long j0, j1; /* Jiffies. */
    wait_queue_head_t wait;

    init_waitqueue_head(&wait);
    j0 = jiffies;
    j1 = j0 + delay;

    wait_event_interruptible_timeout(wait, 0, delay);

    j1 = jiffies; /* Actual value after we delayed. */

    len = sprintf(tmp, "%9li %9li\n", j0, j1);
    if (copy_to_user(buf, tmp, len)) {
        return -EFAULT;
    }
    *f_pos = len;
    return len;
}

static ssize_t jit_schedto(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    char tmp[64];

    unsigned long j0, j1; /* Jiffies. */

    j0 = jiffies;
    j1 = j0 + delay;

    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(delay);

    j1 = jiffies; /* Actual value after we delayed. */

    len = sprintf(tmp, "%9li %9li\n", j0, j1);
    if (copy_to_user(buf, tmp, len)) {
        return -EFAULT;
    }
    *f_pos = len;
    return len;
}

/*
 * The timer example follows
 */

int tdelay = 10;
module_param(tdelay, int, 0);

/* This data structure is used as "data" for the timer and tasklet functions. */
struct jit_data {
    struct timer_list timer;
    struct tasklet_struct tlet;
    int hi; /* tasklet or tasklet_hi */
    wait_queue_head_t wait;
    unsigned long prevjiffies;
    char* tmpBuf;
    int loops;
};
#define JIT_ASYNC_LOOPS 5

void jit_timer_fn(struct timer_list* timer)
{
    struct jit_data* data = container_of(timer, struct jit_data, timer);
    unsigned long j = jiffies;
    size_t len = sprintf(
        data->tmpBuf,
        "%9li  %3li     %i    %6i   %i   %s\n",
        j,
        j - data->prevjiffies,
        in_interrupt() ? 1 : 0,
        current->pid,
        smp_processor_id(),
        current->comm);
    data->tmpBuf += len;
    if (--data->loops) {
        data->timer.expires += tdelay;
        data->prevjiffies = j;
        add_timer(&data->timer);
    } else {
        wake_up(&data->wait);
    }
}

/* the /proc function: allocate everything to allow concurrency */
static ssize_t jit_timer(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    struct jit_data data;
    unsigned long j = jiffies;

    timer_setup(&data.timer, jit_timer_fn, TIMER_INIT_FLAGS);
    init_waitqueue_head(&data.wait);

    /* Write the first lines in the buffer. */
    char tmp[768];
    len = sprintf(tmp, "   time   delta  inirq    pid   cpu command\n");
    len += sprintf(
        tmp + len,
        "%9li  %3li     %i    %6i   %i   %s\n",
        j,
        0L,
        in_interrupt() ? 1 : 0,
        current->pid,
        smp_processor_id(),
        current->comm);

    /* Fill the data for our timer function. */
    data.prevjiffies = j;
    data.tmpBuf = tmp + len;
    data.loops = JIT_ASYNC_LOOPS;

    /* Register the timer. */
    data.timer.expires = j + tdelay; /* Parameter. */
    add_timer(&data.timer);

    /* Wait for the buffer to fill. */
    wait_event(data.wait, !data.loops);
    if (signal_pending(current)) {
        return -ERESTARTSYS;
    }
    len = data.tmpBuf - tmp;
    if (copy_to_user(buf, tmp, len)) {
        return -EFAULT;
    }
    *f_pos = len;
    return len;
}

void jit_tasklet_fn(unsigned long arg)
{
    struct jit_data* data = (struct jit_data*)arg;
    unsigned long j = jiffies;
    data->tmpBuf += sprintf(
        data->tmpBuf,
        "%9li  %3li     %i    %6i   %i   %s\n",
        j,
        j - data->prevjiffies,
        in_interrupt() ? 1 : 0,
        current->pid,
        smp_processor_id(),
        current->comm);

    if (--data->loops) {
        data->prevjiffies = j;
        if (data->hi) {
            tasklet_hi_schedule(&data->tlet);
        } else {
            tasklet_schedule(&data->tlet);
        }
    } else {
        wake_up(&data->wait);
    }
}

/* The /proc function: allocate everything to allow concurrency. */
static ssize_t jit_tasklet(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    struct jit_data data;
    unsigned long j = jiffies;

    init_waitqueue_head(&data.wait);

    /* Write the first lines in the buffer. */
    char tmp[768];
    len = sprintf(tmp, "   time   delta  inirq    pid   cpu command\n");
    len += sprintf(
        tmp + len,
        "%9li  %3li     %i    %6i   %i   %s\n",
        j,
        0L,
        in_interrupt() ? 1 : 0,
        current->pid,
        smp_processor_id(),
        current->comm);

    /* Fill the data for our tasklet function. */
    data.prevjiffies = j;
    data.tmpBuf = tmp + len;
    data.loops = JIT_ASYNC_LOOPS;

    /* register the tasklet */
    tasklet_init(&data.tlet, jit_tasklet_fn, (unsigned long)&data);
    data.hi = 0;
    tasklet_schedule(&data.tlet);

    /* Wait for the buffer to fill. */
    wait_event(data.wait, !data.loops);

    if (signal_pending(current)) {
        return -ERESTARTSYS;
    }
    len = data.tmpBuf - tmp;
    if (copy_to_user(buf, tmp, len)) {
        return -EFAULT;
    }
    *f_pos = len;
    return len;
}

static ssize_t jit_tasklethi(struct file* filp, char __user* buf, size_t len, loff_t* f_pos)
{
    if (*f_pos) {
        return 0;
    }

    struct jit_data data;
    unsigned long j = jiffies;

    init_waitqueue_head(&data.wait);

    /* Write the first lines in the buffer. */
    char tmp[768];
    len = sprintf(tmp, "   time   delta  inirq    pid   cpu command\n");
    len += sprintf(
        tmp + len,
        "%9li  %3li     %i    %6i   %i   %s\n",
        j,
        0L,
        in_interrupt() ? 1 : 0,
        current->pid,
        smp_processor_id(),
        current->comm);

    /* Fill the data for our tasklet function. */
    data.prevjiffies = j;
    data.tmpBuf = tmp + len;
    data.loops = JIT_ASYNC_LOOPS;

    /* register the tasklet */
    tasklet_init(&data.tlet, jit_tasklet_fn, (unsigned long)&data);
    data.hi = 0;
    tasklet_hi_schedule(&data.tlet);

    /* Wait for the buffer to fill. */
    wait_event(data.wait, !data.loops);

    if (signal_pending(current)) {
        return -ERESTARTSYS;
    }
    len = data.tmpBuf - tmp;
    if (copy_to_user(buf, tmp, len)) {
        return -EFAULT;
    }
    *f_pos = len;
    return len;
}

#define PROC_FILES_COUNT 8

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
    {.name = "currentime", .file_ops = {.OPS_READ = jit_currentime}},
    {.name = "jitbusy", .file_ops = {.OPS_READ = jit_busy}},
    {.name = "jitsched", .file_ops = {.OPS_READ = jit_sched}},
    {.name = "jitqueue", .file_ops = {.OPS_READ = jit_queue}},
    {.name = "jitschedto", .file_ops = {.OPS_READ = jit_schedto}},
    {.name = "jitimer", .file_ops = {.OPS_READ = jit_timer}},
    {.name = "jitasklet", .file_ops = {.OPS_READ = jit_tasklet}},
    {.name = "jitasklethi", .file_ops = {.OPS_READ = jit_tasklethi}}};

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

int __init jit_init(void)
{
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

void __exit jit_cleanup(void)
{
    for (size_t i = 0; i < PROC_FILES_COUNT; ++i) {
        proc_remove(proc_files[i]);
        pr_info("/proc/%s removed\n", proc_files_info[i].name);
    }
}

module_init(jit_init);
module_exit(jit_cleanup);
