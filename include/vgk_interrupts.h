#ifndef VGK_INTERRUPTS_H
#define VGK_INTERRUPTS_H

#include <stdint.h>

#define VGK_IDT_EXCEPTION_COUNT 32U
#define VGK_IDT_IST_DOUBLE_FAULT 1U
#define VGK_IDT_IST_NMI           2U
#define VGK_IDT_IST_MACHINE_CHECK 3U

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} VGK_IDT_GATE;

typedef enum {
    VGK_INTERRUPT_OK = 0,
    VGK_INTERRUPT_INVALID_ARGUMENT = 1
} VGK_INTERRUPT_STATUS;

#define VGK_IDT_GATE_INTERRUPT 0x0EU
#define VGK_IDT_GATE_TRAP      0x0FU

VGK_INTERRUPT_STATUS vgk_idt_gate_build(
    VGK_IDT_GATE *gate,
    uint64_t handler,
    uint16_t selector,
    uint8_t ist,
    uint8_t dpl,
    uint8_t gate_type,
    uint8_t present);

/* Builds a neutral table: all exception gates use IST=0. */
VGK_INTERRUPT_STATUS vgk_idt_build_exception_table(
    VGK_IDT_GATE table[VGK_IDT_EXCEPTION_COUNT],
    const uint64_t handlers[VGK_IDT_EXCEPTION_COUNT],
    uint16_t code_selector);

/*
 * Builds the VGK emergency-stack policy:
 * #DF (8)->IST1, #NMI (2)->IST2, #MC (18)->IST3.
 * The caller MUST initialize matching TSS.IST entries before LIDT.
 * This function only prepares descriptors; it allocates no stacks and
 * executes no privileged instruction.
 */
VGK_INTERRUPT_STATUS vgk_idt_build_exception_table_with_ist(
    VGK_IDT_GATE table[VGK_IDT_EXCEPTION_COUNT],
    const uint64_t handlers[VGK_IDT_EXCEPTION_COUNT],
    uint16_t code_selector);

_Static_assert(sizeof(VGK_IDT_GATE) == 16,
               "VGK IDT gate must be exactly 16 bytes");

#endif
