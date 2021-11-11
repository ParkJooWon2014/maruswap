#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x77c62142, "module_layout" },
	{ 0xace0ff9, "kmalloc_caches" },
	{ 0xdd25ed6f, "param_ops_int" },
	{ 0x1f7a308b, "xa_find_after" },
	{ 0x4f31063a, "ib_dealloc_pd_user" },
	{ 0xf1cbf410, "lockdep_init_map_waits" },
	{ 0x88f522ff, "rdma_join_multicast" },
	{ 0x56470118, "__warn_printk" },
	{ 0xb43f9365, "ktime_get" },
	{ 0xf3542a7b, "xa_find" },
	{ 0xfd7f71c9, "rdma_destroy_id" },
	{ 0xecd1fb35, "ib_free_cq" },
	{ 0x74262423, "mutex_unlock" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0xe5840ec6, "ib_wc_status_msg" },
	{ 0x862f9edf, "xa_erase" },
	{ 0x2bb2e2fe, "mutex_trylock" },
	{ 0x473a37dc, "param_ops_string" },
	{ 0xbe98fc2, "rdma_connect_locked" },
	{ 0x5e128aef, "ib_alloc_mr" },
	{ 0x91dff4e, "_raw_spin_unlock_irqrestore" },
	{ 0x962331e0, "__mutex_init" },
	{ 0xc5850110, "printk" },
	{ 0xe1537255, "__list_del_entry_valid" },
	{ 0xa4514649, "rdma_destroy_qp" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0xb14e765, "__ib_alloc_cq" },
	{ 0xc11fdd72, "init_net" },
	{ 0x68f31cbd, "__list_add_valid" },
	{ 0xf7b53f14, "rdma_create_qp" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0xcdef06b5, "ib_register_client" },
	{ 0x9edadcd8, "rdma_resolve_route" },
	{ 0x2d0117d9, "__rdma_create_kernel_id" },
	{ 0xc959d152, "__stack_chk_fail" },
	{ 0xac5fcec0, "in4_pton" },
	{ 0x686c6b2f, "dma_map_page_attrs" },
	{ 0xf56ab873, "__raw_spin_lock_init" },
	{ 0x2ea2c95c, "__x86_indirect_thunk_rax" },
	{ 0x2e9b61c5, "wait_for_completion_interruptible_timeout" },
	{ 0x9fb60db9, "rdma_create_ah" },
	{ 0x8046d0f7, "dev_driver_string" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xb67ed80d, "kmem_cache_alloc_trace" },
	{ 0x96d42af6, "_raw_spin_lock_irqsave" },
	{ 0x9a479b49, "ib_dereg_mr_user" },
	{ 0x37a0cba, "kfree" },
	{ 0xd4dd8539, "dma_sync_single_for_device" },
	{ 0x531199c6, "rdma_resolve_addr" },
	{ 0x31c8bb69, "dma_unmap_page_attrs" },
	{ 0xa9b7c698, "__init_swait_queue_head" },
	{ 0x55d1d3e8, "__ib_alloc_pd" },
	{ 0x41dddf06, "complete" },
	{ 0x907df803, "rdma_event_msg" },
	{ 0xcde1d840, "ib_unregister_client" },
	{ 0xc31db0ce, "is_vmalloc_addr" },
	{ 0xc5579424, "rdma_leave_multicast" },
};

MODULE_INFO(depends, "ib_core,rdma_cm");

