#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

int *a[20];

static int __init hellowolrd_init(void) {
    int i = 0;
    pr_info("Hello world!\n");
    for (i = 0; i < ARRAY_SIZE(a); i++) {
        a[i] = vmalloc(1024 * 1024 * 1024);
        if (a[i] == NULL) {
            printk("Failed\n");
        } else {
            printk("succeded\n");
        }
    }
    return 0;
}

static void __exit hellowolrd_exit(void) {
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(a); i++) {
        vfree(a[i]);
    }
    pr_info("End of the world\n");
}

module_init(hellowolrd_init);
module_exit(hellowolrd_exit);
MODULE_AUTHOR("John Madieu <john.madieu@gmail.com>");
MODULE_LICENSE("GPL");
