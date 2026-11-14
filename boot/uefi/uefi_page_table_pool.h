#ifndef VGK_UEFI_PAGE_TABLE_POOL_H
#define VGK_UEFI_PAGE_TABLE_POOL_H

#include <stdint.h>

typedef int (*VGK_UEFI_POOL_ALLOCATE_FN)(
    void *context, uint64_t pages, uint64_t *physical_address);
typedef int (*VGK_UEFI_POOL_RELEASE_FN)(
    void *context, uint64_t physical_address, uint64_t pages);
typedef int (*VGK_UEFI_POOL_ZERO_FN)(
    void *context, uint64_t physical_address, uint64_t pages);

typedef struct {
    void *context;
    VGK_UEFI_POOL_ALLOCATE_FN allocate;
    VGK_UEFI_POOL_RELEASE_FN release;
    VGK_UEFI_POOL_ZERO_FN zero;
} VGK_UEFI_PAGE_TABLE_POOL_SERVICES;

typedef enum {
    VGK_UEFI_POOL_GROW_OK = 0,
    VGK_UEFI_POOL_GROW_NOT_NEEDED,
    VGK_UEFI_POOL_GROW_INVALID_ARGUMENT,
    VGK_UEFI_POOL_GROW_LIMIT_REACHED,
    VGK_UEFI_POOL_GROW_ALLOCATION_FAILED,
    VGK_UEFI_POOL_GROW_INVALID_ALLOCATION,
    VGK_UEFI_POOL_GROW_ZERO_FAILED,
    VGK_UEFI_POOL_GROW_OLD_RELEASE_FAILED,
    VGK_UEFI_POOL_GROW_CLEANUP_FAILED
} VGK_UEFI_PAGE_TABLE_POOL_RESULT;

/*
 * Grow a page-table pool transactionally:
 * allocate and validate the replacement, zero it, release the old pool,
 * then publish the new base and page count. Callers must refresh the UEFI
 * memory map after success.
 *
 * Callback functions return zero on success and nonzero on failure.
 */
VGK_UEFI_PAGE_TABLE_POOL_RESULT vgk_uefi_page_table_pool_grow(
    const VGK_UEFI_PAGE_TABLE_POOL_SERVICES *services,
    uint64_t *pool_base,
    uint64_t *pool_pages,
    uint64_t required_pages,
    uint64_t reserve_pages,
    uint8_t physical_address_bits);

#endif
