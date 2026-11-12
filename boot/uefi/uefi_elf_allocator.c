#include "uefi_elf_allocator.h"

#define EFI_SUCCESS UINT64_C(0)
#define VGK_EFI_LOADER_DATA UINT32_C(2)
#define VGK_EFI_ALLOCATE_ADDRESS UINT32_C(2)
#define VGK_PAGE_SIZE UINT64_C(4096)

int vgk_uefi_elf_allocate(
    void *context,
    uint64_t physical_address,
    uint64_t page_count,
    void **mapped_address)
{
    VGK_UEFI_ELF_ALLOCATOR *allocator =
        (VGK_UEFI_ELF_ALLOCATOR *)context;
    uint64_t requested_address = physical_address;
    VGK_EFI_STATUS status;

    if (allocator == 0 || allocator->allocate_pages == 0 ||
        allocator->free_pages == 0 || mapped_address == 0 ||
        physical_address == 0 || page_count == 0 ||
        page_count > UINT64_MAX / VGK_PAGE_SIZE ||
        physical_address > UINT64_MAX - page_count * VGK_PAGE_SIZE)
        return -1;

    *mapped_address = 0;
    status = allocator->allocate_pages(
        VGK_EFI_ALLOCATE_ADDRESS, VGK_EFI_LOADER_DATA,
        (VGK_EFI_UINTN)page_count, &requested_address);
    if (status != EFI_SUCCESS)
        return -1;

    /* A successful exact-address request must return the requested base. */
    if (requested_address != physical_address) {
        (void)allocator->free_pages(
            requested_address, (VGK_EFI_UINTN)page_count);
        return -1;
    }

    /* Before SetVirtualAddressMap, UEFI x64 uses identity-accessible addresses. */
    *mapped_address = (void *)(uintptr_t)requested_address;
    return 0;
}

void vgk_uefi_elf_release(
    void *context,
    uint64_t physical_address,
    uint64_t page_count)
{
    VGK_UEFI_ELF_ALLOCATOR *allocator =
        (VGK_UEFI_ELF_ALLOCATOR *)context;

    if (allocator == 0 || allocator->free_pages == 0 ||
        physical_address == 0 || page_count == 0 ||
        page_count > UINT64_MAX / VGK_PAGE_SIZE)
        return;

    (void)allocator->free_pages(
        physical_address, (VGK_EFI_UINTN)page_count);
}
