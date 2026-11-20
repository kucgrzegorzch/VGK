#include "vgk_exception.h"

uint8_t vgk_exception_context_from_page_fault_stack(
    const uint64_t raw_stack[23],
    VGK_EXCEPTION_CONTEXT *context)
{
    uint64_t frame[23];

    if (raw_stack == 0 || context == 0)
        return 0U;
    for (size_t i = 0; i < 23U; ++i)
        frame[i] = raw_stack[i];
    if (frame[15] != 14U)
        return 0U;

    context->r15 = frame[0];
    context->r14 = frame[1];
    context->r13 = frame[2];
    context->r12 = frame[3];
    context->r11 = frame[4];
    context->r10 = frame[5];
    context->r9 = frame[6];
    context->r8 = frame[7];
    context->rdi = frame[8];
    context->rsi = frame[9];
    context->rbp = frame[10];
    context->rdx = frame[11];
    context->rcx = frame[12];
    context->rbx = frame[13];
    context->rax = frame[14];
    context->vector = frame[15];
    context->cr2 = frame[16];
    context->error_code = frame[17];
    context->rip = frame[18];
    context->cs = frame[19];
    context->rflags = frame[20];
    context->rsp = frame[21];
    context->ss = frame[22];
    return 1U;
}


uint8_t vgk_double_fault_context_from_stack(
    const uint64_t raw_stack[22],
    VGK_DOUBLE_FAULT_CONTEXT *context)
{
    uint64_t frame[22];

    if (raw_stack == 0 || context == 0)
        return 0U;
    for (size_t i = 0; i < 22U; ++i)
        frame[i] = raw_stack[i];
    if (frame[15] != 8U || frame[16] != 0U)
        return 0U;

    context->r15 = frame[0];
    context->r14 = frame[1];
    context->r13 = frame[2];
    context->r12 = frame[3];
    context->r11 = frame[4];
    context->r10 = frame[5];
    context->r9 = frame[6];
    context->r8 = frame[7];
    context->rdi = frame[8];
    context->rsi = frame[9];
    context->rbp = frame[10];
    context->rdx = frame[11];
    context->rcx = frame[12];
    context->rbx = frame[13];
    context->rax = frame[14];
    context->vector = frame[15];
    context->error_code = frame[16];
    context->rip = frame[17];
    context->cs = frame[18];
    context->rflags = frame[19];
    context->rsp = frame[20];
    context->ss = frame[21];
    return 1U;
}

_Static_assert(sizeof(VGK_DOUBLE_FAULT_CONTEXT) == 22U * sizeof(uint64_t),
               "VGK double-fault context size mismatch");
_Static_assert(offsetof(VGK_DOUBLE_FAULT_CONTEXT, vector) == 15U * sizeof(uint64_t),
               "VGK double-fault vector offset mismatch");
_Static_assert(offsetof(VGK_DOUBLE_FAULT_CONTEXT, error_code) == 16U * sizeof(uint64_t),
               "VGK double-fault error-code offset mismatch");

uint8_t vgk_exception_stack_frame_is_within_bounds(
    uint64_t frame_start,
    uint64_t frame_size,
    uint64_t stack_base,
    uint64_t stack_end)
{
    if (frame_size == 0U ||
        stack_base >= stack_end ||
        frame_start < stack_base ||
        frame_start > stack_end)
        return 0U;

    return frame_size <= stack_end - frame_start ? 1U : 0U;
}

uint8_t vgk_exception_vector_has_error_code(uint8_t vector)
{
    switch (vector) {
    case 8U:  /* #DF: procesor dostarcza kod bledu o wartosci zero. */
    case 10U: /* #TS */
    case 11U: /* #NP */
    case 12U: /* #SS */
    case 13U: /* #GP */
    case 14U: /* #PF */
    case 17U: /* #AC */
    /* #VE (vector 20) does not push an error code. */
    case 21U: /* #CP */
    case 29U: /* #VC */
    case 30U: /* #SX */
        return 1U;
    default:
        return 0U;
    }
}

_Static_assert(offsetof(VGK_EXCEPTION_CONTEXT, vector) == 15U * sizeof(uint64_t),
               "VGK exception vector offset mismatch");
_Static_assert(offsetof(VGK_EXCEPTION_CONTEXT, error_code) == 16U * sizeof(uint64_t),
               "VGK exception error-code offset mismatch");
_Static_assert(offsetof(VGK_EXCEPTION_CONTEXT, rip) == 17U * sizeof(uint64_t),
               "VGK exception RIP offset mismatch");
_Static_assert(sizeof(VGK_EXCEPTION_CONTEXT) == 23U * sizeof(uint64_t),
               "VGK exception context size mismatch");
