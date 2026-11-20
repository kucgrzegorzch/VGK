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


/* Physical reader supplied by the firmware/kernel mapping layer.
 * Return nonzero only after copying the entire requested range. */
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


#define VGK_ACPI_MADT_MAX_PROCESSORS 64U
#define VGK_ACPI_MADT_MAX_IOAPICS 8U
#define VGK_ACPI_MADT_MAX_OVERRIDES 16U
#define VGK_ACPI_MADT_MAX_NMI_SOURCES 16U
#define VGK_ACPI_MADT_MAX_LOCAL_NMIS 32U

typedef struct {
    uint32_t processor_uid;
    uint32_t apic_id;
    uint32_t flags;
    uint8_t x2apic;
    uint8_t reserved[3];
} VGK_ACPI_MADT_PROCESSOR;

typedef struct {
    uint8_t id;
    uint8_t reserved[3];
    uint32_t address;
    uint32_t gsi_base;
} VGK_ACPI_MADT_IOAPIC;

typedef struct {
    uint8_t bus;
    uint8_t source;
    uint16_t flags;
    uint32_t gsi;
} VGK_ACPI_MADT_OVERRIDE;

typedef struct {
    uint16_t flags;
    uint32_t gsi;
} VGK_ACPI_MADT_NMI_SOURCE;

typedef struct {
    uint32_t processor_uid;
    uint16_t flags;
    uint8_t lint;
    uint8_t x2apic;
} VGK_ACPI_MADT_LOCAL_NMI;

typedef struct {
    uint64_t local_apic_address;
    uint32_t flags;
    uint32_t processor_count;
    VGK_ACPI_MADT_PROCESSOR processors[VGK_ACPI_MADT_MAX_PROCESSORS];
    uint32_t ioapic_count;
    VGK_ACPI_MADT_IOAPIC ioapics[VGK_ACPI_MADT_MAX_IOAPICS];
    uint32_t override_count;
    VGK_ACPI_MADT_OVERRIDE overrides[VGK_ACPI_MADT_MAX_OVERRIDES];
    uint32_t nmi_source_count;
    VGK_ACPI_MADT_NMI_SOURCE nmi_sources[VGK_ACPI_MADT_MAX_NMI_SOURCES];
    uint32_t local_nmi_count;
    VGK_ACPI_MADT_LOCAL_NMI local_nmis[VGK_ACPI_MADT_MAX_LOCAL_NMIS];
} VGK_ACPI_MADT_INFO;

/*
 * Parse validated MADT records into bounded, pointer-free topology data.
 * Unsupported record types are skipped only after their length/bounds pass.
 * No MMIO access or interrupt-controller programming is performed.
 */
VGK_ACPI_STATUS vgk_acpi_madt_parse(
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    uint8_t physical_address_bits,
    VGK_ACPI_PHYSICAL_READ read_physical,
    void *read_context,
    const VGK_ACPI_TABLE_REF *madt,
    VGK_ACPI_MADT_INFO *info_out);

#endif
