#ifndef VGK_UEFI_MEMORY_MAP_H
#define VGK_UEFI_MEMORY_MAP_H

#include <stddef.h>
#include <stdint.h>

#include "uefi_services.h"
#include "vgk_boot_info.h"

/*
 * Convert the firmware descriptor array into the stable VGK memory map.
 * The caller owns both buffers. No allocation or firmware call occurs here.
 * Descriptor stride may be larger than the currently known 40-byte prefix.
 */
int vgk_uefi_normalize_memory_map(
    const void *firmware_map,
    size_t firmware_map_size,
    size_t descriptor_size,
    uint32_t descriptor_version,
    VGK_MEMORY_MAP_ENTRY *output,
    size_t output_capacity,
    size_t *output_count);

#endif
