#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vgk_acpi.h"
#include "vgk_paging.h"

#define PHYS_BASE UINT64_C(0x100000)
#define PHYS_SIZE UINT64_C(0x10000)
#define RSDP_OFFSET 0x1000U
#define ROOT_OFFSET 0x2000U
#define MADT_OFFSET 0x3000U
#define OTHER_OFFSET 0x4000U

static uint8_t physical_memory[PHYS_SIZE];
static VGK_MEMORY_MAP_ENTRY memory_map[3];

static uint64_t apic_page_pool[8U * 512U]
    __attribute__((aligned(4096)));

static uint64_t *walk_page(
    VGK_PAGE_TABLES *tables,
    uint64_t virtual_address)
{
    static const unsigned shifts[3] = {39U, 30U, 21U};
    uint64_t *table = tables->pool;

    for (size_t level = 0; level < 3U; ++level) {
        uint64_t entry = table[(virtual_address >> shifts[level]) & 0x1FFU];
        if ((entry & UINT64_C(1)) == 0U ||
            (entry & UINT64_C(0x80)) != 0U)
            return 0;
        table = (uint64_t *)(uintptr_t)(entry & tables->physical_mask);
    }
    return &table[(virtual_address >> 12) & 0x1FFU];
}

static void assert_uc_nx_mmio_leaf(
    VGK_PAGE_TABLES *tables,
    uint64_t virtual_address,
    uint64_t physical_address)
{
    uint64_t *leaf = walk_page(tables, virtual_address);
    assert(leaf != 0);
    assert((*leaf & tables->physical_mask) == physical_address);
    assert((*leaf & UINT64_C(0x018)) == UINT64_C(0x018));
    assert((*leaf & UINT64_C(0x2)) != 0U);
    assert((*leaf & (UINT64_C(1) << 63)) != 0U);
}

static void set_checksum(uint8_t *bytes, size_t length, size_t offset)
{
    uint8_t sum = 0U;
    bytes[offset] = 0U;
    for (size_t i = 0; i < length; ++i)
        sum = (uint8_t)(sum + bytes[i]);
    bytes[offset] = (uint8_t)(0U - sum);
}

static void make_sdt(uint8_t *table, const char signature[4],
                     uint32_t length, uint8_t revision)
{
    memset(table, 0, length);
    memcpy(table, signature, 4U);
    table[4] = (uint8_t)length;
    table[5] = (uint8_t)(length >> 8);
    table[6] = (uint8_t)(length >> 16);
    table[7] = (uint8_t)(length >> 24);
    table[8] = revision;
}

static void make_rsdp(uint8_t revision, uint64_t root_address,
                      int use_xsdt)
{
    uint8_t *rsdp = physical_memory + RSDP_OFFSET;
    memset(rsdp, 0, 36U);
    memcpy(rsdp, "RSD PTR ", 8U);
    rsdp[15] = revision;
    if (use_xsdt) {
        for (size_t i = 0; i < 8U; ++i)
            rsdp[24U + i] = (uint8_t)(root_address >> (i * 8U));
    } else {
        for (size_t i = 0; i < 4U; ++i)
            rsdp[16U + i] = (uint8_t)(root_address >> (i * 8U));
    }
    rsdp[20] = 36U;
    set_checksum(rsdp, 20U, 8U);
    if (revision >= 2U)
        set_checksum(rsdp, 36U, 32U);
}

static void make_valid_xsdt_madt(void)
{
    uint8_t *xsdt = physical_memory + ROOT_OFFSET;
    uint8_t *madt = physical_memory + MADT_OFFSET;

    memset(physical_memory, 0, sizeof(physical_memory));
    make_sdt(xsdt, "XSDT", 44U, 1U);
    for (size_t i = 0; i < 8U; ++i)
        xsdt[36U + i] = (uint8_t)(
            (PHYS_BASE + MADT_OFFSET) >> (i * 8U));
    set_checksum(xsdt, 44U, 9U);

    make_sdt(madt, "APIC", 44U, 5U);
    /* Local APIC address and flags are zero in this structural test. */
    set_checksum(madt, 44U, 9U);

    make_rsdp(2U, PHYS_BASE + ROOT_OFFSET, 1);
}


