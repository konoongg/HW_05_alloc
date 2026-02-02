#include <linux/stat.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/module.h>

#include "alloc/alloc.h"

static int param_set_alloc(const char *val, const struct kernel_param *kp)
{
    size_t size;
    void *ptr;

    if (kstrtoul(val, 10, &size) != 0)
        return - EINVAL;

    if (size == 0)
        return - EINVAL;

    ptr = allocator_alloc(size);
    if (!ptr)
    {
        pr_warn("alloc error: can't alloc data wtih size %ld\n", size);
        return -ENOMEM;
    }

    pr_info("kernel_alloc: allocated %ld bytes at 0x%016llx", size, (unsigned long long)ptr);

    return 0;
}

static int param_set_free(const char *val, const struct kernel_param *kp)
{
    unsigned long addr;
    void *ptr;

    if (kstrtoul(val, 0, &addr) != 0)
        return -EINVAL;

    ptr = (void *)addr;
    allocator_free(ptr);

    pr_info("kernel_alloc: free 0x%016llx", (unsigned long long)ptr);

    return 0;
}

static int param_get_stat(char *buf, const struct kernel_param *kp)
{
    int len = 0;
    alloc_stats stat = allocator_get_stats();

    len += sprintf(buf + len, "Total: %zu KB", stat.total_memory / 1024);
    len += sprintf(buf + len, " | Free: %zu KB", stat.free_memory / 1024);
    len += sprintf(buf + len, " | Allocated: %zu KB", stat.allocated_memory / 1024);
    len += sprintf(buf + len, " | Blocks: total=%zu free=%zu alloc=%zu", stat.total_blocks, stat.free_blocks, stat.allocated_blocks);
    len += sprintf(buf + len, " | Fragmentation: %ld\n", stat.fragmentation_percent);

    return len;
}

static int param_get_bitmap_info(char *buf, const struct kernel_param *kp)
{
    return allocator_get_info(buf);
}


static const struct kernel_param_ops alloc_params_ops = {
    .set = param_set_alloc,
};

static const struct kernel_param_ops free_params_ops = {
    .set = param_set_free,
};

static const struct kernel_param_ops stat_params_ops = {
    .get = param_get_stat,
};

static const struct kernel_param_ops bitmap_info_params_ops = {
    .get = param_get_bitmap_info,
};

module_param_cb(alloc, &alloc_params_ops, NULL, S_IWUSR);
module_param_cb(free, &free_params_ops, NULL, S_IWUSR);

module_param_cb(stat, &stat_params_ops, NULL, S_IRUSR);
module_param_cb(bitmap_info, &bitmap_info_params_ops, NULL, S_IRUSR);