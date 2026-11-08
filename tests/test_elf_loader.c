#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "elf_loader.h"

#define IMAGE_SIZE 512U
#define PHOFF 64U
#define PHENT 56U

static uint8_t image[IMAGE_SIZE];

static void put16(size_t offset, uint16_t value)
{
    image[offset] = (uint8_t)value;
    image[offset + 1U] = (uint8_t)(value >> 8);
}

static void put32(size_t offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        image[offset + i] = (uint8_t)(value >> (i * 8U));
}

static void put64(size_t offset, uint64_t value)
{
    for (unsigned i = 0; i < 8; ++i)
        image[offset + i] = (uint8_t)(value >> (i * 8U));
}

static void set_segment(size_t index, uint32_t flags, uint64_t offset,
                       uint64_t address, uint64_t file_size,
                       uint64_t memory_size, uint64_t alignment)
{
    size_t p = PHOFF + index * PHENT;
    put32(p, 1U);
    put32(p + 4U, flags);
    put64(p + 8U, offset);
    put64(p + 16U, address);
    put64(p + 24U, address);
    put64(p + 32U, file_size);
    put64(p + 40U, memory_size);
    put64(p + 48U, alignment);
}

static void make_valid_image(void)
{
    memset(image, 0, sizeof(image));
    image[0] = 0x7fU; image[1] = 'E'; image[2] = 'L'; image[3] = 'F';
    image[4] = 2U; image[5] = 1U; image[6] = 1U;
    put16(16U, 2U);
    put16(18U, 62U);
    put32(20U, 1U);
    put64(24U, UINT64_C(0x200000));
    put64(32U, PHOFF);
    put16(52U, 64U);
    put16(54U, PHENT);
    put16(56U, 2U);
    set_segment(0U, 5U, 256U, UINT64_C(0x200000), 16U, 16U, 16U);
    set_segment(1U, 6U, 272U, UINT64_C(0x201000), 16U, 32U, 16U);
}

int main(void)
{
    VGK_ELF_LOAD_PLAN plan;

    make_valid_image();
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) == VGK_ELF_OK);
    assert(plan.entry == UINT64_C(0x200000));
    assert(plan.segment_count == 2U);
    assert(plan.segments[1].memory_size == 32U);

    assert(vgk_elf64_make_load_plan(image, 32U, &plan) == VGK_ELF_BAD_HEADER);

    make_valid_image();
    put16(18U, 3U);
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) ==
           VGK_ELF_UNSUPPORTED_FORMAT);

    make_valid_image();
    put64(PHOFF + 32U, 17U);
    put64(PHOFF + 40U, 16U);
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) ==
           VGK_ELF_BAD_SEGMENT);

    make_valid_image();
    put64(PHOFF + 24U, UINT64_C(0x201000));
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) ==
           VGK_ELF_BAD_SEGMENT);

    make_valid_image();
    set_segment(1U, 6U, 272U, UINT64_C(0x200008), 16U, 32U, 4096U);
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) ==
           VGK_ELF_BAD_SEGMENT);

    make_valid_image();
    set_segment(1U, 6U, 272U, UINT64_C(0x200008), 16U, 32U, 1U);
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) ==
           VGK_ELF_BAD_SEGMENT);

    /* UEFI AllocateAddress works in pages: segment bases must be 4 KiB aligned. */
    make_valid_image();
    set_segment(1U, 6U, 272U, UINT64_C(0x202008), 16U, 32U, 1U);
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) ==
           VGK_ELF_BAD_SEGMENT);

    make_valid_image();
    put64(24U, UINT64_C(0x201000));
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) ==
           VGK_ELF_ENTRY_NOT_EXECUTABLE);

    make_valid_image();
    set_segment(0U, 7U, 256U, UINT64_C(0x200000), 16U, 16U, 4096U);
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) ==
           VGK_ELF_BAD_SEGMENT);

    make_valid_image();
    put32(PHOFF, 3U); /* PT_INTERP is not accepted. */
    assert(vgk_elf64_make_load_plan(image, sizeof(image), &plan) ==
           VGK_ELF_BAD_SEGMENT);

    puts("OK: ELF64 loader plan validation");
    return 0;
}