static void put_le16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *bytes, uint32_t value)
{
    for (size_t i = 0; i < 4U; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8U));
}

static void put_le64(uint8_t *bytes, uint64_t value)
{
    for (size_t i = 0; i < 8U; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8U));
}

static void make_rich_xsdt_madt(void)
{
    uint8_t *xsdt = physical_memory + ROOT_OFFSET;
    uint8_t *madt = physical_memory + MADT_OFFSET;
    uint32_t offset = 44U;

    memset(physical_memory, 0, sizeof(physical_memory));
    make_sdt(xsdt, "XSDT", 44U, 1U);
    put_le64(xsdt + 36U, PHYS_BASE + MADT_OFFSET);
    set_checksum(xsdt, 44U, 9U);

    make_sdt(madt, "APIC", 130U, 5U);
    put_le32(madt + 36U, UINT32_C(0xFEE00000));
    put_le32(madt + 40U, 1U); /* PCAT-compatible dual 8259s */

    /* Type 0: processor UID 1, local APIC ID 2, enabled. */
    madt[offset] = 0U; madt[offset + 1U] = 8U;
    madt[offset + 2U] = 1U; madt[offset + 3U] = 2U;
    put_le32(madt + offset + 4U, 1U); offset += 8U;

    /* Type 1: I/O APIC ID 3 at FEC00000, GSI base 0. */
    madt[offset] = 1U; madt[offset + 1U] = 12U;
    madt[offset + 2U] = 3U; madt[offset + 3U] = 0U;
    put_le32(madt + offset + 4U, UINT32_C(0xFEC00000));
    put_le32(madt + offset + 8U, 0U); offset += 12U;

    /* Type 2: ISA bus IRQ1 overrides to GSI1, conforming flags. */
    madt[offset] = 2U; madt[offset + 1U] = 10U;
    madt[offset + 2U] = 0U; madt[offset + 3U] = 1U;
    put_le32(madt + offset + 4U, 1U);
    put_le16(madt + offset + 8U, 0U); offset += 10U;

    /* Type 3: NMI source with GSI2. */
    madt[offset] = 3U; madt[offset + 1U] = 8U;
    put_le16(madt + offset + 2U, 0U);
    put_le32(madt + offset + 4U, 2U); offset += 8U;

    /* Type 4: processor UID1, NMI on LINT1. */
    madt[offset] = 4U; madt[offset + 1U] = 6U;
    madt[offset + 2U] = 1U; put_le16(madt + offset + 3U, 0U);
    madt[offset + 5U] = 1U; offset += 6U;

    /* Type 5: 64-bit local APIC address override. */
    madt[offset] = 5U; madt[offset + 1U] = 12U;
    put_le16(madt + offset + 2U, 0U);
    put_le64(madt + offset + 4U, UINT64_C(0xFEE01000)); offset += 12U;

    /* Type 9: x2APIC ID 0x123, processor UID2, enabled. */
    madt[offset] = 9U; madt[offset + 1U] = 16U;
    put_le16(madt + offset + 2U, 0U);
    put_le32(madt + offset + 4U, UINT32_C(0x123));
    put_le32(madt + offset + 8U, 1U);
    put_le32(madt + offset + 12U, 2U); offset += 16U;

    /* Type 10: x2APIC NMI for UID2 on LINT0. */
    madt[offset] = 10U; madt[offset + 1U] = 12U;
    put_le16(madt + offset + 2U, 0U);
    put_le16(madt + offset + 4U, 0U);
    put_le32(madt + offset + 6U, 2U);
    madt[offset + 10U] = 0U; madt[offset + 11U] = 0U; offset += 12U;

    /* Unknown future record types are skipped after bounds validation. */
    madt[offset] = 0x7FU; madt[offset + 1U] = 2U; offset += 2U;
    assert(offset == 130U);

    set_checksum(madt, 130U, 9U);
    make_rsdp(2U, PHYS_BASE + ROOT_OFFSET, 1);
}

