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
	{ 0x71c571d5, "frontswap_register_ops" },
	{ 0x74262423, "mutex_unlock" },
	{ 0x2bb2e2fe, "mutex_trylock" },
	{ 0x68f31cbd, "__list_add_valid" },
	{ 0xf1cbf410, "lockdep_init_map_waits" },
	{ 0x962331e0, "__mutex_init" },
	{ 0xf56ab873, "__raw_spin_lock_init" },
	{ 0xa9b7c698, "__init_swait_queue_head" },
	{ 0x45d6230e, "mbswap_multicast_write" },
	{ 0x73ada34a, "mbswap_rdma_read" },
	{ 0xc5850110, "printk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "sib");

