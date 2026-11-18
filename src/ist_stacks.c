#include "vgk_ist_stacks.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Dedicated, page-aligned BSS storage. Guard pages are intentionally deferred
 * until the paging builder can leave unmapped holes around these ranges.
 */
static uint8_t vgk_ist_double_fault_stack[VGK_IST_STACK_SIZE]
    __attribute__((aligned(VGK_IST_STACK_ALIGNMENT), section(".bss.ist")));
static uint8_t vgk_ist_nmi_stack[VGK_IST_STACK_SIZE]
    __attribute__((aligned(VGK_IST_STACK_ALIGNMENT), section(".bss.ist")));
static uint8_t vgk_ist_machine_check_stack[VGK_IST_STACK_SIZE]
    __attribute__((aligned(VGK_IST_STACK_ALIGNMENT), section(".bss.ist")));
static uint8_t vgk_ist_page_fault_stack[VGK_IST_STACK_SIZE]
    __attribute__((aligned(VGK_IST_STACK_ALIGNMENT), section(".bss.ist")));

static int is_canonical_48(uint64_t address)
{
    const uint64_t upper = address >> 48;
    const uint64_t sign = (address >> 47) & UINT64_C(1);
    return sign != 0U ? upper == UINT64_C(0xFFFF) : upper == 0U;
}

VGK_IST_STACKS_STATUS vgk_ist_page_fault_stack_get_bounds(
    uint64_t *base,
    uint64_t *end)
{
    const uintptr_t start = (uintptr_t)vgk_ist_page_fault_stack;
    const uintptr_t limit = start + sizeof(vgk_ist_page_fault_stack);

    if (base == 0 || end == 0)
        return VGK_IST_STACKS_INVALID_ARGUMENT;
    if (start == 0U || limit <= start ||
        limit - start != VGK_IST_STACK_SIZE ||
        (start & (VGK_IST_STACK_ALIGNMENT - 1U)) != 0U)
        return VGK_IST_STACKS_BAD_LAYOUT;

    *base = (uint64_t)start;
    *end = (uint64_t)limit;
    return VGK_IST_STACKS_OK;
}

VGK_IST_STACKS_STATUS vgk_ist_stacks_get_tops(uint64_t ist_tops[7])
{
    const uintptr_t starts[VGK_IST_STACK_COUNT] = {
        (uintptr_t)vgk_ist_double_fault_stack,
        (uintptr_t)vgk_ist_nmi_stack,
        (uintptr_t)vgk_ist_machine_check_stack,
        (uintptr_t)vgk_ist_page_fault_stack
    };
    const uintptr_t ends[VGK_IST_STACK_COUNT] = {
        (uintptr_t)(vgk_ist_double_fault_stack + sizeof(vgk_ist_double_fault_stack)),
        (uintptr_t)(vgk_ist_nmi_stack + sizeof(vgk_ist_nmi_stack)),
        (uintptr_t)(vgk_ist_machine_check_stack + sizeof(vgk_ist_machine_check_stack)),
        (uintptr_t)(vgk_ist_page_fault_stack + sizeof(vgk_ist_page_fault_stack))
    };

    if (ist_tops == 0)
        return VGK_IST_STACKS_INVALID_ARGUMENT;

    for (size_t i = 0; i < VGK_IST_STACK_COUNT; ++i) {
        if (starts[i] == 0U || ends[i] <= starts[i] ||
            ends[i] - starts[i] != VGK_IST_STACK_SIZE ||
            (starts[i] & (VGK_IST_STACK_ALIGNMENT - 1U)) != 0U ||
            (ends[i] & 0xFU) != 0U ||
            !is_canonical_48((uint64_t)starts[i]) ||
            !is_canonical_48((uint64_t)ends[i]))
            return VGK_IST_STACKS_BAD_LAYOUT;

        for (size_t j = 0; j < i; ++j) {
            if (starts[i] < ends[j] && starts[j] < ends[i])
                return VGK_IST_STACKS_BAD_LAYOUT;
        }
    }

    for (size_t i = 0; i < 7U; ++i)
        ist_tops[i] = 0U;

    ist_tops[0] = (uint64_t)ends[0]; /* TSS.IST1: double fault */
    ist_tops[1] = (uint64_t)ends[1]; /* TSS.IST2: NMI */
    ist_tops[2] = (uint64_t)ends[2]; /* TSS.IST3: machine check */
    ist_tops[3] = (uint64_t)ends[3]; /* TSS.IST4: page fault */
    return VGK_IST_STACKS_OK;
}

_Static_assert(VGK_IST_STACK_SIZE >= 4096U,
               "IST stacks must contain at least one page");
_Static_assert((VGK_IST_STACK_ALIGNMENT & (VGK_IST_STACK_ALIGNMENT - 1U)) == 0U,
               "IST stack alignment must be a power of two");
