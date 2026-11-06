#ifndef VGK_UEFI_SERVICES_H
#define VGK_UEFI_SERVICES_H

#include <stddef.h>
#include <stdint.h>

#include "uefi_file.h"

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} VGK_EFI_GUID;

typedef VGK_EFI_STATUS (VGK_EFIAPI *VGK_EFI_HANDLE_PROTOCOL)(
    VGK_EFI_HANDLE handle,
    const VGK_EFI_GUID *protocol,
    void **interface);

typedef struct {
    uint32_t revision;
    uint32_t padding;
    VGK_EFI_HANDLE parent_handle;
    void *system_table;
    VGK_EFI_HANDLE device_handle;
} VGK_EFI_LOADED_IMAGE_PROTOCOL;

/*
 * UEFI Boot Services starts with EFI_TABLE_HEADER followed by function
 * pointers. HandleProtocol is entry 16 (zero based), after 16 preceding
 * service pointers. Keep the reserved slots explicit to preserve the ABI.
 */
typedef struct {
    uint64_t header[3];
    void *before_handle_protocol[16];
    VGK_EFI_HANDLE_PROTOCOL handle_protocol;
} VGK_EFI_BOOT_SERVICES_PREFIX;

_Static_assert(sizeof(VGK_EFI_GUID) == 16, "UEFI GUID must be 16 bytes");
_Static_assert(offsetof(VGK_EFI_LOADED_IMAGE_PROTOCOL, device_handle) == 24,
               "unexpected Loaded Image device handle offset");
_Static_assert(offsetof(VGK_EFI_BOOT_SERVICES_PREFIX, handle_protocol) == 152,
               "unexpected Boot Services HandleProtocol offset");

#endif
