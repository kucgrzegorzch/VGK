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
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, 0U, 0x08U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, UINT64_C(0x0000800000000000), 0x08U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, UINT64_C(0xFFFF000000000000), 0x08U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    /* IDT targets must use a ring-0 selector from the GDT, not LDT/RPL!=0. */
    assert(vgk_idt_gate_build(&gate, address, 0x09U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0x0CU, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0x08U, 8U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0x08U, 0U, 4U,
                              VGK_IDT_GATE_INTERRUPT, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0x08U, 0U, 0U,
                              0x0CU, 1U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    assert(vgk_idt_gate_build(&gate, address, 0x08U, 0U, 0U,
                              VGK_IDT_GATE_INTERRUPT, 2U) == VGK_INTERRUPT_INVALID_ARGUMENT);

    /* A rejected single-gate build must leave every destination byte intact. */
    {
        uint8_t before[sizeof(gate)];
        memset(&gate, 0xD6, sizeof(gate));
        memcpy(before, &gate, sizeof(gate));
        assert(vgk_idt_gate_build(&gate, UINT64_C(0x0000800000000000),
                                  0x08U, 0U, 0U,
                                  VGK_IDT_GATE_INTERRUPT, 1U) ==
               VGK_INTERRUPT_INVALID_ARGUMENT);
        assert(memcmp(before, &gate, sizeof(gate)) == 0);
    }
    {
        uint8_t before[sizeof(gate)];
        memset(&gate, 0xB9, sizeof(gate));
        memcpy(before, &gate, sizeof(gate));
        assert(vgk_idt_gate_build(&gate, address, 0x08U, 8U, 0U,
                                  VGK_IDT_GATE_INTERRUPT, 1U) ==
               VGK_INTERRUPT_INVALID_ARGUMENT);
        assert(memcmp(before, &gate, sizeof(gate)) == 0);
    }

    {
        VGK_IDT_GATE table[VGK_IDT_EXCEPTION_COUNT];
        uint64_t handlers[VGK_IDT_EXCEPTION_COUNT];

        for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i)
            handlers[i] = UINT64_C(0xFFFF800000100000) +
                          ((uint64_t)i * UINT64_C(0x100));

        memset(table, 0xA5, sizeof(table));
        assert(vgk_idt_build_exception_table(table, handlers, 0x08U) == VGK_INTERRUPT_OK);
        for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i) {
            assert(gate_handler(&table[i]) == handlers[i]);
            assert(table[i].selector == 0x08U);
            assert(table[i].ist == 0U);
            assert(table[i].attributes == 0x8EU);
            assert(table[i].reserved == 0U);
        }

        /* Emergency routing is explicit and limited to the selected vectors. */
        assert(vgk_idt_build_exception_table_with_ist(table, handlers, 0x08U) == VGK_INTERRUPT_OK);
        for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i) {
            const uint8_t expected =
                i == 8U ? VGK_IDT_IST_DOUBLE_FAULT :
                i == 2U ? VGK_IDT_IST_NMI :
                i == 18U ? VGK_IDT_IST_MACHINE_CHECK :
                i == 14U ? VGK_IDT_IST_PAGE_FAULT : 0U;
            assert(table[i].ist == expected);
            assert(gate_handler(&table[i]) == handlers[i]);
        }

        memset(table, 0x5A, sizeof(table));
        handlers[17] = 0U;
        assert(vgk_idt_build_exception_table_with_ist(table, handlers, 0x08U) ==
               VGK_INTERRUPT_INVALID_ARGUMENT);
        for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i) {
            const uint8_t *bytes = (const uint8_t *)&table[i];
            for (size_t j = 0; j < sizeof(table[i]); ++j)
                assert(bytes[j] == 0x5AU);
        }
        handlers[17] = UINT64_C(0x0000800000000000);
        memset(table, 0x3C, sizeof(table));
        assert(vgk_idt_build_exception_table_with_ist(table, handlers, 0x08U) ==
               VGK_INTERRUPT_INVALID_ARGUMENT);
        for (uint32_t i = 0; i < VGK_IDT_EXCEPTION_COUNT; ++i) {
            const uint8_t *bytes = (const uint8_t *)&table[i];
            for (size_t j = 0; j < sizeof(table[i]); ++j)
                assert(bytes[j] == 0x3CU);
        }
        assert(vgk_idt_build_exception_table(table, handlers, 0U) == VGK_INTERRUPT_INVALID_ARGUMENT);
        assert(vgk_idt_build_exception_table(0, handlers, 0x08U) == VGK_INTERRUPT_INVALID_ARGUMENT);
        assert(vgk_idt_build_exception_table(table, 0, 0x08U) == VGK_INTERRUPT_INVALID_ARGUMENT);
    }

    {
        VGK_IDT_GATE full_table[VGK_IDT_VECTOR_COUNT]
            __attribute__((aligned(16)));
        uint64_t full_handlers[VGK_IDT_VECTOR_COUNT];

        for (uint32_t i = 0; i < VGK_IDT_VECTOR_COUNT; ++i)
            full_handlers[i] = UINT64_C(0xFFFF800010000000) +
                               ((uint64_t)i * UINT64_C(0x100));

        memset(full_table, 0xA5, sizeof(full_table));
        assert(vgk_idt_build_table(full_table, full_handlers, 0x08U) ==
               VGK_INTERRUPT_OK);
        for (uint32_t i = 0; i < VGK_IDT_VECTOR_COUNT; ++i) {
            assert(gate_handler(&full_table[i]) == full_handlers[i]);
            assert(full_table[i].selector == 0x08U);
            assert(full_table[i].ist == 0U);
            assert(full_table[i].attributes == 0x8EU);
            assert(full_table[i].reserved == 0U);
        }

        assert(vgk_idt_build_table_with_ist(
                   full_table, full_handlers, 0x08U) == VGK_INTERRUPT_OK);
        for (uint32_t i = 0; i < VGK_IDT_VECTOR_COUNT; ++i) {
            const uint8_t expected_ist =
                i == 8U ? VGK_IDT_IST_DOUBLE_FAULT :
                i == 2U ? VGK_IDT_IST_NMI :
                i == 18U ? VGK_IDT_IST_MACHINE_CHECK :
                i == 14U ? VGK_IDT_IST_PAGE_FAULT : 0U;
            assert(full_table[i].ist == expected_ist);
            assert(gate_handler(&full_table[i]) == full_handlers[i]);
        }

        /* Invalid last-vector input must not partially modify the table. */
        memset(full_table, 0x6D, sizeof(full_table));
        full_handlers[VGK_IDT_VECTOR_COUNT - 1U] =
            UINT64_C(0x0000800000000000);
        {
            uint8_t before[sizeof(full_table)];
            memcpy(before, full_table, sizeof(full_table));
            assert(vgk_idt_build_table_with_ist(
                       full_table, full_handlers, 0x08U) ==
                   VGK_INTERRUPT_INVALID_ARGUMENT);
            assert(memcmp(before, full_table, sizeof(full_table)) == 0);
        }

        assert(vgk_idt_build_table(full_table, full_handlers, 0U) ==
               VGK_INTERRUPT_INVALID_ARGUMENT);
        assert(vgk_idt_build_table(0, full_handlers, 0x08U) ==
               VGK_INTERRUPT_INVALID_ARGUMENT);
        assert(vgk_idt_build_table(full_table, 0, 0x08U) ==
               VGK_INTERRUPT_INVALID_ARGUMENT);
    }

    puts("VGK IDT gate and exception table tests: OK");
    return 0;
}
