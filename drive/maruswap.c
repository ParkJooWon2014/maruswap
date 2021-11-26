#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/frontswap.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/page-flags.h>
#include <linux/memcontrol.h>
#include <linux/smp.h>

#include "ib.h"


static int maruswap_store(unsigned type, pgoff_t pageid,
		struct page *page)
{
	if (unlikely(maruswap_multicast_write(page, pageid << PAGE_SHIFT))) {
		pr_err("could not store page remotely\n");
		return -1;
	}

	return 0;
}

static int maruswap_load(unsigned type, pgoff_t pageid, struct page *page)
{
	
	if (unlikely(maruswap_rdma_read(page, pageid << PAGE_SHIFT))) {
		pr_err("could not read page remotely\n");
		return -1;
	}

	return 0;
}

static void maruswap_invalidate_page(unsigned type, pgoff_t offset)
{
	return;
}

static void maruswap_invalidate_area(unsigned type)
{
	pr_err("maruswap_invalidate_area\n");
}

static void maruswap_init(unsigned type)
{
	pr_info("maruswap_init end\n");
}


static int __init maruswap_init_debugfs(void)
{
	return 0;
}

static struct frontswap_ops maruswap_ops = {
	.init = maruswap_init,
	.store = maruswap_store,
	.load = maruswap_load,
	.invalidate_page = maruswap_invalidate_page,
	.invalidate_area = maruswap_invalidate_area,
};

static int __init init_maruswap(void)
{
	frontswap_register_ops(&maruswap_ops);
	if (maruswap_init_debugfs())
		pr_err("maruswap debugfs failed\n");

	pr_info("maruswap module loaded\n");
	return 0;
}

static void __exit exit_maruswap(void)
{
	pr_info("unloading maruswap\n");
}

module_init(init_maruswap);
module_exit(exit_maruswap);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("maruswap driver");
