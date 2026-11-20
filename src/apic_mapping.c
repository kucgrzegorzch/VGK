#include "vgk_apic_mapping.h"

static void apic_zero_bytes(void *destination, size_t length)
{
    uint8_t *bytes = (uint8_t *)destination;
    for (size_t i = 0; i < length; ++i)
        bytes[i] = 0U;
}

static void apic_copy_bytes(
    void *destination,
    const void *source,
    size_t length)
{
    uint8_t *out = (uint8_t *)destination;
    const uint8_t *in = (const uint8_t *)source;
    for (size_t i = 0; i < length; ++i)
        out[i] = in[i];
}

VGK_APIC_MAPPING_RESULT vgk_apic_prepare_mmio_mappings(
    const VGK_CPU_FEATURES *cpu,
    const VGK_ACPI_MADT_INFO *madt,
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    VGK_PAGE_TABLES *inactive_tables,
    VGK_ACPI_MADT_MMIO_RANGE *ranges_out,
    size_t range_capacity,
    size_t *range_count_out)
{
    VGK_ACPI_MADT_MMIO_RANGE planned[VGK_ACPI_MADT_MAX_MMIO_RANGES];
    size_t planned_count = 0U;
    VGK_ACPI_STATUS acpi_status;
    uint64_t physical_limit;
    uint64_t expected_physical_mask;
    uint64_t pool_address;
    size_t pool_bytes;

    if (range_count_out != 0)
        *range_count_out = 0U;
    if (ranges_out != 0 && range_capacity != 0U) {
        size_t clear_count = range_capacity;
        if (clear_count > VGK_ACPI_MADT_MAX_MMIO_RANGES)
            clear_count = VGK_ACPI_MADT_MAX_MMIO_RANGES;
        apic_zero_bytes(ranges_out, clear_count * sizeof(*ranges_out));
    }
    apic_zero_bytes(planned, sizeof(planned));

    if (cpu == 0 || madt == 0 || memory_map == 0 ||
        memory_map_count == 0U || inactive_tables == 0 ||
        ranges_out == 0 || range_count_out == 0)
        return VGK_APIC_MAPPING_INVALID_ARGUMENT;

    if (cpu->local_apic_supported > 1U ||
        cpu->apic_base_valid > 1U ||
        cpu->local_apic_enabled > 1U ||
        cpu->x2apic_supported > 1U ||
        cpu->x2apic_enabled > 1U ||
        cpu->mmio_uc_supported > 1U ||
        cpu->physical_address_bits < 32U ||
        cpu->physical_address_bits > 52U ||
        cpu->nx_supported != 1U ||
        cpu->reserved[0] != 0U || cpu->reserved[1] != 0U ||
        cpu->local_apic_supported == 0U ||
        cpu->apic_base_valid == 0U ||
        cpu->mmio_uc_supported == 0U ||
        (cpu->x2apic_enabled != 0U &&
         (cpu->x2apic_supported == 0U ||
          cpu->local_apic_enabled == 0U)))
        return VGK_APIC_MAPPING_UNSUPPORTED;

    physical_limit = (UINT64_C(1) << cpu->physical_address_bits) - 1U;
    expected_physical_mask =
        physical_limit & ~(VGK_BOOT_PAGE_SIZE - 1U);
    if (cpu->local_apic_base == 0U ||
        cpu->local_apic_base > physical_limit -
            (VGK_BOOT_PAGE_SIZE - 1U))
        return VGK_APIC_MAPPING_UNSUPPORTED;

    if (inactive_tables->pool == 0 ||
        inactive_tables->pool_pages == 0U ||
        inactive_tables->pool_pages >
            SIZE_MAX / (size_t)VGK_BOOT_PAGE_SIZE ||
        inactive_tables->used_pages == 0U ||
        inactive_tables->used_pages > inactive_tables->pool_pages ||
        ((uintptr_t)inactive_tables->pool &
         (uintptr_t)(VGK_BOOT_PAGE_SIZE - 1U)) != 0U ||
        inactive_tables->physical_bits != cpu->physical_address_bits ||
        inactive_tables->physical_mask != expected_physical_mask ||
        inactive_tables->nx_supported != 1U)
        return VGK_APIC_MAPPING_INVALID_ARGUMENT;

    pool_address = (uint64_t)(uintptr_t)inactive_tables->pool;
    pool_bytes = inactive_tables->pool_pages *
        (size_t)VGK_BOOT_PAGE_SIZE;
    if (pool_address > physical_limit ||
        (uint64_t)(pool_bytes - 1U) > physical_limit - pool_address ||
        (uintptr_t)(pool_bytes - 1U) >
            UINTPTR_MAX - (uintptr_t)inactive_tables->pool)
        return VGK_APIC_MAPPING_INVALID_ARGUMENT;

    acpi_status = vgk_acpi_madt_build_mmio_plan(
        madt, cpu->local_apic_base, 1U, cpu->x2apic_enabled,
        planned, VGK_ACPI_MADT_MAX_MMIO_RANGES, &planned_count);
    if (acpi_status != VGK_ACPI_OK)
        return acpi_status == VGK_ACPI_INVALID_ARGUMENT
            ? VGK_APIC_MAPPING_INVALID_ARGUMENT
            : VGK_APIC_MAPPING_BAD_FIRMWARE_DATA;

    acpi_status = vgk_acpi_madt_validate_mmio_plan_memory_map(
        memory_map, memory_map_count, planned, planned_count);
    if (acpi_status != VGK_ACPI_OK)
        return acpi_status == VGK_ACPI_INVALID_ARGUMENT
            ? VGK_APIC_MAPPING_INVALID_ARGUMENT
            : VGK_APIC_MAPPING_BAD_FIRMWARE_DATA;

    if (range_capacity < planned_count)
        return VGK_APIC_MAPPING_INVALID_ARGUMENT;

    for (size_t i = 0; i < planned_count; ++i) {
        VGK_PAGING_RESULT paging_status = vgk_paging_map_mmio_uc(
            inactive_tables,
            planned[i].physical_base,
            planned[i].physical_base,
            planned[i].length);
        if (paging_status != VGK_PAGING_OK)
            return VGK_APIC_MAPPING_PAGING_FAILED;
    }

    apic_copy_bytes(ranges_out, planned, planned_count * sizeof(*ranges_out));
    *range_count_out = planned_count;
    return VGK_APIC_MAPPING_OK;
}
