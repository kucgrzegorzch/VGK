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
            /* MADT includes an 8-byte Local APIC address/flags prefix. */
            if (table_length < VGK_ACPI_SDT_HEADER_SIZE + 8U)
                return VGK_ACPI_BAD_TABLE;
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

static uint16_t read_le16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static int physical_range_fits(
    uint64_t address,
    uint64_t length,
    uint8_t physical_address_bits)
{
    uint64_t physical_limit;

    if (length == 0U || physical_address_bits < 32U ||
        physical_address_bits > 63U)
        return 0;

    physical_limit = (UINT64_C(1) << physical_address_bits) - 1U;
    return address != 0U && address <= physical_limit &&
           length - 1U <= physical_limit - address;
}

static int madt_interrupt_flags_are_valid(uint16_t flags)
{
    const uint16_t polarity = flags & 0x3U;
    const uint16_t trigger = (flags >> 2) & 0x3U;

    return (flags & UINT16_C(0xFFF0)) == 0U &&
           polarity != 2U && trigger != 2U;
}

static void acpi_zero_bytes(void *destination, size_t length)
{
    volatile uint8_t *bytes = (volatile uint8_t *)destination;
    for (size_t i = 0; i < length; ++i)
        bytes[i] = 0U;
}

static void acpi_copy_bytes(
    void *destination,
    const void *source,
    size_t length)
{
    volatile uint8_t *out = (volatile uint8_t *)destination;
    const volatile uint8_t *in = (const volatile uint8_t *)source;
    for (size_t i = 0; i < length; ++i)
        out[i] = in[i];
}

static int madt_add_processor(
    VGK_ACPI_MADT_INFO *info,
    uint32_t uid,
    uint32_t apic_id,
    uint32_t flags,
    uint8_t x2apic)
{
    if (flags & ~UINT32_C(0x3))
        return 0;
    if (info->processor_count >= VGK_ACPI_MADT_MAX_PROCESSORS)
        return 0;

    for (uint32_t i = 0; i < info->processor_count; ++i) {
        if (info->processors[i].processor_uid == uid ||
            info->processors[i].apic_id == apic_id)
            return 0;
    }

    {
        VGK_ACPI_MADT_PROCESSOR *processor =
            &info->processors[info->processor_count++];
        processor->processor_uid = uid;
        processor->apic_id = apic_id;
        processor->flags = flags;
        processor->x2apic = x2apic;
    }
    return 1;
}

static int madt_add_ioapic(
    VGK_ACPI_MADT_INFO *info,
    uint8_t id,
    uint32_t address,
    uint32_t gsi_base,
    uint8_t physical_address_bits)
{
    if (info->ioapic_count >= VGK_ACPI_MADT_MAX_IOAPICS ||
        (address & UINT32_C(0xFFF)) != 0U ||
        !physical_range_fits(address, UINT64_C(0x20),
                             physical_address_bits))
        return 0;

    for (uint32_t i = 0; i < info->ioapic_count; ++i) {
        if (info->ioapics[i].id == id ||
            info->ioapics[i].address == address ||
            info->ioapics[i].gsi_base == gsi_base)
            return 0;
    }

    {
        VGK_ACPI_MADT_IOAPIC *ioapic =
            &info->ioapics[info->ioapic_count++];
        ioapic->id = id;
        ioapic->address = address;
        ioapic->gsi_base = gsi_base;
    }
    return 1;
}

static int madt_add_nmi_source(
    VGK_ACPI_MADT_INFO *info,
    uint16_t flags,
    uint32_t gsi)
{
    if (info->nmi_source_count >= VGK_ACPI_MADT_MAX_NMI_SOURCES ||
        !madt_interrupt_flags_are_valid(flags))
        return 0;

    for (uint32_t i = 0; i < info->nmi_source_count; ++i) {
        if (info->nmi_sources[i].gsi == gsi)
            return 0;
    }

    info->nmi_sources[info->nmi_source_count].flags = flags;
    info->nmi_sources[info->nmi_source_count].gsi = gsi;
    ++info->nmi_source_count;
    return 1;
}

