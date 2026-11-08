#ifndef VGK_UEFI_KERNEL_ENTRY_H
#define VGK_UEFI_KERNEL_ENTRY_H

#include <stdint.h>

#include "uefi_file.h"
#include "vgk_boot_info.h"

/*
 * UEFI x64 calls C functions using the Microsoft x64 ABI. The native VGK
 * kernel entry uses the System V x86-64 convention and expects VGK_BOOT_INFO
 * in RDI. This non-returning assembly bridge performs that register handoff.
 *
 * The kernel entry is responsible for selecting its own private stack before
 * making any C calls. This function must only be called after a successful
 * ExitBootServices; firmware services are no longer available afterwards.
 */
_Noreturn void VGK_EFIAPI vgk_uefi_enter_kernel(
    uint64_t kernel_entry_address,
    const VGK_BOOT_INFO *boot_info
);

#endif
