#ifndef VGK_EXCEPTION_H
#define VGK_EXCEPTION_H

#include <stddef.h>
#include <stdint.h>

/*
 * Znormalizowany zapis diagnostyczny. Procedury asemblerowe beda musialy
 * wypelnic wszystkie pola jawnie; nie jest to bezposredni obraz stosu CPU.
 * W 64-bitowej ramce sprzetowej SS:RSP wystepuja zawsze po RFLAGS.
 */
typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
    uint64_t cr2;
} VGK_EXCEPTION_CONTEXT;

/*
 * Jawnie przepisuje surowa ramke #PF ze stosu awaryjnego do rekordu
 * diagnostycznego. raw_stack ma 23 slowa: GPR[15], vector, CR2,
 * CPU error code, RIP, CS, RFLAGS, RSP, SS.
 * Zwraca 1 przy poprawnych wskaznikach i wektorze 14; w przeciwnym
 * razie zwraca 0 i nie modyfikuje rekordu docelowego.
 */
uint8_t vgk_exception_context_from_page_fault_stack(
    const uint64_t raw_stack[23],
    VGK_EXCEPTION_CONTEXT *context);


/*
 * Normalized #DF diagnostic context. Unlike #PF, #DF has no CR2 slot.
 * Raw frame order: GPR[15], vector, CPU error code, RIP, CS, RFLAGS, RSP, SS.
 */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error_code, rip, cs, rflags, rsp, ss;
} VGK_DOUBLE_FAULT_CONTEXT;

/* Accepts only a hardware #DF frame (vector 8, error code zero). */
uint8_t vgk_double_fault_context_from_stack(
    const uint64_t raw_stack[22],
    VGK_DOUBLE_FAULT_CONTEXT *context);

/*
 * Checks a non-empty frame range [frame_start, frame_start + frame_size)
 * against the half-open stack range [stack_base, stack_end), without
 * performing an overflowing addition.
 */
uint8_t vgk_exception_stack_frame_is_within_bounds(
    uint64_t frame_start,
    uint64_t frame_size,
    uint64_t stack_base,
    uint64_t stack_end);

/* Zwraca 1, gdy procesor dostarcza kod bledu dla danego wektora. */
uint8_t vgk_exception_vector_has_error_code(uint8_t vector);

#endif
