#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "ib.h"

#include <linux/module.h>
#include <linux/frontswap.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/page-flags.h>
#include <linux/memcontrol.h>
#include <linux/smp.h>


static int mbswap_store(unsigned type, pgoff_t pageid,
		struct page *page)
{

	if (mbswap_multicast_write(page, pageid << PAGE_SHIFT)) {
		pr_err("could not store page remotely\n");
		return -1;
	}

	return 0;
}

static int mbswap_load(unsigned type, pgoff_t pageid, struct page *page)
{
	
	if (unlikely(mbswap_rdma_read(page, pageid << PAGE_SHIFT))) {
		pr_err("could not read page remotely\n");
		return -1;
	}

	return 0;
}

static void mbswap_invalidate_page(unsigned type, pgoff_t offset)
{
	return;
}

static void mbswap_invalidate_area(unsigned type)
{
	pr_err("mbswap_invalidate_area\n");
}

static void mbswap_init(unsigned type)
{
	pr_info("mbswap_init end\n");
}


static int __init mbswap_init_debugfs(void)
{
	return 0;
}

static struct frontswap_ops mbswap_ops = {
	.init = mbswap_init,
	.store = mbswap_store,
	.load = mbswap_load,
	.invalidate_page = mbswap_invalidate_page,
	.invalidate_area = mbswap_invalidate_area,
};

static int __init init_mbswap(void)
{
	frontswap_register_ops(&mbswap_ops);
	if (mbswap_init_debugfs())
		pr_err("mbswap debugfs failed\n");

	pr_info("mbswap module loaded\n");
	return 0;
}


static void __exit exit_mbswap(void)
{
	pr_info("unloading mbswap\n");
}

module_init(init_mbswap);
module_exit(exit_mbswap);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mbswap driver");
