#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "vgk_gdt.h"
#include "vgk_interrupts.h"
#include "vgk_ist_stacks.h"

static uint64_t idt_handler(const VGK_IDT_GATE *gate)
{
    return (uint64_t)gate->offset_low |
           ((uint64_t)gate->offset_middle << 16) |
           ((uint64_t)gate->offset_high << 32);
}

static uint64_t tss_base(const uint64_t gdt[VGK_GDT_ENTRY_COUNT])
{
    return ((gdt[3] >> 16) & UINT64_C(0xFFFFFF)) |
           (((gdt[3] >> 56) & UINT64_C(0xFF)) << 24) |
           ((gdt[4] & UINT64_C(0xFFFFFFFF)) << 32);
}

int main(void)
{
    VGK_TSS64 tss __attribute__((aligned(16)));
    uint64_t gdt[VGK_GDT_ENTRY_COUNT] __attribute__((aligned(16)));
    VGK_IDT_GATE idt[VGK_IDT_EXCEPTION_COUNT];
    uint64_t ist_tops[7];
    uint64_t handlers[VGK_IDT_EXCEPTION_COUNT];

    assert(vgk_ist_stacks_get_tops(ist_tops) == VGK_IST_STACKS_OK);
    assert(vgk_tss64_init(&tss, 0U, ist_tops) == VGK_GDT_OK);
    assert(vgk_gdt_build(gdt, &tss) == VGK_GDT_OK);

    for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i)
        handlers[i] = UINT64_C(0xFFFF800000200000) +
                      ((uint64_t)i * UINT64_C(0x100));

    assert(vgk_idt_build_exception_table_with_ist(
               idt, handlers, VGK_GDT_SELECTOR_CODE) == VGK_INTERRUPT_OK);

    /* GDT code selector and every IDT gate selector must agree. */
    assert(VGK_GDT_SELECTOR_CODE == 0x08U);
    assert(gdt[1] == UINT64_C(0x00AF9A000000FFFF));
    for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i) {
        const uint8_t expected_ist =
            i == 8U ? VGK_IDT_IST_DOUBLE_FAULT :
            i == 2U ? VGK_IDT_IST_NMI :
            i == 18U ? VGK_IDT_IST_MACHINE_CHECK : 0U;
        assert(idt[i].selector == VGK_GDT_SELECTOR_CODE);
        assert(idt[i].ist == expected_ist);
        assert(idt[i].attributes == 0x8EU);
        assert(idt[i].reserved == 0U);
        assert(idt_handler(&idt[i]) == handlers[i]);
    }

    /* Each IDT IST selector must resolve to the corresponding TSS.IST slot. */
    assert(tss.ist[0] == ist_tops[0]);
    assert(tss.ist[1] == ist_tops[1]);
    assert(tss.ist[2] == ist_tops[2]);
    for (uint32_t i = 3U; i < 7U; ++i)
        assert(tss.ist[i] == 0U);
    assert(idt[8U].ist == 1U && tss.ist[idt[8U].ist - 1U] == ist_tops[0]);
    assert(idt[2U].ist == 2U && tss.ist[idt[2U].ist - 1U] == ist_tops[1]);
    assert(idt[18U].ist == 3U && tss.ist[idt[18U].ist - 1U] == ist_tops[2]);

    /* The GDT TSS descriptor must point at this initialized TSS object. */
    assert(tss_base(gdt) == (uint64_t)(uintptr_t)&tss);
    assert(((gdt[3] >> 40) & 0xFFU) == 0x89U);
    assert((gdt[4] >> 32) == 0U);

    puts("VGK GDT/TSS/IST/IDT cooperation tests: OK");
    return 0;
}
