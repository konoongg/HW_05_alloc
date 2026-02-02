#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "alloc/alloc.h"

static int __init mod_init(void)
{
    pr_info("hello, from kernel \n");
    int res = allocator_init();

    if (res != ALLOC_OK)
        pr_err("can't init alloc \n");

    return 0;
}

static void __exit mod_exit(void)
{
    allocator_cleanup();
    pr_info("bye, from kernel \n");
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladislav Akhmedov");
MODULE_DESCRIPTION("Kernel module with alloc example");