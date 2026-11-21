#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vgk_apic_mapping.h"

static uint64_t page_pool[8U * 512U] __attribute__((aligned(4096)));

static uint64_t *walk_page(VGK_PAGE_TABLES *tables, uint64_t address)
{
    static const unsigned shifts[3] = {39U, 30U, 21U};
    uint64_t *table = tables->pool;

    for (size_t level = 0; level < 3U; ++level) {
        uint64_t entry = table[(address >> shifts[level]) & 0x1FFU];
        if ((entry & 1U) == 0U || (entry & 0x80U) != 0U)
            return 0;
        table = (uint64_t *)(uintptr_t)(entry & tables->physical_mask);
    }
    return &table[(address >> 12) & 0x1FFU];
}

static void assert_uc_leaf(
    VGK_PAGE_TABLES *tables,
    uint64_t virtual_address,
    uint64_t physical_address)
{
    uint64_t *leaf = walk_page(tables, virtual_address);
    assert(leaf != 0);
    assert((*leaf & tables->physical_mask) == physical_address);
    assert((*leaf & UINT64_C(0x001)) != 0U);
    assert((*leaf & UINT64_C(0x002)) != 0U);
    assert((*leaf & UINT64_C(0x004)) == 0U);
    assert((*leaf & UINT64_C(0x018)) == UINT64_C(0x018));
    assert((*leaf & (UINT64_C(1) << 63)) != 0U);
}

static VGK_CPU_FEATURES valid_cpu(uint8_t x2apic)
{
    VGK_CPU_FEATURES cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.physical_address_bits = 48U;
    cpu.nx_supported = 1U;
    cpu.mmio_uc_supported = 1U;
    cpu.local_apic_supported = 1U;
    cpu.x2apic_supported = x2apic;
    cpu.apic_base_valid = 1U;
    cpu.local_apic_enabled = 1U;
    cpu.x2apic_enabled = x2apic;
    cpu.local_apic_base = UINT64_C(0xFEE01000);
    return cpu;
}

static VGK_ACPI_MADT_INFO valid_madt(void)
{
    VGK_ACPI_MADT_INFO madt;
    memset(&madt, 0, sizeof(madt));
    madt.local_apic_address = UINT64_C(0xFEE01000);
    madt.ioapic_count = 1U;
    madt.ioapics[0].id = 2U;
    madt.ioapics[0].address = UINT32_C(0xFEC00000);
    madt.ioapics[0].gsi_base = 0U;
    return madt;
}

