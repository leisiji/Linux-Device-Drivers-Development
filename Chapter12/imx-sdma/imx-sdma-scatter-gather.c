/*
 * Copyright 2006-2014 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <asm/dma.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_data/dma-imx.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/version.h>

static int g_major; /* major number of device */
static struct class *dma_tm_class;

struct dma_test {
    u32 *wbuf, *wbuf2, *wbuf3;
    u32 *rbuf, *rbuf2, *rbuf3;
    struct dma_chan *dma_m2m_chan;
    struct completion dma_m2m_ok;
    struct scatterlist sg[3], sg2[3];
};

/*
 * There is an errata here in the book.
 * This should be 1024*16 instead of 1024
 */
#define SDMA_BUF_SIZE 1024 * 16

static bool dma_m2m_filter(struct dma_chan *chan, void *param)
{
    if (!imx_dma_is_general_purpose(chan)) {
        return false;
    }
    chan->private = param;
    return true;
}

int sdma_open(struct inode *inode, struct file *filp)
{
    dma_cap_mask_t dma_m2m_mask;
    struct imx_dma_data m2m_dma_data = {0};
    struct dma_test *test = NULL;
    struct dma_chan *chan;

    test = kmalloc(sizeof(*test), GFP_KERNEL);
    if (!test) {
        pr_err("Failed to allocate dma_test.\n");
        return -1;
    }

    init_completion(&test->dma_m2m_ok);

    /* Initialize capabilities */
    dma_cap_zero(dma_m2m_mask);
    dma_cap_set(DMA_MEMCPY, dma_m2m_mask);
    m2m_dma_data.peripheral_type = IMX_DMATYPE_MEMORY;
    m2m_dma_data.priority = DMA_PRIO_HIGH;

    /* 1- Allocate a DMA slave channel. */
    chan = dma_request_channel(dma_m2m_mask, dma_m2m_filter, &m2m_dma_data);
    if (!chan) {
        pr_info("Error opening the SDMA memory to memory channel\n");
        return -EINVAL;
    } else {
        pr_info("opened channel %d, req lin %d\n", chan->chan_id, m2m_dma_data.dma_request);
        test->dma_m2m_chan = chan;
    }

    test->wbuf = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
    if (!test->wbuf) {
        pr_info("error wbuf !!!!!!!!!!!\n");
        return -1;
    }

    test->wbuf2 = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
    if (!test->wbuf2) {
        pr_info("error wbuf2 !!!!!!!!!!!\n");
        return -1;
    }

    test->wbuf3 = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
    if (!test->wbuf3) {
        pr_info("error wbuf3 !!!!!!!!!!!\n");
        return -1;
    }

    test->rbuf = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
    if (!test->rbuf) {
        pr_info("error rbuf !!!!!!!!!!!\n");
        return -1;
    }

    test->rbuf2 = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
    if (!test->rbuf2) {
        pr_info("error rbuf2 !!!!!!!!!!!\n");
        return -1;
    }

    test->rbuf3 = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
    if (!test->rbuf3) {
        pr_info("error rbuf3 !!!!!!!!!!!\n");
        return -1;
    }

    filp->private_data = test;

    return 0;
}

int sdma_release(struct inode *inode, struct file *filp)
{
    struct dma_test *test = filp->private_data;

    if (!test) {
        return -1;
    }

    dma_release_channel(test->dma_m2m_chan);
    kfree(test->wbuf);
    kfree(test->wbuf2);
    kfree(test->wbuf3);
    kfree(test->rbuf);
    kfree(test->rbuf2);
    kfree(test->rbuf3);
    kfree(test);

    return 0;
}

ssize_t sdma_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    int i;
    struct dma_test *test = filp->private_data;

    if (!test) {
        return -1;
    }

    for (i = 0; i < SDMA_BUF_SIZE / 4; i++) {
        if (*(test->rbuf + i) != *(test->wbuf + i)) {
            pr_info("buffer 1 copy falled!\n");
            return 0;
        }
    }
    pr_info("buffer 1 copy passed!\n");

    for (i = 0; i < SDMA_BUF_SIZE / 2 / 4; i++) {
        if (*(test->rbuf2 + i) != *(test->wbuf2 + i)) {
            pr_err("buffer 2 copy falled!\n");
            return 0;
        }
    }
    pr_info("buffer 2 copy passed!\n");

    for (i = 0; i < SDMA_BUF_SIZE / 4; i++) {
        if (*(test->rbuf3 + i) != *(test->wbuf3 + i)) {
            pr_info("buffer 3 copy falled!\n");
            return 0;
        }
    }
    pr_info("buffer 3 copy passed!\n");

    return 0;
}

static void dma_m2m_callback(void *data)
{
    struct completion *ok = data;
    pr_info("in %s\n", __func__);
    complete(ok);
    return;
}

