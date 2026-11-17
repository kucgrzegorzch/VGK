#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "vgk_ist_stacks.h"

static int is_canonical_48(uint64_t address)
{
    const uint64_t upper = address >> 48;
    const uint64_t sign = (address >> 47) & UINT64_C(1);
    return sign != 0U ? upper == UINT64_C(0xFFFF) : upper == 0U;
}

int main(void)
{
    uint64_t tops[7] = {0};

    assert(vgk_ist_stacks_get_tops(tops) == VGK_IST_STACKS_OK);
    for (size_t i = 0; i < VGK_IST_STACK_COUNT; ++i) {
        assert(tops[i] != 0U);
        assert((tops[i] & 0xFU) == 0U);
        assert((tops[i] & (VGK_IST_STACK_ALIGNMENT - 1U)) == 0U);
        assert(is_canonical_48(tops[i]));
        assert(tops[i] >= VGK_IST_STACK_SIZE);
        for (size_t j = 0; j < i; ++j) {
            const uint64_t low_i = tops[i] - VGK_IST_STACK_SIZE;
            const uint64_t low_j = tops[j] - VGK_IST_STACK_SIZE;
            assert(low_i >= tops[j] || low_j >= tops[i]);
        }
    }
    for (size_t i = VGK_IST_STACK_COUNT; i < 7U; ++i)
        assert(tops[i] == 0U);

    assert(vgk_ist_stacks_get_tops(0) == VGK_IST_STACKS_INVALID_ARGUMENT);
    puts("VGK IST stack storage tests: OK");
    return 0;
}
