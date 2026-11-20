#include "vgk_acpi.h"

#define VGK_ACPI_RSDP_MIN_LENGTH UINT64_C(20)
#define VGK_ACPI_RSDP_EXTENDED_PREFIX UINT64_C(24)
#define VGK_ACPI_RSDP_MAX_LENGTH UINT64_C(4096)

static int acpi_memory_type_is_readable(uint64_t type)
{
    return type == VGK_MEMORY_ACPI_RECLAIM ||
           type == VGK_MEMORY_ACPI_NVS ||
           type == VGK_MEMORY_FIRMWARE;
}

/*
 * Require complete coverage, including adjacent descriptors. Gaps,
 * arithmetic overflow, generic reserved memory and MMIO are rejected.
 */
static int range_is_readable_acpi_memory(
    const VGK_MEMORY_MAP_ENTRY *entries,
    size_t entry_count,
    uint64_t address,
    uint64_t length,
    uint8_t physical_address_bits)
{
    uint64_t end;
    uint64_t physical_limit;
    uint64_t cursor = address;

    if (entries == 0 || entry_count == 0 || length == 0 ||
        physical_address_bits < 32U || physical_address_bits > 63U ||
        address > UINT64_MAX - length)
        return 0;

    /*
     * The coverage walk below relies on a canonical, sorted, non-overlapping
     * map. Validate the complete input first, not only descriptors touched
     * by the requested range.
     */
    for (size_t i = 0; i < entry_count; ++i) {
        if (entries[i].length == 0 ||
            entries[i].base > UINT64_MAX - entries[i].length)
            return 0;
        if (i != 0) {
            uint64_t previous_end =
                entries[i - 1U].base + entries[i - 1U].length;
            if (entries[i].base < previous_end)
                return 0;
        }
    }

    physical_limit = (UINT64_C(1) << physical_address_bits) - 1U;
    end = address + length;
    if (end == 0U || end - 1U > physical_limit)
        return 0;

    for (size_t i = 0; i < entry_count; ++i) {
        uint64_t entry_end;

        entry_end = entries[i].base + entries[i].length;

        if (entry_end <= cursor)
            continue;
        if (entries[i].base > cursor ||
            !acpi_memory_type_is_readable(entries[i].type))
            return 0;

        cursor = entry_end < end ? entry_end : end;
        if (cursor == end)
            return 1;
    }

    return 0;
}

static int checksum_is_zero(
    const volatile uint8_t *bytes,
    uint64_t length)
{
    uint8_t sum = 0;

    for (uint64_t i = 0; i < length; ++i)
        sum = (uint8_t)(sum + bytes[i]);
    return sum == 0;
}

int vgk_acpi_rsdp_validate(
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    uint64_t physical_address,
    uint8_t physical_address_bits,
    const volatile uint8_t *bytes,
    uint64_t *length_out)
{
    uint64_t length = VGK_ACPI_RSDP_MIN_LENGTH;

    if (length_out != 0)
        *length_out = 0;

    if (physical_address == 0 || bytes == 0 || length_out == 0 ||
        !range_is_readable_acpi_memory(
            memory_map, memory_map_count, physical_address, length,
            physical_address_bits))
        return 0;

    if (bytes[0] != 'R' || bytes[1] != 'S' ||
        bytes[2] != 'D' || bytes[3] != ' ' ||
        bytes[4] != 'P' || bytes[5] != 'T' ||
        bytes[6] != 'R' || bytes[7] != ' ')
        return 0;

    if (!checksum_is_zero(bytes, VGK_ACPI_RSDP_MIN_LENGTH))
        return 0;

    /* ACPI 1.0 RSDP ends after the legacy 20-byte structure. */
    if (bytes[15] >= 2U) {
        if (!range_is_readable_acpi_memory(
                memory_map, memory_map_count, physical_address,
                VGK_ACPI_RSDP_EXTENDED_PREFIX, physical_address_bits))
            return 0;

        length = (uint64_t)bytes[20] |
                 ((uint64_t)bytes[21] << 8) |
                 ((uint64_t)bytes[22] << 16) |
                 ((uint64_t)bytes[23] << 24);
        if (length < 36U || length > VGK_ACPI_RSDP_MAX_LENGTH ||
            !range_is_readable_acpi_memory(
                memory_map, memory_map_count, physical_address, length,
                physical_address_bits))
            return 0;

        if (!checksum_is_zero(bytes, length))
            return 0;
    }

    *length_out = length;
    return 1;
}


