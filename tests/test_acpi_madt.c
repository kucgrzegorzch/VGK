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

    puts("VGK ACPI RSDT/XSDT and MADT discovery tests: OK");
    return 0;
}
