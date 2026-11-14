#include "uefi_page_table_pool.h"

#include "vgk_boot_info.h"
#include "vgk_paging_pool_policy.h"

VGK_UEFI_PAGE_TABLE_POOL_RESULT vgk_uefi_page_table_pool_grow(
    const VGK_UEFI_PAGE_TABLE_POOL_SERVICES *services,
    uint64_t *pool_base,
    uint64_t *pool_pages,
    uint64_t required_pages,
    uint64_t reserve_pages,
    uint8_t physical_address_bits)
{
    VGK_PAGING_POOL_POLICY_RESULT policy_result;
    uint64_t next_pages = 0U;
    uint64_t next_size;
    uint64_t next_base = 0U;
    uint64_t physical_limit;
    uint64_t current_size;

    if (services == 0 || services->allocate == 0 ||
        services->release == 0 || services->zero == 0 ||
        pool_base == 0 || pool_pages == 0 || *pool_base == 0 ||
        *pool_pages < VGK_BOOT_PAGE_TABLE_POOL_MIN_PAGES ||
        *pool_pages > VGK_BOOT_PAGE_TABLE_POOL_MAX_PAGES ||
        (*pool_base & (VGK_BOOT_PAGE_SIZE - 1U)) != 0U ||
        physical_address_bits < 32U || physical_address_bits > 52U ||
        *pool_pages > UINT64_MAX / VGK_BOOT_PAGE_SIZE)
        return VGK_UEFI_POOL_GROW_INVALID_ARGUMENT;

    current_size = *pool_pages * VGK_BOOT_PAGE_SIZE;
    physical_limit = (UINT64_C(1) << physical_address_bits) - 1U;
    if (*pool_base > physical_limit ||
        current_size - 1U > physical_limit - *pool_base)
        return VGK_UEFI_POOL_GROW_INVALID_ARGUMENT;

    policy_result = vgk_paging_pool_next_size(
        *pool_pages, required_pages, reserve_pages,
        VGK_BOOT_PAGE_TABLE_POOL_MAX_PAGES, &next_pages);
    if (policy_result == VGK_PAGING_POOL_POLICY_OK)
        return VGK_UEFI_POOL_GROW_NOT_NEEDED;
    if (policy_result == VGK_PAGING_POOL_POLICY_LIMIT_REACHED)
        return VGK_UEFI_POOL_GROW_LIMIT_REACHED;
    if (policy_result != VGK_PAGING_POOL_POLICY_GROW ||
        next_pages <= *pool_pages ||
        next_pages > UINT64_MAX / VGK_BOOT_PAGE_SIZE)
        return VGK_UEFI_POOL_GROW_INVALID_ARGUMENT;

    next_size = next_pages * VGK_BOOT_PAGE_SIZE;
    if (services->allocate(
            services->context, next_pages, &next_base) != 0)
        return VGK_UEFI_POOL_GROW_ALLOCATION_FAILED;
    if (next_base == 0U)
        return VGK_UEFI_POOL_GROW_INVALID_ALLOCATION;

    if ((next_base & (VGK_BOOT_PAGE_SIZE - 1U)) != 0U ||
        next_base > UINT64_MAX - next_size ||
        next_base > physical_limit ||
        next_size - 1U > physical_limit - next_base) {
        if (services->release(
                services->context, next_base, next_pages) != 0)
            return VGK_UEFI_POOL_GROW_CLEANUP_FAILED;
        return VGK_UEFI_POOL_GROW_INVALID_ALLOCATION;
    }

    if (services->zero(
            services->context, next_base, next_pages) != 0) {
        if (services->release(
                services->context, next_base, next_pages) != 0)
            return VGK_UEFI_POOL_GROW_CLEANUP_FAILED;
        return VGK_UEFI_POOL_GROW_ZERO_FAILED;
    }

    if (services->release(
            services->context, *pool_base, *pool_pages) != 0) {
        if (services->release(
                services->context, next_base, next_pages) != 0)
            return VGK_UEFI_POOL_GROW_CLEANUP_FAILED;
        return VGK_UEFI_POOL_GROW_OLD_RELEASE_FAILED;
    }

    *pool_base = next_base;
    *pool_pages = next_pages;
    return VGK_UEFI_POOL_GROW_OK;
}
