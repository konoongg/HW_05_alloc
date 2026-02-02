#include <linux/slab.h>
#include <linux/bitmap.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include "alloc.h"

#define ALLOC_COUNT_FRAGMENT 2560
#define ALLOC_SIZE_FRAGMENT 4096
#define ALLOC_ALL_SIZE (ALLOC_COUNT_FRAGMENT * ALLOC_SIZE_FRAGMENT)
#define MAGIC 17654

typedef struct memory_allocator memory_allocator;
typedef struct allocation_info allocation_info;

#define IS_ALLOC true
#define IS_FREE false

struct memory_allocator {
    unsigned long *bitmap;
    void *ptr;
    size_t total_blocks;
    size_t block_size;
    size_t cur_block;
    struct mutex lock;
};

struct allocation_info
{
    int magic;
    size_t start_block;
    size_t num_blocks;
};

memory_allocator alloc;
alloc_stats stats;

static void
stats_init(void)
{
    stats.total_memory = ALLOC_ALL_SIZE;
    stats.total_blocks = ALLOC_COUNT_FRAGMENT;
    stats.free_blocks = ALLOC_COUNT_FRAGMENT;
    stats.free_memory = ALLOC_ALL_SIZE;
    stats.fragmentation_percent =  ((double) stats.allocated_blocks /  stats.free_blocks );
    stats.allocated_memory = 0;
    stats.allocated_blocks = 0;
}

static void
stats_update(bool is_alloc, size_t cnt_blocks, size_t block_size)
{
    if (is_alloc) {
        stats.free_blocks -= cnt_blocks;
        stats.allocated_blocks += cnt_blocks;
        stats.free_memory -= cnt_blocks * block_size;
        stats.allocated_memory += cnt_blocks * block_size;
    } else {
        stats.free_blocks += cnt_blocks;
        stats.allocated_blocks -= cnt_blocks;
        stats.free_memory += cnt_blocks * block_size;
        stats.allocated_memory -= cnt_blocks * block_size;
    }

    stats.fragmentation_percent =  ((double) stats.allocated_blocks /  stats.free_blocks );
}

extern int
allocator_init(void)
{
    alloc.block_size = ALLOC_SIZE_FRAGMENT;
    alloc.total_blocks = ALLOC_COUNT_FRAGMENT;
    alloc.ptr = kmalloc(ALLOC_ALL_SIZE, GFP_KERNEL);
    if (!alloc.ptr)
        return ALLOC_NOMEM;

    stats_init();

    alloc.bitmap = bitmap_zalloc(ALLOC_COUNT_FRAGMENT, GFP_KERNEL);
    if (!alloc.bitmap)
        goto fail;

    alloc.cur_block = 0;

    mutex_init(&alloc->lock);

    return ALLOC_OK;

fail:
    kfree(alloc.ptr);
    return ALLOC_NOMEM;
}

extern void *
allocator_alloc(size_t bytes)
{
    if (unlikely(bytes == 0))
        return NULL;

    size_t blocks_cnt = __KERNEL_DIV_ROUND_UP(bytes + sizeof(allocation_info), alloc.block_size);

    if (unlikely(blocks_cnt > alloc.total_blocks))
        return NULL;

    mutex_lock(&alloc->lock);

again:

    size_t index = bitmap_find_next_zero_area(alloc.bitmap, alloc.total_blocks, alloc.cur_block, blocks_cnt, 0);

    if (index > alloc.total_blocks)
    {
        if (alloc.cur_block != 0)
        {
            alloc.cur_block = 0;
            goto again;
        }
        return NULL;
    }

    bitmap_set(alloc.bitmap, index, blocks_cnt);
    stats_update(IS_ALLOC, blocks_cnt, alloc.block_size);
    alloc.cur_block = (index + blocks_cnt < alloc.total_blocks) ? index + blocks_cnt : 0;

    mutex_unlock(&alloc->lock);

    allocation_info alloc_info;
    alloc_info.num_blocks = blocks_cnt;
    alloc_info.start_block = index;
    alloc_info.magic = MAGIC;

    void *ptr = alloc.ptr + index * alloc.block_size;
    memcpy(ptr, &alloc_info, sizeof(alloc_info));

    return ptr + sizeof(allocation_info);
}

extern void
allocator_free(void *ptr)
{
    allocation_info *info = ptr - sizeof(allocation_info);

    if (info->magic != MAGIC)
    {
        pr_warn("incor ptr in free \n");
        return;
    }

    mutex_lock(&alloc->lock);

    bitmap_clear(alloc.bitmap, info->start_block, info->num_blocks);
    memset((void *)info, 0, info->num_blocks * alloc.block_size);
    stats_update(IS_FREE, info->num_blocks, alloc.block_size);

    mutex_unlock(&alloc->lock);

}

extern alloc_stats allocator_get_stats(void)
{
    mutex_lock(&alloc->lock);
    alloc_stats new_stats = stats;
    mutex_unlock(&alloc->lock);

    return new_stats;
}

extern void
allocator_cleanup(void)
{
    bitmap_free(alloc.bitmap);
    kfree(alloc.ptr);
}
