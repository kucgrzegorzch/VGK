#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "vgk_interrupts.h"

static uint64_t gate_handler(const VGK_IDT_GATE *gate)
{
    return (uint64_t)gate->offset_low |
           ((uint64_t)gate->offset_middle << 16) |
           ((uint64_t)gate->offset_high << 32);
}

int main(void)
{
    VGK_IDT_GATE gate;
    const uint64_t address = UINT64_C(0xFFFF8123456789AB);

    memset(&gate, 0xA5, sizeof(gate));
    assert(vgk_idt_gate_build(&gate, address, 0x08U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_OK);
    assert(gate_handler(&gate) == address);
    assert(gate.selector == 0x08U);
    assert(gate.ist == 0U);
    assert(gate.attributes == 0x8EU);
    assert(gate.reserved == 0U);

    assert(vgk_idt_gate_build(&gate, address, 0x08U, 3U, 3U,
                              VGK_IDT_GATE_TRAP, 1U) == VGK_INTERRUPT_OK);
    assert(gate.ist == 3U);
    assert(gate.attributes == 0xEFU);

    assert(vgk_idt_gate_build(&gate, address, 0x08U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 0U) == VGK_INTERRUPT_OK);
    assert(gate.attributes == 0x0EU);

    assert(vgk_idt_gate_build(0, address, 0x08U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) ==
           VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, 0U, 0x08U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) ==
           VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) ==
           VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0x08U, 8U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) ==
           VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0x08U, 0U, 4U,
                              VGK_IDT_GATE_INTERRUPT, 1U) ==
           VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0x08U, 0U, 0U,
                              0x0CU, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0x08U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 2U) ==
           VGK_INTERRUPT_INVALID_ARGUMENT);

    puts("VGK IDT gate tests: OK");
    return 0;
}
