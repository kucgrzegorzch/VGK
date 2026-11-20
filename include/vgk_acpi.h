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
    uint8_t physical_address_bits,
    const volatile uint8_t *bytes,
    uint64_t *length_out);


/* Physical reader supplied by the firmware/kernel mapping layer. */
typedef int (*VGK_ACPI_PHYSICAL_READ)(
    void *context,
    uint64_t physical_address,
    uint8_t *destination,
    size_t length);

typedef struct {
    uint64_t physical_address;
    uint32_t length;
    uint8_t revision;
    uint8_t reserved[3];
} VGK_ACPI_TABLE_REF;

typedef enum {
    VGK_ACPI_OK = 0,
    VGK_ACPI_INVALID_ARGUMENT = 1,
    VGK_ACPI_BAD_RSDP = 2,
    VGK_ACPI_BAD_ROOT_TABLE = 3,
    VGK_ACPI_BAD_TABLE = 4,
    VGK_ACPI_TABLE_NOT_FOUND = 5,
    VGK_ACPI_READ_FAILED = 6
} VGK_ACPI_STATUS;

/*
 * Validate the ACPI RSDP, its RSDT/XSDT, and locate exactly one valid MADT
 * ("APIC") table. Every physical range must be covered by readable ACPI or
 * firmware memory-map entries before the reader callback is invoked.
 *
 * The callback must safely copy physical memory; this routine never casts
 * arbitrary firmware physical addresses to C pointers.
 */
VGK_ACPI_STATUS vgk_acpi_find_madt(
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    uint64_t rsdp_physical_address,
    const volatile uint8_t *rsdp_bytes,
    uint8_t physical_address_bits,
    VGK_ACPI_PHYSICAL_READ read_physical,
    void *read_context,
    VGK_ACPI_TABLE_REF *madt_out);

#endif
