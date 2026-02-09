#include <linux/vmalloc.h> 
#include <linux/bitmap.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include "alloc.h"

#define ALLOC_COUNT_FRAGMENT 2000
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
    size_t mem;
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
    stats.fragmentation_percent =  0;
    stats.allocated_memory = 0;
    stats.allocated_blocks = 0;
}

static void
stats_update(bool is_alloc, size_t mem_size, size_t cnt_blocks)
{
    if (is_alloc) {
        stats.free_blocks -= cnt_blocks;
        stats.allocated_blocks += cnt_blocks;
        stats.free_memory -= mem_size;
        stats.allocated_memory += mem_size;
    } else {
        stats.free_blocks += cnt_blocks;
        stats.allocated_blocks -= cnt_blocks;
        stats.free_memory += mem_size;
        stats.allocated_memory -= mem_size;
    }

    if (stats.allocated_blocks == 0)
        stats.fragmentation_percent = 0;
    else
        stats.fragmentation_percent = 100 - (stats.allocated_memory * 100 / (stats.allocated_blocks * ALLOC_SIZE_FRAGMENT));
}

extern int
allocator_init(void)
{
    alloc.block_size = ALLOC_SIZE_FRAGMENT;
    alloc.total_blocks = ALLOC_COUNT_FRAGMENT;
    alloc.ptr = vmalloc(ALLOC_ALL_SIZE);
    if (!alloc.ptr)
        return ALLOC_NOMEM;

    stats_init();

    alloc.bitmap = bitmap_zalloc(ALLOC_COUNT_FRAGMENT, GFP_KERNEL);
    if (!alloc.bitmap)
        goto fail;

    alloc.cur_block = 0;

    mutex_init(&alloc.lock);

    return ALLOC_OK;

fail:
    vfree(alloc.ptr);
    return ALLOC_NOMEM;
}

extern void *
allocator_alloc(size_t bytes)
{
    if (!alloc.bitmap)
        return NULL; 

    if (unlikely(bytes == 0))
        return NULL;

    size_t blocks_cnt = __KERNEL_DIV_ROUND_UP(bytes + sizeof(allocation_info), alloc.block_size);

    if (unlikely(blocks_cnt > alloc.total_blocks))
        return NULL;

    mutex_lock(&alloc.lock);

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
    stats_update(IS_ALLOC, bytes, blocks_cnt);
    alloc.cur_block = (index + blocks_cnt < alloc.total_blocks) ? index + blocks_cnt : 0;

    mutex_unlock(&alloc.lock);

    allocation_info alloc_info;
    alloc_info.num_blocks = blocks_cnt;
    alloc_info.start_block = index;
    alloc_info.mem = bytes;
    alloc_info.magic = MAGIC;

    void *ptr = alloc.ptr + index * alloc.block_size;
    memcpy(ptr, &alloc_info, sizeof(alloc_info));

    return ptr + sizeof(allocation_info);
}

extern void
allocator_free(void *ptr)
{
    if (!alloc.bitmap)
        return; 

    allocation_info *info = ptr - sizeof(allocation_info);

    if (!IS_ALIGNED((unsigned long) info, sizeof(void *)))
    {
        pr_warn("incor ptr in free: bad align \n");
        return;
    }

    if (info->magic != MAGIC)
    {
        pr_warn("incor ptr in free \n");
        return;
    }

    mutex_lock(&alloc.lock);

    bitmap_clear(alloc.bitmap, info->start_block, info->num_blocks);
    
    stats_update(IS_FREE, info->mem, info->num_blocks);
    memset((void *)info, 0, info->num_blocks * alloc.block_size);

    mutex_unlock(&alloc.lock);

}

extern alloc_stats
allocator_get_stats(void)
{
    mutex_lock(&alloc.lock);
    alloc_stats new_stats = stats;
    mutex_unlock(&alloc.lock);

    return new_stats;
}

extern int
allocator_get_info(void *buf)
{

    if (!alloc.bitmap)
        return -ENOMEM;

    char *info = vmalloc(ALLOC_COUNT_FRAGMENT + 1);
    if (!info)
        return -ENOMEM;

    unsigned long *bitmap = bitmap_zalloc(ALLOC_COUNT_FRAGMENT, GFP_KERNEL);;

    mutex_lock(&alloc.lock);
    bitmap_copy(bitmap, alloc.bitmap, ALLOC_COUNT_FRAGMENT);
    mutex_unlock(&alloc.lock);

    for (int i = 0; i < ALLOC_COUNT_FRAGMENT; ++i)
    {
        if (test_bit(i, bitmap))
            info[i] = 'X';
        else
            info[i] = '_';
    }
    info[ALLOC_COUNT_FRAGMENT] = '\0';

    int len = sprintf(buf, "bitmap info [%s]", info);

    vfree(info);

    bitmap_free(bitmap);
    return len;
}

extern void
allocator_cleanup(void)
{
    bitmap_free(alloc.bitmap);
    vfree(alloc.ptr);
}
