#ifndef VGK_PAGING_H
#define VGK_PAGING_H

#include <stddef.h>
#include <stdint.h>

#define VGK_PAGING_PAGE_SIZE UINT64_C(4096)
#define VGK_PAGING_MAX_PHYSICAL_BITS 52U

#define VGK_PAGE_WRITE UINT32_C(0x1)
#define VGK_PAGE_EXEC  UINT32_C(0x2)

typedef enum {
    VGK_PAGING_OK = 0,
    VGK_PAGING_INVALID_ARGUMENT,
    VGK_PAGING_BAD_ADDRESS,
    VGK_PAGING_UNSUPPORTED,
    VGK_PAGING_NO_MEMORY,
    VGK_PAGING_ALREADY_MAPPED
} VGK_PAGING_RESULT;

/*
 * Builds inactive four-level x86-64 page tables in caller-provided,
 * page-aligned storage. The pool must be identity-addressable and remain
 * reserved. This module never changes CR3 or CPU control registers.
 */
typedef struct {
    uint64_t *pool;
    size_t pool_pages;
    size_t used_pages;
    uint64_t physical_mask;
    uint8_t physical_bits;
    uint8_t nx_supported;
} VGK_PAGE_TABLES;

VGK_PAGING_RESULT vgk_paging_init(
    VGK_PAGE_TABLES *tables,
    void *page_pool,
    size_t pool_pages,
    uint8_t physical_bits,
    uint8_t nx_supported);

VGK_PAGING_RESULT vgk_paging_map(
    VGK_PAGE_TABLES *tables,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint32_t permissions);

uint64_t vgk_paging_root_physical(const VGK_PAGE_TABLES *tables);

#endif
