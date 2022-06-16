#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>


struct my_data {
    struct timer_list timer;
    int other;
};
static struct my_data data;

void my_timer_callback(struct timer_list *timer)
{
    struct my_data *d = from_timer(d, timer, timer);
    pr_info("other is %d, jiffies is (%ld).\n", d->other, jiffies);
}

static int __init my_init(void)
{
    int retval;
    pr_info("Timer module loaded\n");

    data.other = 100;

    timer_setup(&data.timer, my_timer_callback, 0);
    pr_info("Setup timer to fire in 300ms (%ld)\n", jiffies);

    retval = mod_timer(&data.timer, jiffies + msecs_to_jiffies(300));
    if (retval)
        pr_info("Timer firing failed\n");

    return 0;
}

static void my_exit(void)
{
    int retval = del_timer(&data.timer);
    if (retval)
        pr_info("The timer is still in use...\n");

    pr_info("Timer module unloaded\n");
    return;
}

module_init(my_init);
module_exit(my_exit);
MODULE_AUTHOR("John Madieu <john.madieu@gmail.com>");
MODULE_DESCRIPTION("Standard timer example");
MODULE_LICENSE("GPL");