static int read_physical(void *context, uint64_t address,
                         uint8_t *destination, size_t length)
{
    int *fail_reads = (int *)context;
    uint64_t offset;

    if (fail_reads != 0 && *fail_reads != 0)
        return 0;
    if (address < PHYS_BASE)
        return 0;
    offset = address - PHYS_BASE;
    if (offset > PHYS_SIZE || length > PHYS_SIZE - offset)
        return 0;
    memcpy(destination, physical_memory + (size_t)offset, length);
    return 1;
}

static VGK_ACPI_STATUS find_madt(VGK_ACPI_TABLE_REF *out, int *fail_reads)
{
    return vgk_acpi_find_madt(
        memory_map, 3U,
        PHYS_BASE + RSDP_OFFSET,
        physical_memory + RSDP_OFFSET,
        48U, read_physical, fail_reads, out);
}

static VGK_ACPI_STATUS parse_current_madt(
    VGK_ACPI_MADT_INFO *info,
    int *fail_reads)
{
    VGK_ACPI_TABLE_REF madt;
    VGK_ACPI_STATUS status = find_madt(&madt, fail_reads);
    if (status != VGK_ACPI_OK)
        return status;
    return vgk_acpi_madt_parse(
        memory_map, 3U, 48U, read_physical, fail_reads, &madt, info);
}

