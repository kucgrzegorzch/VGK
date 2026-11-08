#include "elf_loader.h"

#define ELF_HEADER_SIZE 64U
#define ELF_PROGRAM_HEADER_SIZE 56U
#define ELF_CLASS_64 2U
#define ELF_DATA_LSB 1U
#define ELF_VERSION_CURRENT 1U
#define ELF_TYPE_EXEC 2U
#define ELF_MACHINE_X86_64 62U
#define ELF_PT_NULL 0U
#define ELF_PT_LOAD 1U
#define ELF_PF_X 1U
#define ELF_PF_W 2U
#define VGK_ELF_PAGE_SIZE UINT64_C(4096)

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t read_u64(const uint8_t *p)
{
    return (uint64_t)read_u32(p) |
           ((uint64_t)read_u32(p + 4) << 32);
}

static int add_overflows_u64(uint64_t a, uint64_t b)
{
    return a > UINT64_MAX - b;
}

static int is_power_of_two(uint64_t value)
{
    return value != 0 && (value & (value - 1U)) == 0;
}

static void clear_plan(VGK_ELF_LOAD_PLAN *plan)
{
    uint8_t *bytes = (uint8_t *)plan;
    for (size_t i = 0; i < sizeof(*plan); ++i)
        bytes[i] = 0;
}

VGK_ELF_RESULT vgk_elf64_make_load_plan(
    const uint8_t *image,
    size_t image_size,
    VGK_ELF_LOAD_PLAN *plan)
{
    uint64_t entry;
    uint64_t phoff;
    uint16_t phnum;
    uint16_t phentsize;
    int entry_is_executable = 0;

    if (image == 0 || plan == 0)
        return VGK_ELF_INVALID_ARGUMENT;

    clear_plan(plan);

    if (image_size < ELF_HEADER_SIZE ||
        image[0] != 0x7fU || image[1] != 'E' ||
        image[2] != 'L' || image[3] != 'F')
        return VGK_ELF_BAD_HEADER;

    if (image[4] != ELF_CLASS_64 || image[5] != ELF_DATA_LSB ||
        image[6] != ELF_VERSION_CURRENT ||
        read_u16(image + 16) != ELF_TYPE_EXEC ||
        read_u16(image + 18) != ELF_MACHINE_X86_64 ||
        read_u32(image + 20) != ELF_VERSION_CURRENT ||
        read_u16(image + 52) != ELF_HEADER_SIZE)
        return VGK_ELF_UNSUPPORTED_FORMAT;

    phoff = read_u64(image + 32);
    phentsize = read_u16(image + 54);
    phnum = read_u16(image + 56);
    if (phentsize != ELF_PROGRAM_HEADER_SIZE || phnum == 0 ||
        phnum > VGK_ELF_MAX_LOAD_SEGMENTS ||
        phoff < ELF_HEADER_SIZE ||
        phoff > (uint64_t)image_size ||
        (uint64_t)phnum > (UINT64_MAX - phoff) / phentsize ||
        phoff + (uint64_t)phnum * phentsize > (uint64_t)image_size)
        return VGK_ELF_BAD_PROGRAM_TABLE;

    entry = read_u64(image + 24);
    plan->entry = entry;

    for (uint16_t i = 0; i < phnum; ++i) {
        const uint8_t *ph = image + (size_t)phoff +
                            (size_t)i * ELF_PROGRAM_HEADER_SIZE;
        uint32_t type = read_u32(ph);
        uint32_t flags = read_u32(ph + 4);
        uint64_t offset = read_u64(ph + 8);
        uint64_t virtual_address = read_u64(ph + 16);
        uint64_t physical_address = read_u64(ph + 24);
        uint64_t file_size = read_u64(ph + 32);
        uint64_t memory_size = read_u64(ph + 40);
        uint64_t alignment = read_u64(ph + 48);
        uint64_t segment_end;

        if (type == ELF_PT_NULL)
            continue;
        if (type != ELF_PT_LOAD)
            return VGK_ELF_BAD_SEGMENT;
        if (plan->segment_count >= VGK_ELF_MAX_LOAD_SEGMENTS ||
            file_size > memory_size || memory_size == 0 ||
            offset > (uint64_t)image_size ||
            file_size > (uint64_t)image_size - offset ||
            virtual_address != physical_address ||
            add_overflows_u64(physical_address, memory_size))
            return VGK_ELF_BAD_SEGMENT;

        segment_end = physical_address + memory_size;
        if (physical_address < VGK_ELF_MIN_LOAD_ADDRESS ||
            (physical_address & (VGK_ELF_PAGE_SIZE - 1U)) != 0 ||
            segment_end > VGK_ELF_MAX_LOAD_ADDRESS ||
            (flags & ~(ELF_PF_X | ELF_PF_W | 4U)) != 0 ||
            ((flags & ELF_PF_X) != 0 && (flags & ELF_PF_W) != 0))
            return VGK_ELF_BAD_SEGMENT;

        if (alignment > 1U &&
            (!is_power_of_two(alignment) ||
             (virtual_address & (alignment - 1U)) !=
                 (offset & (alignment - 1U))))
            return VGK_ELF_BAD_SEGMENT;

        for (uint32_t j = 0; j < plan->segment_count; ++j) {
            const VGK_ELF_LOAD_SEGMENT *prior = &plan->segments[j];
            uint64_t prior_end = prior->physical_address + prior->memory_size;
            if (physical_address < prior_end &&
                prior->physical_address < segment_end)
                return VGK_ELF_SEGMENT_OVERLAP;
        }

        VGK_ELF_LOAD_SEGMENT *segment =
            &plan->segments[plan->segment_count++];
        segment->file_offset = offset;
        segment->physical_address = physical_address;
        segment->file_size = file_size;
        segment->memory_size = memory_size;
        segment->alignment = alignment;
        segment->flags = flags;

        if ((flags & ELF_PF_X) != 0 &&
            entry >= physical_address && entry < segment_end)
            entry_is_executable = 1;
    }

    if (plan->segment_count == 0)
        return VGK_ELF_BAD_SEGMENT;
    if (!entry_is_executable)
        return VGK_ELF_ENTRY_NOT_EXECUTABLE;

    return VGK_ELF_OK;
}
