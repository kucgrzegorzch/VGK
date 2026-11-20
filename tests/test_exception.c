#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "vgk_exception.h"

int main(void)
{
    static const uint8_t with_error[] = {
        8U, 10U, 11U, 12U, 13U, 14U, 17U, 21U, 29U, 30U
    };
    uint8_t expected[32] = {0};

    for (size_t i = 0; i < sizeof(with_error) / sizeof(with_error[0]); ++i)
        expected[with_error[i]] = 1U;

    for (uint8_t vector = 0U; vector < 32U; ++vector)
        assert(vgk_exception_vector_has_error_code(vector) == expected[vector]);

    {
        /* IST1 frame range validation: half-open stack and frame ranges. */
        const uint64_t base = UINT64_C(0x10000);
        const uint64_t end = UINT64_C(0x14000);
        const uint64_t frame_size = 22U * sizeof(uint64_t);

        assert(vgk_exception_stack_frame_is_within_bounds(
            base, frame_size, base, end) == 1U);
        assert(vgk_exception_stack_frame_is_within_bounds(
            end - frame_size, frame_size, base, end) == 1U);
        assert(vgk_exception_stack_frame_is_within_bounds(
            base - 1U, frame_size, base, end) == 0U);
        assert(vgk_exception_stack_frame_is_within_bounds(
            end - frame_size + 1U, frame_size, base, end) == 0U);
        assert(vgk_exception_stack_frame_is_within_bounds(
            end, frame_size, base, end) == 0U);
        assert(vgk_exception_stack_frame_is_within_bounds(
            base, 0U, base, end) == 0U);
        assert(vgk_exception_stack_frame_is_within_bounds(
            base, frame_size, end, base) == 0U);
        assert(vgk_exception_stack_frame_is_within_bounds(
            UINT64_MAX - 7U, 22U * sizeof(uint64_t),
            base, UINT64_MAX) == 0U);
    }

    assert(sizeof(VGK_EXCEPTION_CONTEXT) == 23U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, vector) == 15U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, error_code) == 16U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, rip) == 17U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, cs) == 18U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, rflags) == 19U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, rsp) == 20U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, ss) == 21U * sizeof(uint64_t));
    assert(offsetof(VGK_EXCEPTION_CONTEXT, cr2) == 22U * sizeof(uint64_t));

    {
        uint64_t raw_stack[23];
        VGK_EXCEPTION_CONTEXT context;
        VGK_EXCEPTION_CONTEXT before;

        for (size_t i = 0; i < 23U; ++i)
            raw_stack[i] = UINT64_C(0xA500000000000000) + i;
        raw_stack[15] = 14U;
        memset(&context, 0xDE, sizeof(context));

        assert(vgk_exception_context_from_page_fault_stack(
                   raw_stack, &context) == 1U);
        assert(context.r15 == raw_stack[0]);
        assert(context.r14 == raw_stack[1]);
        assert(context.r13 == raw_stack[2]);
        assert(context.r12 == raw_stack[3]);
        assert(context.r11 == raw_stack[4]);
        assert(context.r10 == raw_stack[5]);
        assert(context.r9 == raw_stack[6]);
        assert(context.r8 == raw_stack[7]);
        assert(context.rdi == raw_stack[8]);
        assert(context.rsi == raw_stack[9]);
        assert(context.rbp == raw_stack[10]);
        assert(context.rdx == raw_stack[11]);
        assert(context.rcx == raw_stack[12]);
        assert(context.rbx == raw_stack[13]);
        assert(context.rax == raw_stack[14]);
        assert(context.vector == 14U);
        assert(context.cr2 == raw_stack[16]);
        assert(context.error_code == raw_stack[17]);
        assert(context.rip == raw_stack[18]);
        assert(context.cs == raw_stack[19]);
        assert(context.rflags == raw_stack[20]);
        assert(context.rsp == raw_stack[21]);
        assert(context.ss == raw_stack[22]);

        before = context;
        raw_stack[15] = 13U;
        assert(vgk_exception_context_from_page_fault_stack(
                   raw_stack, &context) == 0U);
        assert(memcmp(&context, &before, sizeof(context)) == 0);
        assert(vgk_exception_context_from_page_fault_stack(
                   0, &context) == 0U);
        assert(vgk_exception_context_from_page_fault_stack(
                   raw_stack, 0) == 0U);
    }


    {
        uint64_t raw_stack[22];
        VGK_DOUBLE_FAULT_CONTEXT context;
        VGK_DOUBLE_FAULT_CONTEXT before;

        for (size_t i = 0; i < 22U; ++i)
            raw_stack[i] = UINT64_C(0xDF00000000000000) + i;
        raw_stack[15] = 8U;
        raw_stack[16] = 0U;
        memset(&context, 0xA5, sizeof(context));

        assert(sizeof(VGK_DOUBLE_FAULT_CONTEXT) == 22U * sizeof(uint64_t));
        assert(offsetof(VGK_DOUBLE_FAULT_CONTEXT, vector) == 15U * sizeof(uint64_t));
        assert(offsetof(VGK_DOUBLE_FAULT_CONTEXT, error_code) == 16U * sizeof(uint64_t));
        assert(vgk_double_fault_context_from_stack(raw_stack, &context) == 1U);
        assert(context.r15 == raw_stack[0]);
        assert(context.r14 == raw_stack[1]);
        assert(context.r13 == raw_stack[2]);
        assert(context.r12 == raw_stack[3]);
        assert(context.r11 == raw_stack[4]);
        assert(context.r10 == raw_stack[5]);
        assert(context.r9 == raw_stack[6]);
        assert(context.r8 == raw_stack[7]);
        assert(context.rdi == raw_stack[8]);
        assert(context.rsi == raw_stack[9]);
        assert(context.rbp == raw_stack[10]);
        assert(context.rdx == raw_stack[11]);
        assert(context.rcx == raw_stack[12]);
        assert(context.rbx == raw_stack[13]);
        assert(context.rax == raw_stack[14]);
        assert(context.vector == 8U);
        assert(context.error_code == 0U);
        assert(context.rip == raw_stack[17]);
        assert(context.cs == raw_stack[18]);
        assert(context.rflags == raw_stack[19]);
        assert(context.rsp == raw_stack[20]);
        assert(context.ss == raw_stack[21]);

        before = context;
        raw_stack[16] = 1U;
        assert(vgk_double_fault_context_from_stack(raw_stack, &context) == 0U);
        assert(memcmp(&context, &before, sizeof(context)) == 0);
        raw_stack[16] = 0U;
        raw_stack[15] = 13U;
        assert(vgk_double_fault_context_from_stack(raw_stack, &context) == 0U);
        assert(vgk_double_fault_context_from_stack(0, &context) == 0U);
        assert(vgk_double_fault_context_from_stack(raw_stack, 0) == 0U);
    }

    puts("VGK exception context, #PF/#DF frame mapping, and error-code classification tests: OK");
    return 0;
}