int main(void)
{
    VGK_CPU_FEATURES cpu = valid_cpu(0U);
    VGK_ACPI_MADT_INFO madt = valid_madt();
    VGK_MEMORY_MAP_ENTRY map[2] = {
        {UINT64_C(0xFEE01000), VGK_BOOT_PAGE_SIZE, VGK_MEMORY_MMIO, 0U},
        {UINT64_C(0xFEC00000), VGK_BOOT_PAGE_SIZE, VGK_MEMORY_RESERVED, 0U}
    };
    VGK_ACPI_MADT_MMIO_RANGE ranges[VGK_ACPI_MADT_MAX_MMIO_RANGES];
    VGK_PAGE_TABLES tables;
    size_t count = 99U;

    assert(vgk_paging_init(&tables, page_pool, 8U, 48U, 1U) ==
           VGK_PAGING_OK);
    count = 99U;
    assert(vgk_apic_build_mmio_plan(
               &cpu, &madt, map, 2U, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_OK);
    assert(count == 2U);
    assert(ranges[0].kind == VGK_ACPI_MADT_MMIO_LOCAL_APIC);
    assert(ranges[1].kind == VGK_ACPI_MADT_MMIO_IOAPIC);
    /* Planning is pure: it must not allocate or alter the page tables. */
    assert(tables.used_pages == 1U);
    assert(walk_page(&tables, UINT64_C(0xFEE01000)) == 0);

    count = 99U;
    assert(vgk_apic_prepare_mmio_mappings(
               &cpu, &madt, map, 2U, &tables, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_OK);
    assert(count == 2U);
    assert(ranges[0].kind == VGK_ACPI_MADT_MMIO_LOCAL_APIC);
    assert(ranges[1].kind == VGK_ACPI_MADT_MMIO_IOAPIC);
    assert_uc_leaf(&tables, UINT64_C(0xFEE01000), UINT64_C(0xFEE01000));
    assert_uc_leaf(&tables, UINT64_C(0xFEC00000), UINT64_C(0xFEC00000));

    /* x2APIC uses MSRs for LAPIC: only IOAPIC remains in the MMIO plan. */
    cpu = valid_cpu(1U);
    assert(vgk_paging_init(&tables, page_pool, 8U, 48U, 1U) ==
           VGK_PAGING_OK);
    count = 99U;
    assert(vgk_apic_build_mmio_plan(
               &cpu, &madt, map, 2U, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_OK);
    assert(count == 1U);
    assert(ranges[0].kind == VGK_ACPI_MADT_MMIO_IOAPIC);
    count = 99U;
    assert(vgk_apic_prepare_mmio_mappings(
               &cpu, &madt, map, 2U, &tables, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_OK);
    assert(count == 1U);
    assert(ranges[0].kind == VGK_ACPI_MADT_MMIO_IOAPIC);
    assert_uc_leaf(&tables, UINT64_C(0xFEC00000), UINT64_C(0xFEC00000));
    assert(walk_page(&tables, UINT64_C(0xFEE01000)) == 0);

    /* The firmware MADT LAPIC base must match the CPU APIC-base register. */
    {
        VGK_ACPI_MADT_INFO mismatched = madt;
        mismatched.local_apic_address = UINT64_C(0xFEE00000);
        assert(vgk_apic_build_mmio_plan(
                   &cpu, &mismatched, map, 2U, ranges,
                   VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
               VGK_APIC_MAPPING_BAD_FIRMWARE_DATA);
        assert(count == 0U);
        assert(ranges[0].physical_base == 0U);
    }

    /* PAT layouts that do not make PCD+PWT uncached must fail closed. */
    cpu = valid_cpu(0U);
    cpu.mmio_uc_supported = 0U;
    assert(vgk_paging_init(&tables, page_pool, 8U, 48U, 1U) ==
           VGK_PAGING_OK);
    count = 99U;
    assert(vgk_apic_prepare_mmio_mappings(
               &cpu, &madt, map, 2U, &tables, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_UNSUPPORTED);
    assert(count == 0U);

    /* NX must be available in both the detected CPU and inactive tables. */
    cpu = valid_cpu(0U);
    assert(vgk_paging_init(&tables, page_pool, 8U, 48U, 0U) ==
           VGK_PAGING_OK);
    assert(vgk_apic_prepare_mmio_mappings(
               &cpu, &madt, map, 2U, &tables, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_INVALID_ARGUMENT);
    assert(count == 0U);

    /* Page-table metadata must agree with the CPU physical-address width. */
    assert(vgk_paging_init(&tables, page_pool, 8U, 48U, 1U) ==
           VGK_PAGING_OK);
    tables.physical_mask &= ~UINT64_C(0x1000);
    assert(vgk_apic_prepare_mmio_mappings(
               &cpu, &madt, map, 2U, &tables, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_INVALID_ARGUMENT);
    assert(count == 0U);

    /* The complete inactive table pool must fit the CPU physical range. */
    assert(vgk_paging_init(&tables, page_pool, 8U, 48U, 1U) ==
           VGK_PAGING_OK);
    tables.pool_pages = SIZE_MAX / (size_t)VGK_BOOT_PAGE_SIZE + 1U;
    assert(vgk_apic_prepare_mmio_mappings(
               &cpu, &madt, map, 2U, &tables, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_INVALID_ARGUMENT);
    assert(count == 0U);

    /* Ordinary RAM must never be accepted as an APIC device window. */
    assert(vgk_paging_init(&tables, page_pool, 8U, 48U, 1U) ==
           VGK_PAGING_OK);
    map[0].type = VGK_MEMORY_USABLE;
    assert(vgk_apic_prepare_mmio_mappings(
               &cpu, &madt, map, 2U, &tables, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_BAD_FIRMWARE_DATA);
    assert(count == 0U);
    map[0].type = VGK_MEMORY_MMIO;

    /*
     * Two firmware descriptors must not claim the same APIC page, even
     * when both descriptors individually cover the complete window.
     */
    {
        VGK_MEMORY_MAP_ENTRY overlapping_map[3] = {
            {UINT64_C(0xFEE01000), VGK_BOOT_PAGE_SIZE,
             VGK_MEMORY_MMIO, 0U},
            {UINT64_C(0xFEE01000), VGK_BOOT_PAGE_SIZE,
             VGK_MEMORY_RESERVED, 0U},
            {UINT64_C(0xFEC00000), VGK_BOOT_PAGE_SIZE,
             VGK_MEMORY_RESERVED, 0U}
        };
        count = 99U;
        assert(vgk_apic_build_mmio_plan(
                   &cpu, &madt, overlapping_map, 3U, ranges,
                   VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
               VGK_APIC_MAPPING_BAD_FIRMWARE_DATA);
        assert(count == 0U && ranges[0].physical_base == 0U);
    }

    /*
     * Adjacent partial descriptors are not accepted as a substitute for
     * one complete firmware descriptor covering an APIC device page.
     */
    {
        VGK_MEMORY_MAP_ENTRY fragmented_map[3] = {
            {UINT64_C(0xFEE01000), VGK_BOOT_PAGE_SIZE / 2U,
             VGK_MEMORY_MMIO, 0U},
            {UINT64_C(0xFEE01800), VGK_BOOT_PAGE_SIZE / 2U,
             VGK_MEMORY_MMIO, 0U},
            {UINT64_C(0xFEC00000), VGK_BOOT_PAGE_SIZE,
             VGK_MEMORY_RESERVED, 0U}
        };
        count = 99U;
        assert(vgk_apic_build_mmio_plan(
                   &cpu, &madt, fragmented_map, 3U, ranges,
                   VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
               VGK_APIC_MAPPING_BAD_FIRMWARE_DATA);
        assert(count == 0U && ranges[0].physical_base == 0U);
    }

    /* An undersized output buffer cannot publish a partial plan. */
    assert(vgk_paging_init(&tables, page_pool, 8U, 48U, 1U) ==
           VGK_PAGING_OK);
    count = 99U;
    assert(vgk_apic_prepare_mmio_mappings(
               &cpu, &madt, map, 2U, &tables, ranges, 1U, &count) ==
           VGK_APIC_MAPPING_INVALID_ARGUMENT);
    assert(count == 0U && ranges[0].physical_base == 0U);

    /* A cacheable alias causes a fail-closed result; discard these tables. */
    assert(vgk_paging_init(&tables, page_pool, 8U, 48U, 1U) ==
           VGK_PAGING_OK);
    assert(vgk_paging_map(
               &tables, UINT64_C(0xFEC00000), UINT64_C(0xFEC00000),
               VGK_PAGE_WRITE) == VGK_PAGING_OK);
    count = 99U;
    assert(vgk_apic_prepare_mmio_mappings(
               &cpu, &madt, map, 2U, &tables, ranges,
               VGK_ACPI_MADT_MAX_MMIO_RANGES, &count) ==
           VGK_APIC_MAPPING_PAGING_FAILED);
    assert(count == 0U);

    /*
     * The LAPIC mapping succeeds before the conflicting IOAPIC alias is
     * encountered. Failure must not publish a partial output plan, and the
     * caller must discard the now-partially-modified inactive table set.
     */
    assert(walk_page(&tables, UINT64_C(0xFEE01000)) != 0);
    assert_uc_leaf(&tables, UINT64_C(0xFEE01000), UINT64_C(0xFEE01000));
    assert(walk_page(&tables, UINT64_C(0xFEC00000)) != 0);
    assert((*walk_page(&tables, UINT64_C(0xFEC00000)) &
            UINT64_C(0x018)) == 0U);
    assert(ranges[0].physical_base == 0U);

    puts("OK: CPU/PAT + MADT + firmware map + inactive APIC MMIO paging.");
    return 0;
}
