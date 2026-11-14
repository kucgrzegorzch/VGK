#include "uefi_page_table_pool.h"
#include "vgk_paging_pool_policy.h"

static int range_is_valid(uint64_t address, uint64_t pages)
{
    uint64_t bytes;

    if (address == 0 || pages == 0 ||
        (address & (VGK_UEFI_PAGE_TABLE_POOL_PAGE_SIZE - 1U)) != 0U ||
        pages > UINT64_MAX / VGK_UEFI_PAGE_TABLE_POOL_PAGE_SIZE)
        return 0;

    bytes = pages * VGK_UEFI_PAGE_TABLE_POOL_PAGE_SIZE;
    return address <= UINT64_MAX - bytes;
}

static VGK_UEFI_PAGE_POOL_RESULT release_replacement(
    VGK_UEFI_PAGE_POOL_FREE free_pages,
    void *context,
    uint64_t address,
    uint64_t pages,
    uint64_t *orphan_address,
    uint64_t *orphan_pages,
    VGK_UEFI_PAGE_POOL_RESULT ordinary_result)
{
    if (free_pages(context, address, pages) == 0)
        return ordinary_result;

    *orphan_address = address;
    *orphan_pages = pages;
    return VGK_UEFI_PAGE_POOL_ORPHANED_ALLOCATION;
}

VGK_UEFI_PAGE_POOL_RESULT vgk_uefi_page_table_pool_grow(
    VGK_UEFI_PAGE_TABLE_POOL *pool,
    uint64_t required_pages,
    uint64_t reserve_pages,
    uint64_t max_pages,
    VGK_UEFI_PAGE_POOL_ALLOCATE allocate_pages,
    VGK_UEFI_PAGE_POOL_FREE free_pages,
    void *context,
    uint64_t *orphan_address,
    uint64_t *orphan_pages)
{
    uint64_t target_pages;
    VGK_PAGING_POOL_POLICY_RESULT policy_result;
    uint64_t replacement_address = 0;
    uint64_t replacement_bytes;
    uint64_t old_bytes;
    volatile uint64_t *replacement_words;
    uint64_t word_count;

    if (orphan_address == 0 || orphan_pages == 0)
        return VGK_UEFI_PAGE_POOL_INVALID_ARGUMENT;
    *orphan_address = 0;
    *orphan_pages = 0;

    if (pool == 0 || allocate_pages == 0 || free_pages == 0 ||
        pool->pages == 0 || !range_is_valid(pool->address, pool->pages) ||
        max_pages < pool->pages ||
        max_pages > UINT64_MAX / VGK_UEFI_PAGE_TABLE_POOL_PAGE_SIZE)
        return VGK_UEFI_PAGE_POOL_INVALID_ARGUMENT;

    policy_result = vgk_paging_pool_next_size(
        pool->pages, required_pages, reserve_pages, max_pages,
        &target_pages);
    if (policy_result == VGK_PAGING_POOL_POLICY_OK)
        return VGK_UEFI_PAGE_POOL_OK;
    if (policy_result == VGK_PAGING_POOL_POLICY_LIMIT_REACHED)
        return VGK_UEFI_PAGE_POOL_LIMIT_EXCEEDED;
    if (policy_result != VGK_PAGING_POOL_POLICY_GROW)
        return VGK_UEFI_PAGE_POOL_INVALID_ARGUMENT;

    replacement_bytes =
        target_pages * VGK_UEFI_PAGE_TABLE_POOL_PAGE_SIZE;
    old_bytes = pool->pages * VGK_UEFI_PAGE_TABLE_POOL_PAGE_SIZE;

    if (allocate_pages(context, target_pages, &replacement_address) != 0)
        return VGK_UEFI_PAGE_POOL_ALLOCATE_FAILED;

    if (!range_is_valid(replacement_address, target_pages) ||
        (replacement_address < pool->address + old_bytes &&
         pool->address < replacement_address + replacement_bytes)) {
        return release_replacement(
            free_pages, context, replacement_address, target_pages,
            orphan_address, orphan_pages,
            VGK_UEFI_PAGE_POOL_BAD_ALLOCATION);
    }

    replacement_words = (volatile uint64_t *)(uintptr_t)replacement_address;
    word_count = target_pages *
        (VGK_UEFI_PAGE_TABLE_POOL_PAGE_SIZE / sizeof(uint64_t));
    for (uint64_t i = 0; i < word_count; ++i)
        replacement_words[i] = 0;

    if (free_pages(context, pool->address, pool->pages) != 0) {
        return release_replacement(
            free_pages, context, replacement_address, target_pages,
            orphan_address, orphan_pages,
            VGK_UEFI_PAGE_POOL_FREE_FAILED);
    }

    pool->address = replacement_address;
    pool->pages = target_pages;
    return VGK_UEFI_PAGE_POOL_OK;
}
