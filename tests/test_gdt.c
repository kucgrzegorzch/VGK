#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "vgk_gdt.h"

static uint64_t tss_descriptor_base(const uint64_t *gdt)
{
    const uint64_t low = gdt[3];
    return ((low >> 16) & UINT64_C(0xFFFFFF)) |
           (((low >> 56) & UINT64_C(0xFF)) << 24) |
           (gdt[4] << 32);
}

static uint32_t tss_descriptor_limit(const uint64_t *gdt)
{
    return (uint32_t)((gdt[3] & UINT64_C(0xFFFF)) |
                      (((gdt[3] >> 48) & UINT64_C(0xF)) << 16));
}

int main(void)
{
    VGK_TSS64 tss __attribute__((aligned(16)));
    uint64_t gdt[VGK_GDT_ENTRY_COUNT] __attribute__((aligned(16)));
    const uint64_t ist[7] = {
        UINT64_C(0xFFFF800000010000),
        UINT64_C(0xFFFF800000012000),
        UINT64_C(0xFFFF800000014000),
        0U, 0U, 0U, 0U
    };

    memset(&tss, 0xA5, sizeof(tss));
    memset(gdt, 0xA5, sizeof(gdt));

    assert(vgk_tss64_init(&tss, 0U, ist) == VGK_GDT_OK);
    assert(tss.rsp[0] == 0U);
    assert(tss.rsp[1] == 0U && tss.rsp[2] == 0U);
    assert(tss.ist[0] == ist[0]);
    assert(tss.ist[1] == ist[1]);
    assert(tss.ist[2] == ist[2]);
    for (size_t i = 3; i < 7; ++i)
        assert(tss.ist[i] == 0U);
    assert(tss.iomap_base == sizeof(tss));
    assert(tss.reserved0 == 0U && tss.reserved1 == 0U);
    assert(tss.reserved2 == 0U && tss.reserved3 == 0U);

    assert(vgk_gdt_build(gdt, &tss) == VGK_GDT_OK);
    assert(gdt[0] == 0U);
    assert(gdt[1] == UINT64_C(0x00AF9A000000FFFF));
    assert(gdt[2] == UINT64_C(0x00CF92000000FFFF));
    assert(tss_descriptor_base(gdt) == (uint64_t)(uintptr_t)&tss);
    assert((gdt[4] >> 32) == 0U);
    assert(tss_descriptor_limit(gdt) == VGK_TSS64_SIZE - 1U);
    assert(((gdt[3] >> 40) & 0xFFU) == 0x89U);
    assert((gdt[3] & (UINT64_C(1) << 47)) != 0U);

    assert(vgk_tss64_init(0, 0U, ist) == VGK_GDT_INVALID_ARGUMENT);
    assert(vgk_gdt_build(0, &tss) == VGK_GDT_INVALID_ARGUMENT);
    assert(vgk_gdt_build(gdt, 0) == VGK_GDT_INVALID_ARGUMENT);

    {
        const uint64_t bad_alignment[7] = {
            UINT64_C(0xFFFF800000010008), 0, 0, 0, 0, 0, 0
        };
        assert(vgk_tss64_init(&tss, 0U, bad_alignment) ==
               VGK_GDT_INVALID_ARGUMENT);
    }
    {
        const uint64_t noncanonical[7] = {
            UINT64_C(0x0000800000000000), 0, 0, 0, 0, 0, 0
        };
        assert(vgk_tss64_init(&tss, 0U, noncanonical) ==
               VGK_GDT_INVALID_ARGUMENT);
    }
    {
        const uint64_t bad_rsp0 = UINT64_C(0xFFFF800000010008);
        assert(vgk_tss64_init(&tss, bad_rsp0, ist) ==
               VGK_GDT_INVALID_ARGUMENT);
    }

    /* Reject non-canonical objects before dereferencing them. */
    assert(vgk_tss64_init(
               (VGK_TSS64 *)(uintptr_t)UINT64_C(0x0000800000000000),
               0U, ist) == VGK_GDT_INVALID_ARGUMENT);
    assert(vgk_tss64_init(
               (VGK_TSS64 *)(uintptr_t)UINT64_C(0x00007FFFFFFFFFF0),
               0U, ist) == VGK_GDT_INVALID_ARGUMENT);
    assert(vgk_tss64_init(
               &tss, 0U,
               (const uint64_t *)(uintptr_t)UINT64_C(0x00007FFFFFFFFFF0)) ==
           VGK_GDT_INVALID_ARGUMENT);

    /* Both the GDT storage and the full 104-byte TSS must fit canonically. */
    assert(vgk_gdt_build(
               gdt,
               (const VGK_TSS64 *)(uintptr_t)UINT64_C(0x00007FFFFFFFFFF0)) ==
           VGK_GDT_INVALID_ARGUMENT);
    assert(vgk_gdt_build(
               (uint64_t *)(uintptr_t)UINT64_C(0x00007FFFFFFFFFF0),
               &tss) == VGK_GDT_INVALID_ARGUMENT);

    /* Invalid TSS inputs must not partially overwrite the destination. */
    {
        uint8_t before[sizeof(tss)];
        uint64_t bad_last_ist[7] = {
            ist[0], ist[1], ist[2], 0U, 0U, 0U,
            UINT64_C(0x0000800000000000)
        };
        memset(&tss, 0x6B, sizeof(tss));
        memcpy(before, &tss, sizeof(tss));
        assert(vgk_tss64_init(&tss, 0U, bad_last_ist) ==
               VGK_GDT_INVALID_ARGUMENT);
        assert(memcmp(before, &tss, sizeof(tss)) == 0);
    }
    {
        uint8_t raw_tss[sizeof(VGK_TSS64) + 16U] __attribute__((aligned(16)));
        uint8_t before[sizeof(tss)];
        VGK_TSS64 *misaligned = (VGK_TSS64 *)(uintptr_t)
            ((uintptr_t)raw_tss + 1U);
        memset(&tss, 0x39, sizeof(tss));
        memcpy(before, &tss, sizeof(tss));
        assert(vgk_tss64_init(misaligned, 0U, ist) ==
               VGK_GDT_INVALID_ARGUMENT);
        assert(memcmp(before, &tss, sizeof(tss)) == 0);
    }
    {
        uint8_t raw_tss[sizeof(VGK_TSS64) + 16U] __attribute__((aligned(16)));
        uint64_t before[VGK_GDT_ENTRY_COUNT];
        VGK_TSS64 *misaligned = (VGK_TSS64 *)(uintptr_t)
            ((uintptr_t)raw_tss + 1U);
        memset(gdt, 0xC7, sizeof(gdt));
        memcpy(before, gdt, sizeof(gdt));
        assert(vgk_gdt_build(gdt, misaligned) == VGK_GDT_INVALID_ARGUMENT);
        assert(memcmp(before, gdt, sizeof(gdt)) == 0);
    }


    puts("VGK GDT/TSS descriptor tests: OK");
    return 0;
}
