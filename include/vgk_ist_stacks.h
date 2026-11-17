#ifndef VGK_IST_STACKS_H
#define VGK_IST_STACKS_H

#include <stdint.h>

#define VGK_IST_STACK_COUNT 3U
#define VGK_IST_STACK_SIZE  (16U * 1024U)
#define VGK_IST_STACK_ALIGNMENT 4096U

typedef enum {
    VGK_IST_STACKS_OK = 0,
    VGK_IST_STACKS_INVALID_ARGUMENT = 1,
    VGK_IST_STACKS_BAD_LAYOUT = 2
} VGK_IST_STACKS_STATUS;

/*
 * Returns seven TSS IST top addresses. Slots 1..3 contain the dedicated
 * #DF, NMI and #MC stack tops; slots 4..7 are zero. Storage is static BSS.
 * This function does not load a TSS or activate an IDT.
 */
VGK_IST_STACKS_STATUS vgk_ist_stacks_get_tops(uint64_t ist_tops[7]);

#endif
