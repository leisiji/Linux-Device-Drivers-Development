#include <linux/container_of.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include "linux/printk.h"

struct poll_attr {
    struct attribute attr;
    int value;
};

static struct kobject *poll_kobj;

static ssize_t poll_show(struct kobject *obj, struct attribute *attr, char *buf)
{
    struct poll_attr *poll = container_of(attr, struct poll_attr, attr);
    return scnprintf(buf, PAGE_SIZE, "%s: %d\n", attr->name, poll->value);
}
static ssize_t poll_store(struct kobject *obj, struct attribute *attr, const char *buf, size_t len)
{
    struct poll_attr *poll = container_of(attr, struct poll_attr, attr);

    if (1 != sscanf(buf, "%d", &poll->value)) {
        return -1;
    }
    pr_info("hello: %s\n", obj->name); // 这里打印的是 poll_test
    sysfs_notify(poll_kobj, "poll_group", attr->name);

    return len;
}

static struct sysfs_ops poll_fops = {
    .show = poll_show,
    .store = poll_store,
};

static struct poll_attr poll_attr = {
    .attr = {
        .name = "poll_attr",
        .mode = 0644,
    },
    .value = 0,
};

static struct attribute *poll_attrs[] = {
    &poll_attr.attr,
    NULL,
};

static const struct attribute_group poll_grp = {.name = "poll_group", .attrs = poll_attrs};

static const struct attribute_group *poll_grps[] = {
    &poll_grp,
    NULL,
};

static struct kobj_type poll_kobj_type = {.sysfs_ops = &poll_fops, .default_groups = poll_grps};

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

static struct kobject *my_kobj_root;

static int __init hello_module_init(void)
{
    my_kobj_root = kobject_create_and_add("hello", kernel_kobj);
    if (!my_kobj_root) {
        return -ENOMEM;
    }

    if (sysfs_create_group(my_kobj_root, &attr_group)) {
        kobject_put(my_kobj_root);
        return -1;
    }

    poll_kobj = kmalloc(sizeof(*poll_kobj), GFP_KERNEL);
    if (!poll_kobj) {
        goto err;
    }

    if (kobject_init_and_add(poll_kobj, &poll_kobj_type, my_kobj_root, "poll_test")) {
        goto err;
    }

    return 0;

err:
    if (poll_kobj != NULL) {
        kobject_put(poll_kobj);
    }
    sysfs_remove_group(my_kobj_root, &attr_group);
    kobject_put(my_kobj_root);
    return -1;
}

static void __exit hello_module_exit(void)
{
    kobject_put(poll_kobj);
    kobject_put(my_kobj_root);
}

module_init(hello_module_init);
module_exit(hello_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Madieu <john.madieu@gmail.com>");
