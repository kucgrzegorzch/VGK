#ifndef VGK_UEFI_ELF_ALLOCATOR_H
#define VGK_UEFI_ELF_ALLOCATOR_H

#include "elf_loader.h"
#include "uefi_services.h"

/* Adapter from the ELF loader to firmware page allocation services. */
typedef struct {
    VGK_EFI_ALLOCATE_PAGES allocate_pages;
    VGK_EFI_FREE_PAGES free_pages;
} VGK_UEFI_ELF_ALLOCATOR;

int vgk_uefi_elf_allocate(
    void *context,
    uint64_t physical_address,
    uint64_t page_count,
    void **mapped_address);

void vgk_uefi_elf_release(
    void *context,
    uint64_t physical_address,
    uint64_t page_count);

#endif
