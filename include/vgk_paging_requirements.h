#ifndef VGK_PAGING_REQUIREMENTS_H
#define VGK_PAGING_REQUIREMENTS_H

#include <stddef.h>
#include <stdint.h>

/* The current boot protocol can contribute at most 16 ELF segments plus
 * boot info, memory map, page-table pool, framebuffer and ACPI RSDP. */
#define VGK_PAGING_REQUIREMENTS_MAX_RANGES 32U

typedef struct {
    uint64_t virtual_address;
    uint64_t length;
} VGK_PAGING_VIRTUAL_RANGE;

typedef enum {
    VGK_PAGING_REQUIREMENTS_OK = 0,
    VGK_PAGING_REQUIREMENTS_INVALID_ARGUMENT,
    VGK_PAGING_REQUIREMENTS_BAD_RANGE,
    VGK_PAGING_REQUIREMENTS_TOO_MANY_RANGES
} VGK_PAGING_REQUIREMENTS_RESULT;

typedef struct {
    uint64_t pml4_pages;
    uint64_t pdpt_pages;
    uint64_t pd_pages;
    uint64_t pt_pages;
    uint64_t total_pages;
} VGK_PAGING_REQUIREMENTS;

/*
 * Calculates the exact number of four-level x86-64 page-table pages for
 * page-aligned virtual ranges mapped with 4 KiB leaves.
 *
 * Ranges may overlap. Shared paging structures are counted only once.
 * The result is structural: physical aliases and leaf permissions must
 * still be validated by the page-table builder.
 *
 * An empty range list still requires the single PML4 root page.
 */
VGK_PAGING_REQUIREMENTS_RESULT vgk_paging_requirements_calculate(
    const VGK_PAGING_VIRTUAL_RANGE *ranges,
    size_t range_count,
    VGK_PAGING_REQUIREMENTS *requirements);

#endif
