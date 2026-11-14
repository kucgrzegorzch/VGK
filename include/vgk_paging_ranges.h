#ifndef VGK_PAGING_RANGES_H
#define VGK_PAGING_RANGES_H

#include <stddef.h>
#include <stdint.h>

#include "vgk_boot_info.h"
#include "vgk_paging_requirements.h"

typedef enum {
    VGK_PAGING_RANGES_OK = 0,
    VGK_PAGING_RANGES_INVALID_ARGUMENT,
    VGK_PAGING_RANGES_BAD_OBJECT,
    VGK_PAGING_RANGES_TOO_MANY
} VGK_PAGING_RANGES_RESULT;

/*
 * Builds the identity-mapped object list shared by the UEFI loader and
 * native kernel. ACPI length must come from a separately validated RSDP.
 * The framebuffer is included only when the caller confirms its mapping
 * policy is safe (currently: effective memory type is UC).
 *
 * On failure, *range_count is zero and no partial list is published.
 */
VGK_PAGING_RANGES_RESULT vgk_paging_ranges_build(
    const VGK_BOOT_INFO *boot_info,
    uint64_t boot_info_address,
    uint64_t acpi_rsdp_length,
    int framebuffer_is_uncacheable,
    VGK_PAGING_VIRTUAL_RANGE *ranges,
    size_t range_capacity,
    size_t *range_count);

#endif
