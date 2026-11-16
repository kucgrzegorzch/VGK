#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "vgk_gdt.h"
#include "vgk_interrupts.h"
#include "vgk_ist_stacks.h"

extern const uintptr_t vgk_exception_stub_table[VGK_IDT_EXCEPTION_COUNT];
#define VGK_DECLARE_EXCEPTION_STUB(n) extern void vgk_exception_stub_##n(void)
VGK_DECLARE_EXCEPTION_STUB(0);
VGK_DECLARE_EXCEPTION_STUB(1);
VGK_DECLARE_EXCEPTION_STUB(2);
VGK_DECLARE_EXCEPTION_STUB(3);
VGK_DECLARE_EXCEPTION_STUB(4);
VGK_DECLARE_EXCEPTION_STUB(5);
VGK_DECLARE_EXCEPTION_STUB(6);
VGK_DECLARE_EXCEPTION_STUB(7);
VGK_DECLARE_EXCEPTION_STUB(8);
VGK_DECLARE_EXCEPTION_STUB(9);
VGK_DECLARE_EXCEPTION_STUB(10);
VGK_DECLARE_EXCEPTION_STUB(11);
VGK_DECLARE_EXCEPTION_STUB(12);
VGK_DECLARE_EXCEPTION_STUB(13);
VGK_DECLARE_EXCEPTION_STUB(14);
VGK_DECLARE_EXCEPTION_STUB(15);
VGK_DECLARE_EXCEPTION_STUB(16);
VGK_DECLARE_EXCEPTION_STUB(17);
VGK_DECLARE_EXCEPTION_STUB(18);
VGK_DECLARE_EXCEPTION_STUB(19);
VGK_DECLARE_EXCEPTION_STUB(20);
VGK_DECLARE_EXCEPTION_STUB(21);
VGK_DECLARE_EXCEPTION_STUB(22);
VGK_DECLARE_EXCEPTION_STUB(23);
VGK_DECLARE_EXCEPTION_STUB(24);
VGK_DECLARE_EXCEPTION_STUB(25);
VGK_DECLARE_EXCEPTION_STUB(26);
VGK_DECLARE_EXCEPTION_STUB(27);
VGK_DECLARE_EXCEPTION_STUB(28);
VGK_DECLARE_EXCEPTION_STUB(29);
VGK_DECLARE_EXCEPTION_STUB(30);
VGK_DECLARE_EXCEPTION_STUB(31);
#undef VGK_DECLARE_EXCEPTION_STUB

typedef void (*VGK_EXCEPTION_STUB)(void);
#define VGK_EXCEPTION_STUB_ADDRESS(n) vgk_exception_stub_##n
static VGK_EXCEPTION_STUB const expected_stub_addresses[VGK_IDT_EXCEPTION_COUNT] = {
    VGK_EXCEPTION_STUB_ADDRESS(0), VGK_EXCEPTION_STUB_ADDRESS(1),
    VGK_EXCEPTION_STUB_ADDRESS(2), VGK_EXCEPTION_STUB_ADDRESS(3),
    VGK_EXCEPTION_STUB_ADDRESS(4), VGK_EXCEPTION_STUB_ADDRESS(5),
    VGK_EXCEPTION_STUB_ADDRESS(6), VGK_EXCEPTION_STUB_ADDRESS(7),
    VGK_EXCEPTION_STUB_ADDRESS(8), VGK_EXCEPTION_STUB_ADDRESS(9),
    VGK_EXCEPTION_STUB_ADDRESS(10), VGK_EXCEPTION_STUB_ADDRESS(11),
    VGK_EXCEPTION_STUB_ADDRESS(12), VGK_EXCEPTION_STUB_ADDRESS(13),
    VGK_EXCEPTION_STUB_ADDRESS(14), VGK_EXCEPTION_STUB_ADDRESS(15),
    VGK_EXCEPTION_STUB_ADDRESS(16), VGK_EXCEPTION_STUB_ADDRESS(17),
    VGK_EXCEPTION_STUB_ADDRESS(18), VGK_EXCEPTION_STUB_ADDRESS(19),
    VGK_EXCEPTION_STUB_ADDRESS(20), VGK_EXCEPTION_STUB_ADDRESS(21),
    VGK_EXCEPTION_STUB_ADDRESS(22), VGK_EXCEPTION_STUB_ADDRESS(23),
    VGK_EXCEPTION_STUB_ADDRESS(24), VGK_EXCEPTION_STUB_ADDRESS(25),
    VGK_EXCEPTION_STUB_ADDRESS(26), VGK_EXCEPTION_STUB_ADDRESS(27),
    VGK_EXCEPTION_STUB_ADDRESS(28), VGK_EXCEPTION_STUB_ADDRESS(29),
    VGK_EXCEPTION_STUB_ADDRESS(30), VGK_EXCEPTION_STUB_ADDRESS(31)
};
#undef VGK_EXCEPTION_STUB_ADDRESS

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

    for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i) {
        /* Compare addresses only; exception stubs are never executed here. */
        assert(vgk_exception_stub_table[i] == (uintptr_t)expected_stub_addresses[i]);
        handlers[i] = (uint64_t)vgk_exception_stub_table[i];
        assert(handlers[i] != 0U);
        for (uint32_t j = 0; j < i; ++j)
            assert(handlers[i] != handlers[j]);
    }

    assert(vgk_idt_build_exception_table_with_ist(
               idt, handlers, VGK_GDT_SELECTOR_CODE) == VGK_INTERRUPT_OK);

    /* The IDT must point to the real assembly stubs and use the GDT code selector. */
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
