#ifndef VGK_APIC_MAPPING_H
#define VGK_APIC_MAPPING_H

#include <stddef.h>
#include <stdint.h>

#include "vgk_acpi.h"
#include "vgk_cpu_features.h"
#include "vgk_paging.h"

typedef enum {
    VGK_APIC_MAPPING_OK = 0,
    VGK_APIC_MAPPING_INVALID_ARGUMENT,
    VGK_APIC_MAPPING_UNSUPPORTED,
    VGK_APIC_MAPPING_BAD_FIRMWARE_DATA,
    VGK_APIC_MAPPING_PAGING_FAILED
} VGK_APIC_MAPPING_RESULT;

/*
 * Validate the current CPU/APIC/PAT state, build the MADT-derived device
 * page plan, validate it against the firmware memory map, and apply it to
 * caller-owned inactive page tables as identity-mapped RW+NX+UC pages.
 *
 * This function does not read or write MSRs, touch MMIO, activate CR3,
 * program either APIC, or enable interrupts. If paging fails after one or
 * more pages have been mapped, discard the entire inactive page-table set.
 * Outputs are published only after every validation and mapping succeeds.
 *
 * APIC may currently be disabled: this routine only prepares a future plan.
 */
VGK_APIC_MAPPING_RESULT vgk_apic_prepare_mmio_mappings(
    const VGK_CPU_FEATURES *cpu,
    const VGK_ACPI_MADT_INFO *madt,
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    VGK_PAGE_TABLES *inactive_tables,
    VGK_ACPI_MADT_MMIO_RANGE *ranges_out,
    size_t range_capacity,
    size_t *range_count_out);

#endif
