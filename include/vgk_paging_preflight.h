#ifndef VGK_PAGING_PREFLIGHT_H
#define VGK_PAGING_PREFLIGHT_H

#include <stddef.h>
#include <stdint.h>

#include "vgk_paging_ranges.h"
#include "vgk_paging_requirements.h"

typedef enum {
    VGK_PAGING_PREFLIGHT_OK = 0,
    VGK_PAGING_PREFLIGHT_INVALID_ARGUMENT,
    VGK_PAGING_PREFLIGHT_RANGE_INVALID_ARGUMENT,
    VGK_PAGING_PREFLIGHT_RANGE_BAD_OBJECT,
    VGK_PAGING_PREFLIGHT_RANGE_TOO_MANY,
    VGK_PAGING_PREFLIGHT_REQUIREMENTS_INVALID_ARGUMENT,
    VGK_PAGING_PREFLIGHT_REQUIREMENTS_BAD_RANGE,
    VGK_PAGING_PREFLIGHT_REQUIREMENTS_TOO_MANY_RANGES
} VGK_PAGING_PREFLIGHT_RESULT;

/*
 * Build the standard identity-mapped object list, append optional additional
 * identity-mapped ranges (for example validated APIC MMIO windows), and
 * calculate the combined four-level paging requirements.
 *
 * Additional ranges are page-aligned virtual ranges. The caller must
 * validate boot_info, the ACPI RSDP (and provide its validated length),
 * framebuffer caching policy, and any device-range provenance before
 * calling this function. No page tables are allocated or modified.
 *
 * No partial result is published: requirements is zeroed on every failure.
 */
VGK_PAGING_PREFLIGHT_RESULT vgk_paging_preflight_calculate_with_additional_ranges(
    const VGK_BOOT_INFO *boot_info,
    uint64_t boot_info_address,
    uint64_t acpi_rsdp_length,
    int framebuffer_is_uncacheable,
    const VGK_PAGING_VIRTUAL_RANGE *additional_ranges,
    size_t additional_range_count,
    VGK_PAGING_REQUIREMENTS *requirements);

/*
 * Compatibility wrapper for the standard object list without extra ranges.
 */
VGK_PAGING_PREFLIGHT_RESULT vgk_paging_preflight_calculate(
    const VGK_BOOT_INFO *boot_info,
    uint64_t boot_info_address,
    uint64_t acpi_rsdp_length,
    int framebuffer_is_uncacheable,
    VGK_PAGING_REQUIREMENTS *requirements);

#endif
