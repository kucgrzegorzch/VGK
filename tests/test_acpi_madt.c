#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vgk_acpi.h"

#define PHYS_BASE UINT64_C(0x100000)
#define PHYS_SIZE UINT64_C(0x10000)
#define RSDP_OFFSET 0x1000U
#define ROOT_OFFSET 0x2000U
#define MADT_OFFSET 0x3000U
#define OTHER_OFFSET 0x4000U

static uint8_t physical_memory[PHYS_SIZE];
static VGK_MEMORY_MAP_ENTRY memory_map[1];

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
        memory_map, 1U,
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
        memory_map, 1U, 48U, read_physical, fail_reads, &madt, info);
}

int main(void)
{
    VGK_ACPI_TABLE_REF madt;

    memory_map[0] = (VGK_MEMORY_MAP_ENTRY){
        PHYS_BASE, PHYS_SIZE, VGK_MEMORY_ACPI_RECLAIM, 0U};

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

    /* The parser rejects a forged reference with a mismatched length. */
    make_rich_xsdt_madt();
    {
        VGK_ACPI_TABLE_REF ref;
        VGK_ACPI_MADT_INFO info;
        assert(find_madt(&ref, 0) == VGK_ACPI_OK);
        --ref.length;
        assert(vgk_acpi_madt_parse(
            memory_map, 1U, 48U, read_physical, 0, &ref, &info) ==
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
            memory_map, 1U, 48U, read_physical, &fail_reads,
            &ref, &info) == VGK_ACPI_READ_FAILED);
        assert(info.processor_count == 0U &&
               info.ioapic_count == 0U &&
               info.override_count == 0U);
    }

    puts("VGK ACPI RSDT/XSDT and MADT discovery and record parsing tests: OK");
    return 0;
}
