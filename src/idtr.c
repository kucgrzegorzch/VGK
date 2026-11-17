#include "vgk_idtr.h"

static int is_canonical_48(uint64_t address)
{
    const uint64_t upper = address >> 48;
    const uint64_t sign = (address >> 47) & UINT64_C(1);
    return sign != 0U ? upper == UINT64_C(0xFFFF) : upper == 0U;
}

VGK_IDTR_STATUS vgk_idtr_build(
    VGK_IDTR *descriptor,
    const VGK_IDT_GATE *table,
    uint32_t entry_count)
{
    uint64_t base;
    uint64_t byte_count;
    uint64_t last_byte;

    if (descriptor == 0 || table == 0 ||
        entry_count == 0U || entry_count > VGK_IDT_MAX_ENTRIES)
        return VGK_IDTR_INVALID_ARGUMENT;

    base = (uint64_t)(uintptr_t)table;
    if (base == 0U || !is_canonical_48(base) ||
        (base & UINT64_C(0x0F)) != 0U)
        return VGK_IDTR_INVALID_ARGUMENT;

    byte_count = (uint64_t)entry_count * sizeof(VGK_IDT_GATE);
    if (byte_count == 0U || byte_count > UINT64_C(0x10000))
        return VGK_IDTR_INVALID_ARGUMENT;
    if (base > UINT64_MAX - (byte_count - 1U))
        return VGK_IDTR_INVALID_ARGUMENT;

    last_byte = base + byte_count - 1U;
    if (!is_canonical_48(last_byte))
        return VGK_IDTR_INVALID_ARGUMENT;

    /* Commit only after every input and range check has succeeded. */
    descriptor->limit = (uint16_t)(byte_count - 1U);
    descriptor->base = base;
    return VGK_IDTR_OK;
}
