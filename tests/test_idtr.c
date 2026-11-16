#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "vgk_idtr.h"

static VGK_IDT_GATE valid_table[VGK_IDT_EXCEPTION_COUNT]
    __attribute__((aligned(16)));
static VGK_IDT_GATE maximum_table[4096]
    __attribute__((aligned(16)));

int main(void)
{
    VGK_IDTR descriptor __attribute__((aligned(16)));
    const uint64_t expected_base = (uint64_t)(uintptr_t)valid_table;
    uint8_t before[sizeof(descriptor)];

    memset(&descriptor, 0xA7, sizeof(descriptor));
    assert(vgk_idtr_build(&descriptor, valid_table,
                          VGK_IDT_EXCEPTION_COUNT) == VGK_IDTR_OK);
    assert(descriptor.base == expected_base);
    assert(descriptor.limit ==
           (VGK_IDT_EXCEPTION_COUNT * sizeof(VGK_IDT_GATE)) - 1U);
    assert(descriptor.limit == 511U);

    assert(vgk_idtr_build(&descriptor, maximum_table, 4096U) == VGK_IDTR_OK);
    assert(descriptor.base == (uint64_t)(uintptr_t)maximum_table);
    assert(descriptor.limit == UINT16_MAX);

    /* Every rejected request must leave the descriptor byte-for-byte intact. */
    memset(&descriptor, 0x5C, sizeof(descriptor));
    memcpy(before, &descriptor, sizeof(descriptor));
    assert(vgk_idtr_build(&descriptor, valid_table, 0U) ==
           VGK_IDTR_INVALID_ARGUMENT);
    assert(memcmp(before, &descriptor, sizeof(descriptor)) == 0);

    assert(vgk_idtr_build(&descriptor, valid_table, 4097U) ==
           VGK_IDTR_INVALID_ARGUMENT);
    assert(memcmp(before, &descriptor, sizeof(descriptor)) == 0);

    assert(vgk_idtr_build(&descriptor, 0, VGK_IDT_EXCEPTION_COUNT) ==
           VGK_IDTR_INVALID_ARGUMENT);
    assert(memcmp(before, &descriptor, sizeof(descriptor)) == 0);

    assert(vgk_idtr_build(0, valid_table, VGK_IDT_EXCEPTION_COUNT) ==
           VGK_IDTR_INVALID_ARGUMENT);

    assert(vgk_idtr_build(&descriptor,
                          (const VGK_IDT_GATE *)(uintptr_t)0x0000800000000000ULL,
                          VGK_IDT_EXCEPTION_COUNT) ==
           VGK_IDTR_INVALID_ARGUMENT);
    assert(memcmp(before, &descriptor, sizeof(descriptor)) == 0);

    assert(vgk_idtr_build(&descriptor,
                          (const VGK_IDT_GATE *)(uintptr_t)
                              ((uintptr_t)valid_table + 1U),
                          VGK_IDT_EXCEPTION_COUNT) ==
           VGK_IDTR_INVALID_ARGUMENT);
    assert(memcmp(before, &descriptor, sizeof(descriptor)) == 0);

    assert(vgk_idtr_build(&descriptor,
                          (const VGK_IDT_GATE *)(uintptr_t)0xFFFFFFFFFFFFFFF0ULL,
                          VGK_IDT_EXCEPTION_COUNT) ==
           VGK_IDTR_INVALID_ARGUMENT);
    assert(memcmp(before, &descriptor, sizeof(descriptor)) == 0);

    puts("VGK IDTR descriptor preparation tests: OK");
    return 0;
}
