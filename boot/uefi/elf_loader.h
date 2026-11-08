#ifndef VGK_ELF_LOADER_H
#define VGK_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#define VGK_ELF_MAX_LOAD_SEGMENTS 16U
#define VGK_ELF_MIN_LOAD_ADDRESS UINT64_C(0x00200000)
#define VGK_ELF_MAX_LOAD_ADDRESS UINT64_C(0x40000000)

typedef struct {
    uint64_t file_offset;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
    uint32_t flags;
} VGK_ELF_LOAD_SEGMENT;

typedef struct {
    uint64_t entry;
    uint32_t segment_count;
    VGK_ELF_LOAD_SEGMENT segments[VGK_ELF_MAX_LOAD_SEGMENTS];
} VGK_ELF_LOAD_PLAN;

typedef struct {
    uint64_t physical_address;
    uint64_t page_count;
} VGK_ELF_ALLOCATED_RANGE;

typedef struct {
    uint32_t range_count;
    VGK_ELF_ALLOCATED_RANGE ranges[VGK_ELF_MAX_LOAD_SEGMENTS];
} VGK_ELF_LOADED_IMAGE;

typedef int (*VGK_ELF_ALLOCATE_RANGE)(
    void *context, uint64_t physical_address, uint64_t page_count,
    void **mapped_address);
typedef void (*VGK_ELF_RELEASE_RANGE)(
    void *context, uint64_t physical_address, uint64_t page_count);

typedef enum {
    VGK_ELF_OK = 0,
    VGK_ELF_INVALID_ARGUMENT,
    VGK_ELF_BAD_HEADER,
    VGK_ELF_UNSUPPORTED_FORMAT,
    VGK_ELF_BAD_PROGRAM_TABLE,
    VGK_ELF_BAD_SEGMENT,
    VGK_ELF_SEGMENT_OVERLAP,
    VGK_ELF_ENTRY_NOT_EXECUTABLE
} VGK_ELF_RESULT;

/*
 * Allocate exact physical ranges, copy file bytes, and zero the BSS tail.
 * Allocation callbacks must return a writable mapping for the requested
 * physical address. On failure, every allocation made by this call is freed.
 */
VGK_ELF_RESULT vgk_elf64_load_image(
    const uint8_t *image,
    size_t image_size,
    const VGK_ELF_LOAD_PLAN *plan,
    VGK_ELF_ALLOCATE_RANGE allocate_range,
    VGK_ELF_RELEASE_RANGE release_range,
    void *context,
    VGK_ELF_LOADED_IMAGE *loaded);

/*
 * Validate a bounded ELF64 x86-64 executable and produce a load plan.
 * This function does not allocate memory, copy bytes, or execute the image.
 * The caller must verify the signed release manifest before trusting the image.
 */
VGK_ELF_RESULT vgk_elf64_make_load_plan(
    const uint8_t *image,
    size_t image_size,
    VGK_ELF_LOAD_PLAN *plan);

#endif
