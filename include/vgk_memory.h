#ifndef VGK_MEMORY_H
#define VGK_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#include "vgk_boot_info.h"

#define VGK_MEMORY_PAGE_SIZE UINT64_C(4096)
#define VGK_MEMORY_MAX_RANGES 16384U

typedef enum {
    VGK_MEMORY_OK = 0,
    VGK_MEMORY_INVALID_ARGUMENT,
    VGK_MEMORY_BAD_MAP,
    VGK_MEMORY_NOT_INITIALIZED,
    VGK_MEMORY_NO_MEMORY,
    VGK_MEMORY_RANGE_CAPACITY,
    VGK_MEMORY_OUT_OF_RANGE,
    VGK_MEMORY_DOUBLE_FREE,
    VGK_MEMORY_OVERFLOW
} VGK_MEMORY_RESULT;

typedef struct {
    uint64_t total_pages;
    uint64_t free_pages;
    size_t managed_range_count;
    size_t free_range_count;
} VGK_MEMORY_STATS;

/*
 * Initialize the early physical-page allocator from the normalized VGK map.
 * Only VGK_MEMORY_USABLE ranges are added to the free list.
 *
 * This first version is intended for the single-core early boot phase.
 * It returns physical addresses; callers may dereference them only while
 * the active page tables provide the required identity mapping.
 */
VGK_MEMORY_RESULT vgk_memory_init(const VGK_BOOT_INFO *boot_info);

VGK_MEMORY_RESULT vgk_memory_alloc_pages(
    uint64_t page_count,
    uint64_t alignment_pages,
    uint64_t *physical_address);

VGK_MEMORY_RESULT vgk_memory_free_pages(
    uint64_t physical_address,
    uint64_t page_count);

VGK_MEMORY_RESULT vgk_memory_get_stats(VGK_MEMORY_STATS *stats);

#endif
