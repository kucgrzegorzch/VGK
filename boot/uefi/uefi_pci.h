#ifndef VGK_UEFI_PCI_H
#define VGK_UEFI_PCI_H
#include <stddef.h>
#include <stdint.h>
#include "uefi_services.h"
#include "vgk_pci.h"
#define VGK_UEFI_PCI_MAX_DEVICES 128U
typedef struct VGK_EFI_PCI_IO_PROTOCOL VGK_EFI_PCI_IO_PROTOCOL;
typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_PCI_CONFIG_ACCESS)(VGK_EFI_PCI_IO_PROTOCOL *, uint32_t, uint32_t, VGK_EFI_UINTN, void *);
typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_PCI_GET_LOCATION)(VGK_EFI_PCI_IO_PROTOCOL *, VGK_EFI_UINTN *, VGK_EFI_UINTN *, VGK_EFI_UINTN *, VGK_EFI_UINTN *);
typedef struct { VGK_EFI_PCI_CONFIG_ACCESS read; VGK_EFI_PCI_CONFIG_ACCESS write; } VGK_EFI_PCI_CONFIG_ACCESS_PAIR;
/* ABI prefix only: firmware slots are preserved; memory/I/O/DMA methods are never invoked. */
struct VGK_EFI_PCI_IO_PROTOCOL {
    void *poll_mem; void *poll_io; void *mem; void *io;
    VGK_EFI_PCI_CONFIG_ACCESS_PAIR pci;
    void *after_pci[6];
    VGK_EFI_PCI_GET_LOCATION get_location;
};
int vgk_uefi_pci_read_identity(VGK_EFI_PCI_IO_PROTOCOL *, VGK_PCI_DEVICE *);
int vgk_uefi_pci_enumerate(VGK_EFI_BOOT_SERVICES_PREFIX *, VGK_PCI_DEVICE *, size_t, size_t *);
_Static_assert(offsetof(VGK_EFI_PCI_IO_PROTOCOL, pci) == 32, "unexpected EFI PCI I/O Pci offset");
_Static_assert(offsetof(VGK_EFI_PCI_IO_PROTOCOL, get_location) == 96, "unexpected EFI PCI I/O GetLocation offset");
#endif
