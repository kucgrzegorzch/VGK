#ifndef VGK_EXCEPTION_H
#define VGK_EXCEPTION_H

#include <stddef.h>
#include <stdint.h>

/*
 * Znormalizowany zapis diagnostyczny. Procedury asemblerowe beda musialy
 * wypelnic wszystkie pola jawnie; nie jest to bezposredni obraz stosu CPU.
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

/* Zwraca 1, gdy procesor dostarcza kod bledu dla danego wektora. */
uint8_t vgk_exception_vector_has_error_code(uint8_t vector);

#endif
