#ifndef VGK_ACPI_H
#define VGK_ACPI_H

#include <stddef.h>
#include <stdint.h>

#include "vgk_boot_info.h"

/*
 * Validate an ACPI RSDP before the caller dereferences its physical address.
 * Only explicitly readable ACPI/firmware memory types are accepted.
 * Returns 1 on success and writes the validated byte length to length_out.
 */
int vgk_acpi_rsdp_validate(
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    uint64_t physical_address,
    const volatile uint8_t *bytes,
    uint64_t *length_out);

#endif
