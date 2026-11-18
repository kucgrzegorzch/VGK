#include "vgk_exception.h"

uint8_t vgk_exception_context_from_page_fault_stack(
    const uint64_t raw_stack[23],
    VGK_EXCEPTION_CONTEXT *context)
{
    if (raw_stack == 0 || context == 0 || raw_stack[15] != 14U)
        return 0U;

    context->r15 = raw_stack[0];
    context->r14 = raw_stack[1];
    context->r13 = raw_stack[2];
    context->r12 = raw_stack[3];
    context->r11 = raw_stack[4];
    context->r10 = raw_stack[5];
    context->r9 = raw_stack[6];
    context->r8 = raw_stack[7];
    context->rdi = raw_stack[8];
    context->rsi = raw_stack[9];
    context->rbp = raw_stack[10];
    context->rdx = raw_stack[11];
    context->rcx = raw_stack[12];
    context->rbx = raw_stack[13];
    context->rax = raw_stack[14];
    context->vector = raw_stack[15];
    context->cr2 = raw_stack[16];
    context->error_code = raw_stack[17];
    context->rip = raw_stack[18];
    context->cs = raw_stack[19];
    context->rflags = raw_stack[20];
    context->rsp = raw_stack[21];
    context->ss = raw_stack[22];
    return 1U;
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
