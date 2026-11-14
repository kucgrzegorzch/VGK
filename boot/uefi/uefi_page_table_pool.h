#ifndef VGK_UEFI_PAGE_TABLE_POOL_H
#define VGK_UEFI_PAGE_TABLE_POOL_H

#include <stdint.h>

#define VGK_UEFI_PAGE_TABLE_POOL_PAGE_SIZE UINT64_C(4096)

typedef struct {
    uint64_t address;
    uint64_t pages;
} VGK_UEFI_PAGE_TABLE_POOL;

/*
 * Platform callbacks return zero on success and nonzero on failure.
 * allocate_pages must return one contiguous, page-aligned physical range.
 */
typedef int (*VGK_UEFI_PAGE_POOL_ALLOCATE)(
    void *context,
    uint64_t pages,
    uint64_t *address);

typedef int (*VGK_UEFI_PAGE_POOL_FREE)(
    void *context,
    uint64_t address,
    uint64_t pages);

typedef enum {
    VGK_UEFI_PAGE_POOL_OK = 0,
    VGK_UEFI_PAGE_POOL_INVALID_ARGUMENT,
    VGK_UEFI_PAGE_POOL_LIMIT_EXCEEDED,
    VGK_UEFI_PAGE_POOL_ALLOCATE_FAILED,
    VGK_UEFI_PAGE_POOL_BAD_ALLOCATION,
    VGK_UEFI_PAGE_POOL_FREE_FAILED,
    VGK_UEFI_PAGE_POOL_ORPHANED_ALLOCATION
} VGK_UEFI_PAGE_POOL_RESULT;

/*
 * Grow a contiguous pool without discarding the old allocation until the
 * replacement has been allocated, validated, and zeroed. Growth doubles the
 * current capacity where possible, but never exceeds max_pages.
 *
 * On failure, pool remains unchanged. If firmware refuses to free a newly
 * allocated replacement as well, its address and size are returned through
 * orphan_* so the caller can account for and clean up that allocation.
 * orphan_* are set to zero on entry.
 */
VGK_UEFI_PAGE_POOL_RESULT vgk_uefi_page_table_pool_grow(
    VGK_UEFI_PAGE_TABLE_POOL *pool,
    uint64_t required_pages,
    uint64_t max_pages,
    VGK_UEFI_PAGE_POOL_ALLOCATE allocate_pages,
    VGK_UEFI_PAGE_POOL_FREE free_pages,
    void *context,
    uint64_t *orphan_address,
    uint64_t *orphan_pages);

#endif