static int madt_add_local_nmi(
    VGK_ACPI_MADT_INFO *info,
    uint32_t processor_uid,
    uint16_t flags,
    uint8_t lint,
    uint8_t x2apic)
{
    if (info->local_nmi_count >= VGK_ACPI_MADT_MAX_LOCAL_NMIS || lint > 1U ||
        !madt_interrupt_flags_are_valid(flags))
        return 0;

    /* ACPI uses 0xFF/0xFFFFFFFF to mean all processors. */
    if ((x2apic == 0U && processor_uid == UINT32_C(0xFF)) ||
        (x2apic != 0U && processor_uid == UINT32_MAX))
        processor_uid = UINT32_MAX;

    for (uint32_t i = 0; i < info->local_nmi_count; ++i) {
        if (info->local_nmis[i].processor_uid == processor_uid &&
            info->local_nmis[i].lint == lint)
            return 0;
    }

    {
        VGK_ACPI_MADT_LOCAL_NMI *nmi =
            &info->local_nmis[info->local_nmi_count++];
        nmi->processor_uid = processor_uid;
        nmi->flags = flags;
        nmi->lint = lint;
        nmi->x2apic = x2apic;
    }
    return 1;
}

static int madt_add_override(
    VGK_ACPI_MADT_INFO *info,
    uint8_t bus,
    uint8_t source,
    uint32_t gsi,
    uint16_t flags)
{
    if (info->override_count >= VGK_ACPI_MADT_MAX_OVERRIDES ||
        !madt_interrupt_flags_are_valid(flags))
        return 0;

    for (uint32_t i = 0; i < info->override_count; ++i) {
        if (info->overrides[i].bus == bus &&
            info->overrides[i].source == source)
            return 0;
    }

    {
        VGK_ACPI_MADT_OVERRIDE *override =
            &info->overrides[info->override_count++];
        override->bus = bus;
        override->source = source;
        override->gsi = gsi;
        override->flags = flags;
    }
    return 1;
}

