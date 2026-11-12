#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "uefi_elf_allocator.h"

static uint64_t last_address;
static uint64_t last_pages;
static uint32_t last_allocate_type;
static uint32_t last_memory_type;
static unsigned allocate_calls;
static unsigned free_calls;
static VGK_EFI_STATUS allocate_status;
static int firmware_changes_address;

static VGK_EFI_STATUS VGK_EFIAPI fake_allocate(
    uint32_t type, uint32_t memory_type, VGK_EFI_UINTN pages,
    uint64_t *address)
{
    ++allocate_calls;
    last_allocate_type = type;
    last_memory_type = memory_type;
    last_pages = pages;
    last_address = *address;
    if (allocate_status != 0)
        return allocate_status;
    if (firmware_changes_address)
        *address += 4096U;
    return 0;
}

static VGK_EFI_STATUS VGK_EFIAPI fake_free(uint64_t address,
                                            VGK_EFI_UINTN pages)
{
    ++free_calls;
    last_address = address;
    last_pages = pages;
    return 0;
}

int main(void)
{
    VGK_UEFI_ELF_ALLOCATOR allocator = { fake_allocate, fake_free };
    void *mapped = (void *)(uintptr_t)1U;

    allocate_calls = free_calls = 0;
    allocate_status = 0;
    firmware_changes_address = 0;
    assert(vgk_uefi_elf_allocate(&allocator, 0x200000U, 2U, &mapped) == 0);
    assert((uintptr_t)mapped == 0x200000U);
    assert(last_allocate_type == 2U && last_memory_type == 2U);
    assert(last_address == 0x200000U && last_pages == 2U);
    assert(allocate_calls == 1U && free_calls == 0U);

    vgk_uefi_elf_release(&allocator, 0x200000U, 2U);
    assert(free_calls == 1U);
    assert(last_address == 0x200000U && last_pages == 2U);

    mapped = (void *)(uintptr_t)1U;
    allocate_status = UINT64_C(0x8000000000000009);
    assert(vgk_uefi_elf_allocate(&allocator, 0x300000U, 1U, &mapped) != 0);
    assert(mapped == 0);
    assert(free_calls == 1U);

    allocate_status = 0;
    firmware_changes_address = 1;
    assert(vgk_uefi_elf_allocate(&allocator, 0x400000U, 1U, &mapped) != 0);
    assert(mapped == 0);
    assert(free_calls == 2U);
    assert(last_address == 0x401000U);

    firmware_changes_address = 0;
    assert(vgk_uefi_elf_allocate(&allocator, 0, 1U, &mapped) != 0);
    assert(vgk_uefi_elf_allocate(&allocator, 0x200000U, 0, &mapped) != 0);

    puts("OK: UEFI exact-page allocator adapter");
    return 0;
}
