#include "vgk_interrupts.h"

VGK_INTERRUPT_STATUS vgk_idt_gate_build(
    VGK_IDT_GATE *gate,
    uint64_t handler,
    uint16_t selector,
    uint8_t ist,
    uint8_t dpl,
    uint8_t gate_type,
    uint8_t present)
{
    if (gate == 0 || handler == 0 || selector == 0 ||
        ist > 7U || dpl > 3U ||
        (gate_type != VGK_IDT_GATE_INTERRUPT &&
         gate_type != VGK_IDT_GATE_TRAP) ||
        present > 1U)
        return VGK_INTERRUPT_INVALID_ARGUMENT;

    gate->offset_low = (uint16_t)(handler & UINT64_C(0xFFFF));
    gate->selector = selector;
    gate->ist = ist;
    gate->attributes = (uint8_t)((present << 7) | (dpl << 5) | gate_type);
    gate->offset_middle = (uint16_t)((handler >> 16) & UINT64_C(0xFFFF));
    gate->offset_high = (uint32_t)(handler >> 32);
    gate->reserved = 0;

    return VGK_INTERRUPT_OK;
}

VGK_INTERRUPT_STATUS vgk_idt_build_exception_table(
    VGK_IDT_GATE table[VGK_IDT_EXCEPTION_COUNT],
    const uint64_t handlers[VGK_IDT_EXCEPTION_COUNT],
    uint16_t code_selector)
{
    if (table == 0 || handlers == 0 || code_selector == 0)
        return VGK_INTERRUPT_INVALID_ARGUMENT;

    /* Najpierw sprawdzamy caly zestaw, aby blad nie zostawil pol-tablicy. */
    for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i) {
        if (handlers[i] == 0)
            return VGK_INTERRUPT_INVALID_ARGUMENT;
    }

    for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i) {
        if (vgk_idt_gate_build(&table[i], handlers[i], code_selector,
                               0U, 0U, VGK_IDT_GATE_INTERRUPT, 1U) !=
            VGK_INTERRUPT_OK)
            return VGK_INTERRUPT_INVALID_ARGUMENT;
    }

    return VGK_INTERRUPT_OK;
}

_Static_assert(sizeof(VGK_IDT_GATE) == 16,
               "VGK IDT gate must be exactly 16 bytes");