VGK_ACPI_STATUS vgk_acpi_madt_parse(
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    uint8_t physical_address_bits,
    VGK_ACPI_PHYSICAL_READ read_physical,
    void *read_context,
    const VGK_ACPI_TABLE_REF *madt,
    VGK_ACPI_MADT_INFO *info_out)
{
    VGK_ACPI_MADT_INFO parsed;
    uint8_t header[VGK_ACPI_SDT_HEADER_SIZE];
    uint8_t fixed_fields[8];
    uint32_t table_length = 0U;
    uint32_t offset;
    int local_apic_override_seen = 0;
    VGK_ACPI_STATUS status;

    if (info_out != 0)
        acpi_zero_bytes(info_out, sizeof(*info_out));
    acpi_zero_bytes(&parsed, sizeof(parsed));

    if (memory_map == 0 || memory_map_count == 0U ||
        read_physical == 0 || madt == 0 || info_out == 0 ||
        physical_address_bits < 32U || physical_address_bits > 63U ||
        madt->physical_address == 0U || madt->length < 44U ||
        madt->reserved[0] != 0U ||
        madt->reserved[1] != 0U ||
        madt->reserved[2] != 0U)
        return VGK_ACPI_INVALID_ARGUMENT;

    status = read_sdt_header(
        memory_map, memory_map_count, physical_address_bits,
        read_physical, read_context, madt->physical_address,
        header, &table_length);
    if (status != VGK_ACPI_OK)
        return status;
    if (!signature_matches(header, "APIC") ||
        table_length != madt->length ||
        header[8U] != madt->revision)
        return VGK_ACPI_BAD_TABLE;

    status = checksum_sdt(
        read_physical, read_context, madt->physical_address, table_length);
    if (status != VGK_ACPI_OK)
        return status;

    if (madt->physical_address > UINT64_MAX - VGK_ACPI_SDT_HEADER_SIZE ||
        !read_physical(read_context,
                       madt->physical_address + VGK_ACPI_SDT_HEADER_SIZE,
                       fixed_fields, sizeof(fixed_fields)))
        return VGK_ACPI_READ_FAILED;

    parsed.local_apic_address = read_le32(fixed_fields);
    parsed.flags = read_le32(fixed_fields + 4U);
    if ((parsed.flags & ~UINT32_C(1)) != 0U)
        return VGK_ACPI_BAD_TABLE;

    offset = 44U;
    while (offset < table_length) {
        uint8_t entry_header[2];
        uint8_t entry[255];
        uint8_t entry_type;
        uint8_t entry_length;
        uint64_t entry_address;

        if (table_length - offset < sizeof(entry_header) ||
            madt->physical_address > UINT64_MAX - offset)
            return VGK_ACPI_BAD_TABLE;

        entry_address = madt->physical_address + offset;
        if (!read_physical(read_context, entry_address,
                           entry_header, sizeof(entry_header)))
            return VGK_ACPI_READ_FAILED;

        entry_type = entry_header[0];
        entry_length = entry_header[1];
        if (entry_length < 2U || entry_length > table_length - offset)
            return VGK_ACPI_BAD_TABLE;
        if (!read_physical(read_context, entry_address, entry, entry_length))
            return VGK_ACPI_READ_FAILED;

        switch (entry_type) {
        case 0U: /* Processor Local APIC */
            if (entry_length != 8U ||
                !madt_add_processor(&parsed, entry[2], entry[3],
                                    read_le32(entry + 4U), 0U))
                return VGK_ACPI_BAD_TABLE;
            break;

        case 1U: /* I/O APIC */
            if (entry_length != 12U || entry[3] != 0U ||
                !madt_add_ioapic(&parsed, entry[2],
                                 read_le32(entry + 4U),
                                 read_le32(entry + 8U),
                                 physical_address_bits))
                return VGK_ACPI_BAD_TABLE;
            break;

        case 2U: /* Interrupt Source Override */
            if (entry_length != 10U ||
                !madt_add_override(&parsed, entry[2], entry[3],
                                   read_le32(entry + 4U),
                                   read_le16(entry + 8U)))
                return VGK_ACPI_BAD_TABLE;
            break;

        case 3U: /* NMI Source */
            if (entry_length != 8U ||
                !madt_add_nmi_source(&parsed,
                                     read_le16(entry + 2U),
                                     read_le32(entry + 4U)))
                return VGK_ACPI_BAD_TABLE;
            break;

        case 4U: /* Local APIC NMI */
            if (entry_length != 6U ||
                !madt_add_local_nmi(&parsed, entry[2],
                                    read_le16(entry + 3U), entry[5], 0U))
                return VGK_ACPI_BAD_TABLE;
            break;

        case 5U: /* Local APIC Address Override */
            if (entry_length != 12U ||
                entry[2] != 0U || entry[3] != 0U ||
                local_apic_override_seen ||
                !physical_range_fits(read_le64(entry + 4U),
                                     VGK_BOOT_PAGE_SIZE,
                                     physical_address_bits))
                return VGK_ACPI_BAD_TABLE;
            parsed.local_apic_address = read_le64(entry + 4U);
            local_apic_override_seen = 1;
            break;

        case 9U: /* Processor Local x2APIC */
            if (entry_length != 16U ||
                entry[2] != 0U || entry[3] != 0U ||
                !madt_add_processor(&parsed,
                                    read_le32(entry + 12U),
                                    read_le32(entry + 4U),
                                    read_le32(entry + 8U), 1U))
                return VGK_ACPI_BAD_TABLE;
            break;

        case 10U: /* Local x2APIC NMI */
            if (entry_length != 12U ||
                entry[2] != 0U || entry[3] != 0U || entry[11] != 0U ||
                !madt_add_local_nmi(&parsed,
                                    read_le32(entry + 6U),
                                    read_le16(entry + 4U),
                                    entry[10], 1U))
                return VGK_ACPI_BAD_TABLE;
            break;

        default:
            /* Unknown records are skipped after their generic bounds check. */
            break;
        }

        offset += entry_length;
    }

    if (parsed.local_apic_address == 0U ||
        (parsed.local_apic_address & UINT64_C(0xFFF)) != 0U ||
        !physical_range_fits(parsed.local_apic_address,
                             VGK_BOOT_PAGE_SIZE,
                             physical_address_bits))
        return VGK_ACPI_BAD_TABLE;

    acpi_copy_bytes(info_out, &parsed, sizeof(parsed));
    return VGK_ACPI_OK;
}
