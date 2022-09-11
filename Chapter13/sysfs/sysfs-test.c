#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "linux/sysfs.h"

static int baz;
static int bar;

static ssize_t b_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int var;

    if (strcmp(attr->attr.name, "baz") == 0)
        var = baz;
    else
        var = bar;
    return sysfs_emit(buf, "%d\n", var);
}

static ssize_t b_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,
                       size_t count)
{
    int var, ret;

    ret = kstrtoint(buf, 10, &var);
    if (ret < 0) return ret;

    if (strcmp(attr->attr.name, "baz") == 0)
        baz = var;
    else
        bar = var;
    return count;
}

static struct kobj_attribute baz_attribute = __ATTR(baz, 0664, b_show, b_store);
static struct kobj_attribute bar_attribute = __ATTR(bar, 0664, b_show, b_store);

static struct attribute *attrs[] = {
    &baz_attribute.attr, &bar_attribute.attr,
    NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

static struct kobject *mykobj;

static int __init hello_module_init(void)
{
    mykobj = kobject_create_and_add("hello", kernel_kobj);
    if (!mykobj) {
        return -ENOMEM;
    }

    if (sysfs_create_group(mykobj, &attr_group)) {
        kobject_put(mykobj);
        return -1;
    }

    return 0;
}

static void __exit hello_module_exit(void)
{
    kobject_put(mykobj);
}

module_init(hello_module_init);
module_exit(hello_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Madieu <john.madieu@gmail.com>");
