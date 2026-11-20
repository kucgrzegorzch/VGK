#ifndef VGK_INTERRUPTS_H
#define VGK_INTERRUPTS_H

#include <stdint.h>

#define VGK_IDT_EXCEPTION_COUNT 32U
#define VGK_IDT_VECTOR_COUNT 256U

/* Read-only address table exported by boot/exception_stubs.S. */
extern const uint64_t vgk_exception_stub_table[VGK_IDT_EXCEPTION_COUNT];
/* Complete address table: exception stubs 0..31 plus fail-stop vectors 32..255. */
extern const uint64_t vgk_vector_stub_table[VGK_IDT_VECTOR_COUNT];
#define VGK_IDT_IST_DOUBLE_FAULT 1U
#define VGK_IDT_IST_NMI           2U
#define VGK_IDT_IST_MACHINE_CHECK 3U
#define VGK_IDT_IST_PAGE_FAULT    4U

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


/*
 * Builds all 256 vector gates from caller-supplied handler addresses.
 * Every address is validated before the destination table is modified.
 * The caller must provide a real, ABI-compatible entry stub for every
 * vector; this routine does not invent fallback or IRQ handlers.
 */
VGK_INTERRUPT_STATUS vgk_idt_build_table(
    VGK_IDT_GATE table[VGK_IDT_VECTOR_COUNT],
    const uint64_t handlers[VGK_IDT_VECTOR_COUNT],
    uint16_t code_selector);

/*
 * Same full-table builder, with the VGK emergency IST assignments applied
 * to vectors 2, 8, 14 and 18. Matching TSS.IST slots must already be valid
 * before this table is ever loaded with LIDT.
 */
VGK_INTERRUPT_STATUS vgk_idt_build_table_with_ist(
    VGK_IDT_GATE table[VGK_IDT_VECTOR_COUNT],
    const uint64_t handlers[VGK_IDT_VECTOR_COUNT],
    uint16_t code_selector);

/* Builds a neutral table: all exception gates use IST=0. */
VGK_INTERRUPT_STATUS vgk_idt_build_exception_table(
    VGK_IDT_GATE table[VGK_IDT_EXCEPTION_COUNT],
    const uint64_t handlers[VGK_IDT_EXCEPTION_COUNT],
    uint16_t code_selector);

/*
 * Builds the VGK emergency-stack policy:
 * #DF (8)->IST1, #NMI (2)->IST2, #MC (18)->IST3, #PF (14)->IST4.
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