#define VGK_ACPI_SDT_HEADER_SIZE 36U
#define VGK_ACPI_MAX_TABLE_LENGTH (1024U * 1024U)
#define VGK_ACPI_MAX_ROOT_ENTRIES 256U
#define VGK_ACPI_CHECKSUM_CHUNK 256U

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static uint64_t read_le64(const uint8_t *bytes)
{
    return (uint64_t)read_le32(bytes) |
           ((uint64_t)read_le32(bytes + 4U) << 32);
}


static uint64_t read_le64_volatile(const volatile uint8_t *bytes)
{
    uint64_t value = 0U;
    for (size_t i = 0; i < 8U; ++i)
        value |= (uint64_t)bytes[i] << (i * 8U);
    return value;
}

static int signature_matches(
    const uint8_t signature[4],
    const char expected[4])
{
    for (size_t i = 0; i < 4U; ++i) {
        if (signature[i] != (uint8_t)expected[i])
            return 0;
    }
    return 1;
}

static VGK_ACPI_STATUS read_sdt_header(
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    uint8_t physical_address_bits,
    VGK_ACPI_PHYSICAL_READ read_physical,
    void *read_context,
    uint64_t address,
    uint8_t header[VGK_ACPI_SDT_HEADER_SIZE],
    uint32_t *length_out)
{
    uint32_t length;

    if (address == 0U ||
        !range_is_readable_acpi_memory(
            memory_map, memory_map_count, address,
            VGK_ACPI_SDT_HEADER_SIZE, physical_address_bits))
        return VGK_ACPI_BAD_TABLE;

    if (!read_physical(read_context, address, header,
                       VGK_ACPI_SDT_HEADER_SIZE))
        return VGK_ACPI_READ_FAILED;

    length = read_le32(header + 4U);
    if (length < VGK_ACPI_SDT_HEADER_SIZE ||
        length > VGK_ACPI_MAX_TABLE_LENGTH ||
        !range_is_readable_acpi_memory(
            memory_map, memory_map_count, address, length,
            physical_address_bits))
        return VGK_ACPI_BAD_TABLE;

    *length_out = length;
    return VGK_ACPI_OK;
}

static VGK_ACPI_STATUS checksum_sdt(
    VGK_ACPI_PHYSICAL_READ read_physical,
    void *read_context,
    uint64_t address,
    uint32_t length)
{
    uint8_t chunk[VGK_ACPI_CHECKSUM_CHUNK];
    uint32_t offset = 0U;
    uint8_t sum = 0U;

    while (offset < length) {
        size_t amount = (size_t)(length - offset);
        if (amount > sizeof(chunk))
            amount = sizeof(chunk);
        if (address > UINT64_MAX - offset ||
            !read_physical(read_context, address + offset, chunk, amount))
            return VGK_ACPI_READ_FAILED;
        for (size_t i = 0; i < amount; ++i)
            sum = (uint8_t)(sum + chunk[i]);
        offset += (uint32_t)amount;
    }

    return sum == 0U ? VGK_ACPI_OK : VGK_ACPI_BAD_TABLE;
}