ssize_t sdma_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
    u32 *index1, *index2, *index3, i, ret;
    struct dma_slave_config dma_m2m_config = {0};
    struct dma_async_tx_descriptor *dma_m2m_desc;
    dma_cookie_t cookie;
    struct dma_test *test = filp->private_data;
    struct dma_chan *chan;

    if (!test) {
        return -1;
    }

    index1 = test->wbuf;
    index2 = test->wbuf2;
    index3 = test->wbuf3;

    for (i = 0; i < SDMA_BUF_SIZE / 4; i++) {
        *(index1 + i) = 0x12121212;
    }

    for (i = 0; i < SDMA_BUF_SIZE / 4; i++) {
        *(index2 + i) = 0x34343434;
    }

    for (i = 0; i < SDMA_BUF_SIZE / 4; i++) {
        *(index3 + i) = 0x56565656;
    }

    /* 2- Set slave and controller specific parameters */
    chan = test->dma_m2m_chan;
    dma_m2m_config.direction = DMA_MEM_TO_MEM;
    dma_m2m_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    dmaengine_slave_config(chan, &dma_m2m_config);

    sg_init_table(test->sg, 3);
    sg_set_buf(&test->sg[0], test->wbuf, SDMA_BUF_SIZE);
    sg_set_buf(&test->sg[1], test->wbuf2, SDMA_BUF_SIZE);
    sg_set_buf(&test->sg[2], test->wbuf3, SDMA_BUF_SIZE);
    ret = dma_map_sg(NULL, test->sg, 3, dma_m2m_config.direction);

    sg_init_table(test->sg2, 3);
    sg_set_buf(&test->sg2[0], test->rbuf, SDMA_BUF_SIZE);
    sg_set_buf(&test->sg2[1], test->rbuf2, SDMA_BUF_SIZE);
    sg_set_buf(&test->sg2[2], test->rbuf3, SDMA_BUF_SIZE);
    ret = dma_map_sg(NULL, test->sg2, 3, dma_m2m_config.direction);

    /* 3- Get a descriptor for the transaction. */
    dma_m2m_desc = chan->device->device_prep_dma_memset_sg(chan, test->sg2, 3, 0, DMA_MEM_TO_MEM);
    if (dma_m2m_desc) {
        pr_info("Got a DMA descriptor\n");
    } else {
        pr_info("error in prep_dma_sg\n");
    }

    /* 4- Submit the transaction */
    dma_m2m_desc->callback = dma_m2m_callback;
    dma_m2m_desc->callback_param = &test->dma_m2m_ok;
    cookie = dmaengine_submit(dma_m2m_desc);
    pr_info("Got this cookie: %d\n", cookie);

    /* 5- Issue pending DMA requests and wait for callback notification */
    dma_async_issue_pending(chan);
    pr_info("waiting for DMA transaction...\n");

    /* One can use wait_for_completion_timeout() also */
    wait_for_completion(&test->dma_m2m_ok);

    /* Once the transaction is done, we need to  */
    dma_unmap_sg(NULL, test->sg, 3, dma_m2m_config.direction);
    dma_unmap_sg(NULL, test->sg2, 3, dma_m2m_config.direction);

    return count;
}

struct file_operations dma_fops = {
    .open = sdma_open,
    .release = sdma_release,
    .read = sdma_read,
    .write = sdma_write,
};

static int __init sdma_init_module(void)
{
    struct device *temp_class;

    int error;

    /* register a character device */
    error = register_chrdev(0, "sdma_test", &dma_fops);
    if (error < 0) {
        pr_info("SDMA test driver can't get major number\n");
        return error;
    }
    g_major = error;
    pr_info("SDMA test major number = %d\n", g_major);

    dma_tm_class = class_create(THIS_MODULE, "sdma_test");
    if (IS_ERR(dma_tm_class)) {
        pr_info(KERN_ERR "Error creating sdma test module class.\n");
        unregister_chrdev(g_major, "sdma_test");
        return PTR_ERR(dma_tm_class);
    }

    temp_class = device_create(dma_tm_class, NULL, MKDEV(g_major, 0), NULL, "sdma_test");

    if (IS_ERR(temp_class)) {
        pr_info(KERN_ERR "Error creating sdma test class device.\n");
        class_destroy(dma_tm_class);
        unregister_chrdev(g_major, "sdma_test");
        return -1;
    }

    pr_info("SDMA test Driver Module loaded\n");
    return 0;
}

static void sdma_cleanup_module(void)
{
    unregister_chrdev(g_major, "sdma_test");
    device_destroy(dma_tm_class, MKDEV(g_major, 0));
    class_destroy(dma_tm_class);

    pr_info("SDMA test Driver Module Unloaded\n");
}

module_init(sdma_init_module);
module_exit(sdma_cleanup_module);

MODULE_AUTHOR("Freescale Semiconductor");
MODULE_AUTHOR("John Madieu <john.madieu@gmail.com>");
MODULE_DESCRIPTION("SDMA test driver");
MODULE_LICENSE("GPL");
