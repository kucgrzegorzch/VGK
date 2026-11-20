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
 * Validate CPU/APIC/PAT state and construct a MADT-derived MMIO plan.
 * Every planned device page must be covered by a single MMIO or RESERVED
 * firmware descriptor. The returned ranges are physical, page-aligned,
 * pointer-free records; no page tables or hardware registers are touched.
 *
 * This is the planning half of the two-stage process. Callers can convert
 * these identity-mapped ranges to VGK_PAGING_VIRTUAL_RANGE entries and
 * include them in the page-table capacity preflight before allocating or
 * modifying page tables.
 */
VGK_APIC_MAPPING_RESULT vgk_apic_build_mmio_plan(
    const VGK_CPU_FEATURES *cpu,
    const VGK_ACPI_MADT_INFO *madt,
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    VGK_ACPI_MADT_MMIO_RANGE *ranges_out,
    size_t range_capacity,
    size_t *range_count_out);

/*
 * Apply a previously validated CPU/MADT/memory-map plan to caller-owned
 * inactive page tables as identity-mapped RW+NX+UC pages.
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
