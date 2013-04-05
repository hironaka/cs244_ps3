#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x35ec255d, "module_layout" },
	{ 0x88290f62, "seq_open" },
	{ 0x83fc8ef5, "register_qdisc" },
	{ 0x96c7e43b, "seq_printf" },
	{ 0x999872e2, "remove_proc_entry" },
	{ 0x3f81ddad, "seq_read" },
	{ 0x12a16cb0, "skb_queue_purge" },
	{ 0x50eedeb8, "printk" },
	{ 0xb4390f9a, "mcount" },
	{ 0xd39683f4, "unregister_qdisc" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xe898da75, "kfree_skb" },
	{ 0xfc635aa2, "create_proc_entry" },
	{ 0x65a37537, "seq_lseek" },
	{ 0xb81960ca, "snprintf" },
	{ 0x3134a249, "seq_release" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "D8C5798FCB361DAB9FCB09D");
