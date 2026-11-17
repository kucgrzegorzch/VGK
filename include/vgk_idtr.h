#ifndef VGK_IDTR_H
#define VGK_IDTR_H

#include <stdint.h>
#include "vgk_interrupts.h"

/* x86-64 IDT has 256 vectors (0..255), each represented by a 16-byte gate. */
#define VGK_IDT_MAX_ENTRIES 256U

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} VGK_IDTR;

typedef enum {
    VGK_IDTR_OK = 0,
    VGK_IDTR_INVALID_ARGUMENT = 1
} VGK_IDTR_STATUS;

/*
 * Prepare an IDTR image for an already-built IDT.
 * This function validates the address, alignment and entry count, then
 * writes only the memory descriptor. It never executes LIDT.
 */
VGK_IDTR_STATUS vgk_idtr_build(
    VGK_IDTR *descriptor,
    const VGK_IDT_GATE *table,
    uint32_t entry_count);

_Static_assert(sizeof(VGK_IDTR) == 10,
               "VGK IDTR image must be exactly 10 bytes");

#endif
