#include "vgk_gdt.h"

#include <stddef.h>
#include <stdint.h>

static int is_canonical_48(uint64_t address)
{
    const uint64_t upper = address >> 48;
    const uint64_t sign = (address >> 47) & UINT64_C(1);
    return sign != 0U ? upper == UINT64_C(0xFFFF) : upper == 0U;
}

VGK_GDT_STATUS vgk_tss64_init(
    VGK_TSS64 *tss,
    uint64_t rsp0,
    const uint64_t ist_tops[7])
{
    if (tss == 0 || ist_tops == 0 ||
        ((uintptr_t)tss & (uintptr_t)0xFU) != 0U)
        return VGK_GDT_INVALID_ARGUMENT;

    if (rsp0 != 0U &&
        (((rsp0 & UINT64_C(0xF)) != 0U) || !is_canonical_48(rsp0)))
        return VGK_GDT_INVALID_ARGUMENT;

    for (size_t i = 0; i < 7U; ++i) {
        if (ist_tops[i] != 0U &&
            (((ist_tops[i] & UINT64_C(0xF)) != 0U) ||
             !is_canonical_48(ist_tops[i])))
            return VGK_GDT_INVALID_ARGUMENT;
    }

    tss->reserved0 = 0U;
    tss->rsp[0] = rsp0;
    tss->rsp[1] = 0U;
    tss->rsp[2] = 0U;
    tss->reserved1 = 0U;
    for (size_t i = 0; i < 7U; ++i)
        tss->ist[i] = ist_tops[i];
    tss->reserved2 = 0U;
    tss->reserved3 = 0U;
    tss->iomap_base = (uint16_t)sizeof(*tss);

    return VGK_GDT_OK;
}

VGK_GDT_STATUS vgk_gdt_build(
    uint64_t gdt[VGK_GDT_ENTRY_COUNT],
    const VGK_TSS64 *tss)
{
    uint64_t base;
    const uint64_t limit = sizeof(*tss) - 1U;
    uint64_t low;
    uint64_t high;

    if (gdt == 0 || tss == 0 ||
        ((uintptr_t)tss & (uintptr_t)0xFU) != 0U)
        return VGK_GDT_INVALID_ARGUMENT;

    base = (uint64_t)(uintptr_t)tss;

    /* Access=0x89: present, DPL0, available 64-bit TSS. */
    low = (limit & UINT64_C(0xFFFF)) |
          ((base & UINT64_C(0xFFFFFF)) << 16) |
          (UINT64_C(0x89) << 40) |
          (((limit >> 16) & UINT64_C(0xF)) << 48) |
          (((base >> 24) & UINT64_C(0xFF)) << 56);
    high = base >> 32;

    gdt[0] = 0U;
    gdt[1] = UINT64_C(0x00AF9A000000FFFF); /* 64-bit ring-0 code */
    gdt[2] = UINT64_C(0x00CF92000000FFFF); /* ring-0 data */
    gdt[3] = low;
    gdt[4] = high;

    return VGK_GDT_OK;
}
