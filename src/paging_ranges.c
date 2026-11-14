#include "vgk_paging_ranges.h"

static VGK_PAGING_RANGES_RESULT add_object(
    VGK_PAGING_VIRTUAL_RANGE *ranges,
    size_t capacity,
    size_t *count,
    uint64_t address,
    uint64_t length)
{
    uint64_t page_base;
    uint64_t offset;
    uint64_t mapped_length;
    uint64_t rounded_length;

    if (address == 0U || length == 0U)
        return VGK_PAGING_RANGES_BAD_OBJECT;
    if (*count >= capacity ||
        *count >= VGK_PAGING_REQUIREMENTS_MAX_RANGES)
        return VGK_PAGING_RANGES_TOO_MANY;

    page_base = address & ~(VGK_BOOT_PAGE_SIZE - 1U);
    offset = address - page_base;
    if (length > UINT64_MAX - offset)
        return VGK_PAGING_RANGES_BAD_OBJECT;
    mapped_length = length + offset;
    if (mapped_length > UINT64_MAX - (VGK_BOOT_PAGE_SIZE - 1U))
        return VGK_PAGING_RANGES_BAD_OBJECT;
    rounded_length = (mapped_length + VGK_BOOT_PAGE_SIZE - 1U) &
                     ~(VGK_BOOT_PAGE_SIZE - 1U);
    if (rounded_length == 0U ||
        page_base > UINT64_MAX - (rounded_length - 1U))
        return VGK_PAGING_RANGES_BAD_OBJECT;

    ranges[*count].virtual_address = page_base;
    ranges[*count].length = mapped_length;
    ++(*count);
    return VGK_PAGING_RANGES_OK;
}

VGK_PAGING_RANGES_RESULT vgk_paging_ranges_build(
    const VGK_BOOT_INFO *boot_info,
    uint64_t boot_info_address,
    uint64_t acpi_rsdp_length,
    int framebuffer_is_uncacheable,
    VGK_PAGING_VIRTUAL_RANGE *ranges,
    size_t range_capacity,
    size_t *range_count)
{
    VGK_PAGING_RANGES_RESULT result;
    size_t count = 0U;
    uint64_t pool_size;

    if (range_count == 0)
        return VGK_PAGING_RANGES_INVALID_ARGUMENT;
    *range_count = 0U;

    if (boot_info == 0 || ranges == 0 || range_capacity == 0U ||
        boot_info_address == 0U ||
        (framebuffer_is_uncacheable != 0 &&
         framebuffer_is_uncacheable != 1))
        return VGK_PAGING_RANGES_INVALID_ARGUMENT;

    if (boot_info->kernel_segment_count == 0U ||
        boot_info->kernel_segment_count > VGK_BOOT_MAX_KERNEL_SEGMENTS ||
        boot_info->memory_map_address == 0U ||
        boot_info->memory_map_size == 0U ||
        boot_info->page_table_pool_base == 0U ||
        boot_info->page_table_pool_pages == 0U ||
        boot_info->page_table_pool_pages >
            UINT64_MAX / VGK_BOOT_PAGE_SIZE)
        return VGK_PAGING_RANGES_BAD_OBJECT;

    if (((boot_info->flags & VGK_BOOT_FLAG_ACPI) != 0U) !=
        (acpi_rsdp_length != 0U))
        return VGK_PAGING_RANGES_BAD_OBJECT;

    pool_size = boot_info->page_table_pool_pages * VGK_BOOT_PAGE_SIZE;

#define ADD_RANGE(address_, length_) do { \
    result = add_object(ranges, range_capacity, &count, \
                        (address_), (length_)); \
    if (result != VGK_PAGING_RANGES_OK) \
        goto fail; \
} while (0)

    for (uint32_t i = 0U; i < boot_info->kernel_segment_count; ++i) {
        ADD_RANGE(boot_info->kernel_segments[i].physical_base,
                  boot_info->kernel_segments[i].memory_size);
    }

    ADD_RANGE(boot_info_address, sizeof(*boot_info));
    ADD_RANGE(boot_info->memory_map_address, boot_info->memory_map_size);
    ADD_RANGE(boot_info->page_table_pool_base, pool_size);

    if ((boot_info->flags & VGK_BOOT_FLAG_ACPI) != 0U)
        ADD_RANGE(boot_info->acpi_rsdp_address, acpi_rsdp_length);

    if ((boot_info->flags & VGK_BOOT_FLAG_FRAMEBUFFER) != 0U &&
        framebuffer_is_uncacheable != 0)
        ADD_RANGE(boot_info->framebuffer_base, boot_info->framebuffer_size);

#undef ADD_RANGE
    *range_count = count;
    return VGK_PAGING_RANGES_OK;

fail:
#undef ADD_RANGE
    *range_count = 0U;
    return result;
}