VGK_ACPI_STATUS vgk_acpi_find_madt(
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    uint64_t rsdp_physical_address,
    const volatile uint8_t *rsdp_bytes,
    uint8_t physical_address_bits,
    VGK_ACPI_PHYSICAL_READ read_physical,
    void *read_context,
    VGK_ACPI_TABLE_REF *madt_out)
{
    uint64_t rsdp_length = 0U;
    uint64_t root_address;
    uint32_t root_length = 0U;
    uint32_t entry_size;
    uint32_t entry_count;
    uint8_t root_header[VGK_ACPI_SDT_HEADER_SIZE];
    const char *root_signature;
    uint64_t madt_address = 0U;
    uint32_t madt_length = 0U;
    uint8_t madt_revision = 0U;
    VGK_ACPI_STATUS status;

    if (madt_out != 0) {
        madt_out->physical_address = 0U;
        madt_out->length = 0U;
        madt_out->revision = 0U;
        madt_out->reserved[0] = 0U;
        madt_out->reserved[1] = 0U;
        madt_out->reserved[2] = 0U;
    }

    if (memory_map == 0 || memory_map_count == 0U ||
        rsdp_bytes == 0 || read_physical == 0 || madt_out == 0 ||
        physical_address_bits < 32U || physical_address_bits > 63U)
        return VGK_ACPI_INVALID_ARGUMENT;

    if (!vgk_acpi_rsdp_validate(
            memory_map, memory_map_count, rsdp_physical_address,
            physical_address_bits, rsdp_bytes, &rsdp_length))
        return VGK_ACPI_BAD_RSDP;

    if (rsdp_bytes[15] >= 2U && rsdp_length >= 36U)
        root_address = read_le64_volatile(rsdp_bytes + 24U);
    else
        root_address = 0U;

    if (root_address != 0U) {
        root_signature = "XSDT";
        entry_size = 8U;
    } else {
        root_address = (uint64_t)rsdp_bytes[16] |
                       ((uint64_t)rsdp_bytes[17] << 8) |
                       ((uint64_t)rsdp_bytes[18] << 16) |
                       ((uint64_t)rsdp_bytes[19] << 24);
        root_signature = "RSDT";
        entry_size = 4U;
    }

    status = read_sdt_header(
        memory_map, memory_map_count, physical_address_bits,
        read_physical, read_context, root_address, root_header,
        &root_length);
    if (status != VGK_ACPI_OK)
        return status == VGK_ACPI_READ_FAILED ? status : VGK_ACPI_BAD_ROOT_TABLE;

    if (!signature_matches(root_header, root_signature))
        return VGK_ACPI_BAD_ROOT_TABLE;

    if ((root_length - VGK_ACPI_SDT_HEADER_SIZE) % entry_size != 0U)
        return VGK_ACPI_BAD_ROOT_TABLE;

    entry_count = (root_length - VGK_ACPI_SDT_HEADER_SIZE) / entry_size;
    if (entry_count == 0U || entry_count > VGK_ACPI_MAX_ROOT_ENTRIES)
        return VGK_ACPI_BAD_ROOT_TABLE;

    status = checksum_sdt(read_physical, read_context, root_address, root_length);
    if (status != VGK_ACPI_OK)
        return status == VGK_ACPI_READ_FAILED ? status : VGK_ACPI_BAD_ROOT_TABLE;

    for (uint32_t i = 0; i < entry_count; ++i) {
        uint8_t entry_bytes[8] = {0};
        uint8_t table_header[VGK_ACPI_SDT_HEADER_SIZE];
        uint64_t entry_offset =
            VGK_ACPI_SDT_HEADER_SIZE + ((uint64_t)i * entry_size);
        uint64_t table_address;
        uint32_t table_length = 0U;

        if (root_address > UINT64_MAX - entry_offset ||
            !read_physical(read_context, root_address + entry_offset,
                           entry_bytes, entry_size))
            return VGK_ACPI_READ_FAILED;

        table_address = entry_size == 8U ?
            read_le64(entry_bytes) : (uint64_t)read_le32(entry_bytes);
        if (table_address == 0U)
            return VGK_ACPI_BAD_ROOT_TABLE;

        status = read_sdt_header(
            memory_map, memory_map_count, physical_address_bits,
            read_physical, read_context, table_address, table_header,
            &table_length);
        if (status != VGK_ACPI_OK)
            return status == VGK_ACPI_READ_FAILED ? status : VGK_ACPI_BAD_ROOT_TABLE;

        if (signature_matches(table_header, "APIC")) {
            if (madt_address != 0U)
                return VGK_ACPI_BAD_ROOT_TABLE;
            status = checksum_sdt(
                read_physical, read_context, table_address, table_length);
            if (status != VGK_ACPI_OK)
                return status;
            madt_address = table_address;
            madt_length = table_length;
            madt_revision = table_header[8U];
        }
    }

    if (madt_address == 0U)
        return VGK_ACPI_TABLE_NOT_FOUND;

    madt_out->physical_address = madt_address;
    madt_out->length = madt_length;
    madt_out->revision = madt_revision;
    return VGK_ACPI_OK;
}
