#ifndef VGK_PAGING_PREFLIGHT_H
#define VGK_PAGING_PREFLIGHT_H

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
 * Build the identity-mapped object list and calculate its four-level paging
 * requirements as one shared operation. The caller must validate boot_info,
 * the ACPI RSDP (and provide its validated length), and the framebuffer
 * caching policy before calling this function.
 *
 * No partial result is published: requirements is zeroed on every failure.
 */
VGK_PAGING_PREFLIGHT_RESULT vgk_paging_preflight_calculate(
    const VGK_BOOT_INFO *boot_info,
    uint64_t boot_info_address,
    uint64_t acpi_rsdp_length,
    int framebuffer_is_uncacheable,
    VGK_PAGING_REQUIREMENTS *requirements);

#endif
