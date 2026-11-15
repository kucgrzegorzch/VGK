#include "vgk_interrupts.h"

static int is_canonical_48(uint64_t address)
{
    const uint64_t upper = address >> 48;
    const uint64_t sign = (address >> 47) & UINT64_C(1);
    return sign != 0U ? upper == UINT64_C(0xFFFF) : upper == 0U;
}

VGK_INTERRUPT_STATUS vgk_idt_gate_build(
    VGK_IDT_GATE *gate,
    uint64_t handler,
    uint16_t selector,
    uint8_t ist,
    uint8_t dpl,
    uint8_t gate_type,
    uint8_t present)
{
    if (gate == 0 || handler == 0 || !is_canonical_48(handler) || selector == 0 ||
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

    /* Validate all input before modifying any descriptor. */
    for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i) {
        if (handlers[i] == 0 || !is_canonical_48(handlers[i]))
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

VGK_INTERRUPT_STATUS vgk_idt_build_exception_table_with_ist(
    VGK_IDT_GATE table[VGK_IDT_EXCEPTION_COUNT],
    const uint64_t handlers[VGK_IDT_EXCEPTION_COUNT],
    uint16_t code_selector)
{
    VGK_INTERRUPT_STATUS status =
        vgk_idt_build_exception_table(table, handlers, code_selector);

    if (status != VGK_INTERRUPT_OK)
        return status;

    table[8U].ist = VGK_IDT_IST_DOUBLE_FAULT;
    table[2U].ist = VGK_IDT_IST_NMI;
    table[18U].ist = VGK_IDT_IST_MACHINE_CHECK;
    return VGK_INTERRUPT_OK;
}
