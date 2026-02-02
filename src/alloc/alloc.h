#ifndef HW_05_ALLOC_H
#define HW_05_ALLOC_H

#include <linux/spinlock.h>

typedef struct alloc_stats alloc_stats;

#define ALLOC_OK        0
#define ALLOC_NOMEM     -1
#define ALLOC_INVALID   -2
#define ALLOC_NOT_FOUND -3

struct alloc_stats {
    size_t total_blocks;
    size_t free_blocks;
    size_t allocated_blocks;
    size_t total_memory;
    size_t free_memory;
    size_t allocated_memory;
    size_t fragmentation_percent; 
};

extern int allocator_init(void);
extern void * allocator_alloc(size_t bytes);
extern int allocator_get_info(void *buf);
extern void allocator_free(void *ptr);
extern alloc_stats allocator_get_stats(void);
extern void allocator_cleanup(void);

#endif