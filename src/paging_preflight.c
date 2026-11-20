#include "vgk_paging_preflight.h"

VGK_PAGING_PREFLIGHT_RESULT vgk_paging_preflight_calculate_with_additional_ranges(
    const VGK_BOOT_INFO *boot_info,
    uint64_t boot_info_address,
    uint64_t acpi_rsdp_length,
    int framebuffer_is_uncacheable,
    const VGK_PAGING_VIRTUAL_RANGE *additional_ranges,
    size_t additional_range_count,
    VGK_PAGING_REQUIREMENTS *requirements)
{
    VGK_PAGING_VIRTUAL_RANGE ranges[VGK_PAGING_REQUIREMENTS_MAX_RANGES];
    VGK_PAGING_REQUIREMENTS calculated;
    size_t range_count = 0U;
    VGK_PAGING_RANGES_RESULT ranges_result;
    VGK_PAGING_REQUIREMENTS_RESULT requirements_result;

    if (requirements == 0)
        return VGK_PAGING_PREFLIGHT_INVALID_ARGUMENT;

    requirements->pml4_pages = 0U;
    requirements->pdpt_pages = 0U;
    requirements->pd_pages = 0U;
    requirements->pt_pages = 0U;
    requirements->total_pages = 0U;

    if (boot_info == 0 || boot_info_address == 0U ||
        (framebuffer_is_uncacheable != 0 &&
         framebuffer_is_uncacheable != 1) ||
        (additional_range_count != 0U && additional_ranges == 0))
        return VGK_PAGING_PREFLIGHT_INVALID_ARGUMENT;

    ranges_result = vgk_paging_ranges_build(
        boot_info, boot_info_address, acpi_rsdp_length,
        framebuffer_is_uncacheable, ranges,
        VGK_PAGING_REQUIREMENTS_MAX_RANGES, &range_count);
    switch (ranges_result) {
    case VGK_PAGING_RANGES_OK:
        break;
    case VGK_PAGING_RANGES_INVALID_ARGUMENT:
        return VGK_PAGING_PREFLIGHT_RANGE_INVALID_ARGUMENT;
    case VGK_PAGING_RANGES_BAD_OBJECT:
        return VGK_PAGING_PREFLIGHT_RANGE_BAD_OBJECT;
    case VGK_PAGING_RANGES_TOO_MANY:
        return VGK_PAGING_PREFLIGHT_RANGE_TOO_MANY;
    default:
        return VGK_PAGING_PREFLIGHT_RANGE_BAD_OBJECT;
    }

    if (additional_range_count >
        VGK_PAGING_REQUIREMENTS_MAX_RANGES - range_count)
        return VGK_PAGING_PREFLIGHT_RANGE_TOO_MANY;

    for (size_t i = 0U; i < additional_range_count; ++i)
        ranges[range_count++] = additional_ranges[i];

    requirements_result = vgk_paging_requirements_calculate(
        ranges, range_count, &calculated);
    switch (requirements_result) {
    case VGK_PAGING_REQUIREMENTS_OK:
        *requirements = calculated;
        return VGK_PAGING_PREFLIGHT_OK;
    case VGK_PAGING_REQUIREMENTS_INVALID_ARGUMENT:
        return VGK_PAGING_PREFLIGHT_REQUIREMENTS_INVALID_ARGUMENT;
    case VGK_PAGING_REQUIREMENTS_BAD_RANGE:
        return VGK_PAGING_PREFLIGHT_REQUIREMENTS_BAD_RANGE;
    case VGK_PAGING_REQUIREMENTS_TOO_MANY_RANGES:
        return VGK_PAGING_PREFLIGHT_REQUIREMENTS_TOO_MANY_RANGES;
    default:
        return VGK_PAGING_PREFLIGHT_REQUIREMENTS_BAD_RANGE;
    }
}

VGK_PAGING_PREFLIGHT_RESULT vgk_paging_preflight_calculate(
    const VGK_BOOT_INFO *boot_info,
    uint64_t boot_info_address,
    uint64_t acpi_rsdp_length,
    int framebuffer_is_uncacheable,
    VGK_PAGING_REQUIREMENTS *requirements)
{
    return vgk_paging_preflight_calculate_with_additional_ranges(
        boot_info, boot_info_address, acpi_rsdp_length,
        framebuffer_is_uncacheable, 0, 0U, requirements);
}