int main(void)
{
    VGK_ACPI_TABLE_REF madt;

    memory_map[0] = (VGK_MEMORY_MAP_ENTRY){
        PHYS_BASE, PHYS_SIZE, VGK_MEMORY_ACPI_RECLAIM, 0U};
    memory_map[1] = (VGK_MEMORY_MAP_ENTRY){
        UINT64_C(0xFEC00000), UINT64_C(0x1000), VGK_MEMORY_MMIO, 0U};
    memory_map[2] = (VGK_MEMORY_MAP_ENTRY){
        UINT64_C(0xFEE00000), UINT64_C(0x2000), VGK_MEMORY_MMIO, 0U};

    make_valid_xsdt_madt();
    assert(find_madt(&madt, 0) == VGK_ACPI_OK);
    assert(madt.physical_address == PHYS_BASE + MADT_OFFSET);
    assert(madt.length == 44U);
    assert(madt.revision == 5U);
    assert(madt.reserved[0] == 0U &&
           madt.reserved[1] == 0U && madt.reserved[2] == 0U);

    /* A corrupted root checksum rejects the complete root directory. */
    make_valid_xsdt_madt();
    physical_memory[ROOT_OFFSET + 10U] ^= 1U;
    assert(find_madt(&madt, 0) == VGK_ACPI_BAD_ROOT_TABLE);
    assert(madt.physical_address == 0U);

    /* A corrupted MADT checksum rejects the candidate table. */
    make_valid_xsdt_madt();
    physical_memory[MADT_OFFSET + 10U] ^= 1U;
    assert(find_madt(&madt, 0) == VGK_ACPI_BAD_TABLE);
    assert(madt.physical_address == 0U);

    /* A root entry pointing outside readable ACPI memory is rejected. */
    make_valid_xsdt_madt();
    for (size_t i = 0; i < 8U; ++i)
        physical_memory[ROOT_OFFSET + 36U + i] = 0xFFU;
    set_checksum(physical_memory + ROOT_OFFSET, 44U, 9U);
    assert(find_madt(&madt, 0) == VGK_ACPI_BAD_ROOT_TABLE);

    /* MADT requires its fixed 8-byte Local APIC address/flags prefix. */
    make_valid_xsdt_madt();
    make_sdt(physical_memory + MADT_OFFSET, "APIC", 36U, 5U);
    set_checksum(physical_memory + MADT_OFFSET, 36U, 9U);
    assert(find_madt(&madt, 0) == VGK_ACPI_BAD_TABLE);
    assert(madt.physical_address == 0U);

    /* A structurally valid root without an APIC table reports not-found. */
    make_valid_xsdt_madt();
    make_sdt(physical_memory + OTHER_OFFSET, "FACP", 36U, 6U);
    for (size_t i = 0; i < 8U; ++i)
        physical_memory[ROOT_OFFSET + 36U + i] =
            (uint8_t)((PHYS_BASE + OTHER_OFFSET) >> (i * 8U));
    set_checksum(physical_memory + ROOT_OFFSET, 44U, 9U);
    assert(find_madt(&madt, 0) == VGK_ACPI_TABLE_NOT_FOUND);

    /* Duplicate MADT pointers are ambiguous and must not be accepted. */
    make_valid_xsdt_madt();
    {
        uint8_t *xsdt = physical_memory + ROOT_OFFSET;
        xsdt[4] = 52U;
        xsdt[5] = 0U;
        xsdt[6] = 0U;
        xsdt[7] = 0U;
        for (size_t i = 0; i < 8U; ++i)
            xsdt[44U + i] = (uint8_t)(
                (PHYS_BASE + MADT_OFFSET) >> (i * 8U));
        set_checksum(xsdt, 52U, 9U);
    }
    assert(find_madt(&madt, 0) == VGK_ACPI_BAD_ROOT_TABLE);

    /* Invalid root entry-array byte length is rejected. */
    make_valid_xsdt_madt();
    physical_memory[ROOT_OFFSET + 4U] = 45U;
    set_checksum(physical_memory + ROOT_OFFSET, 45U, 9U);
    assert(find_madt(&madt, 0) == VGK_ACPI_BAD_ROOT_TABLE);

    /* A root table with more than the bounded entry count is rejected. */
    make_valid_xsdt_madt();
    {
        const uint32_t oversized_length = 36U + (257U * 8U);
        uint8_t *xsdt = physical_memory + ROOT_OFFSET;
        make_sdt(xsdt, "XSDT", oversized_length, 1U);
        set_checksum(xsdt, oversized_length, 9U);
        make_rsdp(2U, PHYS_BASE + ROOT_OFFSET, 1);
        assert(find_madt(&madt, 0) == VGK_ACPI_BAD_ROOT_TABLE);
    }

    /* Callback failures are surfaced without publishing a partial result. */
    make_valid_xsdt_madt();
    {
        int fail_reads = 1;
        assert(find_madt(&madt, &fail_reads) == VGK_ACPI_READ_FAILED);
        assert(madt.physical_address == 0U);
    }

    /* ACPI 1.x uses the 32-bit RSDT pointer and 4-byte entries. */
    make_valid_xsdt_madt();
    {
        uint8_t *rsdt = physical_memory + ROOT_OFFSET;
        make_sdt(rsdt, "RSDT", 40U, 1U);
        for (size_t i = 0; i < 4U; ++i)
            rsdt[36U + i] = (uint8_t)(
                (PHYS_BASE + MADT_OFFSET) >> (i * 8U));
        set_checksum(rsdt, 40U, 9U);
        make_rsdp(1U, PHYS_BASE + ROOT_OFFSET, 0);
        assert(find_madt(&madt, 0) == VGK_ACPI_OK);
        assert(madt.physical_address == PHYS_BASE + MADT_OFFSET);
        assert(madt.length == 44U);
    }

    /* ACPI 2+ with a zero XSDT pointer falls back to the 32-bit RSDT. */
    make_valid_xsdt_madt();
    {
        uint8_t *rsdt = physical_memory + ROOT_OFFSET;
        make_sdt(rsdt, "RSDT", 40U, 1U);
        for (size_t i = 0; i < 4U; ++i)
            rsdt[36U + i] = (uint8_t)(
                (PHYS_BASE + MADT_OFFSET) >> (i * 8U));
        set_checksum(rsdt, 40U, 9U);
        make_rsdp(2U, PHYS_BASE + ROOT_OFFSET, 0);
        assert(find_madt(&madt, 0) == VGK_ACPI_OK);
        assert(madt.physical_address == PHYS_BASE + MADT_OFFSET);
    }

    assert(vgk_acpi_find_madt(
        memory_map, 1U, PHYS_BASE + RSDP_OFFSET,
        physical_memory + RSDP_OFFSET, 48U, 0, 0, &madt) ==
        VGK_ACPI_INVALID_ARGUMENT);

    /* Invalid RSDP is rejected before the physical reader is called. */
    make_valid_xsdt_madt();
    physical_memory[RSDP_OFFSET] = 'X';
    assert(find_madt(&madt, 0) == VGK_ACPI_BAD_RSDP);
    assert(madt.physical_address == 0U);

    /* Parse real MADT record layouts into bounded topology data. */
    make_rich_xsdt_madt();
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_OK);
        assert(info.local_apic_address == UINT64_C(0xFEE01000));
        assert(info.flags == 1U);
        assert(info.processor_count == 2U);
        assert(info.processors[0].processor_uid == 1U);
        assert(info.processors[0].apic_id == 2U);
        assert(info.processors[0].flags == 1U);
        assert(info.processors[0].x2apic == 0U);
        assert(info.processors[1].processor_uid == 2U);
        assert(info.processors[1].apic_id == UINT32_C(0x123));
        assert(info.processors[1].x2apic == 1U);
        assert(info.ioapic_count == 1U);
        assert(info.ioapics[0].id == 3U);
        assert(info.ioapics[0].address == UINT32_C(0xFEC00000));
        assert(info.ioapics[0].gsi_base == 0U);
        assert(info.override_count == 1U);
        assert(info.overrides[0].bus == 0U);
        assert(info.overrides[0].source == 1U);
        assert(info.overrides[0].gsi == 1U);
        assert(info.overrides[0].flags == 0U);
        assert(info.nmi_source_count == 1U);
        assert(info.nmi_sources[0].gsi == 2U);
        assert(info.nmi_sources[0].flags == 0U);
        assert(info.local_nmi_count == 2U);
        assert(info.local_nmis[0].processor_uid == 1U);
        assert(info.local_nmis[0].lint == 1U);
        assert(info.local_nmis[0].x2apic == 0U);
        assert(info.local_nmis[1].processor_uid == 2U);
        assert(info.local_nmis[1].lint == 0U);
        assert(info.local_nmis[1].x2apic == 1U);

        /* Firmware's APIC_BASE MSR must agree with the MADT override. */
        assert(vgk_acpi_madt_validate_local_apic_base(
                   &info, UINT64_C(0xFEE01000), 1U) == VGK_ACPI_OK);
        assert(vgk_acpi_madt_validate_local_apic_base(
                   &info, UINT64_C(0xFEE00000), 1U) ==
               VGK_ACPI_BAD_TABLE);
        assert(vgk_acpi_madt_validate_local_apic_base(
                   &info, UINT64_C(0xFEE01000), 0U) ==
               VGK_ACPI_BAD_TABLE);
        assert(vgk_acpi_madt_validate_local_apic_base(
                   &info, UINT64_C(0xFEE01001), 1U) ==
               VGK_ACPI_BAD_TABLE);
        assert(vgk_acpi_madt_validate_local_apic_base(
                   0, UINT64_C(0xFEE01000), 1U) ==
               VGK_ACPI_INVALID_ARGUMENT);

        /* xAPIC mode needs both LAPIC and IOAPIC UC pages. */
        {
            VGK_ACPI_MADT_MMIO_RANGE plan[VGK_ACPI_MADT_MAX_MMIO_RANGES];
            VGK_PAGE_TABLES page_tables;
            size_t plan_count = 99U;
            assert(vgk_acpi_madt_build_mmio_plan(
                       &info, UINT64_C(0xFEE01000), 1U, 0U,
                       plan, VGK_ACPI_MADT_MAX_MMIO_RANGES,
                       &plan_count) == VGK_ACPI_OK);
            assert(plan_count == 2U);
            assert(plan[0].physical_base == UINT64_C(0xFEE01000));
            assert(plan[0].length == VGK_BOOT_PAGE_SIZE);
            assert(plan[0].kind == VGK_ACPI_MADT_MMIO_LOCAL_APIC);
            assert(plan[1].physical_base == UINT64_C(0xFEC00000));
            assert(plan[1].length == VGK_BOOT_PAGE_SIZE);
            assert(plan[1].kind == VGK_ACPI_MADT_MMIO_IOAPIC);
            for (size_t i = 0; i < sizeof(plan[0].reserved); ++i)
                assert(plan[0].reserved[i] == 0U);

            /* Feed the xAPIC plan into the real inactive page-table builder. */
            assert(vgk_paging_init(
                       &page_tables, apic_page_pool, 8U, 48U, 1U) ==
                   VGK_PAGING_OK);
            for (size_t i = 0; i < plan_count; ++i) {
                assert(plan[i].length == VGK_BOOT_PAGE_SIZE);
                assert(vgk_paging_map_mmio_uc(
                           &page_tables,
                           plan[i].physical_base,
                           plan[i].physical_base,
                           plan[i].length) == VGK_PAGING_OK);
                assert_uc_nx_mmio_leaf(
                    &page_tables, plan[i].physical_base,
                    plan[i].physical_base);
            }

            /* x2APIC mode uses MSRs for LAPIC and maps only IOAPIC pages. */
            assert(vgk_acpi_madt_build_mmio_plan(
                       &info, UINT64_C(0xFEE01000), 1U, 1U,
                       plan, VGK_ACPI_MADT_MAX_MMIO_RANGES,
                       &plan_count) == VGK_ACPI_OK);
            assert(plan_count == 1U);
            assert(plan[0].physical_base == UINT64_C(0xFEC00000));
            assert(plan[0].kind == VGK_ACPI_MADT_MMIO_IOAPIC);

            /* x2APIC maps the IOAPIC page but leaves LAPIC MMIO unmapped. */
            assert(vgk_paging_init(
                       &page_tables, apic_page_pool, 8U, 48U, 1U) ==
                   VGK_PAGING_OK);
            assert(vgk_paging_map_mmio_uc(
                       &page_tables, plan[0].physical_base,
                       plan[0].physical_base, plan[0].length) ==
                   VGK_PAGING_OK);
            assert_uc_nx_mmio_leaf(
                &page_tables, UINT64_C(0xFEC00000),
                UINT64_C(0xFEC00000));
            assert(walk_page(&page_tables, UINT64_C(0xFEE01000)) == 0);

            /* A short output buffer fails without publishing a partial plan. */
            plan_count = 99U;
            assert(vgk_acpi_madt_build_mmio_plan(
                       &info, UINT64_C(0xFEE01000), 1U, 0U,
                       plan, 1U, &plan_count) ==
                   VGK_ACPI_INVALID_ARGUMENT);
            assert(plan_count == 0U && plan[0].physical_base == 0U);

            assert(vgk_acpi_madt_build_mmio_plan(
                       &info, UINT64_C(0xFEE00000), 1U, 0U,
                       plan, VGK_ACPI_MADT_MAX_MMIO_RANGES,
                       &plan_count) == VGK_ACPI_BAD_TABLE);
            assert(plan_count == 0U);

            assert(vgk_acpi_madt_build_mmio_plan(
                       &info, UINT64_C(0xFEE01000), 1U, 2U,
                       plan, VGK_ACPI_MADT_MAX_MMIO_RANGES,
                       &plan_count) == VGK_ACPI_INVALID_ARGUMENT);

            /* IOAPIC may not overlap the LAPIC page, even in x2APIC mode. */
            {
                VGK_ACPI_MADT_INFO invalid = info;
                invalid.ioapics[0].address = UINT32_C(0xFEE01000);
                assert(vgk_acpi_madt_build_mmio_plan(
                           &invalid, UINT64_C(0xFEE01000), 1U, 1U,
                           plan, VGK_ACPI_MADT_MAX_MMIO_RANGES,
                           &plan_count) == VGK_ACPI_BAD_TABLE);
                assert(plan_count == 0U);
            }

            /* Duplicate I/O APIC entries cannot produce duplicate mappings. */
            {
                VGK_ACPI_MADT_INFO invalid = info;
                invalid.ioapic_count = 2U;
                invalid.ioapics[1] = invalid.ioapics[0];
                assert(vgk_acpi_madt_build_mmio_plan(
                           &invalid, UINT64_C(0xFEE01000), 1U, 1U,
                           plan, VGK_ACPI_MADT_MAX_MMIO_RANGES,
                           &plan_count) == VGK_ACPI_BAD_TABLE);
                assert(plan_count == 0U);
            }

            /* Two distinct IOAPIC pages cannot claim the same GSI base. */
            {
                VGK_ACPI_MADT_INFO invalid = info;
                invalid.ioapic_count = 2U;
                invalid.ioapics[1] = invalid.ioapics[0];
                invalid.ioapics[1].id = 4U;
                invalid.ioapics[1].address = UINT32_C(0xFEC01000);
                assert(vgk_acpi_madt_build_mmio_plan(
                           &invalid, UINT64_C(0xFEE01000), 1U, 1U,
                           plan, VGK_ACPI_MADT_MAX_MMIO_RANGES,
                           &plan_count) == VGK_ACPI_BAD_TABLE);
                assert(plan_count == 0U);
            }
        }
    }

    /* Firmware may classify a device aperture as generic reserved memory. */
    make_rich_xsdt_madt();
    memory_map[2].type = VGK_MEMORY_RESERVED;
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_OK);
    }
    memory_map[2].type = VGK_MEMORY_MMIO;

    /* A gap in the normalized map cannot be treated as an MMIO aperture. */
    make_rich_xsdt_madt();
    {
        VGK_ACPI_TABLE_REF ref;
        VGK_ACPI_MADT_INFO info;
        assert(find_madt(&ref, 0) == VGK_ACPI_OK);
        memory_map[2].base = UINT64_C(0xFEE02000);
        assert(vgk_acpi_madt_parse(
            memory_map, 3U, 48U, read_physical, 0, &ref, &info) ==
            VGK_ACPI_BAD_TABLE);
        memory_map[2].base = UINT64_C(0xFEE00000);
    }

    /* Overlapping normalized descriptors invalidate the APIC range proof. */
    {
        VGK_ACPI_TABLE_REF ref;
        VGK_ACPI_MADT_INFO info;
        assert(find_madt(&ref, 0) == VGK_ACPI_OK);
        memory_map[2].base = UINT64_C(0xFEC00800);
        assert(vgk_acpi_madt_parse(
            memory_map, 3U, 48U, read_physical, 0, &ref, &info) ==
            VGK_ACPI_BAD_TABLE);
        memory_map[2].base = UINT64_C(0xFEE00000);
    }

    /* An I/O APIC address inside ACPI reclaimable RAM is not MMIO. */
    make_rich_xsdt_madt();
    put_le32(physical_memory + MADT_OFFSET + 56U,
             (uint32_t)(PHYS_BASE + UINT64_C(0x9000)));
    set_checksum(physical_memory + MADT_OFFSET, 130U, 9U);
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_BAD_TABLE);
        assert(info.ioapic_count == 0U);
    }

    /* A Local APIC override into ACPI table memory is rejected as well. */
    make_rich_xsdt_madt();
    put_le64(physical_memory + MADT_OFFSET + 92U,
             PHYS_BASE + UINT64_C(0x8000));
    set_checksum(physical_memory + MADT_OFFSET, 130U, 9U);
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_BAD_TABLE);
        assert(info.local_apic_address == 0U);
    }

    /* Duplicate processor UID/APIC ID values are rejected. */
    make_rich_xsdt_madt();
    put_le32(physical_memory + MADT_OFFSET + 112U, 1U);
    set_checksum(physical_memory + MADT_OFFSET, 130U, 9U);
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_BAD_TABLE);
        assert(info.processor_count == 0U);
    }

    /* An unaligned I/O APIC MMIO base is rejected. */
    make_rich_xsdt_madt();
    put_le32(physical_memory + MADT_OFFSET + 56U,
             UINT32_C(0xFEC00001));
    set_checksum(physical_memory + MADT_OFFSET, 130U, 9U);
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_BAD_TABLE);
        assert(info.ioapic_count == 0U);
    }

    /* Reserved polarity encoding in an interrupt-source override is invalid. */
    make_rich_xsdt_madt();
    put_le16(physical_memory + MADT_OFFSET + 72U, 2U);
    set_checksum(physical_memory + MADT_OFFSET, 130U, 9U);
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_BAD_TABLE);
        assert(info.override_count == 0U);
    }

    /* Duplicate NMI source GSI records are ambiguous and rejected. */
    make_rich_xsdt_madt();
    {
        uint8_t *madt = physical_memory + MADT_OFFSET;
        madt[4U] = 140U; madt[5U] = 0U;
        madt[6U] = 0U; madt[7U] = 0U;
        madt[128U] = 3U; madt[129U] = 8U;
        put_le16(madt + 130U, 0U);
        put_le32(madt + 132U, 2U);
        madt[136U] = 0x7FU; madt[137U] = 2U;
        set_checksum(madt, 140U, 9U);
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_BAD_TABLE);
        assert(info.nmi_source_count == 0U);
    }

    /* A one-byte record cannot make forward progress through the table. */
    make_rich_xsdt_madt();
    physical_memory[MADT_OFFSET + 45U] = 1U;
    set_checksum(physical_memory + MADT_OFFSET, 130U, 9U);
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_BAD_TABLE);
    }

    /* A zero local APIC base without a valid override is rejected. */
    make_rich_xsdt_madt();
    put_le64(physical_memory + MADT_OFFSET + 92U, 0U);
    set_checksum(physical_memory + MADT_OFFSET, 130U, 9U);
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_BAD_TABLE);
    }

    /* Processor records are bounded; the 65th entry is rejected. */
    memset(physical_memory, 0, sizeof(physical_memory));
    {
        const uint32_t madt_length = 44U + (65U * 8U);
        uint8_t *xsdt = physical_memory + ROOT_OFFSET;
        uint8_t *madt = physical_memory + MADT_OFFSET;
        make_sdt(xsdt, "XSDT", 44U, 1U);
        put_le64(xsdt + 36U, PHYS_BASE + MADT_OFFSET);
        set_checksum(xsdt, 44U, 9U);
        make_sdt(madt, "APIC", madt_length, 5U);
        put_le32(madt + 36U, UINT32_C(0xFEE00000));
        for (uint32_t i = 0; i < 65U; ++i) {
            uint8_t *entry = madt + 44U + (i * 8U);
            entry[0] = 0U; entry[1] = 8U;
            entry[2] = (uint8_t)i; entry[3] = (uint8_t)i;
            put_le32(entry + 4U, 1U);
        }
        set_checksum(madt, madt_length, 9U);
        make_rsdp(2U, PHYS_BASE + ROOT_OFFSET, 1);
    }
    {
        VGK_ACPI_MADT_INFO info;
        assert(parse_current_madt(&info, 0) == VGK_ACPI_BAD_TABLE);
        assert(info.processor_count == 0U);
    }

    /* The parser rejects a forged reference with a mismatched length. */
    make_rich_xsdt_madt();
    {
        VGK_ACPI_TABLE_REF ref;
        VGK_ACPI_MADT_INFO info;
        assert(find_madt(&ref, 0) == VGK_ACPI_OK);
        --ref.length;
        assert(vgk_acpi_madt_parse(
            memory_map, 3U, 48U, read_physical, 0, &ref, &info) ==
            VGK_ACPI_BAD_TABLE);
        assert(info.processor_count == 0U &&
               info.ioapic_count == 0U &&
               info.override_count == 0U);
    }

    /* A callback failure during MADT parsing is returned to the caller. */
    make_rich_xsdt_madt();
    {
        VGK_ACPI_TABLE_REF ref;
        VGK_ACPI_MADT_INFO info;
        int fail_reads = 1;
        assert(find_madt(&ref, 0) == VGK_ACPI_OK);
        assert(vgk_acpi_madt_parse(
            memory_map, 3U, 48U, read_physical, &fail_reads,
            &ref, &info) == VGK_ACPI_READ_FAILED);
        assert(info.processor_count == 0U &&
               info.ioapic_count == 0U &&
               info.override_count == 0U);
    }

    puts("VGK ACPI RSDT/XSDT and MADT discovery and record parsing tests: OK");
    return 0;
}
