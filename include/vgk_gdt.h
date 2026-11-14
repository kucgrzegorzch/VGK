#ifndef VGK_GDT_H
#define VGK_GDT_H

#include <stdint.h>

#define VGK_GDT_ENTRY_COUNT 5U
#define VGK_GDT_SELECTOR_CODE 0x08U
#define VGK_GDT_SELECTOR_DATA 0x10U
#define VGK_GDT_SELECTOR_TSS  0x18U
#define VGK_TSS64_SIZE 104U

/*
 * 64-bit TSS layout from Intel SDM. The I/O bitmap is intentionally absent;
 * iomap_base points one byte past the TSS limit.
 */
/* Keep the architectural 104-byte size; align each allocated TSS object to 16 bytes. */
typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} VGK_TSS64;

typedef enum {
    VGK_GDT_OK = 0,
    VGK_GDT_INVALID_ARGUMENT = 1
} VGK_GDT_STATUS;

/*
 * Initialize a TSS without executing privileged instructions.
 * rsp0 may be zero until a privilege-transition stack is provisioned.
 * Each nonzero IST top must be canonical and 16-byte aligned.
 */
VGK_GDT_STATUS vgk_tss64_init(
    VGK_TSS64 *tss,
    uint64_t rsp0,
    const uint64_t ist_tops[7]);

/*
 * Build a private GDT: null, 64-bit kernel code, kernel data, and a
 * two-slot available 64-bit TSS descriptor. Does not execute LGDT/LTR.
 */
VGK_GDT_STATUS vgk_gdt_build(
    uint64_t gdt[VGK_GDT_ENTRY_COUNT],
    const VGK_TSS64 *tss);

_Static_assert(sizeof(VGK_TSS64) == VGK_TSS64_SIZE,
               "VGK x86-64 TSS must be exactly 104 bytes");
_Static_assert(_Alignof(VGK_TSS64) == 1,
               "VGK TSS fields must retain their packed architectural layout");

#endif
